
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/ebitmap.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/services.h>

#include "debug.h"
#include "policydb_validate.h"

#define bool_xor(a, b) (!(a) != !(b))
#define bool_xnor(a, b) !bool_xor(a, b)

typedef struct validate {
	uint32_t nprim;
	ebitmap_t gaps;
} validate_t;

typedef struct map_arg {
	validate_t *flavors;
	sepol_handle_t *handle;
	int mls;
} map_arg_t;

static int create_gap_ebitmap(char **val_to_name, uint32_t nprim, ebitmap_t *gaps)
{
	unsigned int i;

	ebitmap_init(gaps);

	for (i = 0; i < nprim; i++) {
		if (!val_to_name[i]) {
			if (ksu_ebitmap_set_bit(gaps, i, 1))
				return -1;
		}
	}

	return 0;
}

static int validate_init(validate_t *flavor, char **val_to_name, uint32_t nprim)
{
	flavor->nprim = nprim;
	if (create_gap_ebitmap(val_to_name, nprim, &flavor->gaps))
		return -1;

	return 0;
}

static int validate_array_init(policydb_t *p, validate_t flavors[])
{
	if (validate_init(&flavors[SYM_CLASSES], p->p_class_val_to_name, p->p_classes.nprim))
		goto bad;
	if (validate_init(&flavors[SYM_ROLES], p->p_role_val_to_name, p->p_roles.nprim))
		goto bad;
	if (p->policyvers < POLICYDB_VERSION_AVTAB || p->policyvers > POLICYDB_VERSION_PERMISSIVE) {
		if (validate_init(&flavors[SYM_TYPES], p->p_type_val_to_name, p->p_types.nprim))
			goto bad;
	} else {
		/*
		 * For policy versions between 20 and 23, attributes exist in the policy,
		 * but they only exist in the type_attr_map, so there will be references
		 * to gaps and we just have to treat this case as if there were no gaps.
		 */
		flavors[SYM_TYPES].nprim = p->p_types.nprim;
		ebitmap_init(&flavors[SYM_TYPES].gaps);
	}
	if (validate_init(&flavors[SYM_USERS], p->p_user_val_to_name, p->p_users.nprim))
		goto bad;
	if (validate_init(&flavors[SYM_BOOLS], p->p_bool_val_to_name, p->p_bools.nprim))
		goto bad;
	if (validate_init(&flavors[SYM_LEVELS], p->p_sens_val_to_name, p->p_levels.nprim))
		goto bad;
	if (validate_init(&flavors[SYM_CATS], p->p_cat_val_to_name, p->p_cats.nprim))
		goto bad;

	return 0;

bad:
	return -1;
}

/*
 * Functions to validate both kernel and module policydbs
 */

int value_isvalid(uint32_t value, uint32_t nprim)
{
	if (!value || value > nprim)
		return 0;

	return 1;
}

static int validate_value(uint32_t value, validate_t *flavor)
{
	if (!value || value > flavor->nprim)
		goto bad;
	if (ksu_ebitmap_get_bit(&flavor->gaps, value-1))
		goto bad;

	return 0;

bad:
	return -1;
}

static int validate_ebitmap(ebitmap_t *map, validate_t *flavor)
{
	if (ebitmap_length(map) > 0 && ebitmap_highest_set_bit(map) >= flavor->nprim)
		goto bad;
	if (ebitmap_match_any(map, &flavor->gaps))
		goto bad;

	return 0;

bad:
	return -1;
}

static int validate_type_set(type_set_t *type_set, validate_t *type)
{
	if (validate_ebitmap(&type_set->types, type))
		goto bad;
	if (validate_ebitmap(&type_set->negset, type))
		goto bad;

	switch (type_set->flags) {
	case 0:
	case TYPE_STAR:
	case TYPE_COMP:
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	return -1;
}

static int validate_empty_type_set(type_set_t *type_set)
{
	if (!ebitmap_is_empty(&type_set->types))
		goto bad;
	if (!ebitmap_is_empty(&type_set->negset))
		goto bad;
	if (type_set->flags != 0)
		goto bad;

	return 0;

bad:
	return -1;
}

static int validate_role_set(role_set_t *role_set, validate_t *role)
{
	if (validate_ebitmap(&role_set->roles, role))
		goto bad;

	switch (role_set->flags) {
	case 0:
	case ROLE_STAR:
	case ROLE_COMP:
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	return -1;
}

static int validate_scope(__attribute__ ((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	scope_datum_t *scope_datum = (scope_datum_t *)d;
	uint32_t *nprim = (uint32_t *)args;
	unsigned int i;

	switch (scope_datum->scope) {
	case SCOPE_REQ:
	case SCOPE_DECL:
		break;
	default:
		goto bad;
	}

	for (i = 0; i < scope_datum->decl_ids_len; i++) {
		if (!value_isvalid(scope_datum->decl_ids[i], *nprim))
			goto bad;
	}

	return 0;

bad:
	return -1;
}

static int validate_scopes(sepol_handle_t *handle, symtab_t scopes[], avrule_block_t *block)
{
	avrule_decl_t *decl;
	unsigned int i;
	unsigned int num_decls = 0;

	for (; block != NULL; block = block->next) {
		for (decl = block->branch_list; decl; decl = decl->next) {
			num_decls++;
		}
	}

	for (i = 0; i < SYM_NUM; i++) {
		if (ksu_hashtab_map(scopes[i].table, validate_scope, &num_decls))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid scope");
	return -1;
}

static int validate_constraint_nodes(sepol_handle_t *handle, unsigned int nperms, constraint_node_t *cons, validate_t flavors[])
{
	constraint_expr_t *cexp;

	for (; cons; cons = cons->next) {
		if (nperms == 0 && cons->permissions != 0)
			goto bad;
		if (nperms > 0 && cons->permissions == 0)
			goto bad;
		if (nperms > 0 && nperms != PERM_SYMTAB_SIZE && cons->permissions >= (UINT32_C(1) << nperms))
			goto bad;

		for (cexp = cons->expr; cexp; cexp = cexp->next) {
			if (cexp->expr_type == CEXPR_NAMES) {
				if (cexp->attr & CEXPR_XTARGET && nperms != 0)
					goto bad;
				if (!(cexp->attr & CEXPR_TYPE)) {
					if (validate_empty_type_set(cexp->type_names))
						goto bad;
				}

				switch (cexp->op) {
				case CEXPR_EQ:
				case CEXPR_NEQ:
					break;
				default:
					goto bad;
				}

				switch (cexp->attr) {
				case CEXPR_USER:
				case CEXPR_USER | CEXPR_TARGET:
				case CEXPR_USER | CEXPR_XTARGET:
					if (validate_ebitmap(&cexp->names, &flavors[SYM_USERS]))
						goto bad;
					break;
				case CEXPR_ROLE:
				case CEXPR_ROLE | CEXPR_TARGET:
				case CEXPR_ROLE | CEXPR_XTARGET:
					if (validate_ebitmap(&cexp->names, &flavors[SYM_ROLES]))
						goto bad;
					break;
				case CEXPR_TYPE:
				case CEXPR_TYPE | CEXPR_TARGET:
				case CEXPR_TYPE | CEXPR_XTARGET:
					if (validate_ebitmap(&cexp->names, &flavors[SYM_TYPES]))
						goto bad;
					if (validate_type_set(cexp->type_names, &flavors[SYM_TYPES]))
						goto bad;
					break;
				default:
					goto bad;
				}
			} else if (cexp->expr_type == CEXPR_ATTR) {
				if (!ebitmap_is_empty(&cexp->names))
					goto bad;
				if (validate_empty_type_set(cexp->type_names))
					goto bad;

				switch (cexp->op) {
				case CEXPR_EQ:
				case CEXPR_NEQ:
					break;
				case CEXPR_DOM:
				case CEXPR_DOMBY:
				case CEXPR_INCOMP:
					if ((cexp->attr & CEXPR_USER) || (cexp->attr & CEXPR_TYPE))
						goto bad;
					break;
				default:
					goto bad;
				}

				switch (cexp->attr) {
				case CEXPR_USER:
				case CEXPR_ROLE:
				case CEXPR_TYPE:
				case CEXPR_L1L2:
				case CEXPR_L1H2:
				case CEXPR_H1L2:
				case CEXPR_H1H2:
				case CEXPR_L1H1:
				case CEXPR_L2H2:
					break;
				default:
					goto bad;
				}
			} else {
				switch (cexp->expr_type) {
				case CEXPR_NOT:
				case CEXPR_AND:
				case CEXPR_OR:
					break;
				default:
					goto bad;
				}

				if (cexp->op != 0)
					goto bad;
				if (cexp->attr != 0)
					goto bad;
				if (!ebitmap_is_empty(&cexp->names))
					goto bad;
				if (validate_empty_type_set(cexp->type_names))
					goto bad;
			}
		}
	}

	return 0;

bad:
	ERR(handle, "Invalid constraint expr");
	return -1;
}

static int validate_class_datum(sepol_handle_t *handle, class_datum_t *class, validate_t flavors[])
{
	if (validate_value(class->s.value, &flavors[SYM_CLASSES]))
		goto bad;
	if (class->permissions.nprim > PERM_SYMTAB_SIZE)
		goto bad;
	if (validate_constraint_nodes(handle, class->permissions.nprim, class->constraints, flavors))
		goto bad;
	if (validate_constraint_nodes(handle, 0, class->validatetrans, flavors))
		goto bad;

	switch (class->default_user) {
	case 0:
	case DEFAULT_SOURCE:
	case DEFAULT_TARGET:
		break;
	default:
		goto bad;
	}

	switch (class->default_role) {
	case 0:
	case DEFAULT_SOURCE:
	case DEFAULT_TARGET:
		break;
	default:
		goto bad;
	}

	switch (class->default_type) {
	case 0:
	case DEFAULT_SOURCE:
	case DEFAULT_TARGET:
		break;
	default:
		goto bad;
	}

	switch (class->default_range) {
	case 0:
	case DEFAULT_SOURCE_LOW:
	case DEFAULT_SOURCE_HIGH:
	case DEFAULT_SOURCE_LOW_HIGH:
	case DEFAULT_TARGET_LOW:
	case DEFAULT_TARGET_HIGH:
	case DEFAULT_TARGET_LOW_HIGH:
	case DEFAULT_GLBLUB:
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid class datum");
	return -1;
}

static int validate_class_datum_wrapper(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	map_arg_t *margs = args;

	return validate_class_datum(margs->handle, d, margs->flavors);
}

static int validate_common_datum(sepol_handle_t *handle, common_datum_t *common)
{
	if (common->permissions.nprim > PERM_SYMTAB_SIZE)
		goto bad;

	return 0;

bad:
	ERR(handle, "Invalid common class datum");
	return -1;
}

static int validate_common_datum_wrapper(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	map_arg_t *margs = args;

	return validate_common_datum(margs->handle, d);
}

static int validate_role_datum(sepol_handle_t *handle, role_datum_t *role, validate_t flavors[])
{
	if (validate_value(role->s.value, &flavors[SYM_ROLES]))
		goto bad;
	if (validate_ebitmap(&role->dominates, &flavors[SYM_ROLES]))
		goto bad;
	if (validate_type_set(&role->types, &flavors[SYM_TYPES]))
		goto bad;
	if (role->bounds && validate_value(role->bounds, &flavors[SYM_ROLES]))
		goto bad;
	if (validate_ebitmap(&role->roles, &flavors[SYM_ROLES]))
		goto bad;

	return 0;

bad:
	ERR(handle, "Invalid role datum");
	return -1;
}

static int validate_role_datum_wrapper(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	map_arg_t *margs = args;

	return validate_role_datum(margs->handle, d, margs->flavors);
}

static int validate_type_datum(sepol_handle_t *handle, type_datum_t *type, validate_t flavors[])
{
	if (validate_value(type->s.value, &flavors[SYM_TYPES]))
		goto bad;
	if (validate_ebitmap(&type->types, &flavors[SYM_TYPES]))
		goto bad;
	if (type->bounds && validate_value(type->bounds, &flavors[SYM_TYPES]))
		goto bad;

	switch (type->flavor) {
	case TYPE_TYPE:
	case TYPE_ATTRIB:
	case TYPE_ALIAS:
		break;
	default:
		goto bad;
	}

	switch (type->flags) {
	case 0:
	case TYPE_FLAGS_PERMISSIVE:
	case TYPE_FLAGS_EXPAND_ATTR_TRUE:
	case TYPE_FLAGS_EXPAND_ATTR_FALSE:
	case TYPE_FLAGS_EXPAND_ATTR:
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid type datum");
	return -1;
}

static int validate_type_datum_wrapper(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	map_arg_t *margs = args;

	return validate_type_datum(margs->handle, d, margs->flavors);
}

static int validate_mls_semantic_cat(mls_semantic_cat_t *cat, validate_t *cats)
{
	for (; cat; cat = cat->next) {
		if (validate_value(cat->low, cats))
			goto bad;
		if (validate_value(cat->high, cats))
			goto bad;
	}

	return 0;

bad:
	return -1;
}

static int validate_mls_semantic_level(mls_semantic_level_t *level, validate_t *sens, validate_t *cats)
{
	if (level->sens == 0)
		return 0;
	if (validate_value(level->sens, sens))
		goto bad;
	if (validate_mls_semantic_cat(level->cat, cats))
		goto bad;

	return 0;

bad:
	return -1;
}

static int validate_mls_semantic_range(mls_semantic_range_t *range, validate_t *sens, validate_t *cats)
{
	if (validate_mls_semantic_level(&range->level[0], sens, cats))
		goto bad;
	if (validate_mls_semantic_level(&range->level[1], sens, cats))
		goto bad;

	return 0;

bad:
	return -1;
}

static int validate_mls_level(mls_level_t *level, validate_t *sens, validate_t *cats)
{
	if (validate_value(level->sens, sens))
		goto bad;
	if (validate_ebitmap(&level->cat, cats))
		goto bad;

	return 0;

	bad:
	return -1;
}

static int validate_level_datum(__attribute__ ((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	level_datum_t *level = d;
	validate_t *flavors = args;

	return validate_mls_level(level->level, &flavors[SYM_LEVELS], &flavors[SYM_CATS]);
}

static int validate_mls_range(mls_range_t *range, validate_t *sens, validate_t *cats)
{
	if (validate_mls_level(&range->level[0], sens, cats))
		goto bad;
	if (validate_mls_level(&range->level[1], sens, cats))
		goto bad;

	return 0;

	bad:
	return -1;
}

static int validate_user_datum(sepol_handle_t *handle, user_datum_t *user, validate_t flavors[], int mls)
{
	if (validate_value(user->s.value, &flavors[SYM_USERS]))
		goto bad;
	if (validate_role_set(&user->roles, &flavors[SYM_ROLES]))
		goto bad;
	if (validate_mls_semantic_range(&user->range, &flavors[SYM_LEVELS], &flavors[SYM_CATS]))
		goto bad;
	if (validate_mls_semantic_level(&user->dfltlevel, &flavors[SYM_LEVELS], &flavors[SYM_CATS]))
		goto bad;
	if (mls && validate_mls_range(&user->exp_range, &flavors[SYM_LEVELS], &flavors[SYM_CATS]))
		goto bad;
	if (mls && validate_mls_level(&user->exp_dfltlevel, &flavors[SYM_LEVELS], &flavors[SYM_CATS]))
		goto bad;
	if (user->bounds && validate_value(user->bounds, &flavors[SYM_USERS]))
		goto bad;

	return 0;

bad:
	ERR(handle, "Invalid user datum");
	return -1;
}

static int validate_user_datum_wrapper(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	map_arg_t *margs = args;

	return validate_user_datum(margs->handle, d, margs->flavors, margs->mls);
}

static int validate_bool_datum(sepol_handle_t *handle, cond_bool_datum_t *boolean, validate_t flavors[])
{
	if (validate_value(boolean->s.value, &flavors[SYM_BOOLS]))
		goto bad;

	switch (boolean->state) {
	case 0:
	case 1:
		break;
	default:
		goto bad;
	}

	switch (boolean->flags) {
	case 0:
	case COND_BOOL_FLAGS_TUNABLE:
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid bool datum");
	return -1;
}

static int validate_bool_datum_wrapper(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	map_arg_t *margs = args;

	return validate_bool_datum(margs->handle, d, margs->flavors);
}

static int validate_datum_array_gaps(sepol_handle_t *handle, policydb_t *p, validate_t flavors[])
{
	unsigned int i;

	for (i = 0; i < p->p_classes.nprim; i++) {
		if (bool_xnor(p->class_val_to_struct[i], ksu_ebitmap_get_bit(&flavors[SYM_CLASSES].gaps, i)))
			goto bad;
	}

	for (i = 0; i < p->p_roles.nprim; i++) {
		if (bool_xnor(p->role_val_to_struct[i], ksu_ebitmap_get_bit(&flavors[SYM_ROLES].gaps, i)))
			goto bad;
	}

	/*
	 * For policy versions between 20 and 23, attributes exist in the policy,
	 * but only in the type_attr_map, so all gaps must be assumed to be valid.
	 */
	if (p->policyvers < POLICYDB_VERSION_AVTAB || p->policyvers > POLICYDB_VERSION_PERMISSIVE) {
		for (i = 0; i < p->p_types.nprim; i++) {
			if (bool_xnor(p->type_val_to_struct[i], ksu_ebitmap_get_bit(&flavors[SYM_TYPES].gaps, i)))
				goto bad;
		}
	}

	for (i = 0; i < p->p_users.nprim; i++) {
		if (bool_xnor(p->user_val_to_struct[i], ksu_ebitmap_get_bit(&flavors[SYM_USERS].gaps, i)))
			goto bad;
	}

	for (i = 0; i < p->p_bools.nprim; i++) {
		if (bool_xnor(p->bool_val_to_struct[i], ksu_ebitmap_get_bit(&flavors[SYM_BOOLS].gaps, i)))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid datum array gaps");
	return -1;
}

static int validate_datum(__attribute__ ((unused))hashtab_key_t k, hashtab_datum_t d, void *args)
{
	symtab_datum_t *s = d;
	uint32_t *nprim = (uint32_t *)args;

	return !value_isvalid(s->value, *nprim);
}

static int validate_datum_array_entries(sepol_handle_t *handle, policydb_t *p, validate_t flavors[])
{
	map_arg_t margs = { flavors, handle, p->mls };

	if (ksu_hashtab_map(p->p_commons.table, validate_common_datum_wrapper, &margs))
		goto bad;

	if (ksu_hashtab_map(p->p_classes.table, validate_class_datum_wrapper, &margs))
		goto bad;

	if (ksu_hashtab_map(p->p_roles.table, validate_role_datum_wrapper, &margs))
		goto bad;

	if (ksu_hashtab_map(p->p_types.table, validate_type_datum_wrapper, &margs))
		goto bad;

	if (ksu_hashtab_map(p->p_users.table, validate_user_datum_wrapper, &margs))
		goto bad;

	if (p->mls && ksu_hashtab_map(p->p_levels.table, validate_level_datum, flavors))
		goto bad;

	if (ksu_hashtab_map(p->p_cats.table, validate_datum, &flavors[SYM_CATS]))
		goto bad;

	if (ksu_hashtab_map(p->p_bools.table, validate_bool_datum_wrapper, &margs))
		goto bad;

	return 0;

bad:
	ERR(handle, "Invalid datum array entries");
	return -1;
}

/*
 * Functions to validate a kernel policydb
 */

static int validate_avtab_key(avtab_key_t *key, int conditional, validate_t flavors[])
{
	if (validate_value(key->source_type, &flavors[SYM_TYPES]))
		goto bad;
	if (validate_value(key->target_type, &flavors[SYM_TYPES]))
		goto bad;
	if (validate_value(key->target_class, &flavors[SYM_CLASSES]))
		goto bad;
	switch (0xFFF & key->specified) {
	case AVTAB_ALLOWED:
	case AVTAB_AUDITALLOW:
	case AVTAB_AUDITDENY:
	case AVTAB_TRANSITION:
	case AVTAB_MEMBER:
	case AVTAB_CHANGE:
		break;
	case AVTAB_XPERMS_ALLOWED:
	case AVTAB_XPERMS_AUDITALLOW:
	case AVTAB_XPERMS_DONTAUDIT:
		if (conditional)
			goto bad;
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	return -1;
}

static int validate_avtab_key_and_datum(avtab_key_t *k, avtab_datum_t *d, void *args)
{
	validate_t *flavors = (validate_t *)args;

	if (validate_avtab_key(k, 0, flavors))
		return -1;

	if ((k->specified & AVTAB_TYPE) && validate_value(d->data, &flavors[SYM_TYPES]))
		return -1;

	return 0;
}

static int validate_avtab(sepol_handle_t *handle, avtab_t *avtab, validate_t flavors[])
{
	if (avtab_map(avtab, validate_avtab_key_and_datum, flavors)) {
		ERR(handle, "Invalid avtab");
		return -1;
	}

	return 0;
}

static int validate_cond_av_list(sepol_handle_t *handle, cond_av_list_t *cond_av, validate_t flavors[])
{
	avtab_ptr_t avtab_ptr;

	for (; cond_av; cond_av = cond_av->next) {
		for (avtab_ptr = cond_av->node; avtab_ptr; avtab_ptr = avtab_ptr->next) {
			if (validate_avtab_key(&avtab_ptr->key, 1, flavors)) {
				ERR(handle, "Invalid cond av list");
				return -1;
			}
		}
	}

	return 0;
}

static int validate_avrules(sepol_handle_t *handle, avrule_t *avrule, int conditional, validate_t flavors[])
{
	class_perm_node_t *class;

	for (; avrule; avrule = avrule->next) {
		if (validate_type_set(&avrule->stypes, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_type_set(&avrule->ttypes, &flavors[SYM_TYPES]))
			goto bad;
		class = avrule->perms;
		for (; class; class = class->next) {
			if (validate_value(class->tclass, &flavors[SYM_CLASSES]))
				goto bad;
		}

		switch(avrule->specified) {
		case AVRULE_ALLOWED:
		case AVRULE_AUDITALLOW:
		case AVRULE_AUDITDENY:
		case AVRULE_DONTAUDIT:
		case AVRULE_TRANSITION:
		case AVRULE_MEMBER:
		case AVRULE_CHANGE:
			break;
		case AVRULE_NEVERALLOW:
		case AVRULE_XPERMS_ALLOWED:
		case AVRULE_XPERMS_AUDITALLOW:
		case AVRULE_XPERMS_DONTAUDIT:
		case AVRULE_XPERMS_NEVERALLOW:
			if (conditional)
				goto bad;
			break;
		default:
			goto bad;
		}

		if (avrule->specified & AVRULE_XPERMS) {
			if (!avrule->xperms)
				goto bad;
			switch (avrule->xperms->specified) {
			case AVRULE_XPERMS_IOCTLFUNCTION:
			case AVRULE_XPERMS_IOCTLDRIVER:
				break;
			default:
				goto bad;
			}
		} else if (avrule->xperms)
			goto bad;

		switch(avrule->flags) {
		case 0:
		case RULE_SELF:
			break;
		default:
			goto bad;
		}
	}

	return 0;

bad:
	ERR(handle, "Invalid avrule");
	return -1;
}

static int validate_bool_id_array(sepol_handle_t *handle, uint32_t bool_ids[], unsigned int nbools, validate_t *bool)
{
	unsigned int i;

	if (nbools >= COND_MAX_BOOLS)
		goto bad;

	for (i=0; i < nbools; i++) {
		if (validate_value(bool_ids[i], bool))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid bool id array");
	return -1;
}

static int validate_cond_expr(sepol_handle_t *handle, struct cond_expr *expr, validate_t *bool)
{
	int depth = -1;

	for (; expr; expr = expr->next) {
		switch(expr->expr_type) {
		case COND_BOOL:
			if (validate_value(expr->bool, bool))
				goto bad;
			if (depth == (COND_EXPR_MAXDEPTH - 1))
				goto bad;
			depth++;
			break;
		case COND_NOT:
			if (depth < 0)
				goto bad;
			break;
		case COND_OR:
		case COND_AND:
		case COND_XOR:
		case COND_EQ:
		case COND_NEQ:
			if (depth < 1)
				goto bad;
			depth--;
			break;
		default:
			goto bad;
		}
	}

	if (depth != 0)
		goto bad;

	return 0;

bad:
	ERR(handle, "Invalid cond expression");
	return -1;
}

static int validate_cond_list(sepol_handle_t *handle, cond_list_t *cond, validate_t flavors[])
{
	for (; cond; cond = cond->next) {
		if (validate_cond_expr(handle, cond->expr, &flavors[SYM_BOOLS]))
			goto bad;
		if (validate_cond_av_list(handle, cond->true_list, flavors))
			goto bad;
		if (validate_cond_av_list(handle, cond->false_list, flavors))
			goto bad;
		if (validate_avrules(handle, cond->avtrue_list, 1, flavors))
			goto bad;
		if (validate_avrules(handle, cond->avfalse_list, 1, flavors))
			goto bad;
		if (validate_bool_id_array(handle, cond->bool_ids, cond->nbools, &flavors[SYM_BOOLS]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid cond list");
	return -1;
}

static int validate_role_transes(sepol_handle_t *handle, role_trans_t *role_trans, validate_t flavors[])
{
	for (; role_trans; role_trans = role_trans->next) {
		if (validate_value(role_trans->role, &flavors[SYM_ROLES]))
			goto bad;
		if (validate_value(role_trans->type, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_value(role_trans->tclass, &flavors[SYM_CLASSES]))
			goto bad;
		if (validate_value(role_trans->new_role, &flavors[SYM_ROLES]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid role trans");
	return -1;
}

static int validate_role_allows(sepol_handle_t *handle, role_allow_t *role_allow, validate_t flavors[])
{
	for (; role_allow; role_allow = role_allow->next) {
		if (validate_value(role_allow->role, &flavors[SYM_ROLES]))
			goto bad;
		if (validate_value(role_allow->new_role, &flavors[SYM_ROLES]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid role allow");
	return -1;
}

static int validate_filename_trans(hashtab_key_t k, hashtab_datum_t d, void *args)
{
	filename_trans_key_t *ftk = (filename_trans_key_t *)k;
	filename_trans_datum_t *ftd = d;
	validate_t *flavors = (validate_t *)args;

	if (validate_value(ftk->ttype, &flavors[SYM_TYPES]))
		goto bad;
	if (validate_value(ftk->tclass, &flavors[SYM_CLASSES]))
		goto bad;
	for (; ftd; ftd = ftd->next) {
		if (validate_ebitmap(&ftd->stypes, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_value(ftd->otype, &flavors[SYM_TYPES]))
			goto bad;
	}

	return 0;

bad:
	return -1;
}

static int validate_filename_trans_hashtab(sepol_handle_t *handle, hashtab_t filename_trans, validate_t flavors[])
{
	if (ksu_hashtab_map(filename_trans, validate_filename_trans, flavors)) {
		ERR(handle, "Invalid filename trans");
		return -1;
	}

	return 0;
}

static int validate_context(context_struct_t *con, validate_t flavors[], int mls)
{
	if (validate_value(con->user, &flavors[SYM_USERS]))
		return -1;
	if (validate_value(con->role, &flavors[SYM_ROLES]))
		return -1;
	if (validate_value(con->type, &flavors[SYM_TYPES]))
		return -1;
	if (mls && validate_mls_range(&con->range, &flavors[SYM_LEVELS], &flavors[SYM_CATS]))
		return -1;

	return 0;
}

static int validate_ocontexts(sepol_handle_t *handle, policydb_t *p, validate_t flavors[])
{
	ocontext_t *octx;
	unsigned int i;

	for (i = 0; i < OCON_NUM; i++) {
		for (octx = p->ocontexts[i]; octx; octx = octx->next) {
			if (validate_context(&octx->context[0], flavors, p->mls))
				goto bad;

			if (p->target_platform == SEPOL_TARGET_SELINUX) {
				switch (i) {
				case OCON_FS:
				case OCON_NETIF:
					if (validate_context(&octx->context[1], flavors, p->mls))
						goto bad;
					break;
				case OCON_FSUSE:
					switch (octx->v.behavior) {
					case SECURITY_FS_USE_XATTR:
					case SECURITY_FS_USE_TRANS:
					case SECURITY_FS_USE_TASK:
						break;
					default:
						goto bad;
					}
				}
			}
		}
	}

	return 0;

bad:
	ERR(handle, "Invalid ocontext");
	return -1;
}

static int validate_genfs(sepol_handle_t *handle, policydb_t *p, validate_t flavors[])
{
	genfs_t *genfs;
	ocontext_t *octx;

	for (genfs = p->genfs; genfs; genfs = genfs->next) {
		for (octx = genfs->head; octx; octx = octx->next) {
			if (validate_context(&octx->context[0], flavors, p->mls))
				goto bad;
		}
	}

	return 0;

bad:
	ERR(handle, "Invalid genfs");
	return -1;
}

/*
 * Functions to validate a module policydb
 */

static int validate_role_trans_rules(sepol_handle_t *handle, role_trans_rule_t *role_trans, validate_t flavors[])
{
	for (; role_trans; role_trans = role_trans->next) {
		if (validate_role_set(&role_trans->roles, &flavors[SYM_ROLES]))
			goto bad;
		if (validate_type_set(&role_trans->types, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_ebitmap(&role_trans->classes, &flavors[SYM_CLASSES]))
			goto bad;
		if (validate_value(role_trans->new_role, &flavors[SYM_ROLES]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid role trans rule");
	return -1;
}

static int validate_role_allow_rules(sepol_handle_t *handle, role_allow_rule_t *role_allow, validate_t flavors[])
{
	for (; role_allow; role_allow = role_allow->next) {
		if (validate_role_set(&role_allow->roles, &flavors[SYM_ROLES]))
			goto bad;
		if (validate_role_set(&role_allow->new_roles, &flavors[SYM_ROLES]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid role allow rule");
	return -1;
}

static int validate_range_trans_rules(sepol_handle_t *handle, range_trans_rule_t *range_trans, validate_t flavors[])
{
	for (; range_trans; range_trans = range_trans->next) {
		if (validate_type_set(&range_trans->stypes, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_type_set(&range_trans->ttypes, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_ebitmap(&range_trans->tclasses, &flavors[SYM_CLASSES]))
			goto bad;
		if (validate_mls_semantic_range(&range_trans->trange, &flavors[SYM_LEVELS], &flavors[SYM_CATS]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid range trans rule");
	return -1;
}

static int validate_scope_index(sepol_handle_t *handle, scope_index_t *scope_index, validate_t flavors[])
{
	if (validate_ebitmap(&scope_index->p_classes_scope, &flavors[SYM_CLASSES]))
		goto bad;
	if (validate_ebitmap(&scope_index->p_roles_scope, &flavors[SYM_ROLES]))
		goto bad;
	if (validate_ebitmap(&scope_index->p_types_scope, &flavors[SYM_TYPES]))
		goto bad;
	if (validate_ebitmap(&scope_index->p_users_scope, &flavors[SYM_USERS]))
		goto bad;
	if (validate_ebitmap(&scope_index->p_bools_scope, &flavors[SYM_BOOLS]))
		goto bad;
	if (validate_ebitmap(&scope_index->p_sens_scope, &flavors[SYM_LEVELS]))
		goto bad;
	if (validate_ebitmap(&scope_index->p_cat_scope, &flavors[SYM_CATS]))
		goto bad;
	if (scope_index->class_perms_len > flavors[SYM_CLASSES].nprim)
		goto bad;

	return 0;

bad:
	ERR(handle, "Invalid scope");
	return -1;
}


static int validate_filename_trans_rules(sepol_handle_t *handle, filename_trans_rule_t *filename_trans, validate_t flavors[])
{
	for (; filename_trans; filename_trans = filename_trans->next) {
		if (validate_type_set(&filename_trans->stypes, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_type_set(&filename_trans->ttypes, &flavors[SYM_TYPES]))
			goto bad;
		if (validate_value(filename_trans->tclass,&flavors[SYM_CLASSES] ))
			goto bad;
		if (validate_value(filename_trans->otype, &flavors[SYM_TYPES]))
			goto bad;

		/* currently only the RULE_SELF flag can be set */
		if ((filename_trans->flags & ~RULE_SELF) != 0)
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid filename trans rule list");
	return -1;
}

static int validate_symtabs(sepol_handle_t *handle, symtab_t symtabs[], validate_t flavors[])
{
	unsigned int i;

	for (i = 0; i < SYM_NUM; i++) {
		if (ksu_hashtab_map(symtabs[i].table, validate_datum, &flavors[i].nprim)) {
			ERR(handle, "Invalid symtab");
			return -1;
		}
	}

	return 0;
}

static int validate_avrule_blocks(sepol_handle_t *handle, avrule_block_t *avrule_block, validate_t flavors[])
{
	avrule_decl_t *decl;

	for (; avrule_block; avrule_block = avrule_block->next) {
		for (decl = avrule_block->branch_list; decl != NULL; decl = decl->next) {
			if (validate_cond_list(handle, decl->cond_list, flavors))
				goto bad;
			if (validate_avrules(handle, decl->avrules, 0, flavors))
				goto bad;
			if (validate_role_trans_rules(handle, decl->role_tr_rules, flavors))
				goto bad;
			if (validate_role_allow_rules(handle, decl->role_allow_rules, flavors))
				goto bad;
			if (validate_range_trans_rules(handle, decl->range_tr_rules, flavors))
				goto bad;
			if (validate_scope_index(handle, &decl->required, flavors))
				goto bad;
			if (validate_scope_index(handle, &decl->declared, flavors))
				goto bad;
			if (validate_filename_trans_rules(handle, decl->filename_trans_rules, flavors))
				goto bad;
			if (validate_symtabs(handle, decl->symtab, flavors))
				goto bad;
		}

		switch (avrule_block->flags) {
		case 0:
		case AVRULE_OPTIONAL:
			break;
		default:
			goto bad;
		}
	}

	return 0;

bad:
	ERR(handle, "Invalid avrule block");
	return -1;
}

static int validate_permissives(sepol_handle_t *handle, policydb_t *p, validate_t flavors[])
{
	ebitmap_node_t *node;
	unsigned i;

	ebitmap_for_each_positive_bit(&p->permissive_map, node, i) {
		if (validate_value(i, &flavors[SYM_TYPES]))
			goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid permissive type");
	return -1;
}

static int validate_properties(sepol_handle_t *handle, policydb_t *p)
{
	switch (p->policy_type) {
	case POLICY_KERN:
		if (p->policyvers < POLICYDB_VERSION_MIN || p->policyvers > POLICYDB_VERSION_MAX)
			goto bad;
		break;
	case POLICY_BASE:
	case POLICY_MOD:
		if (p->policyvers < MOD_POLICYDB_VERSION_MIN || p->policyvers > MOD_POLICYDB_VERSION_MAX)
			goto bad;
		break;
	default:
		goto bad;
	}

	switch (p->target_platform) {
	case SEPOL_TARGET_SELINUX:
	case SEPOL_TARGET_XEN:
		break;
	default:
		goto bad;
	}

	switch (p->mls) {
	case 0:
	case 1:
		break;
	default:
		goto bad;
	}

	switch (p->handle_unknown) {
	case SEPOL_DENY_UNKNOWN:
	case SEPOL_REJECT_UNKNOWN:
	case SEPOL_ALLOW_UNKNOWN:
		break;
	default:
		goto bad;
	}

	return 0;

bad:
	ERR(handle, "Invalid policy property");
	return -1;
}

static void validate_array_destroy(validate_t flavors[])
{
	unsigned int i;

	for (i = 0; i < SYM_NUM; i++) {
		ksu_ebitmap_destroy(&flavors[i].gaps);
	}
}

/*
 * Validate policydb
 */
int validate_policydb(sepol_handle_t *handle, policydb_t *p)
{
	validate_t flavors[SYM_NUM] = {};

	if (validate_array_init(p, flavors))
		goto bad;

	if (validate_properties(handle, p))
		goto bad;

	if (p->policy_type == POLICY_KERN) {
		if (validate_avtab(handle, &p->te_avtab, flavors))
			goto bad;
		if (p->policyvers >= POLICYDB_VERSION_BOOL)
			if (validate_cond_list(handle, p->cond_list, flavors))
				goto bad;
		if (validate_role_transes(handle, p->role_tr, flavors))
			goto bad;
		if (validate_role_allows(handle, p->role_allow, flavors))
			goto bad;
		if (p->policyvers >= POLICYDB_VERSION_FILENAME_TRANS)
			if (validate_filename_trans_hashtab(handle, p->filename_trans, flavors))
				goto bad;
	} else {
		if (validate_avrule_blocks(handle, p->global, flavors))
			goto bad;
	}

	if (validate_ocontexts(handle, p, flavors))
		goto bad;

	if (validate_genfs(handle, p, flavors))
		goto bad;

	if (validate_scopes(handle, p->scope, p->global))
		goto bad;

	if (validate_datum_array_gaps(handle, p, flavors))
		goto bad;

	if (validate_datum_array_entries(handle, p, flavors))
		goto bad;

	if (validate_permissives(handle, p, flavors))
		goto bad;

	validate_array_destroy(flavors);

	return 0;

bad:
	ERR(handle, "Invalid policydb");
	validate_array_destroy(flavors);
	return -1;
}

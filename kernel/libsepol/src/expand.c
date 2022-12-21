/* Authors: Karl MacMillan <kmacmillan@mentalrootkit.com>
 *          Jason Tang <jtang@tresys.com>
 *	    Joshua Brindle <jbrindle@tresys.com>
 *
 * Copyright (C) 2004-2005 Tresys Technology, LLC
 * Copyright (C) 2007 Red Hat, Inc.
 * Copyright (C) 2017 Mellanox Technologies, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "context.h"
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/hierarchy.h>
#include <sepol/policydb/avrule_block.h>

// #include <stdlib.h>
// #include <stdarg.h>
// #include <stdio.h>
#include <linux/string.h>
// #include <assert.h>
// #include <inttypes.h>
#include <linux/types.h>

#include "debug.h"
#include "private.h"

typedef struct expand_state {
	int verbose;
	uint32_t *typemap;
	uint32_t *boolmap;
	uint32_t *rolemap;
	uint32_t *usermap;
	policydb_t *base;
	policydb_t *out;
	sepol_handle_t *handle;
	int expand_neverallow;
} expand_state_t;

static void expand_state_init(expand_state_t * state)
{
	memset(state, 0, sizeof(expand_state_t));
}

static int map_ebitmap(ebitmap_t * src, ebitmap_t * dst, uint32_t * map)
{
	unsigned int i;
	ebitmap_node_t *tnode;
	ebitmap_init(dst);

	ebitmap_for_each_positive_bit(src, tnode, i) {
		if (!map[i])
			continue;
		if (ksu_ebitmap_set_bit(dst, map[i] - 1, 1))
			return -1;
	}
	return 0;
}

static int ebitmap_expand_roles(policydb_t *p, ebitmap_t *roles)
{
	ebitmap_node_t *node;
	unsigned int bit;
	role_datum_t *role;
	ebitmap_t tmp;

	ebitmap_init(&tmp);
	ebitmap_for_each_positive_bit(roles, node, bit) {
		role = p->role_val_to_struct[bit];
		assert(role);
		if (role->flavor != ROLE_ATTRIB) {
			if (ksu_ebitmap_set_bit(&tmp, bit, 1)) {
				ksu_ebitmap_destroy(&tmp);
				return -1;
			}
		} else {
			if (ebitmap_union(&tmp, &role->roles)) {
				ksu_ebitmap_destroy(&tmp);
				return -1;
			}
		}
	}
	ksu_ebitmap_destroy(roles);
	if (ksu_ebitmap_cpy(roles, &tmp)) {
		ksu_ebitmap_destroy(&tmp);
		return -1;
	}
	ksu_ebitmap_destroy(&tmp);
	return 0;
}

static int type_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id, *new_id;
	type_datum_t *type, *new_type;
	expand_state_t *state;

	id = (char *)key;
	type = (type_datum_t *) datum;
	state = (expand_state_t *) data;

	if ((type->flavor == TYPE_TYPE && !type->primary)
	    || type->flavor == TYPE_ALIAS) {
		/* aliases are handled later */
		return 0;
	}
	if (!is_id_enabled(id, state->base, SYM_TYPES)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying type or attribute %s", id);

	new_id = strdup(id);
	if (new_id == NULL) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	new_type = (type_datum_t *) malloc(sizeof(type_datum_t));
	if (!new_type) {
		ERR(state->handle, "Out of memory!");
		free(new_id);
		return SEPOL_ENOMEM;
	}
	memset(new_type, 0, sizeof(type_datum_t));

	new_type->flavor = type->flavor;
	new_type->flags = type->flags;
	new_type->s.value = ++state->out->p_types.nprim;
	if (new_type->s.value > UINT16_MAX) {
		free(new_id);
		free(new_type);
		ERR(state->handle, "type space overflow");
		return -1;
	}
	new_type->primary = 1;
	state->typemap[type->s.value - 1] = new_type->s.value;

	ret = hashtab_insert(state->out->p_types.table,
			     (hashtab_key_t) new_id,
			     (hashtab_datum_t) new_type);
	if (ret) {
		free(new_id);
		free(new_type);
		ERR(state->handle, "hashtab overflow");
		return -1;
	}

	if (new_type->flags & TYPE_FLAGS_PERMISSIVE)
		if (ksu_ebitmap_set_bit(&state->out->permissive_map, new_type->s.value, 1)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

	return 0;
}

static int attr_convert_callback(hashtab_key_t key, hashtab_datum_t datum,
				 void *data)
{
	char *id;
	type_datum_t *type, *new_type;
	expand_state_t *state;
	ebitmap_t tmp_union;

	id = (char *)key;
	type = (type_datum_t *) datum;
	state = (expand_state_t *) data;

	if (type->flavor != TYPE_ATTRIB)
		return 0;

	if (!is_id_enabled(id, state->base, SYM_TYPES)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "converting attribute %s", id);

	new_type = hashtab_search(state->out->p_types.table, id);
	if (!new_type) {
		ERR(state->handle, "attribute %s vanished!", id);
		return -1;
	}
	if (map_ebitmap(&type->types, &tmp_union, state->typemap)) {
		ERR(state->handle, "out of memory");
		return -1;
	}

	/* then union tmp_union onto &new_type->types */
	if (ebitmap_union(&new_type->types, &tmp_union)) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	ksu_ebitmap_destroy(&tmp_union);

	return 0;
}

static int perm_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id, *new_id;
	symtab_t *s;
	perm_datum_t *perm, *new_perm;

	id = key;
	perm = (perm_datum_t *) datum;
	s = (symtab_t *) data;

	new_perm = (perm_datum_t *) malloc(sizeof(perm_datum_t));
	if (!new_perm) {
		return -1;
	}
	memset(new_perm, 0, sizeof(perm_datum_t));

	new_id = strdup(id);
	if (!new_id) {
		free(new_perm);
		return -1;
	}

	new_perm->s.value = perm->s.value;
	s->nprim++;

	ret = hashtab_insert(s->table, new_id, (hashtab_datum_t) new_perm);
	if (ret) {
		free(new_id);
		free(new_perm);
		return -1;
	}

	return 0;
}

static int common_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
				void *data)
{
	int ret;
	char *id, *new_id;
	common_datum_t *common, *new_common;
	expand_state_t *state;

	id = (char *)key;
	common = (common_datum_t *) datum;
	state = (expand_state_t *) data;

	if (state->verbose)
		INFO(state->handle, "copying common %s", id);

	new_common = (common_datum_t *) malloc(sizeof(common_datum_t));
	if (!new_common) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	memset(new_common, 0, sizeof(common_datum_t));
	if (ksu_symtab_init(&new_common->permissions, PERM_SYMTAB_SIZE)) {
		ERR(state->handle, "Out of memory!");
		free(new_common);
		return -1;
	}

	new_id = strdup(id);
	if (!new_id) {
		ERR(state->handle, "Out of memory!");
		/* free memory created by ksu_symtab_init first, then free new_common */
		symtab_destroy(&new_common->permissions);
		free(new_common);
		return -1;
	}

	new_common->s.value = common->s.value;
	state->out->p_commons.nprim++;

	ret =
	    hashtab_insert(state->out->p_commons.table, new_id,
			   (hashtab_datum_t) new_common);
	if (ret) {
		ERR(state->handle, "hashtab overflow");
		free(new_common);
		free(new_id);
		return -1;
	}

	if (ksu_hashtab_map
	    (common->permissions.table, perm_copy_callback,
	     &new_common->permissions)) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	return 0;
}

static int constraint_node_clone(constraint_node_t ** dst,
				 constraint_node_t * src,
				 expand_state_t * state)
{
	constraint_node_t *new_con = NULL, *last_new_con = NULL;
	constraint_expr_t *new_expr = NULL;
	*dst = NULL;
	while (src != NULL) {
		constraint_expr_t *expr, *expr_l = NULL;
		new_con =
		    (constraint_node_t *) malloc(sizeof(constraint_node_t));
		if (!new_con) {
			goto out_of_mem;
		}
		memset(new_con, 0, sizeof(constraint_node_t));
		new_con->permissions = src->permissions;
		for (expr = src->expr; expr; expr = expr->next) {
			if ((new_expr = calloc(1, sizeof(*new_expr))) == NULL) {
				goto out_of_mem;
			}
			if (constraint_expr_init(new_expr) == -1) {
				goto out_of_mem;
			}
			new_expr->expr_type = expr->expr_type;
			new_expr->attr = expr->attr;
			new_expr->op = expr->op;
			if (new_expr->expr_type == CEXPR_NAMES) {
				if (new_expr->attr & CEXPR_TYPE) {
					/*
					 * Copy over constraint policy source types and/or
					 * attributes for sepol_compute_av_reason_buffer(3)
					 * so that utilities can analyse constraint errors.
					 */
					if (map_ebitmap(&expr->type_names->types,
							&new_expr->type_names->types,
							state->typemap)) {
						ERR(NULL, "Failed to map type_names->types");
						goto out_of_mem;
					}
					/* Type sets require expansion and conversion. */
					if (expand_convert_type_set(state->out,
								    state->
								    typemap,
								    expr->
								    type_names,
								    &new_expr->
								    names, 1)) {
						goto out_of_mem;
					}
				} else if (new_expr->attr & CEXPR_ROLE) {
					if (map_ebitmap(&expr->names, &new_expr->names, state->rolemap)) {
						goto out_of_mem;
					}
					if (ebitmap_expand_roles(state->out, &new_expr->names)) {
						goto out_of_mem;
					}
				} else if (new_expr->attr & CEXPR_USER) {
					if (map_ebitmap(&expr->names, &new_expr->names, state->usermap)) {
						goto out_of_mem;
					}
				} else {
					/* Other kinds of sets do not. */
					if (ksu_ebitmap_cpy(&new_expr->names,
							&expr->names)) {
						goto out_of_mem;
					}
				}
			}
			if (expr_l) {
				expr_l->next = new_expr;
			} else {
				new_con->expr = new_expr;
			}
			expr_l = new_expr;
			new_expr = NULL;
		}
		if (last_new_con == NULL) {
			*dst = new_con;
		} else {
			last_new_con->next = new_con;
		}
		last_new_con = new_con;
		src = src->next;
	}

	return 0;
      out_of_mem:
	ERR(state->handle, "Out of memory!");
	if (new_con)
		free(new_con);
	constraint_expr_destroy(new_expr);
	return -1;
}

static int class_copy_default_new_object(expand_state_t *state,
					 class_datum_t *olddatum,
					 class_datum_t *newdatum)
{
	if (olddatum->default_user) {
		if (newdatum->default_user && olddatum->default_user != newdatum->default_user) {
			ERR(state->handle, "Found conflicting default user definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_user = olddatum->default_user;

	}
	if (olddatum->default_role) {
		if (newdatum->default_role && olddatum->default_role != newdatum->default_role) {
			ERR(state->handle, "Found conflicting default role definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_role = olddatum->default_role;
	}
	if (olddatum->default_type) {
		if (newdatum->default_type && olddatum->default_type != newdatum->default_type) {
			ERR(state->handle, "Found conflicting default type definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_type = olddatum->default_type;
	}
	if (olddatum->default_range) {
		if (newdatum->default_range && olddatum->default_range != newdatum->default_range) {
			ERR(state->handle, "Found conflicting default range definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_range = olddatum->default_range;
	}
	return 0;
}

static int class_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			       void *data)
{
	int ret;
	char *id, *new_id;
	class_datum_t *class, *new_class;
	expand_state_t *state;

	id = (char *)key;
	class = (class_datum_t *) datum;
	state = (expand_state_t *) data;

	if (!is_id_enabled(id, state->base, SYM_CLASSES)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying class %s", id);

	new_class = (class_datum_t *) malloc(sizeof(class_datum_t));
	if (!new_class) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	memset(new_class, 0, sizeof(class_datum_t));
	if (ksu_symtab_init(&new_class->permissions, PERM_SYMTAB_SIZE)) {
		ERR(state->handle, "Out of memory!");
		free(new_class);
		return -1;
	}

	new_class->s.value = class->s.value;
	state->out->p_classes.nprim++;

	ret = class_copy_default_new_object(state, class, new_class);
	if (ret) {
		free(new_class);
		return ret;
	}
	
	new_id = strdup(id);
	if (!new_id) {
		ERR(state->handle, "Out of memory!");
		free(new_class);
		return -1;
	}

	ret =
	    hashtab_insert(state->out->p_classes.table, new_id,
			   (hashtab_datum_t) new_class);
	if (ret) {
		ERR(state->handle, "hashtab overflow");
		free(new_class);
		free(new_id);
		return -1;
	}

	if (ksu_hashtab_map
	    (class->permissions.table, perm_copy_callback,
	     &new_class->permissions)) {
		ERR(state->handle, "hashtab overflow");
		return -1;
	}

	if (class->comkey) {
		new_class->comkey = strdup(class->comkey);
		if (!new_class->comkey) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

		new_class->comdatum =
		    hashtab_search(state->out->p_commons.table,
				   new_class->comkey);
		if (!new_class->comdatum) {
			ERR(state->handle, "could not find common datum %s",
			    new_class->comkey);
			return -1;
		}
		new_class->permissions.nprim +=
		    new_class->comdatum->permissions.nprim;
	}

	return 0;
}

static int constraint_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
				    void *data)
{
	char *id;
	class_datum_t *class, *new_class;
	expand_state_t *state;

	id = (char *)key;
	class = (class_datum_t *) datum;
	state = (expand_state_t *) data;

	new_class = hashtab_search(state->out->p_classes.table, id);
	if (!new_class) {
		ERR(state->handle, "class %s vanished", id);
		return -1;
	}

	/* constraints */
	if (constraint_node_clone
	    (&new_class->constraints, class->constraints, state) == -1
	    || constraint_node_clone(&new_class->validatetrans,
				     class->validatetrans, state) == -1) {
		return -1;
	}
	return 0;
}

/*
 * The boundaries have to be copied after the types/roles/users are copied,
 * because it refers hashtab to lookup destinated objects.
 */
static int type_bounds_copy_callback(hashtab_key_t key,
				     hashtab_datum_t datum, void *data)
{
	expand_state_t *state = (expand_state_t *) data;
	type_datum_t *type = (type_datum_t *) datum;
	type_datum_t *dest;
	uint32_t bounds_val;

	if (!type->bounds)
		return 0;

	if (!is_id_enabled((char *)key, state->base, SYM_TYPES))
		return 0;

	bounds_val = state->typemap[type->bounds - 1];

	dest = hashtab_search(state->out->p_types.table, (char *)key);
	if (!dest) {
		ERR(state->handle, "Type lookup failed for %s", (char *)key);
		return -1;
	}
	if (dest->bounds != 0 && dest->bounds != bounds_val) {
		ERR(state->handle, "Inconsistent boundary for %s", (char *)key);
		return -1;
	}
	dest->bounds = bounds_val;

	return 0;
}

static int role_bounds_copy_callback(hashtab_key_t key,
				     hashtab_datum_t datum, void *data)
{
	expand_state_t *state = (expand_state_t *) data;
	role_datum_t *role = (role_datum_t *) datum;
	role_datum_t *dest;
	uint32_t bounds_val;

	if (!role->bounds)
		return 0;

	if (!is_id_enabled((char *)key, state->base, SYM_ROLES))
		return 0;

	bounds_val = state->rolemap[role->bounds - 1];

	dest = hashtab_search(state->out->p_roles.table, (char *)key);
	if (!dest) {
		ERR(state->handle, "Role lookup failed for %s", (char *)key);
		return -1;
	}
	if (dest->bounds != 0 && dest->bounds != bounds_val) {
		ERR(state->handle, "Inconsistent boundary for %s", (char *)key);
		return -1;
	}
	dest->bounds = bounds_val;

	return 0;
}

static int user_bounds_copy_callback(hashtab_key_t key,
				     hashtab_datum_t datum, void *data)
{
	expand_state_t *state = (expand_state_t *) data;
	user_datum_t *user = (user_datum_t *) datum;
	user_datum_t *dest;
	uint32_t bounds_val;

	if (!user->bounds)
		return 0;

	if (!is_id_enabled((char *)key, state->base, SYM_USERS))
		return 0;

	bounds_val = state->usermap[user->bounds - 1];

	dest = hashtab_search(state->out->p_users.table, (char *)key);
	if (!dest) {
		ERR(state->handle, "User lookup failed for %s", (char *)key);
		return -1;
	}
	if (dest->bounds != 0 && dest->bounds != bounds_val) {
		ERR(state->handle, "Inconsistent boundary for %s", (char *)key);
		return -1;
	}
	dest->bounds = bounds_val;

	return 0;
}

/* The aliases have to be copied after the types and attributes to be certain that
 * the out symbol table will have the type that the alias refers. Otherwise, we
 * won't be able to find the type value for the alias. We can't depend on the
 * declaration ordering because of the hash table.
 */
static int alias_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			       void *data)
{
	int ret;
	char *id, *new_id;
	type_datum_t *alias, *new_alias;
	expand_state_t *state;
	uint32_t prival;

	id = (char *)key;
	alias = (type_datum_t *) datum;
	state = (expand_state_t *) data;

	/* ignore regular types */
	if (alias->flavor == TYPE_TYPE && alias->primary)
		return 0;

	/* ignore attributes */
	if (alias->flavor == TYPE_ATTRIB)
		return 0;

	if (alias->flavor == TYPE_ALIAS) 
		prival = alias->primary;
	else 
		prival = alias->s.value;

	if (!is_id_enabled(state->base->p_type_val_to_name[prival - 1],
			state->base, SYM_TYPES)) {
		/* The primary type for this alias is not enabled, the alias 
 		 * shouldn't be either */
		return 0;
	}
		
	if (state->verbose)
		INFO(state->handle, "copying alias %s", id);

	new_id = strdup(id);
	if (!new_id) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	new_alias = (type_datum_t *) malloc(sizeof(type_datum_t));
	if (!new_alias) {
		ERR(state->handle, "Out of memory!");
		free(new_id);
		return SEPOL_ENOMEM;
	}
	memset(new_alias, 0, sizeof(type_datum_t));
	if (alias->flavor == TYPE_TYPE)
		new_alias->s.value = state->typemap[alias->s.value - 1];
	else if (alias->flavor == TYPE_ALIAS)
		new_alias->s.value = state->typemap[alias->primary - 1];
	else
		assert(0);	/* unreachable */

	new_alias->flags = alias->flags;

	ret = hashtab_insert(state->out->p_types.table,
			     (hashtab_key_t) new_id,
			     (hashtab_datum_t) new_alias);

	if (ret) {
		ERR(state->handle, "hashtab overflow");
		free(new_alias);
		free(new_id);
		return -1;
	}

	state->typemap[alias->s.value - 1] = new_alias->s.value;

	if (new_alias->flags & TYPE_FLAGS_PERMISSIVE)
		if (ksu_ebitmap_set_bit(&state->out->permissive_map, new_alias->s.value, 1)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

	return 0;
}

static int role_remap_dominates(hashtab_key_t key __attribute__ ((unused)), hashtab_datum_t datum, void *data)
{
	ebitmap_t mapped_roles;
	role_datum_t *role = (role_datum_t *) datum;
	expand_state_t *state = (expand_state_t *) data;

	if (map_ebitmap(&role->dominates, &mapped_roles, state->rolemap))
		return -1;

	ksu_ebitmap_destroy(&role->dominates);	
	
	if (ksu_ebitmap_cpy(&role->dominates, &mapped_roles))
		return -1;

	ksu_ebitmap_destroy(&mapped_roles);

	return 0;
}

/* For the role attribute in the base module, escalate its counterpart's
 * types.types ebitmap in the out module to the counterparts of all the
 * regular role that belongs to the current role attribute. Note, must be
 * invoked after role_copy_callback so that state->rolemap is available.
 */
static int role_fix_callback(hashtab_key_t key, hashtab_datum_t datum,
			     void *data)
{
	char *id, *base_reg_role_id;
	role_datum_t *role, *new_role, *regular_role;
	expand_state_t *state;
	ebitmap_node_t *rnode;
	unsigned int i;
	ebitmap_t mapped_roles;

	id = key;
	role = (role_datum_t *)datum;
	state = (expand_state_t *)data;

	if (strcmp(id, OBJECT_R) == 0) {
		/* object_r is never a role attribute by far */
		return 0;
	}

	if (!is_id_enabled(id, state->base, SYM_ROLES)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (role->flavor != ROLE_ATTRIB)
		return 0;

	if (state->verbose)
		INFO(state->handle, "fixing role attribute %s", id);

	new_role =
		(role_datum_t *)hashtab_search(state->out->p_roles.table, id);

	assert(new_role != NULL && new_role->flavor == ROLE_ATTRIB);

	ebitmap_init(&mapped_roles);
	if (map_ebitmap(&role->roles, &mapped_roles, state->rolemap))
		return -1;
	if (ebitmap_union(&new_role->roles, &mapped_roles)) {
		ERR(state->handle, "Out of memory!");
		ksu_ebitmap_destroy(&mapped_roles);
		return -1;
	}
	ksu_ebitmap_destroy(&mapped_roles);

	ebitmap_for_each_positive_bit(&role->roles, rnode, i) {
		/* take advantage of sym_val_to_name[]
		 * of the base module */
		base_reg_role_id = state->base->p_role_val_to_name[i];
		regular_role = (role_datum_t *)hashtab_search(
					state->out->p_roles.table,
					base_reg_role_id);
		assert(regular_role != NULL &&
		       regular_role->flavor == ROLE_ROLE);

		if (ebitmap_union(&regular_role->types.types,
				  &new_role->types.types)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
	}

	return 0;
}

static int role_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id, *new_id;
	role_datum_t *role;
	role_datum_t *new_role;
	expand_state_t *state;
	ebitmap_t tmp_union_types;

	id = key;
	role = (role_datum_t *) datum;
	state = (expand_state_t *) data;

	if (strcmp(id, OBJECT_R) == 0) {
		/* object_r is always value 1 */
		state->rolemap[role->s.value - 1] = 1;
		return 0;
	}

	if (!is_id_enabled(id, state->base, SYM_ROLES)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying role %s", id);

	new_role =
	    (role_datum_t *) hashtab_search(state->out->p_roles.table, id);
	if (!new_role) {
		new_role = (role_datum_t *) malloc(sizeof(role_datum_t));
		if (!new_role) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		memset(new_role, 0, sizeof(role_datum_t));

		new_id = strdup(id);
		if (!new_id) {
			ERR(state->handle, "Out of memory!");
			free(new_role);
			return -1;
		}

		state->out->p_roles.nprim++;
		new_role->flavor = role->flavor;
		new_role->s.value = state->out->p_roles.nprim;
		state->rolemap[role->s.value - 1] = new_role->s.value;
		ret = hashtab_insert(state->out->p_roles.table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_role);

		if (ret) {
			ERR(state->handle, "hashtab overflow");
			free(new_role);
			free(new_id);
			return -1;
		}
	}

	/* The dominates bitmap is going to be wrong for the moment, 
 	 * we'll come back later and remap them, after we are sure all 
 	 * the roles have been added */
	if (ebitmap_union(&new_role->dominates, &role->dominates)) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	ebitmap_init(&tmp_union_types);

	/* convert types in the role datum in the global symtab */
	if (expand_convert_type_set
	    (state->out, state->typemap, &role->types, &tmp_union_types, 1)) {
		ksu_ebitmap_destroy(&tmp_union_types);
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	if (ebitmap_union(&new_role->types.types, &tmp_union_types)) {
		ERR(state->handle, "Out of memory!");
		ksu_ebitmap_destroy(&tmp_union_types);
		return -1;
	}
	ksu_ebitmap_destroy(&tmp_union_types);

	return 0;
}

int mls_semantic_level_expand(mls_semantic_level_t * sl, mls_level_t * l,
			      policydb_t * p, sepol_handle_t * h)
{
	mls_semantic_cat_t *cat;
	level_datum_t *levdatum;
	unsigned int i;

	mls_level_init(l);

	if (!p->mls)
		return 0;

	/* Required not declared. */
	if (!sl->sens)
		return 0;

	/* Invalid sensitivity */
	if (sl->sens > p->p_levels.nprim || !p->p_sens_val_to_name[sl->sens - 1])
		return -1;

	l->sens = sl->sens;
	levdatum = (level_datum_t *) hashtab_search(p->p_levels.table,
						    p->p_sens_val_to_name[l->sens - 1]);
	if (!levdatum) {
		ERR(h, "%s: Impossible situation found, nothing in p_levels.table.",
		    __func__);
		// errno = ENOENT;
		return -1;
	}
	for (cat = sl->cat; cat; cat = cat->next) {
		if (cat->low > cat->high) {
			ERR(h, "Category range is not valid %s.%s",
			    p->p_cat_val_to_name[cat->low - 1],
			    p->p_cat_val_to_name[cat->high - 1]);
			return -1;
		}
		for (i = cat->low - 1; i < cat->high; i++) {
			if (!ksu_ebitmap_get_bit(&levdatum->level->cat, i)) {
				ERR(h, "Category %s can not be associated with "
				    "level %s",
				    p->p_cat_val_to_name[i],
				    p->p_sens_val_to_name[l->sens - 1]);
				return -1;
			}
			if (ksu_ebitmap_set_bit(&l->cat, i, 1)) {
				ERR(h, "Out of memory!");
				return -1;
			}
		}
	}

	return 0;
}

int mls_semantic_range_expand(mls_semantic_range_t * sr, mls_range_t * r,
			      policydb_t * p, sepol_handle_t * h)
{
	if (mls_semantic_level_expand(&sr->level[0], &r->level[0], p, h) < 0)
		return -1;

	if (mls_semantic_level_expand(&sr->level[1], &r->level[1], p, h) < 0) {
		mls_level_destroy(&r->level[0]);
		return -1;
	}

	if (!mls_level_dom(&r->level[1], &r->level[0])) {
		mls_range_destroy(r);
		ERR(h, "MLS range high level does not dominate low level");
		return -1;
	}

	return 0;
}

static int user_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	expand_state_t *state;
	user_datum_t *user;
	user_datum_t *new_user;
	char *id, *new_id;
	ebitmap_t tmp_union;

	id = key;
	user = (user_datum_t *) datum;
	state = (expand_state_t *) data;

	if (!is_id_enabled(id, state->base, SYM_USERS)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying user %s", id);

	new_user =
	    (user_datum_t *) hashtab_search(state->out->p_users.table, id);
	if (!new_user) {
		new_user = (user_datum_t *) malloc(sizeof(user_datum_t));
		if (!new_user) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		memset(new_user, 0, sizeof(user_datum_t));

		state->out->p_users.nprim++;
		new_user->s.value = state->out->p_users.nprim;
		state->usermap[user->s.value - 1] = new_user->s.value;

		new_id = strdup(id);
		if (!new_id) {
			ERR(state->handle, "Out of memory!");
			free(new_user);
			return -1;
		}
		ret = hashtab_insert(state->out->p_users.table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_user);
		if (ret) {
			ERR(state->handle, "hashtab overflow");
			user_datum_destroy(new_user);
			free(new_user);
			free(new_id);
			return -1;
		}

		/* expand the semantic MLS info */
		if (mls_semantic_range_expand(&user->range,
					      &new_user->exp_range,
					      state->out, state->handle)) {
			return -1;
		}
		if (mls_semantic_level_expand(&user->dfltlevel,
					      &new_user->exp_dfltlevel,
					      state->out, state->handle)) {
			return -1;
		}
		if (!mls_level_between(&new_user->exp_dfltlevel,
				       &new_user->exp_range.level[0],
				       &new_user->exp_range.level[1])) {
			ERR(state->handle, "default level not within user "
			    "range");
			return -1;
		}
	} else {
		/* require that the MLS info match */
		mls_range_t tmp_range;
		mls_level_t tmp_level;

		if (mls_semantic_range_expand(&user->range, &tmp_range,
					      state->out, state->handle)) {
			return -1;
		}
		if (mls_semantic_level_expand(&user->dfltlevel, &tmp_level,
					      state->out, state->handle)) {
			mls_range_destroy(&tmp_range);
			return -1;
		}
		if (!mls_range_eq(&new_user->exp_range, &tmp_range) ||
		    !mls_level_eq(&new_user->exp_dfltlevel, &tmp_level)) {
			mls_range_destroy(&tmp_range);
			mls_level_destroy(&tmp_level);
			return -1;
		}
		mls_range_destroy(&tmp_range);
		mls_level_destroy(&tmp_level);
	}

	ebitmap_init(&tmp_union);

	/* get global roles for this user */
	if (role_set_expand(&user->roles, &tmp_union, state->out, state->base, state->rolemap)) {
		ERR(state->handle, "Out of memory!");
		ksu_ebitmap_destroy(&tmp_union);
		return -1;
	}

	if (ebitmap_union(&new_user->roles.roles, &tmp_union)) {
		ERR(state->handle, "Out of memory!");
		ksu_ebitmap_destroy(&tmp_union);
		return -1;
	}
	ksu_ebitmap_destroy(&tmp_union);

	return 0;
}

static int bool_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	expand_state_t *state;
	cond_bool_datum_t *bool, *new_bool;
	char *id, *new_id;

	id = key;
	bool = (cond_bool_datum_t *) datum;
	state = (expand_state_t *) data;

	if (!is_id_enabled(id, state->base, SYM_BOOLS)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (bool->flags & COND_BOOL_FLAGS_TUNABLE) {
		/* Skip tunables */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying boolean %s", id);

	new_bool = (cond_bool_datum_t *) malloc(sizeof(cond_bool_datum_t));
	if (!new_bool) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	new_id = strdup(id);
	if (!new_id) {
		ERR(state->handle, "Out of memory!");
		free(new_bool);
		return -1;
	}

	state->out->p_bools.nprim++;
	new_bool->s.value = state->out->p_bools.nprim;

	ret = hashtab_insert(state->out->p_bools.table,
			     (hashtab_key_t) new_id,
			     (hashtab_datum_t) new_bool);
	if (ret) {
		ERR(state->handle, "hashtab overflow");
		free(new_bool);
		free(new_id);
		return -1;
	}

	state->boolmap[bool->s.value - 1] = new_bool->s.value;

	new_bool->state = bool->state;
	new_bool->flags = bool->flags;

	return 0;
}

static int sens_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	expand_state_t *state = (expand_state_t *) data;
	level_datum_t *level = (level_datum_t *) datum, *new_level = NULL;
	char *id = (char *)key, *new_id = NULL;

	if (!is_id_enabled(id, state->base, SYM_LEVELS)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying sensitivity level %s", id);

	new_level = (level_datum_t *) malloc(sizeof(level_datum_t));
	if (!new_level)
		goto out_of_mem;
	level_datum_init(new_level);
	new_level->level = (mls_level_t *) malloc(sizeof(mls_level_t));
	if (!new_level->level)
		goto out_of_mem;
	mls_level_init(new_level->level);
	new_id = strdup(id);
	if (!new_id)
		goto out_of_mem;

	if (mls_level_cpy(new_level->level, level->level)) {
		goto out_of_mem;
	}
	new_level->isalias = level->isalias;
	state->out->p_levels.nprim++;

	if (hashtab_insert(state->out->p_levels.table,
			   (hashtab_key_t) new_id,
			   (hashtab_datum_t) new_level)) {
		goto out_of_mem;
	}
	return 0;

      out_of_mem:
	ERR(state->handle, "Out of memory!");
	if (new_level != NULL && new_level->level != NULL) {
		mls_level_destroy(new_level->level);
		free(new_level->level);
	}
	level_datum_destroy(new_level);
	free(new_level);
	free(new_id);
	return -1;
}

static int cats_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	expand_state_t *state = (expand_state_t *) data;
	cat_datum_t *cat = (cat_datum_t *) datum, *new_cat = NULL;
	char *id = (char *)key, *new_id = NULL;

	if (!is_id_enabled(id, state->base, SYM_CATS)) {
		/* identifier's scope is not enabled */
		return 0;
	}

	if (state->verbose)
		INFO(state->handle, "copying category attribute %s", id);

	new_cat = (cat_datum_t *) malloc(sizeof(cat_datum_t));
	if (!new_cat)
		goto out_of_mem;
	cat_datum_init(new_cat);
	new_id = strdup(id);
	if (!new_id)
		goto out_of_mem;

	new_cat->s.value = cat->s.value;
	new_cat->isalias = cat->isalias;
	state->out->p_cats.nprim++;
	if (hashtab_insert(state->out->p_cats.table,
			   (hashtab_key_t) new_id, (hashtab_datum_t) new_cat)) {
		goto out_of_mem;
	}

	return 0;

      out_of_mem:
	ERR(state->handle, "Out of memory!");
	cat_datum_destroy(new_cat);
	free(new_cat);
	free(new_id);
	return -1;
}

static int copy_role_allows(expand_state_t * state, role_allow_rule_t * rules)
{
	unsigned int i, j;
	role_allow_t *cur_allow, *n, *l;
	role_allow_rule_t *cur;
	ebitmap_t roles, new_roles;
	ebitmap_node_t *snode, *tnode;

	/* start at the end of the list */
	for (l = state->out->role_allow; l && l->next; l = l->next) ;

	cur = rules;
	while (cur) {
		ebitmap_init(&roles);
		ebitmap_init(&new_roles);

		if (role_set_expand(&cur->roles, &roles, state->out, state->base, state->rolemap)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

		if (role_set_expand(&cur->new_roles, &new_roles, state->out, state->base, state->rolemap)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

		ebitmap_for_each_positive_bit(&roles, snode, i) {
			ebitmap_for_each_positive_bit(&new_roles, tnode, j) {
				/* check for duplicates */
				cur_allow = state->out->role_allow;
				while (cur_allow) {
					if ((cur_allow->role == i + 1) &&
					    (cur_allow->new_role == j + 1))
						break;
					cur_allow = cur_allow->next;
				}
				if (cur_allow)
					continue;
				n = (role_allow_t *)
				    malloc(sizeof(role_allow_t));
				if (!n) {
					ERR(state->handle, "Out of memory!");
					return -1;
				}
				memset(n, 0, sizeof(role_allow_t));
				n->role = i + 1;
				n->new_role = j + 1;
				if (l) {
					l->next = n;
				} else {
					state->out->role_allow = n;
				}
				l = n;
			}
		}

		ksu_ebitmap_destroy(&roles);
		ksu_ebitmap_destroy(&new_roles);

		cur = cur->next;
	}

	return 0;
}

static int copy_role_trans(expand_state_t * state, role_trans_rule_t * rules)
{
	unsigned int i, j, k;
	role_trans_t *n, *l, *cur_trans;
	role_trans_rule_t *cur;
	ebitmap_t roles, types;
	ebitmap_node_t *rnode, *tnode, *cnode;

	/* start at the end of the list */
	for (l = state->out->role_tr; l && l->next; l = l->next) ;

	cur = rules;
	while (cur) {
		ebitmap_init(&roles);
		ebitmap_init(&types);

		if (role_set_expand(&cur->roles, &roles, state->out, state->base, state->rolemap)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		if (expand_convert_type_set
		    (state->out, state->typemap, &cur->types, &types, 1)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		ebitmap_for_each_positive_bit(&roles, rnode, i) {
			ebitmap_for_each_positive_bit(&types, tnode, j) {
				ebitmap_for_each_positive_bit(&cur->classes, cnode, k) {
					cur_trans = state->out->role_tr;
					while (cur_trans) {
						unsigned int mapped_role;

						mapped_role = state->rolemap[cur->new_role - 1];

						if ((cur_trans->role ==
								i + 1) &&
						    (cur_trans->type ==
								j + 1) &&
						    (cur_trans->tclass ==
								k + 1)) {
							if (cur_trans->new_role == mapped_role) {
								break;
							} else {
								ERR(state->handle,
									"Conflicting role trans rule %s %s : %s { %s vs %s }",
									state->out->p_role_val_to_name[i],
									state->out->p_type_val_to_name[j],
									state->out->p_class_val_to_name[k],
									state->out->p_role_val_to_name[mapped_role - 1],
									state->out->p_role_val_to_name[cur_trans->new_role - 1]);
								return -1;
							}
						}
						cur_trans = cur_trans->next;
					}
					if (cur_trans)
						continue;

					n = (role_trans_t *)
						malloc(sizeof(role_trans_t));
					if (!n) {
						ERR(state->handle,
							"Out of memory!");
						return -1;
					}
					memset(n, 0, sizeof(role_trans_t));
					n->role = i + 1;
					n->type = j + 1;
					n->tclass = k + 1;
					n->new_role = state->rolemap
							[cur->new_role - 1];
					if (l)
						l->next = n;
					else
						state->out->role_tr = n;

					l = n;
				}
			}
		}

		ksu_ebitmap_destroy(&roles);
		ksu_ebitmap_destroy(&types);

		cur = cur->next;
	}
	return 0;
}

static int expand_filename_trans_helper(expand_state_t *state,
					filename_trans_rule_t *rule,
					unsigned int s, unsigned int t)
{
	uint32_t mapped_otype, present_otype;
	int rc;

	mapped_otype = state->typemap[rule->otype - 1];

	rc = policydb_filetrans_insert(
		state->out, s + 1, t + 1,
		rule->tclass, rule->name,
		NULL, mapped_otype, &present_otype
	);
	if (rc == SEPOL_EEXIST) {
		/* duplicate rule, ignore */
		if (present_otype == mapped_otype)
			return 0;

		ERR(state->handle, "Conflicting name-based type_transition %s %s:%s \"%s\":  %s vs %s",
		    state->out->p_type_val_to_name[s],
		    state->out->p_type_val_to_name[t],
		    state->out->p_class_val_to_name[rule->tclass - 1],
		    rule->name,
		    state->out->p_type_val_to_name[present_otype - 1],
		    state->out->p_type_val_to_name[mapped_otype - 1]);
		return -1;
	} else if (rc < 0) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	return 0;
}

static int expand_filename_trans(expand_state_t *state, filename_trans_rule_t *rules)
{
	unsigned int i, j;
	filename_trans_rule_t *cur_rule;
	ebitmap_t stypes, ttypes;
	ebitmap_node_t *snode, *tnode;
	int rc;

	cur_rule = rules;
	while (cur_rule) {
		ebitmap_init(&stypes);
		ebitmap_init(&ttypes);

		if (expand_convert_type_set(state->out, state->typemap,
					    &cur_rule->stypes, &stypes, 1)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

		if (expand_convert_type_set(state->out, state->typemap,
					    &cur_rule->ttypes, &ttypes, 1)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}


		ebitmap_for_each_positive_bit(&stypes, snode, i) {
			ebitmap_for_each_positive_bit(&ttypes, tnode, j) {
				rc = expand_filename_trans_helper(
					state, cur_rule, i, j
				);
				if (rc)
					return rc;
			}
			if (cur_rule->flags & RULE_SELF) {
				rc = expand_filename_trans_helper(
					state, cur_rule, i, i
				);
				if (rc)
					return rc;
			}
		}

		ksu_ebitmap_destroy(&stypes);
		ksu_ebitmap_destroy(&ttypes);

		cur_rule = cur_rule->next;
	}
	return 0;
}

static int exp_rangetr_helper(uint32_t stype, uint32_t ttype, uint32_t tclass,
			      mls_semantic_range_t * trange,
			      expand_state_t * state)
{
	range_trans_t *rt = NULL, key;
	mls_range_t *r, *exp_range = NULL;
	int rc = -1;

	exp_range = calloc(1, sizeof(*exp_range));
	if (!exp_range) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	if (mls_semantic_range_expand(trange, exp_range, state->out,
				      state->handle))
		goto err;

	/* check for duplicates/conflicts */
	key.source_type = stype;
	key.target_type = ttype;
	key.target_class = tclass;
	r = hashtab_search(state->out->range_tr, (hashtab_key_t) &key);
	if (r) {
		if (mls_range_eq(r, exp_range)) {
			/* duplicate, ignore */
			mls_range_destroy(exp_range);
			free(exp_range);
			return 0;
		}

		/* conflict */
		ERR(state->handle,
		    "Conflicting range trans rule %s %s : %s",
		    state->out->p_type_val_to_name[stype - 1],
		    state->out->p_type_val_to_name[ttype - 1],
		    state->out->p_class_val_to_name[tclass - 1]);
		goto err;
	}

	rt = calloc(1, sizeof(*rt));
	if (!rt) {
		ERR(state->handle, "Out of memory!");
		goto err;
	}
	rt->source_type = stype;
	rt->target_type = ttype;
	rt->target_class = tclass;

	rc = hashtab_insert(state->out->range_tr, (hashtab_key_t) rt,
			    exp_range);
	if (rc) {
		ERR(state->handle, "Out of memory!");
		goto err;

	}

	return 0;
err:
	free(rt);
	if (exp_range) {
		mls_range_destroy(exp_range);
		free(exp_range);
	}
	return -1;
}

static int expand_range_trans(expand_state_t * state,
			      range_trans_rule_t * rules)
{
	unsigned int i, j, k;
	range_trans_rule_t *rule;

	ebitmap_t stypes, ttypes;
	ebitmap_node_t *snode, *tnode, *cnode;

	if (state->verbose)
		INFO(state->handle, "expanding range transitions");

	for (rule = rules; rule; rule = rule->next) {
		ebitmap_init(&stypes);
		ebitmap_init(&ttypes);

		/* expand the type sets */
		if (expand_convert_type_set(state->out, state->typemap,
					    &rule->stypes, &stypes, 1)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		if (expand_convert_type_set(state->out, state->typemap,
					    &rule->ttypes, &ttypes, 1)) {
			ksu_ebitmap_destroy(&stypes);
			ERR(state->handle, "Out of memory!");
			return -1;
		}

		/* loop on source type */
		ebitmap_for_each_positive_bit(&stypes, snode, i) {
			/* loop on target type */
			ebitmap_for_each_positive_bit(&ttypes, tnode, j) {
				/* loop on target class */
				ebitmap_for_each_positive_bit(&rule->tclasses, cnode, k) {
					if (exp_rangetr_helper(i + 1,
							       j + 1,
							       k + 1,
							       &rule->trange,
							       state)) {
						ksu_ebitmap_destroy(&stypes);
						ksu_ebitmap_destroy(&ttypes);
						return -1;
					}
				}
			}
		}

		ksu_ebitmap_destroy(&stypes);
		ksu_ebitmap_destroy(&ttypes);
	}

	return 0;
}

/* Search for an AV tab node within a hash table with the given key.
 * If the node does not exist, create it and return it; otherwise
 * return the pre-existing one.
*/
static avtab_ptr_t find_avtab_node(sepol_handle_t * handle,
				   avtab_t * avtab, avtab_key_t * key,
				   cond_av_list_t ** cond,
				   av_extended_perms_t *xperms)
{
	avtab_ptr_t node;
	avtab_datum_t avdatum;
	cond_av_list_t *nl;
	int match = 0;

	/* AVTAB_XPERMS entries are not necessarily unique */
	if (key->specified & AVTAB_XPERMS) {
		if (xperms == NULL) {
			ERR(handle, "searching xperms NULL");
			node = NULL;
		} else {
			node = ksu_avtab_search_node(avtab, key);
			while (node) {
				if ((node->datum.xperms->specified == xperms->specified) &&
					(node->datum.xperms->driver == xperms->driver)) {
					match = 1;
					break;
				}
				node = ksu_avtab_search_node_next(node, key->specified);
			}
			if (!match)
				node = NULL;
		}
	} else {
		node = ksu_avtab_search_node(avtab, key);
	}

	/* If this is for conditional policies, keep searching in case
	   the node is part of my conditional avtab. */
	if (cond) {
		while (node) {
			if (node->parse_context == cond)
				break;
			node = ksu_avtab_search_node_next(node, key->specified);
		}
	}

	if (!node) {
		memset(&avdatum, 0, sizeof avdatum);
		/*
		 * AUDITDENY, aka DONTAUDIT, are &= assigned, versus |= for
		 * others. Initialize the data accordingly.
		 */
		avdatum.data = key->specified == AVTAB_AUDITDENY ? ~UINT32_C(0) : UINT32_C(0);
		/* this is used to get the node - insertion is actually unique */
		node = ksu_avtab_insert_nonunique(avtab, key, &avdatum);
		if (!node) {
			ERR(handle, "hash table overflow");
			return NULL;
		}
		if (cond) {
			node->parse_context = cond;
			nl = (cond_av_list_t *) malloc(sizeof(cond_av_list_t));
			if (!nl) {
				ERR(handle, "Memory error");
				return NULL;
			}
			memset(nl, 0, sizeof(cond_av_list_t));
			nl->node = node;
			nl->next = *cond;
			*cond = nl;
		}
	}

	return node;
}

static uint32_t avrule_to_avtab_spec(uint32_t specification)
{
	return (specification == AVRULE_DONTAUDIT) ?
		AVTAB_AUDITDENY : specification;
}

#define EXPAND_RULE_SUCCESS   1
#define EXPAND_RULE_CONFLICT  0
#define EXPAND_RULE_ERROR    -1

static int expand_terule_helper(sepol_handle_t * handle,
				policydb_t * p, uint32_t * typemap,
				uint32_t specified, cond_av_list_t ** cond,
				cond_av_list_t ** other, uint32_t stype,
				uint32_t ttype, class_perm_node_t * perms,
				avtab_t * avtab, int enabled)
{
	avtab_key_t avkey;
	avtab_datum_t *avdatump;
	avtab_ptr_t node;
	class_perm_node_t *cur;
	int conflict;
	uint32_t oldtype = 0;

	if (!(specified & (AVRULE_TRANSITION|AVRULE_MEMBER|AVRULE_CHANGE))) {
		ERR(handle, "Invalid specification: %"PRIu32, specified);
		return EXPAND_RULE_ERROR;
	}

	avkey.specified = avrule_to_avtab_spec(specified);
	avkey.source_type = stype + 1;
	avkey.target_type = ttype + 1;

	cur = perms;
	while (cur) {
		uint32_t remapped_data =
		    typemap ? typemap[cur->data - 1] : cur->data;
		avkey.target_class = cur->tclass;

		conflict = 0;
		/* check to see if the expanded TE already exists --
		 * either in the global scope or in another
		 * conditional AV tab */
		node = ksu_avtab_search_node(&p->te_avtab, &avkey);
		if (node) {
			conflict = 1;
		} else {
			node = ksu_avtab_search_node(&p->te_cond_avtab, &avkey);
			if (node && node->parse_context != other) {
				conflict = 2;
			}
		}

		if (conflict) {
			avdatump = &node->datum;
			if (specified & AVRULE_TRANSITION) {
				oldtype = avdatump->data;
			} else if (specified & AVRULE_MEMBER) {
				oldtype = avdatump->data;
			} else if (specified & AVRULE_CHANGE) {
				oldtype = avdatump->data;
			}

			if (oldtype == remapped_data) {
				/* if the duplicate is inside the same scope (eg., unconditional 
				 * or in same conditional then ignore it */
				if ((conflict == 1 && cond == NULL)
				    || node->parse_context == cond)
					return EXPAND_RULE_SUCCESS;
				ERR(handle, "duplicate TE rule for %s %s:%s %s",
				    p->p_type_val_to_name[avkey.source_type -
							  1],
				    p->p_type_val_to_name[avkey.target_type -
							  1],
				    p->p_class_val_to_name[avkey.target_class -
							   1],
				    p->p_type_val_to_name[oldtype - 1]);
				return EXPAND_RULE_CONFLICT;
			}
			ERR(handle,
			    "conflicting TE rule for (%s, %s:%s):  old was %s, new is %s",
			    p->p_type_val_to_name[avkey.source_type - 1],
			    p->p_type_val_to_name[avkey.target_type - 1],
			    p->p_class_val_to_name[avkey.target_class - 1],
			    p->p_type_val_to_name[oldtype - 1],
			    p->p_type_val_to_name[remapped_data - 1]);
			return EXPAND_RULE_CONFLICT;
		}

		node = find_avtab_node(handle, avtab, &avkey, cond, NULL);
		if (!node)
			return -1;
		if (enabled) {
			node->key.specified |= AVTAB_ENABLED;
		} else {
			node->key.specified &= ~AVTAB_ENABLED;
		}

		avdatump = &node->datum;
		avdatump->data = remapped_data;

		cur = cur->next;
	}

	return EXPAND_RULE_SUCCESS;
}

/* 0 for success -1 indicates failure */
static int allocate_xperms(sepol_handle_t * handle, avtab_datum_t * avdatump,
			   av_extended_perms_t * extended_perms)
{
	unsigned int i;

	avtab_extended_perms_t *xperms = avdatump->xperms;
	if (!xperms) {
		xperms = (avtab_extended_perms_t *)
			calloc(1, sizeof(avtab_extended_perms_t));
		if (!xperms) {
			ERR(handle, "Out of memory!");
			return -1;
		}
		avdatump->xperms = xperms;
	}

	switch (extended_perms->specified) {
	case AVRULE_XPERMS_IOCTLFUNCTION:
		xperms->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		break;
	case AVRULE_XPERMS_IOCTLDRIVER:
		xperms->specified = AVTAB_XPERMS_IOCTLDRIVER;
		break;
	default:
		return -1;
	}

	xperms->driver = extended_perms->driver;
	for (i = 0; i < ARRAY_SIZE(xperms->perms); i++)
		xperms->perms[i] |= extended_perms->perms[i];

	return 0;
}

static int expand_avrule_helper(sepol_handle_t * handle,
				uint32_t specified,
				cond_av_list_t ** cond,
				uint32_t stype, uint32_t ttype,
				class_perm_node_t * perms, avtab_t * avtab,
				int enabled, av_extended_perms_t *extended_perms)
{
	avtab_key_t avkey;
	avtab_datum_t *avdatump;
	avtab_ptr_t node;
	class_perm_node_t *cur;

	/* bail early if dontaudit's are disabled and it's a dontaudit rule */
	if ((specified & (AVRULE_DONTAUDIT|AVRULE_XPERMS_DONTAUDIT))
	     && handle && handle->disable_dontaudit)
			return EXPAND_RULE_SUCCESS;

	avkey.source_type = stype + 1;
	avkey.target_type = ttype + 1;
	avkey.specified = avrule_to_avtab_spec(specified);

	cur = perms;
	while (cur) {
		avkey.target_class = cur->tclass;

		node = find_avtab_node(handle, avtab, &avkey, cond, extended_perms);
		if (!node)
			return EXPAND_RULE_ERROR;
		if (enabled) {
			node->key.specified |= AVTAB_ENABLED;
		} else {
			node->key.specified &= ~AVTAB_ENABLED;
		}

		avdatump = &node->datum;
		switch (specified) {
		case AVRULE_ALLOWED:
		case AVRULE_AUDITALLOW:
		case AVRULE_NEVERALLOW:
			avdatump->data |= cur->data;
			break;
		case AVRULE_DONTAUDIT:
			avdatump->data &= ~cur->data;
			break;
		case AVRULE_AUDITDENY:
			/* Since a '0' in an auditdeny mask represents
			 * a permission we do NOT want to audit
			 * (dontaudit), we use the '&' operand to
			 * ensure that all '0's in the mask are
			 * retained (much unlike the allow and
			 * auditallow cases).
			 */
			avdatump->data &= cur->data;
			break;
		case AVRULE_XPERMS_ALLOWED:
		case AVRULE_XPERMS_AUDITALLOW:
		case AVRULE_XPERMS_DONTAUDIT:
		case AVRULE_XPERMS_NEVERALLOW:
			if (allocate_xperms(handle, avdatump, extended_perms))
				return EXPAND_RULE_ERROR;
			break;
		default:
			ERR(handle, "Unknown specification: %"PRIu32, specified);
			return EXPAND_RULE_ERROR;
		}

		cur = cur->next;
	}
	return EXPAND_RULE_SUCCESS;
}

static int expand_rule_helper(sepol_handle_t * handle,
			      policydb_t * p, uint32_t * typemap,
			      avrule_t * source_rule, avtab_t * dest_avtab,
			      cond_av_list_t ** cond, cond_av_list_t ** other,
			      int enabled,
			      ebitmap_t * stypes, ebitmap_t * ttypes)
{
	unsigned int i, j;
	int retval;
	ebitmap_node_t *snode, *tnode;

	ebitmap_for_each_positive_bit(stypes, snode, i) {
		if (source_rule->flags & RULE_SELF) {
			if (source_rule->specified & (AVRULE_AV | AVRULE_XPERMS)) {
				retval = expand_avrule_helper(handle, source_rule->specified,
							      cond, i, i, source_rule->perms,
							      dest_avtab, enabled, source_rule->xperms);
				if (retval != EXPAND_RULE_SUCCESS)
					return retval;
			} else {
				retval = expand_terule_helper(handle, p, typemap,
							      source_rule->specified, cond,
							      other, i, i, source_rule->perms,
							      dest_avtab, enabled);
				if (retval != EXPAND_RULE_SUCCESS)
					return retval;
			}
		}
		ebitmap_for_each_positive_bit(ttypes, tnode, j) {
			if (source_rule->specified & (AVRULE_AV | AVRULE_XPERMS)) {
				retval = expand_avrule_helper(handle, source_rule->specified,
							      cond, i, j, source_rule->perms,
							      dest_avtab, enabled, source_rule->xperms);
				if (retval != EXPAND_RULE_SUCCESS)
					return retval;
			} else {
				retval = expand_terule_helper(handle, p, typemap,
							      source_rule->specified, cond,
							      other, i, j, source_rule->perms,
							      dest_avtab, enabled);
				if (retval != EXPAND_RULE_SUCCESS)
					return retval;
			}
		}
	}

	return EXPAND_RULE_SUCCESS;
}

/*
 * Expand a rule into a given avtab - checking for conflicting type
 * rules in the destination policy.  Return EXPAND_RULE_SUCCESS on 
 * success, EXPAND_RULE_CONFLICT if the rule conflicts with something
 * (and hence was not added), or EXPAND_RULE_ERROR on error.
 */
static int convert_and_expand_rule(sepol_handle_t * handle,
				   policydb_t * dest_pol, uint32_t * typemap,
				   avrule_t * source_rule, avtab_t * dest_avtab,
				   cond_av_list_t ** cond,
				   cond_av_list_t ** other, int enabled,
				   int do_neverallow)
{
	int retval;
	ebitmap_t stypes, ttypes;
	unsigned char alwaysexpand;

	if (!do_neverallow && source_rule->specified & AVRULE_NEVERALLOW)
		return EXPAND_RULE_SUCCESS;
	if (!do_neverallow && source_rule->specified & AVRULE_XPERMS_NEVERALLOW)
		return EXPAND_RULE_SUCCESS;

	ebitmap_init(&stypes);
	ebitmap_init(&ttypes);

	/* Force expansion for type rules and for self rules. */
	alwaysexpand = ((source_rule->specified & AVRULE_TYPE) ||
			(source_rule->flags & RULE_SELF));

	if (expand_convert_type_set
	    (dest_pol, typemap, &source_rule->stypes, &stypes, alwaysexpand))
		return EXPAND_RULE_ERROR;
	if (expand_convert_type_set
	    (dest_pol, typemap, &source_rule->ttypes, &ttypes, alwaysexpand))
		return EXPAND_RULE_ERROR;

	retval = expand_rule_helper(handle, dest_pol, typemap,
				    source_rule, dest_avtab,
				    cond, other, enabled, &stypes, &ttypes);
	ksu_ebitmap_destroy(&stypes);
	ksu_ebitmap_destroy(&ttypes);
	return retval;
}

static int cond_avrule_list_copy(policydb_t * dest_pol, avrule_t * source_rules,
				 avtab_t * dest_avtab, cond_av_list_t ** list,
				 cond_av_list_t ** other, uint32_t * typemap,
				 int enabled, expand_state_t * state)
{
	avrule_t *cur;

	cur = source_rules;
	while (cur) {
		if (convert_and_expand_rule(state->handle, dest_pol,
					    typemap, cur, dest_avtab,
					    list, other, enabled,
					    0) != EXPAND_RULE_SUCCESS) {
			return -1;
		}

		cur = cur->next;
	}

	return 0;
}

static int cond_node_map_bools(expand_state_t * state, cond_node_t * cn)
{
	cond_expr_t *cur;
	unsigned int i;

	cur = cn->expr;
	while (cur) {
		if (cur->bool)
			cur->bool = state->boolmap[cur->bool - 1];
		cur = cur->next;
	}

	for (i = 0; i < min(cn->nbools, COND_MAX_BOOLS); i++)
		cn->bool_ids[i] = state->boolmap[cn->bool_ids[i] - 1];

	if (cond_normalize_expr(state->out, cn)) {
		ERR(state->handle, "Error while normalizing conditional");
		return -1;
	}

	return 0;
}

/* copy the nodes in *reverse* order -- the result is that the last
 * given conditional appears first in the policy, so as to match the
 * behavior of the upstream compiler */
static int cond_node_copy(expand_state_t * state, cond_node_t * cn)
{
	cond_node_t *new_cond, *tmp;

	if (cn == NULL) {
		return 0;
	}
	if (cond_node_copy(state, cn->next)) {
		return -1;
	}

	/* If current cond_node_t is of tunable, its effective branch
	 * has been appended to its home decl->avrules list during link
	 * and now we should just skip it. */
	if (cn->flags & COND_NODE_FLAGS_TUNABLE)
		return 0;

	if (cond_normalize_expr(state->base, cn)) {
		ERR(state->handle, "Error while normalizing conditional");
		return -1;
	}

	/* create a new temporary conditional node with the booleans
	 * mapped */
	tmp = cond_node_create(state->base, cn);
	if (!tmp) {
		ERR(state->handle, "Out of memory");
		return -1;
	}

	if (cond_node_map_bools(state, tmp)) {
		cond_node_destroy(tmp);
		free(tmp);
		ERR(state->handle, "Error mapping booleans");
		return -1;
	}

	new_cond = cond_node_search(state->out, state->out->cond_list, tmp);
	if (!new_cond) {
		cond_node_destroy(tmp);
		free(tmp);
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	cond_node_destroy(tmp);
	free(tmp);

	if (cond_avrule_list_copy
	    (state->out, cn->avtrue_list, &state->out->te_cond_avtab,
	     &new_cond->true_list, &new_cond->false_list, state->typemap,
	     new_cond->cur_state, state))
		return -1;
	if (cond_avrule_list_copy
	    (state->out, cn->avfalse_list, &state->out->te_cond_avtab,
	     &new_cond->false_list, &new_cond->true_list, state->typemap,
	     !new_cond->cur_state, state))
		return -1;

	return 0;
}

static int context_copy(context_struct_t * dst, context_struct_t * src,
			expand_state_t * state)
{
	dst->user = state->usermap[src->user - 1];
	dst->role = state->rolemap[src->role - 1];
	dst->type = state->typemap[src->type - 1];
	return mls_context_cpy(dst, src);
}

static int ocontext_copy_xen(expand_state_t *state)
{
	unsigned int i;
	ocontext_t *c, *n, *l;

	for (i = 0; i < OCON_NUM; i++) {
		l = NULL;
		for (c = state->base->ocontexts[i]; c; c = c->next) {
			if (i == OCON_XEN_ISID && !c->context[0].user) {
				INFO(state->handle,
				     "No context assigned to SID %s, omitting from policy",
				     c->u.name);
				continue;
			}
			n = malloc(sizeof(ocontext_t));
			if (!n) {
				ERR(state->handle, "Out of memory!");
				return -1;
			}
			memset(n, 0, sizeof(ocontext_t));
			if (l)
				l->next = n;
			else
				state->out->ocontexts[i] = n;
			l = n;
			switch (i) {
			case OCON_XEN_ISID:
				n->sid[0] = c->sid[0];
				break;
			case OCON_XEN_PIRQ:
				n->u.pirq = c->u.pirq;
				break;
			case OCON_XEN_IOPORT:
				n->u.ioport.low_ioport = c->u.ioport.low_ioport;
				n->u.ioport.high_ioport =
					c->u.ioport.high_ioport;
				break;
			case OCON_XEN_IOMEM:
				n->u.iomem.low_iomem  = c->u.iomem.low_iomem;
				n->u.iomem.high_iomem = c->u.iomem.high_iomem;
				break;
			case OCON_XEN_PCIDEVICE:
				n->u.device = c->u.device;
				break;
			case OCON_XEN_DEVICETREE:
				n->u.name = strdup(c->u.name);
				if (!n->u.name) {
					ERR(state->handle, "Out of memory!");
					return -1;
				}
				break;
			default:
				/* shouldn't get here */
				ERR(state->handle, "Unknown ocontext");
				return -1;
			}
			if (context_copy(&n->context[0], &c->context[0],
				state)) {
				ERR(state->handle, "Out of memory!");
				return -1;
			}
		}
	}
	return 0;
}

static int ocontext_copy_selinux(expand_state_t *state)
{
	unsigned int i, j;
	ocontext_t *c, *n, *l;

	for (i = 0; i < OCON_NUM; i++) {
		l = NULL;
		for (c = state->base->ocontexts[i]; c; c = c->next) {
			if (i == OCON_ISID && !c->context[0].user) {
				INFO(state->handle,
				     "No context assigned to SID %s, omitting from policy",
				     c->u.name);
				continue;
			}
			n = malloc(sizeof(ocontext_t));
			if (!n) {
				ERR(state->handle, "Out of memory!");
				return -1;
			}
			memset(n, 0, sizeof(ocontext_t));
			if (l)
				l->next = n;
			else
				state->out->ocontexts[i] = n;
			l = n;
			switch (i) {
			case OCON_ISID:
				n->sid[0] = c->sid[0];
				break;
			case OCON_FS:	/* FALLTHROUGH */
			case OCON_NETIF:
				n->u.name = strdup(c->u.name);
				if (!n->u.name) {
					ERR(state->handle, "Out of memory!");
					return -1;
				}
				if (context_copy
				    (&n->context[1], &c->context[1], state)) {
					ERR(state->handle, "Out of memory!");
					return -1;
				}
				break;
			case OCON_IBPKEY:
				n->u.ibpkey.subnet_prefix = c->u.ibpkey.subnet_prefix;

				n->u.ibpkey.low_pkey = c->u.ibpkey.low_pkey;
				n->u.ibpkey.high_pkey = c->u.ibpkey.high_pkey;
			break;
			case OCON_IBENDPORT:
				n->u.ibendport.dev_name = strdup(c->u.ibendport.dev_name);
				if (!n->u.ibendport.dev_name) {
					ERR(state->handle, "Out of memory!");
					return -1;
				}
				n->u.ibendport.port = c->u.ibendport.port;
				break;
			case OCON_PORT:
				n->u.port.protocol = c->u.port.protocol;
				n->u.port.low_port = c->u.port.low_port;
				n->u.port.high_port = c->u.port.high_port;
				break;
			case OCON_NODE:
				n->u.node.addr = c->u.node.addr;
				n->u.node.mask = c->u.node.mask;
				break;
			case OCON_FSUSE:
				n->v.behavior = c->v.behavior;
				n->u.name = strdup(c->u.name);
				if (!n->u.name) {
					ERR(state->handle, "Out of memory!");
					return -1;
				}
				break;
			case OCON_NODE6:
				for (j = 0; j < 4; j++)
					n->u.node6.addr[j] = c->u.node6.addr[j];
				for (j = 0; j < 4; j++)
					n->u.node6.mask[j] = c->u.node6.mask[j];
				break;
			default:
				/* shouldn't get here */
				ERR(state->handle, "Unknown ocontext");
				return -1;
			}
			if (context_copy(&n->context[0], &c->context[0], state)) {
				ERR(state->handle, "Out of memory!");
				return -1;
			}
		}
	}
	return 0;
}

static int ocontext_copy(expand_state_t *state, uint32_t target)
{
	int rc = -1;
	switch (target) {
	case SEPOL_TARGET_SELINUX:
		rc = ocontext_copy_selinux(state);
		break;
	case SEPOL_TARGET_XEN:
		rc = ocontext_copy_xen(state);
		break;
	default:
		ERR(state->handle, "Unknown target");
		return -1;
	}
	return rc;
}

static int genfs_copy(expand_state_t * state)
{
	ocontext_t *c, *newc, *l;
	genfs_t *genfs, *newgenfs, *end;

	end = NULL;
	for (genfs = state->base->genfs; genfs; genfs = genfs->next) {
		newgenfs = malloc(sizeof(genfs_t));
		if (!newgenfs) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		memset(newgenfs, 0, sizeof(genfs_t));
		newgenfs->fstype = strdup(genfs->fstype);
		if (!newgenfs->fstype) {
			free(newgenfs);
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		if (!end)
			state->out->genfs = newgenfs;
		else
			end->next = newgenfs;
		end = newgenfs;

		l = NULL;
		for (c = genfs->head; c; c = c->next) {
			newc = malloc(sizeof(ocontext_t));
			if (!newc) {
				ERR(state->handle, "Out of memory!");
				return -1;
			}
			memset(newc, 0, sizeof(ocontext_t));
			newc->u.name = strdup(c->u.name);
			if (!newc->u.name) {
				ERR(state->handle, "Out of memory!");
				free(newc);
				return -1;
			}
			newc->v.sclass = c->v.sclass;
			context_copy(&newc->context[0], &c->context[0], state);
			if (l)
				l->next = newc;
			else
				newgenfs->head = newc;
			l = newc;
		}
	}
	return 0;
}

static int type_attr_map(hashtab_key_t key
			 __attribute__ ((unused)), hashtab_datum_t datum,
			 void *ptr)
{
	type_datum_t *type;
	expand_state_t *state = ptr;
	policydb_t *p = state->out;
	unsigned int i;
	ebitmap_node_t *tnode;
	int value;

	type = (type_datum_t *) datum;
	value = type->s.value;

	if (type->flavor == TYPE_ATTRIB) {
		if (!(type->flags & TYPE_FLAGS_EXPAND_ATTR_TRUE)) {
			if (ksu_ebitmap_cpy(&p->attr_type_map[value - 1], &type->types)) {
				goto oom;
			}
			ebitmap_for_each_positive_bit(&type->types, tnode, i) {
				if (ksu_ebitmap_set_bit(&p->type_attr_map[i], value - 1, 1)) {
					goto oom;
				}
			}
		} else {
			/* Attribute is being expanded, so remove */
			if (ksu_ebitmap_set_bit(&p->type_attr_map[value - 1], value - 1, 0)) {
				goto oom;
			}
		}
	} else {
		if (ksu_ebitmap_set_bit(&p->attr_type_map[value - 1], value - 1, 1)) {
			goto oom;
		}
	}

	return 0;

oom:
	ERR(state->handle, "Out of memory!");
	return -1;
}

/* converts typeset using typemap and expands into ebitmap_t types using the attributes in the passed in policy.
 * this should not be called until after all the blocks have been processed and the attributes in target policy
 * are complete. */
int expand_convert_type_set(policydb_t * p, uint32_t * typemap,
			    type_set_t * set, ebitmap_t * types,
			    unsigned char alwaysexpand)
{
	type_set_t tmpset;

	type_set_init(&tmpset);

	if (map_ebitmap(&set->types, &tmpset.types, typemap))
		return -1;

	if (map_ebitmap(&set->negset, &tmpset.negset, typemap))
		return -1;

	tmpset.flags = set->flags;

	if (type_set_expand(&tmpset, types, p, alwaysexpand))
		return -1;

	type_set_destroy(&tmpset);

	return 0;
}

/* Expand a rule into a given avtab - checking for conflicting type
 * rules.  Return 1 on success, 0 if the rule conflicts with something
 * (and hence was not added), or -1 on error. */
int expand_rule(sepol_handle_t * handle,
		policydb_t * source_pol,
		avrule_t * source_rule, avtab_t * dest_avtab,
		cond_av_list_t ** cond, cond_av_list_t ** other, int enabled)
{
	int retval;
	ebitmap_t stypes, ttypes;

	if ((source_rule->specified & AVRULE_NEVERALLOW)
		|| (source_rule->specified & AVRULE_XPERMS_NEVERALLOW))
		return 1;

	ebitmap_init(&stypes);
	ebitmap_init(&ttypes);

	if (type_set_expand(&source_rule->stypes, &stypes, source_pol, 1))
		return -1;
	if (type_set_expand(&source_rule->ttypes, &ttypes, source_pol, 1))
		return -1;
	retval = expand_rule_helper(handle, source_pol, NULL,
				    source_rule, dest_avtab,
				    cond, other, enabled, &stypes, &ttypes);
	ksu_ebitmap_destroy(&stypes);
	ksu_ebitmap_destroy(&ttypes);
	return retval;
}

/* Expand a role set into an ebitmap containing the roles.
 * This handles the attribute and flags.
 * Attribute expansion depends on if the rolemap is available.
 * During module compile the rolemap is not available, the
 * possible duplicates of a regular role and the role attribute
 * the regular role belongs to could be properly handled by
 * copy_role_trans and copy_role_allow.
 */
int role_set_expand(role_set_t * x, ebitmap_t * r, policydb_t * out, policydb_t * base, uint32_t * rolemap)
{
	unsigned int i;
	ebitmap_node_t *rnode;
	ebitmap_t mapped_roles, roles;
	policydb_t *p = out;
	role_datum_t *role;

	ebitmap_init(r);

	if (x->flags & ROLE_STAR) {
		for (i = 0; i < p->p_roles.nprim; i++)
			if (ksu_ebitmap_set_bit(r, i, 1))
				return -1;
		return 0;
	}

	ebitmap_init(&mapped_roles);
	ebitmap_init(&roles);
	
	if (rolemap) {
		assert(base != NULL);
		ebitmap_for_each_positive_bit(&x->roles, rnode, i) {
			/* take advantage of p_role_val_to_struct[]
			 * of the base module */
			role = base->role_val_to_struct[i];
			assert(role != NULL);
			if (role->flavor == ROLE_ATTRIB) {
				if (ebitmap_union(&roles,
						  &role->roles))
					goto bad;
			} else {
				if (ksu_ebitmap_set_bit(&roles, i, 1))
					goto bad;
			}
		}
		if (map_ebitmap(&roles, &mapped_roles, rolemap))
			goto bad;
	} else {
		if (ksu_ebitmap_cpy(&mapped_roles, &x->roles))
			goto bad;
	}

	ebitmap_for_each_positive_bit(&mapped_roles, rnode, i) {
		if (ksu_ebitmap_set_bit(r, i, 1))
			goto bad;
	}

	ksu_ebitmap_destroy(&mapped_roles);
	ksu_ebitmap_destroy(&roles);

	/* if role is to be complimented, invert the entire bitmap here */
	if (x->flags & ROLE_COMP) {
		for (i = 0; i < p->p_roles.nprim; i++) {
			if (ksu_ebitmap_get_bit(r, i)) {
				if (ksu_ebitmap_set_bit(r, i, 0))
					return -1;
			} else {
				if (ksu_ebitmap_set_bit(r, i, 1))
					return -1;
			}
		}
	}
	return 0;

bad:
	ksu_ebitmap_destroy(&mapped_roles);
	ksu_ebitmap_destroy(&roles);
	return -1;
}

/* Expand a type set into an ebitmap containing the types. This
 * handles the negset, attributes, and flags.
 * Attribute expansion depends on several factors:
 * - if alwaysexpand is 1, then they will be expanded,
 * - if the type set has a negset or flags, then they will be expanded,
 * - otherwise, they will not be expanded.
 */
int type_set_expand(type_set_t * set, ebitmap_t * t, policydb_t * p,
		    unsigned char alwaysexpand)
{
	unsigned int i;
	ebitmap_t types, neg_types;
	ebitmap_node_t *tnode;
	unsigned char expand = alwaysexpand || !ebitmap_is_empty(&set->negset) || set->flags;
	type_datum_t *type;
	int rc =-1;

	ebitmap_init(&types);
	ebitmap_init(t);

	/* First go through the types and OR all the attributes to types */
	ebitmap_for_each_positive_bit(&set->types, tnode, i) {
		/*
		 * invalid policies might have more types set in the ebitmap than
		 * what's available in the type_val_to_struct mapping
		 */
		if (i >= p->p_types.nprim)
			goto err_types;

		type = p->type_val_to_struct[i];

		if (!type) {
			goto err_types;
		}

		if (type->flavor == TYPE_ATTRIB &&
		    (expand || (type->flags & TYPE_FLAGS_EXPAND_ATTR_TRUE))) {
			if (ebitmap_union(&types, &type->types)) {
				goto err_types;
			}
		} else {
			if (ksu_ebitmap_set_bit(&types, i, 1)) {
				goto err_types;
			}
		}
	}

	/* Now do the same thing for negset */
	ebitmap_init(&neg_types);
	ebitmap_for_each_positive_bit(&set->negset, tnode, i) {
		if (p->type_val_to_struct[i] &&
		    p->type_val_to_struct[i]->flavor == TYPE_ATTRIB) {
			if (ebitmap_union
			    (&neg_types,
			     &p->type_val_to_struct[i]->types)) {
				goto err_neg;
			}
		} else {
			if (ksu_ebitmap_set_bit(&neg_types, i, 1)) {
				goto err_neg;
			}
		}
	}

	if (set->flags & TYPE_STAR) {
		/* set all types not in neg_types */
		for (i = 0; i < p->p_types.nprim; i++) {
			if (ksu_ebitmap_get_bit(&neg_types, i))
				continue;
			if (p->type_val_to_struct[i] &&
			    p->type_val_to_struct[i]->flavor == TYPE_ATTRIB)
				continue;
			if (ksu_ebitmap_set_bit(t, i, 1))
				goto err_neg;
		}
		goto out;
	}

	ebitmap_for_each_positive_bit(&types, tnode, i) {
		if (!ksu_ebitmap_get_bit(&neg_types, i))
			if (ksu_ebitmap_set_bit(t, i, 1))
				goto err_neg;
	}

	if (set->flags & TYPE_COMP) {
		for (i = 0; i < p->p_types.nprim; i++) {
			if (p->type_val_to_struct[i] &&
			    p->type_val_to_struct[i]->flavor == TYPE_ATTRIB) {
				assert(!ksu_ebitmap_get_bit(t, i));
				continue;
			}
			if (ksu_ebitmap_get_bit(t, i)) {
				if (ksu_ebitmap_set_bit(t, i, 0))
					goto err_neg;
			} else {
				if (ksu_ebitmap_set_bit(t, i, 1))
					goto err_neg;
			}
		}
	}

	  out:
	rc = 0;

	  err_neg:
	ksu_ebitmap_destroy(&neg_types);
	  err_types:
	ksu_ebitmap_destroy(&types);

	return rc;
}

static int copy_neverallow(policydb_t * dest_pol, uint32_t * typemap,
			   avrule_t * source_rule)
{
	ebitmap_t stypes, ttypes;
	avrule_t *avrule;
	class_perm_node_t *cur_perm, *new_perm, *tail_perm;
	av_extended_perms_t *xperms = NULL;

	ebitmap_init(&stypes);
	ebitmap_init(&ttypes);

	if (expand_convert_type_set
	    (dest_pol, typemap, &source_rule->stypes, &stypes, 1))
		return -1;
	if (expand_convert_type_set
	    (dest_pol, typemap, &source_rule->ttypes, &ttypes, 1))
		return -1;

	avrule = (avrule_t *) malloc(sizeof(avrule_t));
	if (!avrule)
		return -1;

	avrule_init(avrule);
	avrule->specified = source_rule->specified;
	avrule->line = source_rule->line;
	avrule->flags = source_rule->flags;
	avrule->source_line = source_rule->source_line;
	if (source_rule->source_filename) {
		avrule->source_filename = strdup(source_rule->source_filename);
		if (!avrule->source_filename)
			goto err;
	}

	if (ksu_ebitmap_cpy(&avrule->stypes.types, &stypes))
		goto err;

	if (ksu_ebitmap_cpy(&avrule->ttypes.types, &ttypes))
		goto err;

	cur_perm = source_rule->perms;
	tail_perm = NULL;
	while (cur_perm) {
		new_perm =
		    (class_perm_node_t *) malloc(sizeof(class_perm_node_t));
		if (!new_perm)
			goto err;
		class_perm_node_init(new_perm);
		new_perm->tclass = cur_perm->tclass;
		assert(new_perm->tclass);

		/* once we have modules with permissions we'll need to map the permissions (and classes) */
		new_perm->data = cur_perm->data;

		if (!avrule->perms)
			avrule->perms = new_perm;

		if (tail_perm)
			tail_perm->next = new_perm;
		tail_perm = new_perm;
		cur_perm = cur_perm->next;
	}

	/* copy over extended permissions */
	if (source_rule->xperms) {
		xperms = calloc(1, sizeof(av_extended_perms_t));
		if (!xperms)
			goto err;
		memcpy(xperms, source_rule->xperms, sizeof(av_extended_perms_t));
		avrule->xperms = xperms;
	}

	/* just prepend the avrule to the first branch; it'll never be
	   written to disk */
	if (!dest_pol->global->branch_list->avrules)
		dest_pol->global->branch_list->avrules = avrule;
	else {
		avrule->next = dest_pol->global->branch_list->avrules;
		dest_pol->global->branch_list->avrules = avrule;
	}

	ksu_ebitmap_destroy(&stypes);
	ksu_ebitmap_destroy(&ttypes);

	return 0;

      err:
	ksu_ebitmap_destroy(&stypes);
	ksu_ebitmap_destroy(&ttypes);
	ksu_ebitmap_destroy(&avrule->stypes.types);
	ksu_ebitmap_destroy(&avrule->ttypes.types);
	cur_perm = avrule->perms;
	while (cur_perm) {
		tail_perm = cur_perm->next;
		free(cur_perm);
		cur_perm = tail_perm;
	}
	free(xperms);
	free(avrule);
	return -1;
}

/* 
 * Expands the avrule blocks for a policy. RBAC rules are copied. Neverallow
 * rules are copied or expanded as per the settings in the state object; all
 * other AV rules are expanded.  If neverallow rules are expanded, they are not
 * copied, otherwise they are copied for later use by the assertion checker.
 */
static int copy_and_expand_avrule_block(expand_state_t * state)
{
	avrule_block_t *curblock = state->base->global;
	avrule_block_t *prevblock;
	int retval = -1;

	if (ksu_avtab_alloc(&state->out->te_avtab, MAX_AVTAB_SIZE)) {
 		ERR(state->handle, "Out of Memory!");
 		return -1;
 	}
 
 	if (ksu_avtab_alloc(&state->out->te_cond_avtab, MAX_AVTAB_SIZE)) {
 		ERR(state->handle, "Out of Memory!");
 		return -1;
 	}

	while (curblock) {
		avrule_decl_t *decl = curblock->enabled;
		avrule_t *cur_avrule;

		if (decl == NULL) {
			/* nothing was enabled within this block */
			goto cont;
		}

		/* copy role allows and role trans */
		if (copy_role_allows(state, decl->role_allow_rules) != 0 ||
		    copy_role_trans(state, decl->role_tr_rules) != 0) {
			goto cleanup;
		}

		if (expand_filename_trans(state, decl->filename_trans_rules))
			goto cleanup;

		/* expand the range transition rules */
		if (expand_range_trans(state, decl->range_tr_rules))
			goto cleanup;

		/* copy rules */
		cur_avrule = decl->avrules;
		while (cur_avrule != NULL) {
			if (!(state->expand_neverallow)
			    && cur_avrule->specified & (AVRULE_NEVERALLOW | AVRULE_XPERMS_NEVERALLOW)) {
				/* copy this over directly so that assertions are checked later */
				if (copy_neverallow
				    (state->out, state->typemap, cur_avrule))
					ERR(state->handle,
					    "Error while copying neverallow.");
			} else {
				if (cur_avrule->specified & (AVRULE_NEVERALLOW | AVRULE_XPERMS_NEVERALLOW))
					state->out->unsupported_format = 1;
				if (convert_and_expand_rule
				    (state->handle, state->out, state->typemap,
				     cur_avrule, &state->out->te_avtab, NULL,
				     NULL, 0,
				     state->expand_neverallow) !=
				    EXPAND_RULE_SUCCESS) {
					goto cleanup;
				}
			}
			cur_avrule = cur_avrule->next;
		}

		/* copy conditional rules */
		if (cond_node_copy(state, decl->cond_list))
			goto cleanup;

      cont:
		prevblock = curblock;
		curblock = curblock->next;

		if (state->handle && state->handle->expand_consume_base) {
			/* set base top avrule block in case there
 			 * is an error condition and the policy needs 
 			 * to be destroyed */
			state->base->global = curblock;
			avrule_block_destroy(prevblock);
		}
	}

	retval = 0;

      cleanup:
	return retval;
}

/* 
 * This function allows external users of the library (such as setools) to
 * expand only the avrules and optionally perform expansion of neverallow rules
 * or expand into the same policy for analysis purposes.
 */
int expand_module_avrules(sepol_handle_t * handle, policydb_t * base,
			  policydb_t * out, uint32_t * typemap,
			  uint32_t * boolmap, uint32_t * rolemap,
			  uint32_t * usermap, int verbose,
			  int expand_neverallow)
{
	expand_state_t state;

	expand_state_init(&state);

	state.base = base;
	state.out = out;
	state.typemap = typemap;
	state.boolmap = boolmap;
	state.rolemap = rolemap;
	state.usermap = usermap;
	state.handle = handle;
	state.verbose = verbose;
	state.expand_neverallow = expand_neverallow;

	return copy_and_expand_avrule_block(&state);
}

static void discard_tunables(sepol_handle_t *sh, policydb_t *pol)
{
	avrule_block_t *block;
	avrule_decl_t *decl;
	cond_node_t *cur_node;
	cond_expr_t *cur_expr;
	int cur_state, preserve_tunables = 0;
	avrule_t *tail, *to_be_appended;

	if (sh && sh->preserve_tunables)
		preserve_tunables = 1;

	/* Iterate through all cond_node of all enabled decls, if a cond_node
	 * is about tunable, calculate its state value and concatenate one of
	 * its avrule list to the current decl->avrules list. On the other
	 * hand, the disabled unused branch of a tunable would be discarded.
	 *
	 * Note, such tunable cond_node would be skipped over in expansion,
	 * so we won't have to worry about removing it from decl->cond_list
	 * here :-)
	 *
	 * If tunables are requested to be preserved then they would be
	 * "transformed" as booleans by having their TUNABLE flag cleared.
	 */
	for (block = pol->global; block != NULL; block = block->next) {
		decl = block->enabled;
		if (decl == NULL || decl->enabled == 0)
			continue;

		tail = decl->avrules;
		while (tail && tail->next)
			tail = tail->next;

		for (cur_node = decl->cond_list; cur_node != NULL;
		     cur_node = cur_node->next) {
			int booleans, tunables, i;
			cond_bool_datum_t *booldatum;
			cond_bool_datum_t *tmp[COND_EXPR_MAXDEPTH];

			booleans = tunables = 0;
			memset(tmp, 0, sizeof(cond_bool_datum_t *) * COND_EXPR_MAXDEPTH);

			for (cur_expr = cur_node->expr; cur_expr != NULL;
			     cur_expr = cur_expr->next) {
				if (cur_expr->expr_type != COND_BOOL)
					continue;
				booldatum = pol->bool_val_to_struct[cur_expr->bool - 1];
				if (booldatum->flags & COND_BOOL_FLAGS_TUNABLE)
					tmp[tunables++] = booldatum;
				else
					booleans++;
			}

			/* bool_copy_callback() at link phase has ensured
			 * that no mixture of tunables and booleans in one
			 * expression. However, this would be broken by the
			 * request to preserve tunables */
			if (!preserve_tunables)
				assert(!(booleans && tunables));

			if (booleans || preserve_tunables) {
				cur_node->flags &= ~COND_NODE_FLAGS_TUNABLE;
				if (tunables) {
					for (i = 0; i < tunables; i++)
						tmp[i]->flags &= ~COND_BOOL_FLAGS_TUNABLE;
				}
			} else {
				cur_node->flags |= COND_NODE_FLAGS_TUNABLE;
				cur_state = cond_evaluate_expr(pol, cur_node->expr);
				if (cur_state == -1) {
					printf("Expression result was "
					       "undefined, skipping all"
					       "rules\n");
					continue;
				}

				to_be_appended = (cur_state == 1) ?
					cur_node->avtrue_list : cur_node->avfalse_list;

				if (tail)
					tail->next = to_be_appended;
				else
					tail = decl->avrules = to_be_appended;

				/* Now that the effective branch has been
				 * appended, neutralize its original pointer */
				if (cur_state == 1)
					cur_node->avtrue_list = NULL;
				else
					cur_node->avfalse_list = NULL;

				/* Update the tail of decl->avrules for
				 * further concatenation */
				while (tail && tail->next)
					tail = tail->next;
			}
		}
	}
}

/* Linking should always be done before calling expand, even if
 * there is only a base since all optionals are dealt with at link time
 * the base passed in should be indexed and avrule blocks should be 
 * enabled.
 */
int expand_module(sepol_handle_t * handle,
		  policydb_t * base, policydb_t * out, int verbose, int check)
{
	int retval = -1;
	unsigned int i;
	expand_state_t state;
	avrule_block_t *curblock;

	/* Append tunable's avtrue_list or avfalse_list to the avrules list
	 * of its home decl depending on its state value, so that the effect
	 * rules of a tunable would be added to te_avtab permanently. Whereas
	 * the disabled unused branch would be discarded.
	 *
	 * Originally this function is called at the very end of link phase,
	 * however, we need to keep the linked policy intact for analysis
	 * purpose. */
	discard_tunables(handle, base);

	expand_state_init(&state);

	state.verbose = verbose;
	state.typemap = NULL;
	state.base = base;
	state.out = out;
	state.handle = handle;

	if (base->policy_type != POLICY_BASE) {
		ERR(handle, "Target of expand was not a base policy.");
		return -1;
	}

	state.out->policy_type = POLICY_KERN;
	state.out->policyvers = POLICYDB_VERSION_MAX;
	if (state.base->name) {
		state.out->name = strdup(state.base->name);
	}

	/* Copy mls state from base to out */
	out->mls = base->mls;
	out->handle_unknown = base->handle_unknown;

	/* Copy target from base to out */
	out->target_platform = base->target_platform;

	/* Copy policy capabilities */
	if (ksu_ebitmap_cpy(&out->policycaps, &base->policycaps)) {
		ERR(handle, "Out of memory!");
		goto cleanup;
	}

	if ((state.typemap =
	     (uint32_t *) calloc(state.base->p_types.nprim,
				 sizeof(uint32_t))) == NULL) {
		ERR(handle, "Out of memory!");
		goto cleanup;
	}

	state.boolmap = (uint32_t *)calloc(state.base->p_bools.nprim, sizeof(uint32_t));
	if (!state.boolmap) {
		ERR(handle, "Out of memory!");
		goto cleanup;
	}

	state.rolemap = (uint32_t *)calloc(state.base->p_roles.nprim, sizeof(uint32_t));
	if (!state.rolemap) {
		ERR(handle, "Out of memory!");
		goto cleanup;
	}

	state.usermap = (uint32_t *)calloc(state.base->p_users.nprim, sizeof(uint32_t));
	if (!state.usermap) {
		ERR(handle, "Out of memory!");
		goto cleanup;
	}

	/* order is important - types must be first */

	/* copy types */
	if (ksu_hashtab_map(state.base->p_types.table, type_copy_callback, &state)) {
		goto cleanup;
	}

	/* convert attribute type sets */
	if (ksu_hashtab_map
	    (state.base->p_types.table, attr_convert_callback, &state)) {
		goto cleanup;
	}

	/* copy commons */
	if (ksu_hashtab_map
	    (state.base->p_commons.table, common_copy_callback, &state)) {
		goto cleanup;
	}

	/* copy classes, note, this does not copy constraints, constraints can't be
	 * copied until after all the blocks have been processed and attributes are complete */
	if (ksu_hashtab_map
	    (state.base->p_classes.table, class_copy_callback, &state)) {
		goto cleanup;
	}

	/* copy type bounds */
	if (ksu_hashtab_map(state.base->p_types.table,
			type_bounds_copy_callback, &state))
		goto cleanup;

	/* copy aliases */
	if (ksu_hashtab_map(state.base->p_types.table, alias_copy_callback, &state))
		goto cleanup;

	/* index here so that type indexes are available for role_copy_callback */
	if (policydb_index_others(handle, out, verbose)) {
		ERR(handle, "Error while indexing out symbols");
		goto cleanup;
	}

	/* copy roles */
	if (ksu_hashtab_map(state.base->p_roles.table, role_copy_callback, &state))
		goto cleanup;
	if (ksu_hashtab_map(state.base->p_roles.table,
			role_bounds_copy_callback, &state))
		goto cleanup;

	/* copy MLS's sensitivity level and categories - this needs to be done
	 * before expanding users (they need to be indexed too) */
	if (ksu_hashtab_map(state.base->p_levels.table, sens_copy_callback, &state))
		goto cleanup;
	if (ksu_hashtab_map(state.base->p_cats.table, cats_copy_callback, &state))
		goto cleanup;
	if (policydb_index_others(handle, out, verbose)) {
		ERR(handle, "Error while indexing out symbols");
		goto cleanup;
	}

	/* copy users */
	if (ksu_hashtab_map(state.base->p_users.table, user_copy_callback, &state))
		goto cleanup;
	if (ksu_hashtab_map(state.base->p_users.table,
			user_bounds_copy_callback, &state))
		goto cleanup;

	/* copy bools */
	if (ksu_hashtab_map(state.base->p_bools.table, bool_copy_callback, &state))
		goto cleanup;

	if (policydb_index_classes(out)) {
		ERR(handle, "Error while indexing out classes");
		goto cleanup;
	}
	if (policydb_index_others(handle, out, verbose)) {
		ERR(handle, "Error while indexing out symbols");
		goto cleanup;
	}

	/* loop through all decls and union attributes, roles, users */
	for (curblock = state.base->global; curblock != NULL;
	     curblock = curblock->next) {
		avrule_decl_t *decl = curblock->enabled;

		if (decl == NULL) {
			/* nothing was enabled within this block */
			continue;
		}

		/* convert attribute type sets */
		if (ksu_hashtab_map
		    (decl->p_types.table, attr_convert_callback, &state)) {
			goto cleanup;
		}

		/* copy roles */
		if (ksu_hashtab_map
		    (decl->p_roles.table, role_copy_callback, &state))
			goto cleanup;

		/* copy users */
		if (ksu_hashtab_map
		    (decl->p_users.table, user_copy_callback, &state))
			goto cleanup;

	}

	/* remap role dominates bitmaps */
	 if (ksu_hashtab_map(state.out->p_roles.table, role_remap_dominates, &state)) {
		goto cleanup;
	}

	/* escalate the type_set_t in a role attribute to all regular roles
	 * that belongs to it. */
	if (ksu_hashtab_map(state.base->p_roles.table, role_fix_callback, &state))
		goto cleanup;

	if (copy_and_expand_avrule_block(&state) < 0) {
		ERR(handle, "Error during expand");
		goto cleanup;
	}

	/* copy constraints */
	if (ksu_hashtab_map
	    (state.base->p_classes.table, constraint_copy_callback, &state)) {
		goto cleanup;
	}

	cond_optimize_lists(state.out->cond_list);
	if (evaluate_conds(state.out))
		goto cleanup;

	/* copy ocontexts */
	if (ocontext_copy(&state, out->target_platform))
		goto cleanup;

	/* copy genfs */
	if (genfs_copy(&state))
		goto cleanup;

	/* Build the type<->attribute maps and remove attributes. */
	state.out->attr_type_map = calloc(state.out->p_types.nprim,
					  sizeof(ebitmap_t));
	state.out->type_attr_map = calloc(state.out->p_types.nprim,
					  sizeof(ebitmap_t));
	if (!state.out->attr_type_map || !state.out->type_attr_map) {
		ERR(handle, "Out of memory!");
		goto cleanup;
	}
	for (i = 0; i < state.out->p_types.nprim; i++) {
		/* add the type itself as the degenerate case */
		if (ksu_ebitmap_set_bit(&state.out->type_attr_map[i], i, 1)) {
			ERR(handle, "Out of memory!");
			goto cleanup;
		}
	}
	if (ksu_hashtab_map(state.out->p_types.table, type_attr_map, &state))
		goto cleanup;
	if (check) {
		if (hierarchy_check_constraints(handle, state.out))
			goto cleanup;

		if (check_assertions
		    (handle, state.out,
		     state.out->global->branch_list->avrules))
			 goto cleanup;
	}

	retval = 0;

      cleanup:
	free(state.typemap);
	free(state.boolmap);
	free(state.rolemap);
	free(state.usermap);
	return retval;
}

static int expand_avtab_insert(avtab_t * a, avtab_key_t * k, avtab_datum_t * d)
{
	avtab_ptr_t node;
	avtab_datum_t *avd;
	avtab_extended_perms_t *xperms;
	unsigned int i;
	unsigned int match = 0;

	if (k->specified & AVTAB_XPERMS) {
		/*
		 * AVTAB_XPERMS entries are not necessarily unique.
		 * find node with matching xperms
		 */
		node = ksu_avtab_search_node(a, k);
		while (node) {
			if ((node->datum.xperms->specified == d->xperms->specified) &&
				(node->datum.xperms->driver == d->xperms->driver)) {
				match = 1;
				break;
			}
			node = ksu_avtab_search_node_next(node, k->specified);
		}
		if (!match)
			node = NULL;
	} else {
		node = ksu_avtab_search_node(a, k);
	}

	if (!node || ((k->specified & AVTAB_ENABLED) !=
			(node->key.specified & AVTAB_ENABLED))) {
		node = ksu_avtab_insert_nonunique(a, k, d);
		if (!node) {
			ERR(NULL, "Out of memory!");
			return -1;
		}
		return 0;
	}

	avd = &node->datum;
	xperms = node->datum.xperms;
	switch (k->specified & ~AVTAB_ENABLED) {
	case AVTAB_ALLOWED:
	case AVTAB_AUDITALLOW:
		avd->data |= d->data;
		break;
	case AVTAB_AUDITDENY:
		avd->data &= d->data;
		break;
	case AVTAB_XPERMS_ALLOWED:
	case AVTAB_XPERMS_AUDITALLOW:
	case AVTAB_XPERMS_DONTAUDIT:
		for (i = 0; i < ARRAY_SIZE(xperms->perms); i++)
			xperms->perms[i] |= d->xperms->perms[i];
		break;
	default:
		ERR(NULL, "Type conflict!");
		return -1;
	}

	return 0;
}

struct expand_avtab_data {
	avtab_t *expa;
	policydb_t *p;

};

static int expand_avtab_node(avtab_key_t * k, avtab_datum_t * d, void *args)
{
	struct expand_avtab_data *ptr = args;
	avtab_t *expa = ptr->expa;
	policydb_t *p = ptr->p;
	type_datum_t *stype = p->type_val_to_struct[k->source_type - 1];
	type_datum_t *ttype = p->type_val_to_struct[k->target_type - 1];
	ebitmap_t *sattr = &p->attr_type_map[k->source_type - 1];
	ebitmap_t *tattr = &p->attr_type_map[k->target_type - 1];
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	avtab_key_t newkey;
	int rc;

	newkey.target_class = k->target_class;
	newkey.specified = k->specified;

	if (stype && ttype && stype->flavor != TYPE_ATTRIB && ttype->flavor != TYPE_ATTRIB) {
		/* Both are individual types, no expansion required. */
		return expand_avtab_insert(expa, k, d);
	}

	if (stype && stype->flavor != TYPE_ATTRIB) {
		/* Source is an individual type, target is an attribute. */
		newkey.source_type = k->source_type;
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			newkey.target_type = j + 1;
			rc = expand_avtab_insert(expa, &newkey, d);
			if (rc)
				return -1;
		}
		return 0;
	}

	if (ttype && ttype->flavor != TYPE_ATTRIB) {
		/* Target is an individual type, source is an attribute. */
		newkey.target_type = k->target_type;
		ebitmap_for_each_positive_bit(sattr, snode, i) {
			newkey.source_type = i + 1;
			rc = expand_avtab_insert(expa, &newkey, d);
			if (rc)
				return -1;
		}
		return 0;
	}

	/* Both source and target type are attributes. */
	ebitmap_for_each_positive_bit(sattr, snode, i) {
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			newkey.source_type = i + 1;
			newkey.target_type = j + 1;
			rc = expand_avtab_insert(expa, &newkey, d);
			if (rc)
				return -1;
		}
	}

	return 0;
}

int expand_avtab(policydb_t * p, avtab_t * a, avtab_t * expa)
{
	struct expand_avtab_data data;

	if (ksu_avtab_alloc(expa, MAX_AVTAB_SIZE)) {
		ERR(NULL, "Out of memory!");
		return -1;
	}

	data.expa = expa;
	data.p = p;
	return avtab_map(a, expand_avtab_node, &data);
}

static int expand_cond_insert(cond_av_list_t ** l,
			      avtab_t * expa,
			      avtab_key_t * k, avtab_datum_t * d)
{
	avtab_ptr_t node;
	avtab_datum_t *avd;
	cond_av_list_t *nl;

	node = ksu_avtab_search_node(expa, k);
	if (!node ||
	    (k->specified & AVTAB_ENABLED) !=
	    (node->key.specified & AVTAB_ENABLED)) {
		node = ksu_avtab_insert_nonunique(expa, k, d);
		if (!node) {
			ERR(NULL, "Out of memory!");
			return -1;
		}
		node->parse_context = (void *)1;
		nl = (cond_av_list_t *) malloc(sizeof(*nl));
		if (!nl) {
			ERR(NULL, "Out of memory!");
			return -1;
		}
		memset(nl, 0, sizeof(*nl));
		nl->node = node;
		nl->next = *l;
		*l = nl;
		return 0;
	}

	avd = &node->datum;
	switch (k->specified & ~AVTAB_ENABLED) {
	case AVTAB_ALLOWED:
	case AVTAB_AUDITALLOW:
		avd->data |= d->data;
		break;
	case AVTAB_AUDITDENY:
		avd->data &= d->data;
		break;
	default:
		ERR(NULL, "Type conflict!");
		return -1;
	}

	return 0;
}

static int expand_cond_av_node(policydb_t * p,
			       avtab_ptr_t node,
			       cond_av_list_t ** newl, avtab_t * expa)
{
	avtab_key_t *k = &node->key;
	avtab_datum_t *d = &node->datum;
	type_datum_t *stype = p->type_val_to_struct[k->source_type - 1];
	type_datum_t *ttype = p->type_val_to_struct[k->target_type - 1];
	ebitmap_t *sattr = &p->attr_type_map[k->source_type - 1];
	ebitmap_t *tattr = &p->attr_type_map[k->target_type - 1];
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	avtab_key_t newkey;
	int rc;

	newkey.target_class = k->target_class;
	newkey.specified = k->specified;

	if (stype && ttype && stype->flavor != TYPE_ATTRIB && ttype->flavor != TYPE_ATTRIB) {
		/* Both are individual types, no expansion required. */
		return expand_cond_insert(newl, expa, k, d);
	}

	if (stype && stype->flavor != TYPE_ATTRIB) {
		/* Source is an individual type, target is an attribute. */
		newkey.source_type = k->source_type;
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			newkey.target_type = j + 1;
			rc = expand_cond_insert(newl, expa, &newkey, d);
			if (rc)
				return -1;
		}
		return 0;
	}

	if (ttype && ttype->flavor != TYPE_ATTRIB) {
		/* Target is an individual type, source is an attribute. */
		newkey.target_type = k->target_type;
		ebitmap_for_each_positive_bit(sattr, snode, i) {
			newkey.source_type = i + 1;
			rc = expand_cond_insert(newl, expa, &newkey, d);
			if (rc)
				return -1;
		}
		return 0;
	}

	/* Both source and target type are attributes. */
	ebitmap_for_each_positive_bit(sattr, snode, i) {
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			newkey.source_type = i + 1;
			newkey.target_type = j + 1;
			rc = expand_cond_insert(newl, expa, &newkey, d);
			if (rc)
				return -1;
		}
	}

	return 0;
}

int expand_cond_av_list(policydb_t * p, cond_av_list_t * l,
			cond_av_list_t ** newl, avtab_t * expa)
{
	cond_av_list_t *cur;
	avtab_ptr_t node;
	int rc;

	if (ksu_avtab_alloc(expa, MAX_AVTAB_SIZE)) {
		ERR(NULL, "Out of memory!");
		return -1;
	}

	*newl = NULL;
	for (cur = l; cur; cur = cur->next) {
		node = cur->node;
		rc = expand_cond_av_node(p, node, newl, expa);
		if (rc)
			return rc;
	}

	return 0;
}

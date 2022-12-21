#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#include <sepol/kernel_to_conf.h>
#include <sepol/policydb/avtab.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/polcaps.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/services.h>
#include <sepol/policydb/util.h>

#include "kernel_to_common.h"


static char *cond_expr_to_str(struct policydb *pdb, struct cond_expr *expr)
{
	struct cond_expr *curr;
	struct strs *stack;
	char *new_val;
	char *str = NULL;
	int rc;

	rc = strs_stack_init(&stack);
	if (rc != 0) {
		goto exit;
	}

	for (curr = expr; curr != NULL; curr = curr->next) {
		if (curr->expr_type == COND_BOOL) {
			char *val1 = pdb->p_bool_val_to_name[curr->bool - 1];
			new_val = create_str("%s", 1, val1);
		} else {
			const char *op;
			uint32_t num_params;
			char *val1 = NULL;
			char *val2 = NULL;

			switch(curr->expr_type) {
			case COND_NOT:	op = "!";  num_params = 1; break;
			case COND_OR:	op = "||"; num_params = 2; break;
			case COND_AND:	op = "&&"; num_params = 2; break;
			case COND_XOR:	op = "^";  num_params = 2; break;
			case COND_EQ:	op = "=="; num_params = 2; break;
			case COND_NEQ:	op = "!="; num_params = 2; break;
			default:
				sepol_log_err("Unknown conditional operator: %i", curr->expr_type);
				goto exit;
			}

			if (num_params == 2) {
				val2 = strs_stack_pop(stack);
				if (!val2) {
					sepol_log_err("Invalid conditional expression");
					goto exit;
				}
			}
			val1 = strs_stack_pop(stack);
			if (!val1) {
				sepol_log_err("Invalid conditional expression");
				free(val2);
				goto exit;
			}
			if (num_params == 2) {
				new_val = create_str("(%s %s %s)", 3, val1, op, val2);
				free(val2);
			} else {
				new_val = create_str("%s %s", 2, op, val1);
			}
			free(val1);
		}
		if (!new_val) {
			sepol_log_err("Invalid conditional expression");
			goto exit;
		}
		rc = strs_stack_push(stack, new_val);
		if (rc != 0) {
			sepol_log_err("Out of memory");
			goto exit;
		}
	}

	new_val = strs_stack_pop(stack);
	if (!new_val || !strs_stack_empty(stack)) {
		sepol_log_err("Invalid conditional expression");
		goto exit;
	}

	str = new_val;

	strs_stack_destroy(&stack);
	return str;

exit:
	if (stack) {
		while ((new_val = strs_stack_pop(stack)) != NULL) {
			free(new_val);
		}
		strs_stack_destroy(&stack);
	}

	return NULL;
}

static char *constraint_expr_to_str(struct policydb *pdb, struct constraint_expr *expr, int *use_mls)
{
	struct constraint_expr *curr;
	struct strs *stack = NULL;
	char *new_val = NULL;
	const char *op;
	char *str = NULL;
	int rc;

	*use_mls = 0;

	rc = strs_stack_init(&stack);
	if (rc != 0) {
		goto exit;
	}

	for (curr = expr; curr; curr = curr->next) {
		if (curr->expr_type == CEXPR_ATTR || curr->expr_type == CEXPR_NAMES) {
			const char *attr1 = NULL;
			const char *attr2 = NULL;

			switch (curr->op) {
			case CEXPR_EQ:      op = "==";     break;
			case CEXPR_NEQ:     op = "!=";    break;
			case CEXPR_DOM:     op = "dom";    break;
			case CEXPR_DOMBY:   op = "domby";  break;
			case CEXPR_INCOMP:  op = "incomp"; break;
			default:
				sepol_log_err("Unknown constraint operator: %i", curr->op);
				goto exit;
			}

			switch (curr->attr) {
			case CEXPR_USER:                 attr1 ="u1"; attr2 ="u2"; break;
			case CEXPR_USER | CEXPR_TARGET:  attr1 ="u2"; attr2 ="";   break;
			case CEXPR_USER | CEXPR_XTARGET: attr1 ="u3"; attr2 ="";   break;
			case CEXPR_ROLE:                 attr1 ="r1"; attr2 ="r2"; break;
			case CEXPR_ROLE | CEXPR_TARGET:  attr1 ="r2"; attr2 ="";   break;
			case CEXPR_ROLE | CEXPR_XTARGET: attr1 ="r3"; attr2 ="";   break;
			case CEXPR_TYPE:                 attr1 ="t1"; attr2 ="t2"; break;
			case CEXPR_TYPE | CEXPR_TARGET:  attr1 ="t2"; attr2 ="";   break;
			case CEXPR_TYPE | CEXPR_XTARGET: attr1 ="t3"; attr2 ="";   break;
			case CEXPR_L1L2:                 attr1 ="l1"; attr2 ="l2"; break;
			case CEXPR_L1H2:                 attr1 ="l1"; attr2 ="h2"; break;
			case CEXPR_H1L2:                 attr1 ="h1"; attr2 ="l2"; break;
			case CEXPR_H1H2:                 attr1 ="h1"; attr2 ="h2"; break;
			case CEXPR_L1H1:                 attr1 ="l1"; attr2 ="h1"; break;
			case CEXPR_L2H2:                 attr1 ="l2"; attr2 ="h2"; break;
			default:
				sepol_log_err("Unknown constraint attribute: %i", curr->attr);
				goto exit;
			}

			if (curr->attr >= CEXPR_XTARGET) {
				*use_mls = 1;
			}

			if (curr->expr_type == CEXPR_ATTR) {
				new_val = create_str("%s %s %s", 3, attr1, op, attr2);
			} else {
				char *names = NULL;
				if (curr->attr & CEXPR_TYPE) {
					struct type_set *ts = curr->type_names;
					names = ebitmap_to_str(&ts->types, pdb->p_type_val_to_name, 1);
				} else if (curr->attr & CEXPR_USER) {
					names = ebitmap_to_str(&curr->names, pdb->p_user_val_to_name, 1);
				} else if (curr->attr & CEXPR_ROLE) {
					names = ebitmap_to_str(&curr->names, pdb->p_role_val_to_name, 1);
				}
				if (!names) {
					names = strdup("NO_IDENTIFIER");
					if (!names) {
						sepol_log_err("Out of memory");
						goto exit;
					}
				}
				if (strchr(names, ' ')) {
					new_val = create_str("%s %s { %s }", 3, attr1, op, names);
				} else {
					new_val = create_str("%s %s %s", 3, attr1, op, names);
				}
				free(names);
			}
		} else {
			uint32_t num_params;
			char *val1 = NULL;
			char *val2 = NULL;

			switch (curr->expr_type) {
			case CEXPR_NOT: op = "not"; num_params = 1; break;
			case CEXPR_AND: op = "and"; num_params = 2; break;
			case CEXPR_OR:  op = "or";  num_params = 2; break;
			default:
				sepol_log_err("Unknown constraint expression type: %i", curr->expr_type);
				goto exit;
			}

			if (num_params == 2) {
				val2 = strs_stack_pop(stack);
				if (!val2) {
					sepol_log_err("Invalid constraint expression");
					goto exit;
				}
			}
			val1 = strs_stack_pop(stack);
			if (!val1) {
				sepol_log_err("Invalid constraint expression");
				goto exit;
			}

			if (num_params == 2) {
				new_val = create_str("(%s %s %s)", 3, val1, op, val2);
				free(val2);
			} else {
				new_val = create_str("%s (%s)", 2, op, val1);
			}
			free(val1);
		}
		if (!new_val) {
			goto exit;
		}
		rc = strs_stack_push(stack, new_val);
		if (rc != 0) {
			sepol_log_err("Out of memory");
			goto exit;
		}
	}

	new_val = strs_stack_pop(stack);
	if (!new_val || !strs_stack_empty(stack)) {
		sepol_log_err("Invalid constraint expression");
		goto exit;
	}

	str = new_val;

	strs_stack_destroy(&stack);

	return str;

exit:
	if (stack) {
		while ((new_val = strs_stack_pop(stack)) != NULL) {
			free(new_val);
		}
		strs_stack_destroy(&stack);
	}

	return NULL;
}

static int class_constraint_rules_to_strs(struct policydb *pdb, char *classkey,
					  class_datum_t *class,
					  struct constraint_node *constraint_rules,
					  struct strs *mls_list,
					  struct strs *non_mls_list)
{
	struct constraint_node *curr;
	struct strs *strs;
	const char *flavor, *perm_prefix, *perm_suffix;
	char *perms, *expr;
	int is_mls;
	int rc = 0;

	for (curr = constraint_rules; curr != NULL; curr = curr->next) {
		if (curr->permissions == 0) {
			continue;
		}
		expr = constraint_expr_to_str(pdb, curr->expr, &is_mls);
		if (!expr) {
			rc = -1;
			goto exit;
		}

		perms = sepol_av_to_string(pdb, class->s.value, curr->permissions);
		if (strchr(perms, ' ')) {
			perm_prefix = "{ ";
			perm_suffix = " }";
		} else {
			perm_prefix = "";
			perm_suffix = "";
		}
		if (is_mls) {
			flavor = "mlsconstrain";
			strs = mls_list;
		} else {
			flavor = "constrain";
			strs = non_mls_list;
		}

		rc = strs_create_and_add(strs, "%s %s %s%s%s %s;", 6,
					 flavor, classkey,
					 perm_prefix, perms+1, perm_suffix,
					 expr);
		free(expr);
		if (rc != 0) {
			goto exit;
		}
	}

	return 0;
exit:
	sepol_log_err("Error gathering constraint rules\n");
	return rc;
}

static int class_validatetrans_rules_to_strs(struct policydb *pdb, char *classkey,
					     struct constraint_node *validatetrans_rules,
					     struct strs *mls_list,
					     struct strs *non_mls_list)
{
	struct constraint_node *curr;
	struct strs *strs;
	const char *flavor;
	char *expr;
	int is_mls;
	int rc = 0;

	for (curr = validatetrans_rules; curr != NULL; curr = curr->next) {
		expr = constraint_expr_to_str(pdb, curr->expr, &is_mls);
		if (!expr) {
			rc = -1;
			goto exit;
		}

		if (is_mls) {
			flavor = "mlsvalidatetrans";
			strs = mls_list;
		} else {
			flavor = "validatetrans";
			strs = non_mls_list;
		}

		rc = strs_create_and_add(strs, "%s %s %s;", 3, flavor, classkey, expr);
		free(expr);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	return rc;
}

static int constraint_rules_to_strs(struct policydb *pdb, struct strs *mls_strs, struct strs *non_mls_strs)
{
	class_datum_t *class;
	char *name;
	unsigned i;
	int rc = 0;

	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (class && class->constraints) {
			name = pdb->p_class_val_to_name[i];
			rc = class_constraint_rules_to_strs(pdb, name, class, class->constraints, mls_strs, non_mls_strs);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(mls_strs);
	strs_sort(non_mls_strs);

exit:
	return rc;
}

static int validatetrans_rules_to_strs(struct policydb *pdb, struct strs *mls_strs, struct strs *non_mls_strs)
{
	class_datum_t *class;
	char *name;
	unsigned i;
	int rc = 0;

	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (class && class->validatetrans) {
			name = pdb->p_class_val_to_name[i];
			rc = class_validatetrans_rules_to_strs(pdb, name, class->validatetrans, mls_strs, non_mls_strs);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(mls_strs);
	strs_sort(non_mls_strs);

exit:
	return rc;
}

static int write_handle_unknown_to_conf(FILE *out, struct policydb *pdb)
{
	const char *action;

	switch (pdb->handle_unknown) {
	case SEPOL_DENY_UNKNOWN:
		action = "deny";
		break;
	case SEPOL_REJECT_UNKNOWN:
		action = "reject";
		break;
	case SEPOL_ALLOW_UNKNOWN:
		action = "allow";
		break;
	default:
		sepol_log_err("Unknown value for handle-unknown: %i", pdb->handle_unknown);
		return -1;
	}

	sepol_printf(out, "# handle_unknown %s\n", action);

	return 0;
}

static int write_class_decl_rules_to_conf(FILE *out, struct policydb *pdb)
{
	char *name;
	unsigned i;

	for (i=0; i < pdb->p_classes.nprim; i++) {
		name = pdb->p_class_val_to_name[i];
		sepol_printf(out, "class %s\n", name);
	}

	return 0;
}

static int write_sids_to_conf(FILE *out, const char *const *sid_to_str,
			      unsigned num_sids, struct ocontext *isids)
{
	struct ocontext *isid;
	struct strs *strs;
	char *sid;
	char unknown[18];
	unsigned i;
	int rc;

	rc = strs_init(&strs, num_sids+1);
	if (rc != 0) {
		goto exit;
	}

	for (isid = isids; isid != NULL; isid = isid->next) {
		i = isid->sid[0];
		if (i < num_sids) {
			sid = (char *)sid_to_str[i];
		} else {
			snprintf(unknown, sizeof(unknown), "%s%u", "UNKNOWN", i);
			sid = strdup(unknown);
			if (!sid) {
				rc = -1;
				goto exit;
			}
		}
		rc = strs_add_at_index(strs, sid, i);
		if (rc != 0) {
			goto exit;
		}
	}

	for (i=0; i<strs_num_items(strs); i++) {
		sid = strs_read_at_index(strs, i);
		if (!sid) {
			continue;
		}
		sepol_printf(out, "sid %s\n", sid);
	}

exit:
	for (i=num_sids; i<strs_num_items(strs); i++) {
		sid = strs_read_at_index(strs, i);
		free(sid);
	}
	strs_destroy(&strs);
	if (rc != 0) {
		sepol_log_err("Error writing sid rules to policy.conf\n");
	}

	return rc;
}

static int write_sid_decl_rules_to_conf(FILE *out, struct policydb *pdb)
{
	int rc = 0;

	if (pdb->target_platform == SEPOL_TARGET_SELINUX) {
		rc = write_sids_to_conf(out, selinux_sid_to_str, SELINUX_SID_SZ,
					pdb->ocontexts[0]);
	} else if (pdb->target_platform == SEPOL_TARGET_XEN) {
		rc = write_sids_to_conf(out, xen_sid_to_str, XEN_SID_SZ,
					pdb->ocontexts[0]);
	} else {
		sepol_log_err("Unknown target platform: %i", pdb->target_platform);
		rc = -1;
	}

	return rc;
}
static char *class_or_common_perms_to_str(symtab_t *permtab)
{
	struct strs *strs;
	char *perms = NULL;
	int rc = 0;

	rc = strs_init(&strs, permtab->nprim);
	if (rc != 0) {
		goto exit;
	}

	rc = ksu_hashtab_map(permtab->table, hashtab_ordered_to_strs, strs);
	if (rc != 0) {
		goto exit;
	}

	if (strs_num_items(strs) > 0) {
		perms = strs_to_str(strs);
	}

exit:
	strs_destroy(&strs);

	return perms;
}

static int write_class_and_common_rules_to_conf(FILE *out, struct policydb *pdb)
{
	class_datum_t *class;
	common_datum_t *common;
	int *used;
	char *name, *perms;
	unsigned i;
	int rc = 0;

	/* common */
	used = calloc(pdb->p_commons.nprim, sizeof(*used));
	if (!used) {
		sepol_log_err("Out of memory");
		rc = -1;
		goto exit;
	}
	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (!class) continue;
		name = class->comkey;
		if (!name) continue;
		common = hashtab_search(pdb->p_commons.table, name);
		if (!common) {
			rc = -1;
			free(used);
			goto exit;
		}
		/* Only write common rule once */
		if (!used[common->s.value-1]) {
			perms = class_or_common_perms_to_str(&common->permissions);
			if (!perms) {
				rc = -1;
				free(used);
				goto exit;
			}
			sepol_printf(out, "common %s { %s }\n", name, perms);
			free(perms);
			used[common->s.value-1] = 1;
		}
	}
	free(used);

	/* class */
	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (!class) continue;
		name = pdb->p_class_val_to_name[i];
		sepol_printf(out, "class %s", name);
		if (class->comkey) {
			sepol_printf(out, " inherits %s", class->comkey);
		}
		perms = class_or_common_perms_to_str(&class->permissions);
		if (perms) {
			sepol_printf(out, " { %s }", perms);
			free(perms);
		}
		sepol_printf(out, "\n");
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing class rules to policy.conf\n");
	}

	return rc;
}

static int write_default_user_to_conf(FILE *out, char *class_name, class_datum_t *class)
{
	const char *dft;

	switch (class->default_user) {
	case DEFAULT_SOURCE:
		dft = "source";
		break;
	case DEFAULT_TARGET:
		dft = "target";
		break;
	default:
		sepol_log_err("Unknown default role value: %i", class->default_user);
		return -1;
	}
	sepol_printf(out, "default_user { %s } %s;\n", class_name, dft);

	return 0;
}

static int write_default_role_to_conf(FILE *out, char *class_name, class_datum_t *class)
{
	const char *dft;

	switch (class->default_role) {
	case DEFAULT_SOURCE:
		dft = "source";
		break;
	case DEFAULT_TARGET:
		dft = "target";
		break;
	default:
		sepol_log_err("Unknown default role value: %i", class->default_role);
		return -1;
	}
	sepol_printf(out, "default_role { %s } %s;\n", class_name, dft);

	return 0;
}

static int write_default_type_to_conf(FILE *out, char *class_name, class_datum_t *class)
{
	const char *dft;

	switch (class->default_type) {
	case DEFAULT_SOURCE:
		dft = "source";
		break;
	case DEFAULT_TARGET:
		dft = "target";
		break;
	default:
		sepol_log_err("Unknown default type value: %i", class->default_type);
		return -1;
	}
	sepol_printf(out, "default_type { %s } %s;\n", class_name, dft);

	return 0;
}

static int write_default_range_to_conf(FILE *out, char *class_name, class_datum_t *class)
{
	const char *dft;

	switch (class->default_range) {
	case DEFAULT_SOURCE_LOW:
		dft = "source low";
		break;
	case DEFAULT_SOURCE_HIGH:
		dft = "source high";
		break;
	case DEFAULT_SOURCE_LOW_HIGH:
		dft = "source low-high";
		break;
	case DEFAULT_TARGET_LOW:
		dft = "target low";
		break;
	case DEFAULT_TARGET_HIGH:
		dft = "target high";
		break;
	case DEFAULT_TARGET_LOW_HIGH:
		dft = "target low-high";
		break;
	case DEFAULT_GLBLUB:
		dft = "glblub";
		break;
	default:
		sepol_log_err("Unknown default type value: %i", class->default_range);
		return -1;
	}
	sepol_printf(out, "default_range { %s } %s;\n", class_name, dft);

	return 0;
}

static int write_default_rules_to_conf(FILE *out, struct policydb *pdb)
{
	class_datum_t *class;
	unsigned i;
	int rc = 0;

	/* default_user */
	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (!class) continue;
		if (class->default_user != 0) {
			rc = write_default_user_to_conf(out, pdb->p_class_val_to_name[i], class);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	/* default_role */
	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (!class) continue;
		if (class->default_role != 0) {
			rc = write_default_role_to_conf(out, pdb->p_class_val_to_name[i], class);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	/* default_type */
	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (!class) continue;
		if (class->default_type != 0) {
			rc = write_default_type_to_conf(out, pdb->p_class_val_to_name[i], class);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	if (!pdb->mls) {
		return 0;
	}

	/* default_range */
	for (i=0; i < pdb->p_classes.nprim; i++) {
		class = pdb->class_val_to_struct[i];
		if (!class) continue;
		if (class->default_range != 0) {
			rc = write_default_range_to_conf(out, pdb->p_class_val_to_name[i], class);
			if (rc != 0) {
				goto exit;
			}
		}
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing default rules to policy.conf\n");
	}

	return rc;
}

static int map_sensitivity_aliases_to_strs(char *key, void *data, void *args)
{
	level_datum_t *sens = data;
	struct strs *strs = args;
	int rc = 0;

	if (sens->isalias) {
		rc = strs_add(strs, key);
	}

	return rc;
}

static int write_sensitivity_rules_to_conf(FILE *out, struct policydb *pdb)
{
	level_datum_t *level;
	struct strs *strs;
	char **sens_alias_map = NULL;
	char *name, *prev, *alias;
	unsigned i, j, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_levels.nprim);
	if (rc != 0) {
		goto exit;
	}

	rc = ksu_hashtab_map(pdb->p_levels.table, map_sensitivity_aliases_to_strs, strs);
	if (rc != 0) {
		goto exit;
	}

	num = strs_num_items(strs);

	if (num > 0) {
		sens_alias_map = calloc(sizeof(*sens_alias_map), pdb->p_levels.nprim);
		if (!sens_alias_map) {
			rc = -1;
			goto exit;
		}

		/* map aliases to sensitivities */
		for (i=0; i < num; i++) {
			name = strs_read_at_index(strs, i);
			level = hashtab_search(pdb->p_levels.table, name);
			if (!level) {
				rc = -1;
				goto exit;
			}
			j = level->level->sens - 1;
			if (!sens_alias_map[j]) {
				sens_alias_map[j] = strdup(name);
				if (!sens_alias_map[j]) {
					rc = -1;
					goto exit;
				}
			} else {
				alias = sens_alias_map[j];
				sens_alias_map[j] = create_str("%s %s", 2, alias, name);
				free(alias);
				if (!sens_alias_map[j]) {
					rc = -1;
					goto exit;
				}
			}
		}
	}

	/* sensitivities */
	for (i=0; i < pdb->p_levels.nprim; i++) {
		name = pdb->p_sens_val_to_name[i];
		if (!name) continue;
		level = hashtab_search(pdb->p_levels.table, name);
		if (!level) {
			rc = -1;
			goto exit;
		}
		if (level->isalias) continue;

		if (sens_alias_map && sens_alias_map[i]) {
			alias = sens_alias_map[i];
			if (strchr(alias, ' ')) {
				sepol_printf(out, "sensitivity %s alias { %s };\n", name, alias);
			} else {
				sepol_printf(out, "sensitivity %s alias %s;\n", name, alias);
			}
		} else {
			sepol_printf(out, "sensitivity %s;\n", name);
		}
	}

	/* dominance */
	sepol_printf(out, "dominance { ");
	prev = NULL;
	for (i=0; i < pdb->p_levels.nprim; i++) {
		name = pdb->p_sens_val_to_name[i];
		if (!name) continue;
		level = hashtab_search(pdb->p_levels.table, name);
		if (!level) {
			rc = -1;
			goto exit;
		}
		if (level->isalias) continue;

		if (prev) {
			sepol_printf(out, "%s ", prev);
		}
		prev = name;
	}
	if (prev) {
		sepol_printf(out, "%s", prev);
	}
	sepol_printf(out, " }\n");

exit:
	if (sens_alias_map) {
		for (i=0; i < pdb->p_levels.nprim; i++) {
			free(sens_alias_map[i]);
		}
		free(sens_alias_map);
	}

	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing sensitivity rules to CIL\n");
	}

	return rc;
}

static int map_category_aliases_to_strs(char *key, void *data, void *args)
{
	cat_datum_t *cat = data;
	struct strs *strs = args;
	int rc = 0;

	if (cat->isalias) {
		rc = strs_add(strs, key);
	}

	return rc;
}

static int write_category_rules_to_conf(FILE *out, struct policydb *pdb)
{
	cat_datum_t *cat;
	struct strs *strs;
	char **cat_alias_map = NULL;
	char *name, *alias;
	unsigned i, j, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_cats.nprim);
	if (rc != 0) {
		goto exit;
	}

	rc = ksu_hashtab_map(pdb->p_cats.table, map_category_aliases_to_strs, strs);
	if (rc != 0) {
		goto exit;
	}

	num = strs_num_items(strs);

	if (num > 0) {
		cat_alias_map = calloc(sizeof(*cat_alias_map), pdb->p_cats.nprim);
		if (!cat_alias_map) {
			rc = -1;
			goto exit;
		}

		/* map aliases to categories */
		for (i=0; i < num; i++) {
			name = strs_read_at_index(strs, i);
			cat = hashtab_search(pdb->p_cats.table, name);
			if (!cat) {
				rc = -1;
				goto exit;
			}
			j = cat->s.value - 1;
			if (!cat_alias_map[j]) {
				cat_alias_map[j] = strdup(name);
				if (!cat_alias_map[j]) {
					rc = -1;
					goto exit;
				}
			} else {
				alias = cat_alias_map[j];
				cat_alias_map[j] = create_str("%s %s", 2, alias, name);
				free(alias);
				if (!cat_alias_map[j]) {
					rc = -1;
					goto exit;
				}
			}
		}
	}

	/* categories */
	for (i=0; i < pdb->p_cats.nprim; i++) {
		name = pdb->p_cat_val_to_name[i];
		if (!name) continue;
		cat = hashtab_search(pdb->p_cats.table, name);
		if (!cat) {
			rc = -1;
			goto exit;
		}
		if (cat->isalias) continue;

		if (cat_alias_map && cat_alias_map[i]) {
			alias = cat_alias_map[i];
			if (strchr(alias, ' ')) {
				sepol_printf(out, "category %s alias { %s };\n", name, alias);
			} else {
				sepol_printf(out, "category %s alias %s;\n", name, alias);
			}
		} else {
			sepol_printf(out, "category %s;\n", name);
		}
	}

exit:
	if (cat_alias_map) {
		for (i=0; i < pdb->p_cats.nprim; i++) {
			free(cat_alias_map[i]);
		}
		free(cat_alias_map);
	}

	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing category rules to policy.conf\n");
	}

	return rc;
}

static size_t cats_ebitmap_len(struct ebitmap *cats, char **val_to_name)
{
	struct ebitmap_node *node;
	uint32_t i, start, range;
	size_t len = 0;

	range = 0;
	ebitmap_for_each_positive_bit(cats, node, i) {
		if (range == 0)
			start = i;

		range++;

		if (ksu_ebitmap_get_bit(cats, i+1))
			continue;

		len += strlen(val_to_name[start]) + 1;
		if (range > 1) {
			len += strlen(val_to_name[i]) + 1;
		}

		range = 0;
	}

	return len;
}

static char *cats_ebitmap_to_str(struct ebitmap *cats, char **val_to_name)
{
	struct ebitmap_node *node;
	uint32_t i, start, range, first;
	char *catsbuf = NULL, *p;
	char sep;
	int len, remaining;

	remaining = (int)cats_ebitmap_len(cats, val_to_name);
	if (remaining == 0) {
		goto exit;
	}
	catsbuf = malloc(remaining);
	if (!catsbuf) {
		goto exit;
	}

	p = catsbuf;

	first = 1;
	range = 0;
	ebitmap_for_each_positive_bit(cats, node, i) {
		if (range == 0)
			start = i;

		range++;

		if (ksu_ebitmap_get_bit(cats, i+1))
			continue;

		if (range > 1) {
			sep = (range == 2) ? ',' : '.';
			len = snprintf(p, remaining, "%s%s%c%s",
				       first ? "" : ",",
				       val_to_name[start], sep, val_to_name[i]);
		} else {
			len = snprintf(p, remaining, "%s%s", first ? "" : ",",
				       val_to_name[start]);

		}
		if (len < 0 || len >= remaining) {
			goto exit;
		}
		p += len;
		remaining -= len;
		first = 0;
		range = 0;
	}

	*p = '\0';

	return catsbuf;

exit:
	free(catsbuf);
	return NULL;
}

static int write_level_rules_to_conf(FILE *out, struct policydb *pdb)
{
	level_datum_t *level;
	char *name, *cats;
	unsigned i;
	int rc = 0;

	for (i=0; i < pdb->p_levels.nprim; i++) {
		name = pdb->p_sens_val_to_name[i];
		if (!name) continue;
		level = hashtab_search(pdb->p_levels.table, name);
		if (!level) {
			rc = -1;
			goto exit;
		}
		if (level->isalias) continue;

		if (!ebitmap_is_empty(&level->level->cat)) {
			cats = cats_ebitmap_to_str(&level->level->cat, pdb->p_cat_val_to_name);
			sepol_printf(out, "level %s:%s;\n", name, cats);
			free(cats);
		} else {
			sepol_printf(out, "level %s;\n", name);
		}
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing level rules to policy.conf\n");
	}

	return rc;
}

static int write_mls_rules_to_conf(FILE *out, struct policydb *pdb)
{
	int rc = 0;

	if (!pdb->mls) {
		return 0;
	}

	rc = write_sensitivity_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_category_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_level_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing mls rules to policy.conf\n");
	}

	return rc;
}

static int write_polcap_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct strs *strs;
	struct ebitmap_node *node;
	const char *name;
	uint32_t i;
	int rc = 0;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	ebitmap_for_each_positive_bit(&pdb->policycaps, node, i) {
		name = sepol_polcap_getname(i);
		if (name == NULL) {
			sepol_log_err("Unknown policy capability id: %i", i);
			rc = -1;
			goto exit;
		}

		rc = strs_create_and_add(strs, "policycap %s;", 1, name);
		if (rc != 0) {
			goto exit;
		}
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing polcap rules to policy.conf\n");
	}

	return rc;
}

static int write_type_attributes_to_conf(FILE *out, struct policydb *pdb)
{
	type_datum_t *type;
	char *name;
	struct strs *strs;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_types.nprim);
	if (rc != 0) {
		goto exit;
	}

	for (i=0; i < pdb->p_types.nprim; i++) {
		type = pdb->type_val_to_struct[i];
		if (type && type->flavor == TYPE_ATTRIB) {
			rc = strs_add(strs, pdb->p_type_val_to_name[i]);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);
	for (i = 0; i < num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			rc = -1;
			goto exit;
		}
		sepol_printf(out, "attribute %s;\n", name);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing typeattribute rules to policy.conf\n");
	}

	return rc;
}

static int write_role_attributes_to_conf(FILE *out, struct policydb *pdb)
{
	role_datum_t *role;
	char *name;
	struct strs *strs;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_roles.nprim);
	if (rc != 0) {
		goto exit;
	}

	for (i=0; i < pdb->p_roles.nprim; i++) {
		role = pdb->role_val_to_struct[i];
		if (role && role->flavor == ROLE_ATTRIB) {
			rc = strs_add(strs, pdb->p_role_val_to_name[i]);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);
	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			rc = -1;
			goto exit;
		}
		sepol_printf(out, "attribute_role %s;\n", name);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing roleattribute rules to policy.conf\n");
	}

	return rc;
}

static int map_boolean_to_strs(char *key, void *data, void *args)
{
	struct strs *strs = (struct strs *)args;
	struct cond_bool_datum *boolean = data;
	const char *value;

	value = boolean->state ? "true" : "false";

	return strs_create_and_add(strs, "bool %s %s;", 2, key, value);
}

static int write_boolean_decl_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct strs *strs;
	int rc = 0;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	rc = ksu_hashtab_map(pdb->p_bools.table, map_boolean_to_strs, strs);
	if (rc != 0) {
		goto exit;
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing boolean declarations to policy.conf\n");
	}

	return rc;
}

static int write_type_decl_rules_to_conf(FILE *out, struct policydb *pdb)
{
	type_datum_t *type;
	struct strs *strs;
	char *name;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_types.nprim);
	if (rc != 0) {
		goto exit;
	}

	for (i=0; i < pdb->p_types.nprim; i++) {
		type = pdb->type_val_to_struct[i];
		if (type && type->flavor == TYPE_TYPE && type->primary) {
			rc = strs_add(strs, pdb->p_type_val_to_name[i]);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);
	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			rc = -1;
			goto exit;
		}
		sepol_printf(out, "type %s;\n", name);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing type declarations to policy.con\n");
	}

	return rc;
}

static int map_count_type_aliases(__attribute__((unused)) char *key, void *data, void *args)
{
	type_datum_t *datum = data;
	unsigned *count = args;

	if (datum->primary == 0 && datum->flavor == TYPE_TYPE)
		(*count)++;

	return SEPOL_OK;
}

static int map_type_aliases_to_strs(char *key, void *data, void *args)
{
	type_datum_t *datum = data;
	struct strs *strs = args;
	int rc = 0;

	if (datum->primary == 0 && datum->flavor == TYPE_TYPE)
		rc = strs_add(strs, key);

	return rc;
}

static int write_type_alias_rules_to_conf(FILE *out, struct policydb *pdb)
{
	type_datum_t *alias;
	struct strs *strs;
	char *name;
	char *type;
	unsigned i, num = 0;
	int rc = 0;

	rc = ksu_hashtab_map(pdb->p_types.table, map_count_type_aliases, &num);
	if (rc != 0) {
		goto exit;
	}

	rc = strs_init(&strs, num);
	if (rc != 0) {
		goto exit;
	}

	rc = ksu_hashtab_map(pdb->p_types.table, map_type_aliases_to_strs, strs);
	if (rc != 0) {
		goto exit;
	}

	strs_sort(strs);

	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			rc = -1;
			goto exit;
		}
		alias = hashtab_search(pdb->p_types.table, name);
		if (!alias) {
			rc = -1;
			goto exit;
		}
		type = pdb->p_type_val_to_name[alias->s.value - 1];
		sepol_printf(out, "typealias %s alias %s;\n", type, name);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing type alias rules to policy.conf\n");
	}

	return rc;
}

static int write_type_bounds_rules_to_conf(FILE *out, struct policydb *pdb)
{
	type_datum_t *type;
	struct strs *strs;
	char *parent;
	char *child;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_types.nprim);
	if (rc != 0) {
		goto exit;
	}

	for (i=0; i < pdb->p_types.nprim; i++) {
		type = pdb->type_val_to_struct[i];
		if (type && type->flavor == TYPE_TYPE) {
			if (type->bounds > 0) {
				rc = strs_add(strs, pdb->p_type_val_to_name[i]);
				if (rc != 0) {
					goto exit;
				}
			}
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);
	for (i=0; i<num; i++) {
		child = strs_read_at_index(strs, i);
		if (!child) {
			rc = -1;
			goto exit;
		}
		type = hashtab_search(pdb->p_types.table, child);
		if (!type) {
			rc = -1;
			goto exit;
		}
		parent = pdb->p_type_val_to_name[type->bounds - 1];
		sepol_printf(out, "typebounds %s %s;\n", parent, child);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing type bounds rules to policy.conf\n");
	}

	return rc;
}

static char *attr_strs_to_str(struct strs *strs)
{
	char *str = NULL;
	size_t len = 0;
	char *p;
	unsigned i;
	int rc;

	if (strs->num == 0) {
		goto exit;
	}

	/* 2*strs->num - 1 because ", " follows all but last attr (followed by '\0') */
	len = strs_len_items(strs) + 2*strs->num - 1;
	str = malloc(len);
	if (!str) {
		sepol_log_err("Out of memory");
		goto exit;
	}

	p = str;
	for (i=0; i<strs->num; i++) {
		if (!strs->list[i]) continue;
		len = strlen(strs->list[i]);
		rc = snprintf(p, len+1, "%s", strs->list[i]);
		if (rc < 0 || rc > (int)len) {
			free(str);
			str = NULL;
			goto exit;
		}
		p += len;
		if (i < strs->num - 1) {
			*p++ = ',';
			*p++ = ' ';
		}
	}

	*p = '\0';

exit:
	return str;
}

static char *attrmap_to_str(struct ebitmap *map, char **val_to_name)
{
	struct strs *strs;
	char *str = NULL;
	int rc;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	rc = ebitmap_to_strs(map, strs, val_to_name);
	if (rc != 0) {
		goto exit;
	}

	strs_sort(strs);

	str = attr_strs_to_str(strs);

exit:
	strs_destroy(&strs);

	return str;
}

static int write_type_attribute_sets_to_conf(FILE *out, struct policydb *pdb)
{
	type_datum_t *type;
	struct strs *strs;
	ebitmap_t attrmap;
	char *name, *attrs;
	unsigned i;
	int rc;

	rc = strs_init(&strs, pdb->p_types.nprim);
	if (rc != 0) {
		goto exit;
	}

	for (i=0; i < pdb->p_types.nprim; i++) {
		type = pdb->type_val_to_struct[i];
		if (!type || type->flavor != TYPE_TYPE || !type->primary) continue;
		if (ebitmap_cardinality(&pdb->type_attr_map[i]) == 1) continue;

		rc = ksu_ebitmap_cpy(&attrmap, &pdb->type_attr_map[i]);
		if (rc != 0) {
			goto exit;
		}
		rc = ksu_ebitmap_set_bit(&attrmap, i, 0);
		if (rc != 0) {
			ksu_ebitmap_destroy(&attrmap);
			goto exit;
		}
		name = pdb->p_type_val_to_name[i];
		attrs = attrmap_to_str(&attrmap, pdb->p_type_val_to_name);
		ksu_ebitmap_destroy(&attrmap);
		if (!attrs) {
			rc = -1;
			goto exit;
		}

		rc = strs_create_and_add(strs, "typeattribute %s %s;",
					 2, name, attrs);
		free(attrs);
		if (rc != 0) {
			goto exit;
		}
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing typeattributeset rules to policy.conf\n");
	}

	return rc;
}

static int write_type_permissive_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct strs *strs;
	char *name;
	struct ebitmap_node *node;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_types.nprim);
	if (rc != 0) {
		goto exit;
	}

	ebitmap_for_each_positive_bit(&pdb->permissive_map, node, i) {
		rc = strs_add(strs, pdb->p_type_val_to_name[i-1]);
		if (rc != 0) {
			goto exit;
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);
	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			rc = -1;
			goto exit;
		}
		sepol_printf(out, "permissive %s;\n", name);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing typepermissive rules to policy.conf\n");
	}

	return rc;
}

static char *avtab_node_to_str(struct policydb *pdb, avtab_key_t *key, avtab_datum_t *datum)
{
	uint32_t data = datum->data;
	type_datum_t *type;
	const char *flavor, *src, *tgt, *class, *perms, *new;
	char *rule = NULL;

	switch (0xFFF & key->specified) {
	case AVTAB_ALLOWED:
		flavor = "allow";
		break;
	case AVTAB_AUDITALLOW:
		flavor = "auditallow";
		break;
	case AVTAB_AUDITDENY:
		flavor = "dontaudit";
		data = ~data;
		break;
	case AVTAB_XPERMS_ALLOWED:
		flavor = "allowxperm";
		break;
	case AVTAB_XPERMS_AUDITALLOW:
		flavor = "auditallowxperm";
		break;
	case AVTAB_XPERMS_DONTAUDIT:
		flavor = "dontauditxperm";
		break;
	case AVTAB_TRANSITION:
		flavor = "type_transition";
		break;
	case AVTAB_MEMBER:
		flavor = "type_member";
		break;
	case AVTAB_CHANGE:
		flavor = "type_change";
		break;
	default:
		sepol_log_err("Unknown avtab type: %i", key->specified);
		goto exit;
	}

	src = pdb->p_type_val_to_name[key->source_type - 1];
	tgt = pdb->p_type_val_to_name[key->target_type - 1];
	if (key->source_type == key->target_type && !(key->specified & AVTAB_TYPE)) {
		type = pdb->type_val_to_struct[key->source_type - 1];
		if (type->flavor != TYPE_ATTRIB) {
			tgt = "self";
		}
	}
	class = pdb->p_class_val_to_name[key->target_class - 1];

	if (key->specified & AVTAB_AV) {
		perms = sepol_av_to_string(pdb, key->target_class, data);
		if (perms == NULL) {
			sepol_log_err("Failed to generate permission string");
			goto exit;
		}
		rule = create_str("%s %s %s:%s { %s };", 5,
				  flavor, src, tgt, class, perms+1);
	} else if (key->specified & AVTAB_XPERMS) {
		perms = sepol_extended_perms_to_string(datum->xperms);
		if (perms == NULL) {
			sepol_log_err("Failed to generate extended permission string");
			goto exit;
		}

		rule = create_str("%s %s %s:%s %s;", 5, flavor, src, tgt, class, perms);
	} else {
		new = pdb->p_type_val_to_name[data - 1];

		rule = create_str("%s %s %s:%s %s;", 5, flavor, src, tgt, class, new);
	}

	if (!rule) {
		goto exit;
	}

	return rule;

exit:
	return NULL;
}

struct map_avtab_args {
	struct policydb *pdb;
	uint32_t flavor;
	struct strs *strs;
};

static int map_avtab_write_helper(avtab_key_t *key, avtab_datum_t *datum, void *args)
{
	struct map_avtab_args *map_args = args;
	uint32_t flavor = map_args->flavor;
	struct policydb *pdb = map_args->pdb;
	struct strs *strs = map_args->strs;
	char *rule;
	int rc = 0;

	if (key->specified & flavor) {
		rule = avtab_node_to_str(pdb, key, datum);
		if (!rule) {
			rc = -1;
			goto exit;
		}
		rc = strs_add(strs, rule);
		if (rc != 0) {
			free(rule);
			goto exit;
		}
	}

exit:
	return rc;
}

static int write_avtab_flavor_to_conf(FILE *out, struct policydb *pdb, uint32_t flavor, int indent)
{
	struct map_avtab_args args;
	struct strs *strs;
	int rc = 0;

	rc = strs_init(&strs, 1000);
	if (rc != 0) {
		goto exit;
	}

	args.pdb = pdb;
	args.flavor = flavor;
	args.strs = strs;

	rc = avtab_map(&pdb->te_avtab, map_avtab_write_helper, &args);
	if (rc != 0) {
		goto exit;
	}

	strs_sort(strs);
	strs_write_each_indented(strs, out, indent);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	return rc;
}

static int write_avtab_to_conf(FILE *out, struct policydb *pdb, int indent)
{
	unsigned i;
	int rc = 0;

	for (i = 0; i < AVTAB_FLAVORS_SZ; i++) {
		rc = write_avtab_flavor_to_conf(out, pdb, avtab_flavors[i], indent);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing avtab rules to policy.conf\n");
	}

	return rc;
}

struct map_filename_trans_args {
	struct policydb *pdb;
	struct strs *strs;
};

static int map_filename_trans_to_str(hashtab_key_t key, void *data, void *arg)
{
	filename_trans_key_t *ft = (filename_trans_key_t *)key;
	filename_trans_datum_t *datum = data;
	struct map_filename_trans_args *map_args = arg;
	struct policydb *pdb = map_args->pdb;
	struct strs *strs = map_args->strs;
	char *src, *tgt, *class, *filename, *new;
	struct ebitmap_node *node;
	uint32_t bit;
	int rc;

	tgt = pdb->p_type_val_to_name[ft->ttype - 1];
	class = pdb->p_class_val_to_name[ft->tclass - 1];
	filename = ft->name;
	do {
		new = pdb->p_type_val_to_name[datum->otype - 1];

		ebitmap_for_each_positive_bit(&datum->stypes, node, bit) {
			src = pdb->p_type_val_to_name[bit];
			rc = strs_create_and_add(strs,
						 "type_transition %s %s:%s %s \"%s\";",
						 5, src, tgt, class, new, filename);
			if (rc)
				return rc;
		}

		datum = datum->next;
	} while (datum);

	return 0;
}

static int write_filename_trans_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct map_filename_trans_args args;
	struct strs *strs;
	int rc = 0;

	rc = strs_init(&strs, 100);
	if (rc != 0) {
		goto exit;
	}

	args.pdb = pdb;
	args.strs = strs;

	rc = ksu_hashtab_map(pdb->filename_trans, map_filename_trans_to_str, &args);
	if (rc != 0) {
		goto exit;
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing filename typetransition rules to policy.conf\n");
	}

	return rc;
}

static char *level_to_str(struct policydb *pdb, struct mls_level *level)
{
	ebitmap_t *cats = &level->cat;
	char *level_str = NULL;
	char *sens_str = pdb->p_sens_val_to_name[level->sens - 1];
	char *cats_str;

	if (!ebitmap_is_empty(cats)) {
		cats_str = cats_ebitmap_to_str(cats, pdb->p_cat_val_to_name);
		level_str = create_str("%s:%s", 2, sens_str, cats_str);
		free(cats_str);
	} else {
		level_str = create_str("%s", 1, sens_str);
	}

	return level_str;
}

static char *range_to_str(struct policydb *pdb, mls_range_t *range)
{
	char *low = NULL;
	char *high = NULL;
	char *range_str = NULL;

	low = level_to_str(pdb, &range->level[0]);
	if (!low) {
		goto exit;
	}

	high = level_to_str(pdb, &range->level[1]);
	if (!high) {
		goto exit;
	}

	range_str = create_str("%s - %s", 2, low, high);

exit:
	free(low);
	free(high);

	return range_str;
}

struct map_range_trans_args {
	struct policydb *pdb;
	struct strs *strs;
};

static int map_range_trans_to_str(hashtab_key_t key, void *data, void *arg)
{
	range_trans_t *rt = (range_trans_t *)key;
	mls_range_t *mls_range = data;
	struct map_range_trans_args *map_args = arg;
	struct policydb *pdb = map_args->pdb;
	struct strs *strs = map_args->strs;
	char *src, *tgt, *class, *range;
	int rc;

	src = pdb->p_type_val_to_name[rt->source_type - 1];
	tgt = pdb->p_type_val_to_name[rt->target_type - 1];
	class = pdb->p_class_val_to_name[rt->target_class - 1];
	range = range_to_str(pdb, mls_range);
	if (!range) {
		rc = -1;
		goto exit;
	}

	rc = strs_create_and_add(strs, "range_transition %s %s:%s %s;", 4,
				 src, tgt, class, range);
	free(range);
	if (rc != 0) {
		goto exit;
	}

exit:
	return rc;
}

static int write_range_trans_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct map_range_trans_args args;
	struct strs *strs;
	int rc = 0;

	rc = strs_init(&strs, 100);
	if (rc != 0) {
		goto exit;
	}

	args.pdb = pdb;
	args.strs = strs;

	rc = ksu_hashtab_map(pdb->range_tr, map_range_trans_to_str, &args);
	if (rc != 0) {
		goto exit;
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing range transition rules to policy.conf\n");
	}

	return rc;
}

static int write_cond_av_list_to_conf(FILE *out, struct policydb *pdb, cond_av_list_t *cond_list, int indent)
{
	cond_av_list_t *cond_av;
	avtab_ptr_t node;
	uint32_t flavor;
	avtab_key_t *key;
	avtab_datum_t *datum;
	struct strs *strs;
	char *rule;
	unsigned i;
	int rc;

	for (i = 0; i < AVTAB_FLAVORS_SZ; i++) {
		flavor = avtab_flavors[i];
		rc = strs_init(&strs, 64);
		if (rc != 0) {
			goto exit;
		}

		for (cond_av = cond_list; cond_av != NULL; cond_av = cond_av->next) {
			node = cond_av->node;
			key = &node->key;
			datum = &node->datum;
			if (key->specified & flavor) {
				rule = avtab_node_to_str(pdb, key, datum);
				if (!rule) {
					rc = -1;
					goto exit;
				}
				rc = strs_add(strs, rule);
				if (rc != 0) {
					free(rule);
					goto exit;
				}
			}
		}

		strs_sort(strs);
		strs_write_each_indented(strs, out, indent);
		strs_free_all(strs);
		strs_destroy(&strs);
	}

	return 0;

exit:
	strs_free_all(strs);
	strs_destroy(&strs);
	return rc;
}

struct cond_data {
	char *expr;
	struct cond_node *cond;
};

static int cond_node_cmp(const void *a, const void *b)
{
	const struct cond_data *aa = a;
	const struct cond_data *bb = b;
	return strcmp(aa->expr, bb->expr);
}

static int write_cond_nodes_to_conf(FILE *out, struct policydb *pdb)
{
	struct cond_data *cond_data;
	char *expr;
	struct cond_node *cond;
	unsigned i, num;
	int rc = 0;

	num = 0;
	for (cond = pdb->cond_list; cond != NULL; cond = cond->next) {
		num++;
	}

	if (num == 0) {
		return 0;
	}

	cond_data = calloc(sizeof(struct cond_data), num);
	if (!cond_data) {
		rc = -1;
		goto exit;
	}

	i = 0;
	for (cond = pdb->cond_list; cond != NULL; cond = cond->next) {
		cond_data[i].cond = cond;
		expr = cond_expr_to_str(pdb, cond->expr);
		if (!expr) {
			num = i;
			goto exit;
		}
		cond_data[i].expr = expr;
		i++;
	}

	qsort(cond_data, num, sizeof(*cond_data), cond_node_cmp);

	for (i=0; i<num; i++) {
		expr = cond_data[i].expr;
		cond = cond_data[i].cond;

		sepol_printf(out, "if (%s) {\n", expr);

		if (cond->true_list != NULL) {
			rc = write_cond_av_list_to_conf(out, pdb, cond->true_list, 1);
			if (rc != 0) {
				goto exit;
			}
		}

		if (cond->false_list != NULL) {
			sepol_printf(out, "} else {\n");
			rc = write_cond_av_list_to_conf(out, pdb, cond->false_list, 1);
			if (rc != 0) {
				goto exit;
			}
		}
		sepol_printf(out, "}\n");
	}

exit:
	if (cond_data) {
		for (i=0; i<num; i++) {
			free(cond_data[i].expr);
		}
		free(cond_data);
	}

	if (rc != 0) {
		sepol_log_err("Error writing conditional rules to policy.conf\n");
	}

	return rc;
}

static int write_role_decl_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct role_datum *role;
	struct strs *strs;
	char *name, *types, *p1, *p2;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_roles.nprim);
	if (rc != 0) {
		goto exit;
	}

	/* Start at 1 to skip object_r */
	for (i=1; i < pdb->p_roles.nprim; i++) {
		role = pdb->role_val_to_struct[i];
		if (role && role->flavor == ROLE_ROLE) {
			rc = strs_add(strs, pdb->p_role_val_to_name[i]);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);

	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			continue;
		}
		sepol_printf(out, "role %s;\n", name);
	}

	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) continue;
		role = hashtab_search(pdb->p_roles.table, name);
		if (!role) {
			rc = -1;
			goto exit;
		}
		if (ebitmap_is_empty(&role->types.types)) continue;
		types = ebitmap_to_str(&role->types.types, pdb->p_type_val_to_name, 1);
		if (!types) {
			rc = -1;
			goto exit;
		}
		if (strlen(types) > 900) {
			p1 = types;
			while (p1) {
				p2 = p1;
				while (p2 - p1 < 600) {
					p2 = strchr(p2, ' ');
					if (!p2)
						break;
					p2++;
				}
				if (p2) {
					*(p2-1) = '\0';
				}
				sepol_printf(out, "role %s types { %s };\n", name, p1);
				p1 = p2;
			}
		} else {
			sepol_printf(out, "role %s types { %s };\n", name, types);
		}
		free(types);
	}

exit:
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing role declarations to policy.conf\n");
	}

	return rc;
}

static int write_role_transition_rules_to_conf(FILE *out, struct policydb *pdb)
{
	role_trans_t *curr = pdb->role_tr;
	struct strs *strs;
	char *role, *type, *class, *new;
	int rc = 0;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	while (curr) {
		role = pdb->p_role_val_to_name[curr->role - 1];
		type = pdb->p_type_val_to_name[curr->type - 1];
		class = pdb->p_class_val_to_name[curr->tclass - 1];
		new = pdb->p_role_val_to_name[curr->new_role - 1];

		rc = strs_create_and_add(strs, "role_transition %s %s:%s %s;", 4,
					 role, type, class, new);
		if (rc != 0) {
			goto exit;
		}

		curr = curr->next;
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing role transition rules to policy.conf\n");
	}

	return rc;
}

static int write_role_allow_rules_to_conf(FILE *out, struct policydb *pdb)
{
	role_allow_t *curr = pdb->role_allow;
	struct strs *strs;
	char *role, *new;
	int rc = 0;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	while (curr) {
		role = pdb->p_role_val_to_name[curr->role - 1];
		new =  pdb->p_role_val_to_name[curr->new_role - 1];

		rc = strs_create_and_add(strs, "allow %s %s;", 2, role, new);
		if (rc != 0) {
			goto exit;
		}

		curr = curr->next;
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing role allow rules to policy.conf\n");
	}

	return rc;
}

static int write_user_decl_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct user_datum *user;
	struct strs *strs;
	char *name, *roles, *level, *range;
	unsigned i, num;
	int rc = 0;

	rc = strs_init(&strs, pdb->p_users.nprim);
	if (rc != 0) {
		goto exit;
	}

	for (i=0; i < pdb->p_users.nprim; i++) {
		if (!pdb->p_user_val_to_name[i]) continue;
		rc = strs_add(strs, pdb->p_user_val_to_name[i]);
		if (rc != 0) {
			goto exit;
		}
	}

	strs_sort(strs);

	num = strs_num_items(strs);

	for (i=0; i<num; i++) {
		name = strs_read_at_index(strs, i);
		if (!name) {
			continue;
		}
		user = hashtab_search(pdb->p_users.table, name);
		if (!user) {
			rc = -1;
			goto exit;
		}
		sepol_printf(out, "user %s", name);

		if (!ebitmap_is_empty(&user->roles.roles)) {
			roles = ebitmap_to_str(&user->roles.roles,
					       pdb->p_role_val_to_name, 1);
			if (!roles) {
				rc = -1;
				goto exit;
			}
			if (strchr(roles, ' ')) {
				sepol_printf(out, " roles { %s }", roles);
			} else {
				sepol_printf(out, " roles %s", roles);
			}
			free(roles);
		}

		if (pdb->mls) {
			level = level_to_str(pdb, &user->exp_dfltlevel);
			if (!level) {
				rc = -1;
				goto exit;
			}
			sepol_printf(out, " level %s", level);
			free(level);

			range = range_to_str(pdb, &user->exp_range);
			if (!range) {
				rc = -1;
				goto exit;
			}
			sepol_printf(out, " range %s", range);
			free(range);
		}
		sepol_printf(out, ";\n");
	}

exit:
	if (strs)
		strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing user declarations to policy.conf\n");
	}

	return rc;
}

static char *context_to_str(struct policydb *pdb, struct context_struct *con)
{
	char *user, *role, *type, *range;
	char *ctx = NULL;

	user = pdb->p_user_val_to_name[con->user - 1];
	role = pdb->p_role_val_to_name[con->role - 1];
	type = pdb->p_type_val_to_name[con->type - 1];

	if (pdb->mls) {
		range = range_to_str(pdb, &con->range);
		ctx = create_str("%s:%s:%s:%s", 4, user, role, type, range);
		free(range);
	} else {
		ctx = create_str("%s:%s:%s", 3, user, role, type);
	}

	return ctx;
}

static int write_sid_context_rules_to_conf(FILE *out, struct policydb *pdb, const char *const *sid_to_str, unsigned num_sids)
{
	struct ocontext *isid;
	struct strs *strs;
	char *sid;
	char unknown[18];
	char *ctx, *rule;
	unsigned i;
	int rc;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	for (isid = pdb->ocontexts[0]; isid != NULL; isid = isid->next) {
		i = isid->sid[0];
		if (i < num_sids) {
			sid = (char *)sid_to_str[i];
		} else {
			snprintf(unknown, sizeof(unknown), "%s%u", "UNKNOWN", i);
			sid = unknown;
		}

		ctx = context_to_str(pdb, &isid->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		rule = create_str("sid %s %s", 2, sid, ctx);
		free(ctx);
		if (!rule) {
			rc = -1;
			goto exit;
		}

		rc = strs_add_at_index(strs, rule, i);
		if (rc != 0) {
			free(rule);
			goto exit;
		}
	}

	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing sidcontext rules to policy.conf\n");
	}

	return rc;
}

static int write_selinux_isid_rules_to_conf(FILE *out, struct policydb *pdb)
{
	return write_sid_context_rules_to_conf(out, pdb, selinux_sid_to_str,
					       SELINUX_SID_SZ);
}

static int write_selinux_fsuse_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *fsuse;
	const char *behavior;
	char *name, *ctx;
	int rc = 0;

	for (fsuse = pdb->ocontexts[5]; fsuse != NULL; fsuse = fsuse->next) {
		switch (fsuse->v.behavior) {
		case SECURITY_FS_USE_XATTR: behavior = "xattr"; break;
		case SECURITY_FS_USE_TRANS: behavior = "trans"; break;
		case SECURITY_FS_USE_TASK:  behavior = "task"; break;
		default:
			sepol_log_err("Unknown fsuse behavior: %i", fsuse->v.behavior);
			rc = -1;
			goto exit;
		}

		name = fsuse->u.name;
		ctx = context_to_str(pdb, &fsuse->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "fs_use_%s %s %s;\n", behavior, name, ctx);

		free(ctx);
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing fsuse rules to policy.conf\n");
	}

	return rc;
}

static int write_genfscon_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct genfs *genfs;
	struct ocontext *ocon;
	struct strs *strs;
	char *fstype, *name, *ctx;
	uint32_t sclass;
	const char *file_type;
	int rc;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	for (genfs = pdb->genfs; genfs != NULL; genfs = genfs->next) {
		for (ocon = genfs->head; ocon != NULL; ocon = ocon->next) {
			fstype = genfs->fstype;
			name = ocon->u.name;

			sclass = ocon->v.sclass;
			file_type = NULL;
			if (sclass) {
				const char *class_name = pdb->p_class_val_to_name[sclass-1];
				if (strcmp(class_name, "file") == 0) {
					file_type = "--";
				} else if (strcmp(class_name, "dir") == 0) {
					file_type = "-d";
				} else if (strcmp(class_name, "chr_file") == 0) {
					file_type = "-c";
				} else if (strcmp(class_name, "blk_file") == 0) {
					file_type = "-b";
				} else if (strcmp(class_name, "sock_file") == 0) {
					file_type = "-s";
				} else if (strcmp(class_name, "fifo_file") == 0) {
					file_type = "-p";
				} else if (strcmp(class_name, "lnk_file") == 0) {
					file_type = "-l";
				} else {
					rc = -1;
					goto exit;
				}
			}

			ctx = context_to_str(pdb, &ocon->context[0]);
			if (!ctx) {
				rc = -1;
				goto exit;
			}

			if (file_type) {
				rc = strs_create_and_add(strs, "genfscon %s \"%s\" %s %s", 4,
										 fstype, name, file_type, ctx);
			} else {
				rc = strs_create_and_add(strs, "genfscon %s \"%s\" %s", 3,
										 fstype, name, ctx);
			}
			free(ctx);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	strs_sort(strs);
	strs_write_each(strs, out);

exit:
	strs_free_all(strs);
	strs_destroy(&strs);

	if (rc != 0) {
		sepol_log_err("Error writing genfscon rules to policy.conf\n");
	}

	return rc;
}

static int write_selinux_port_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *portcon;
	const char *protocol;
	uint16_t low;
	uint16_t high;
	char low_high_str[44]; /* 2^64 <= 20 digits so "low-high" <= 44 chars */
	char *ctx;
	int rc = 0;

	for (portcon = pdb->ocontexts[2]; portcon != NULL; portcon = portcon->next) {
		switch (portcon->u.port.protocol) {
		case IPPROTO_TCP: protocol = "tcp"; break;
		case IPPROTO_UDP: protocol = "udp"; break;
		case IPPROTO_DCCP: protocol = "dccp"; break;
		case IPPROTO_SCTP: protocol = "sctp"; break;
		default:
			sepol_log_err("Unknown portcon protocol: %i", portcon->u.port.protocol);
			rc = -1;
			goto exit;
		}

		low = portcon->u.port.low_port;
		high = portcon->u.port.high_port;
		if (low == high) {
			rc = snprintf(low_high_str, 44, "%u", low);
		} else {
			rc = snprintf(low_high_str, 44, "%u-%u", low, high);
		}
		if (rc < 0 || rc >= 44) {
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &portcon->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "portcon %s %s %s\n", protocol, low_high_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing portcon rules to policy.conf\n");
	}

	return rc;
}

static int write_selinux_netif_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *netif;
	char *name, *ctx1, *ctx2;
	int rc = 0;

	for (netif = pdb->ocontexts[3]; netif != NULL; netif = netif->next) {
		name = netif->u.name;
		ctx1 = context_to_str(pdb, &netif->context[0]);
		if (!ctx1) {
			rc = -1;
			goto exit;
		}
		ctx2 = context_to_str(pdb, &netif->context[1]);
		if (!ctx2) {
			free(ctx1);
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "netifcon %s %s %s\n", name, ctx1, ctx2);

		free(ctx1);
		free(ctx2);
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing netifcon rules to policy.conf\n");
	}

	return rc;
}

static int write_selinux_node_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *node;
	char addr[INET_ADDRSTRLEN];
	char mask[INET_ADDRSTRLEN];
	char *ctx;
	int rc = 0;

	for (node = pdb->ocontexts[4]; node != NULL; node = node->next) {
		if (inet_ntop(AF_INET, &node->u.node.addr, addr, INET_ADDRSTRLEN) == NULL) {
			sepol_log_err("Nodecon address is invalid: %m");
			rc = -1;
			goto exit;
		}

		if (inet_ntop(AF_INET, &node->u.node.mask, mask, INET_ADDRSTRLEN) == NULL) {
			sepol_log_err("Nodecon mask is invalid: %m");
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &node->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "nodecon %s %s %s\n", addr, mask, ctx);

		free(ctx);
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing nodecon rules to policy.conf\n");
	}

	return rc;
}


static int write_selinux_node6_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *node6;
	char addr[INET6_ADDRSTRLEN];
	char mask[INET6_ADDRSTRLEN];
	char *ctx;
	int rc = 0;

	for (node6 = pdb->ocontexts[6]; node6 != NULL; node6 = node6->next) {
		if (inet_ntop(AF_INET6, &node6->u.node6.addr, addr, INET6_ADDRSTRLEN) == NULL) {
			sepol_log_err("Nodecon address is invalid: %m");
			rc = -1;
			goto exit;
		}

		if (inet_ntop(AF_INET6, &node6->u.node6.mask, mask, INET6_ADDRSTRLEN) == NULL) {
			sepol_log_err("Nodecon mask is invalid: %m");
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &node6->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "nodecon %s %s %s\n", addr, mask, ctx);

		free(ctx);
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing nodecon rules to policy.conf\n");
	}

	return rc;
}

static int write_selinux_ibpkey_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *ibpkeycon;
	char subnet_prefix_str[INET6_ADDRSTRLEN];
	struct in6_addr subnet_prefix = IN6ADDR_ANY_INIT;
	uint16_t low;
	uint16_t high;
	char low_high_str[44]; /* 2^64 <= 20 digits so "low-high" <= 44 chars */
	char *ctx;
	int rc = 0;

	for (ibpkeycon = pdb->ocontexts[OCON_IBPKEY]; ibpkeycon != NULL;
	     ibpkeycon = ibpkeycon->next) {
		memcpy(&subnet_prefix.s6_addr, &ibpkeycon->u.ibpkey.subnet_prefix,
		       sizeof(ibpkeycon->u.ibpkey.subnet_prefix));

		if (inet_ntop(AF_INET6, &subnet_prefix.s6_addr,
			      subnet_prefix_str, INET6_ADDRSTRLEN) == NULL) {
			sepol_log_err("ibpkeycon address is invalid: %m");
			rc = -1;
			goto exit;
		}

		low = ibpkeycon->u.ibpkey.low_pkey;
		high = ibpkeycon->u.ibpkey.high_pkey;
		if (low == high) {
			rc = snprintf(low_high_str, 44, "%u", low);
		} else {
			rc = snprintf(low_high_str, 44, "%u-%u", low, high);
		}
		if (rc < 0 || rc >= 44) {
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &ibpkeycon->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "ibpkeycon %s %s %s\n", subnet_prefix_str,
			     low_high_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing ibpkeycon rules to policy.conf\n");
	}

	return rc;
}

static int write_selinux_ibendport_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *ibendportcon;
	char port_str[4];
	char *ctx;
	int rc = 0;

	for (ibendportcon = pdb->ocontexts[OCON_IBENDPORT];
	     ibendportcon != NULL; ibendportcon = ibendportcon->next) {
		rc = snprintf(port_str, 4, "%u", ibendportcon->u.ibendport.port);
		if (rc < 0 || rc >= 4) {
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &ibendportcon->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "ibendportcon %s %s %s\n", ibendportcon->u.ibendport.dev_name, port_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing ibendportcon rules to policy.conf\n");
	}

	return rc;
}

static int write_xen_isid_rules_to_conf(FILE *out, struct policydb *pdb)
{
	return write_sid_context_rules_to_conf(out, pdb, xen_sid_to_str, XEN_SID_SZ);
}


static int write_xen_pirq_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *pirq;
	char pirq_str[21]; /* 2^64-1 <= 20 digits */
	char *ctx;
	int rc = 0;

	for (pirq = pdb->ocontexts[1]; pirq != NULL; pirq = pirq->next) {
		rc = snprintf(pirq_str, 21, "%i", pirq->u.pirq);
		if (rc < 0 || rc >= 21) {
			fprintf(stderr,"error1\n");
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &pirq->context[0]);
		if (!ctx) {
			rc = -1;
			fprintf(stderr,"error2\n");
			goto exit;
		}

		sepol_printf(out, "pirqcon %s %s\n", pirq_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing pirqcon rules to policy.conf\n");
	}

	return rc;
}

static int write_xen_ioport_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *ioport;
	uint32_t low;
	uint32_t high;
	char low_high_str[40]; /* 2^64-1 <= 16 digits (hex) so low-high < 40 chars */
	char *ctx;
	int rc = 0;

	for (ioport = pdb->ocontexts[2]; ioport != NULL; ioport = ioport->next) {
		low = ioport->u.ioport.low_ioport;
		high = ioport->u.ioport.high_ioport;
		if (low == high) {
			rc = snprintf(low_high_str, 40, "0x%x", low);
		} else {
			rc = snprintf(low_high_str, 40, "0x%x-0x%x", low, high);
		}
		if (rc < 0 || rc >= 40) {
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &ioport->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "ioportcon %s %s\n", low_high_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing ioportcon rules to policy.conf\n");
	}

	return rc;
}

static int write_xen_iomem_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *iomem;
	uint64_t low;
	uint64_t high;
	char low_high_str[40]; /* 2^64-1 <= 16 digits (hex) so low-high < 40 chars */
	char *ctx;
	int rc = 0;

	for (iomem = pdb->ocontexts[3]; iomem != NULL; iomem = iomem->next) {
		low = iomem->u.iomem.low_iomem;
		high = iomem->u.iomem.high_iomem;
		if (low == high) {
			rc = snprintf(low_high_str, 40, "0x%"PRIx64, low);
		} else {
			rc = snprintf(low_high_str, 40, "0x%"PRIx64"-0x%"PRIx64, low, high);
		}
		if (rc < 0 || rc >= 40) {
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &iomem->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "iomemcon %s %s\n", low_high_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing iomemcon rules to policy.conf\n");
	}

	return rc;
}

static int write_xen_pcidevice_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *pcid;
	char device_str[20]; /* 2^64-1 <= 16 digits (hex) so < 19 chars */
	char *ctx;
	int rc = 0;

	for (pcid = pdb->ocontexts[4]; pcid != NULL; pcid = pcid->next) {
		rc = snprintf(device_str, 20, "0x%lx", (unsigned long)pcid->u.device);
		if (rc < 0 || rc >= 20) {
			rc = -1;
			goto exit;
		}

		ctx = context_to_str(pdb, &pcid->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "pcidevicecon %s %s\n", device_str, ctx);

		free(ctx);
	}

	rc = 0;

exit:
	if (rc != 0) {
		sepol_log_err("Error writing pcidevicecon rules to policy.conf\n");
	}

	return rc;
}

static int write_xen_devicetree_rules_to_conf(FILE *out, struct policydb *pdb)
{
	struct ocontext *dtree;
	char *name, *ctx;
	int rc = 0;

	for (dtree = pdb->ocontexts[5]; dtree != NULL; dtree = dtree->next) {
		name = dtree->u.name;
		ctx = context_to_str(pdb, &dtree->context[0]);
		if (!ctx) {
			rc = -1;
			goto exit;
		}

		sepol_printf(out, "devicetreecon \"%s\" %s\n", name, ctx);

		free(ctx);
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error writing devicetreecon rules to policy.conf\n");
	}

	return rc;
}

int sepol_kernel_policydb_to_conf(FILE *out, struct policydb *pdb)
{
	struct strs *mls_constraints = NULL;
	struct strs *non_mls_constraints = NULL;
	struct strs *mls_validatetrans = NULL;
	struct strs *non_mls_validatetrans = NULL;
	int rc = 0;

	rc = strs_init(&mls_constraints, 32);
	if (rc != 0) {
		goto exit;
	}

	rc = strs_init(&non_mls_constraints, 32);
	if (rc != 0) {
		goto exit;
	}

	rc = strs_init(&mls_validatetrans, 32);
	if (rc != 0) {
		goto exit;
	}

	rc = strs_init(&non_mls_validatetrans, 32);
	if (rc != 0) {
		goto exit;
	}

	if (pdb == NULL) {
		sepol_log_err("No policy");
		rc = -1;
		goto exit;
	}

	if (pdb->policy_type != SEPOL_POLICY_KERN) {
		sepol_log_err("Policy is not a kernel policy");
		rc = -1;
		goto exit;
	}

	if (pdb->policyvers >= POLICYDB_VERSION_AVTAB && pdb->policyvers <= POLICYDB_VERSION_PERMISSIVE) {
		/*
		 * For policy versions between 20 and 23, attributes exist in the policy,
		 * but only in the type_attr_map. This means that there are gaps in both
		 * the type_val_to_struct and p_type_val_to_name arrays and policy rules
		 * can refer to those gaps.
		 */
		sepol_log_err("Writing policy versions between 20 and 23 as a policy.conf is not supported");
		rc = -1;
		goto exit;
	}

	rc = constraint_rules_to_strs(pdb, mls_constraints, non_mls_constraints);
	if (rc != 0) {
		goto exit;
	}

	rc = validatetrans_rules_to_strs(pdb, mls_validatetrans, non_mls_validatetrans);
	if (rc != 0) {
		goto exit;
	}

	rc = write_handle_unknown_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_class_decl_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_sid_decl_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_class_and_common_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_default_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_mls_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	strs_write_each(mls_constraints, out);
	strs_write_each(mls_validatetrans, out);

	rc = write_polcap_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_type_attributes_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_role_attributes_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_boolean_decl_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_type_decl_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_type_alias_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_type_bounds_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_type_attribute_sets_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_type_permissive_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_avtab_to_conf(out, pdb, 0);
	if (rc != 0) {
		goto exit;
	}
	write_filename_trans_rules_to_conf(out, pdb);

	if (pdb->mls) {
		rc = write_range_trans_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}
	}

	rc = write_cond_nodes_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_role_decl_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_role_transition_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_role_allow_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = write_user_decl_rules_to_conf(out, pdb);
	if (rc != 0) {
		goto exit;
	}

	strs_write_each(non_mls_constraints, out);
	strs_write_each(non_mls_validatetrans, out);

	rc = sort_ocontexts(pdb);
	if (rc != 0) {
		goto exit;
	}

	if (pdb->target_platform == SEPOL_TARGET_SELINUX) {
		rc = write_selinux_isid_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_fsuse_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_genfscon_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_port_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_netif_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_node_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_node6_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_ibpkey_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_selinux_ibendport_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}
	} else if (pdb->target_platform == SEPOL_TARGET_XEN) {
		rc = write_xen_isid_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_genfscon_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_xen_pirq_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_xen_iomem_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_xen_ioport_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_xen_pcidevice_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}

		rc = write_xen_devicetree_rules_to_conf(out, pdb);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	strs_free_all(mls_constraints);
	strs_destroy(&mls_constraints);
	strs_free_all(non_mls_constraints);
	strs_destroy(&non_mls_constraints);
	strs_free_all(mls_validatetrans);
	strs_destroy(&mls_validatetrans);
	strs_free_all(non_mls_validatetrans);
	strs_destroy(&non_mls_validatetrans);

	return rc;
}

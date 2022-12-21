/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>

#include <sepol/policydb/polcaps.h>
#include <sepol/errcodes.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_log.h"
#include "cil_mem.h"
#include "cil_tree.h"
#include "cil_list.h"
#include "cil_find.h"
#include "cil_stack.h"

#include "cil_verify.h"

static int __cil_is_reserved_name(const char *name, enum cil_flavor flavor)
{
	switch (flavor) {
	case CIL_BOOL:
	case CIL_TUNABLE:
		if ((name == CIL_KEY_EQ) || (name == CIL_KEY_NEQ))
			return CIL_TRUE;
		break;
	case CIL_PERM:
	case CIL_MAP_PERM:
	case CIL_USER:
	case CIL_USERATTRIBUTE:
	case CIL_ROLE:
	case CIL_ROLEATTRIBUTE:
		if (name == CIL_KEY_ALL)
			return CIL_TRUE;
		break;
	case CIL_TYPE:
	case CIL_TYPEATTRIBUTE:
	case CIL_TYPEALIAS:
		if ((name == CIL_KEY_ALL) || (name == CIL_KEY_SELF))
			return CIL_TRUE;
		break;
	case CIL_CAT:
	case CIL_CATSET:
	case CIL_CATALIAS:
	case CIL_PERMISSIONX:
		if ((name == CIL_KEY_ALL) || (name == CIL_KEY_RANGE))
			return CIL_TRUE;
		break;
	default:
		/* All of these are not used in expressions */
		return CIL_FALSE;
		break;
	}

	/* Everything not under the default case is also checked for these */
	if ((name == CIL_KEY_AND) || (name == CIL_KEY_OR) || (name == CIL_KEY_NOT) || (name == CIL_KEY_XOR)) {
		return CIL_TRUE;
	}

	return CIL_FALSE;
}

int cil_verify_name(const struct cil_db *db, const char *name, enum cil_flavor flavor)
{
	int rc = SEPOL_ERR;
	int len;
	int i = 0;

	if (name == NULL) {
		cil_log(CIL_ERR, "Name is NULL\n");
		goto exit;
	}

	len = strlen(name);
	if (len >= CIL_MAX_NAME_LENGTH) {
		cil_log(CIL_ERR, "Name length greater than max name length of %d", 
			CIL_MAX_NAME_LENGTH);
		rc = SEPOL_ERR;
		goto exit;
	}

	if (!isalpha(name[0])) {
			cil_log(CIL_ERR, "First character in %s is not a letter\n", name);
			goto exit;
	}

	if (db->qualified_names == CIL_FALSE) {
		for (i = 1; i < len; i++) {
			if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-') {
				cil_log(CIL_ERR, "Invalid character \"%c\" in %s\n", name[i], name);
				goto exit;
			}
		}
	} else {
		for (i = 1; i < len; i++) {
			if (!isalnum(name[i]) && name[i] != '_' && name[i] != '-' && name[i] != '.') {
				cil_log(CIL_ERR, "Invalid character \"%c\" in %s\n", name[i], name);
				goto exit;
			}
		}
	}

	if (__cil_is_reserved_name(name, flavor)) {
		cil_log(CIL_ERR, "Name %s is a reserved word\n", name);
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR, "Invalid name\n");
	return rc;
}

int __cil_verify_syntax(struct cil_tree_node *parse_current, enum cil_syntax s[], size_t len)
{
	struct cil_tree_node *c = parse_current;
	size_t i = 0;

	while (i < len && c != NULL) {
		if ((s[i] & CIL_SYN_STRING) && c->data != NULL && c->cl_head == NULL) {
			c = c->next;
			i++;
		} else if ((s[i] & CIL_SYN_LIST) && c->data == NULL && c->cl_head != NULL) {
			c = c->next;
			i++;
		} else if ((s[i] & CIL_SYN_EMPTY_LIST) && c->data == NULL && c->cl_head == NULL) {
			c = c->next;
			i++;
		} else if ((s[i] & CIL_SYN_N_LISTS) || (s[i] & CIL_SYN_N_STRINGS)) {
			while (c != NULL) {
				if ((s[i] & CIL_SYN_N_LISTS) && c->data == NULL && c->cl_head != NULL) {
					c = c->next;
				} else if ((s[i] & CIL_SYN_N_STRINGS) && c->data != NULL && c->cl_head == NULL) {
					c = c->next;
				} else {
					goto exit;
				}
			}
			i++;
			break; /* Only CIL_SYN_END allowed after these */
		} else {
			goto exit;
		}
	}

	if (i < len && (s[i] & CIL_SYN_END) && c == NULL) {
		return SEPOL_OK;
	}

exit:
	cil_log(CIL_ERR, "Invalid syntax\n");
	return SEPOL_ERR;
}

int cil_verify_expr_syntax(struct cil_tree_node *current, enum cil_flavor op, enum cil_flavor expr_flavor)
{
	int rc;
	enum cil_syntax syntax[] = {
		CIL_SYN_STRING,
		CIL_SYN_STRING | CIL_SYN_LIST,
		CIL_SYN_STRING | CIL_SYN_LIST,
		CIL_SYN_END
	};
	int syntax_len = sizeof(syntax)/sizeof(*syntax);

	switch (op) {
	case CIL_NOT:
		syntax[2] = CIL_SYN_END;
		syntax_len = 3;
		break;
	case CIL_AND:
	case CIL_OR:
	case CIL_XOR:
		break;
	case CIL_EQ:
	case CIL_NEQ:
		if (expr_flavor != CIL_BOOL && expr_flavor != CIL_TUNABLE ) {
			cil_log(CIL_ERR,"Invalid operator (%s) for set expression\n", (char*)current->data);
			goto exit;
		}
		break;
	case CIL_ALL:
		if (expr_flavor == CIL_BOOL || expr_flavor == CIL_TUNABLE) {
			cil_log(CIL_ERR,"Invalid operator (%s) for boolean or tunable expression\n", (char*)current->data);
			goto exit;
		}
		syntax[1] = CIL_SYN_END;
		syntax_len = 2;
		break;
	case CIL_RANGE:
		if (expr_flavor != CIL_CAT && expr_flavor != CIL_PERMISSIONX) {
			cil_log(CIL_ERR,"Operator (%s) only valid for catset and permissionx expression\n", (char*)current->data);
			goto exit;
		}
		syntax[1] = CIL_SYN_STRING;
		syntax[2] = CIL_SYN_STRING;
		break;
	case CIL_NONE: /* String or List */
		syntax[0] = CIL_SYN_N_STRINGS | CIL_SYN_N_LISTS;
		syntax[1] = CIL_SYN_END;
		syntax_len = 2;
		break;
	default:
		cil_log(CIL_ERR,"Unexpected value (%s) for expression operator\n", (char*)current->data);
		goto exit;
	}

	rc = __cil_verify_syntax(current, syntax, syntax_len);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

int cil_verify_constraint_leaf_expr_syntax(enum cil_flavor l_flavor, enum cil_flavor r_flavor, enum cil_flavor op, enum cil_flavor expr_flavor)
{
	if (r_flavor == CIL_STRING || r_flavor == CIL_LIST) {
		if (l_flavor == CIL_CONS_L1 || l_flavor == CIL_CONS_L2 || l_flavor == CIL_CONS_H1 || l_flavor == CIL_CONS_H2 ) {
			cil_log(CIL_ERR, "l1, l2, h1, and h2 cannot be used on the left side with a string or list on the right side\n");
			goto exit;
		} else if (l_flavor == CIL_CONS_U3 || l_flavor == CIL_CONS_R3 || l_flavor == CIL_CONS_T3) {
			if (expr_flavor != CIL_VALIDATETRANS && expr_flavor != CIL_MLSVALIDATETRANS) {
				cil_log(CIL_ERR, "u3, r3, and t3 can only be used with (mls)validatetrans rules\n");
				goto exit;
			}
		}
	} else {
		if (r_flavor == CIL_CONS_U1 || r_flavor == CIL_CONS_R1 || r_flavor == CIL_CONS_T1) {
			cil_log(CIL_ERR, "u1, r1, and t1 are not allowed on the right side\n");
			goto exit;
		} else if (r_flavor == CIL_CONS_U3 || r_flavor == CIL_CONS_R3 || r_flavor == CIL_CONS_T3) {
			cil_log(CIL_ERR, "u3, r3, and t3 are not allowed on the right side\n");
			goto exit;
		} else if (r_flavor == CIL_CONS_U2) {
			if (op != CIL_EQ && op != CIL_NEQ) {
				cil_log(CIL_ERR, "u2 on the right side must be used with eq or neq as the operator\n");
				goto exit;
			} else if (l_flavor != CIL_CONS_U1) {
				cil_log(CIL_ERR, "u2 on the right side must be used with u1 on the left\n");
				goto exit;
			}
		} else if (r_flavor == CIL_CONS_R2) {
			if (l_flavor != CIL_CONS_R1) {
				cil_log(CIL_ERR, "r2 on the right side must be used with r1 on the left\n");
				goto exit;
			}
		} else if (r_flavor == CIL_CONS_T2) {
			if (op != CIL_EQ && op != CIL_NEQ) {
				cil_log(CIL_ERR, "t2 on the right side must be used with eq or neq as the operator\n");
				goto exit;
			} else if (l_flavor != CIL_CONS_T1) {
				cil_log(CIL_ERR, "t2 on the right side must be used with t1 on the left\n");
				goto exit;
			}
		} else if (r_flavor == CIL_CONS_L2) {
			if (l_flavor != CIL_CONS_L1 && l_flavor != CIL_CONS_H1) {
				cil_log(CIL_ERR, "l2 on the right side must be used with l1 or h1 on the left\n");
				goto exit;
			}
		} else if (r_flavor == CIL_CONS_H2) {
			if (l_flavor != CIL_CONS_L1 && l_flavor != CIL_CONS_L2 && l_flavor != CIL_CONS_H1 ) {
				cil_log(CIL_ERR, "h2 on the right side must be used with l1, l2, or h1 on the left\n");
				goto exit;
			}
		} else if (r_flavor == CIL_CONS_H1) {
			if (l_flavor != CIL_CONS_L1) {
				cil_log(CIL_ERR, "h1 on the right side must be used with l1 on the left\n");
				goto exit;
			}
		}
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

int cil_verify_constraint_expr_syntax(struct cil_tree_node *current, enum cil_flavor op)
{
	int rc;
	enum cil_syntax syntax[] = {
		CIL_SYN_STRING,
		CIL_SYN_END,
		CIL_SYN_END,
		CIL_SYN_END
	};
	int syntax_len = sizeof(syntax)/sizeof(*syntax);

	switch (op) {
	case CIL_NOT:
		syntax[1] = CIL_SYN_LIST;
		syntax_len--;
		break;
	case CIL_AND:
	case CIL_OR:
		syntax[1] = CIL_SYN_LIST;
		syntax[2] = CIL_SYN_LIST;
		break;
	case CIL_EQ:
	case CIL_NEQ:
		syntax[1] = CIL_SYN_STRING;
		syntax[2] = CIL_SYN_STRING | CIL_SYN_LIST;
		break;
	case CIL_CONS_DOM:
	case CIL_CONS_DOMBY:
	case CIL_CONS_INCOMP:
		syntax[1] = CIL_SYN_STRING;
		syntax[2] = CIL_SYN_STRING;
		break;
	default:
		cil_log(CIL_ERR, "Invalid operator (%s) for constraint expression\n", (char*)current->data);
		goto exit;
	}

	rc = __cil_verify_syntax(current, syntax, syntax_len);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Invalid constraint syntax\n");
		goto exit;
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

int cil_verify_conditional_blocks(struct cil_tree_node *current)
{
	int found_true = CIL_FALSE;
	int found_false = CIL_FALSE;

	if (current->cl_head->data == CIL_KEY_CONDTRUE) {
		found_true = CIL_TRUE;
	} else if (current->cl_head->data == CIL_KEY_CONDFALSE) {
		found_false = CIL_TRUE;
	} else {
		cil_tree_log(current, CIL_ERR, "Expected true or false block in conditional");
		return SEPOL_ERR;
	}

	current = current->next;
	if (current != NULL) {
		if (current->cl_head->data == CIL_KEY_CONDTRUE) {
			if (found_true) {
				cil_tree_log(current, CIL_ERR, "More than one true block in conditional");
				return SEPOL_ERR;
			}
		} else if (current->cl_head->data == CIL_KEY_CONDFALSE) {
			if (found_false) {
				cil_tree_log(current, CIL_ERR, "More than one false block in conditional");
				return SEPOL_ERR;
			}
		} else {
			cil_tree_log(current, CIL_ERR, "Expected true or false block in conditional");
			return SEPOL_ERR;
		}
	}

	return SEPOL_OK;
}

int cil_verify_decl_does_not_shadow_macro_parameter(struct cil_macro *macro, struct cil_tree_node *node, const char *name)
{
	struct cil_list_item *item;
	struct cil_list *param_list = macro->params;
	if (param_list != NULL) {
		cil_list_for_each(item, param_list) {
			struct cil_param *param = item->data;
			if (param->flavor == node->flavor) {
				if (param->str == name) {
					cil_log(CIL_ERR, "%s %s shadows a macro parameter in macro declaration\n", cil_node_to_string(node), name);
					return SEPOL_ERR;
				}
			}
		}
	}
	return SEPOL_OK;
}

static int cil_verify_no_self_reference(enum cil_flavor flavor, struct cil_symtab_datum *datum, struct cil_stack *stack);

static int __verify_no_self_reference_in_expr(struct cil_list *expr, struct cil_stack *stack)
{
	struct cil_list_item *item;
	int rc = SEPOL_OK;

	if (!expr) {
		return SEPOL_OK;
	}

	cil_list_for_each(item, expr) {
		if (item->flavor == CIL_DATUM) {
			struct cil_symtab_datum* datum = item->data;
			rc = cil_verify_no_self_reference(FLAVOR(datum), datum, stack);
		} else if (item->flavor == CIL_LIST) {
			rc = __verify_no_self_reference_in_expr(item->data, stack);
		}
		if (rc != SEPOL_OK) {
			return SEPOL_ERR;
		}
	}

	return SEPOL_OK;
}

static int cil_verify_no_self_reference(enum cil_flavor flavor, struct cil_symtab_datum *datum, struct cil_stack *stack)
{
	struct cil_stack_item *item;
	int i = 0;
	int rc = SEPOL_OK;

	cil_stack_for_each(stack, i, item) {
		struct cil_symtab_datum *prev = item->data;
		if (datum == prev) {
			cil_tree_log(NODE(datum), CIL_ERR, "Self-reference found for %s", datum->name);
			return SEPOL_ERR;
		}
	}

	switch (flavor) {
	case CIL_USERATTRIBUTE: {
		struct cil_userattribute *attr = (struct cil_userattribute *)datum;
		cil_stack_push(stack, CIL_DATUM, datum);
		rc = __verify_no_self_reference_in_expr(attr->expr_list, stack);
		cil_stack_pop(stack);
		break;
	}
	case CIL_ROLEATTRIBUTE: {
		struct cil_roleattribute *attr = (struct cil_roleattribute *)datum;
		cil_stack_push(stack, CIL_DATUM, datum);
		rc = __verify_no_self_reference_in_expr(attr->expr_list, stack);
		cil_stack_pop(stack);
		break;
	}
	case CIL_TYPEATTRIBUTE: {
		struct cil_typeattribute *attr = (struct cil_typeattribute *)datum;
		cil_stack_push(stack, CIL_DATUM, datum);
		rc = __verify_no_self_reference_in_expr(attr->expr_list, stack);
		cil_stack_pop(stack);
		break;
	}
	case CIL_CATSET: {
		struct cil_catset *set = (struct cil_catset *)datum;
		cil_stack_push(stack, CIL_DATUM, datum);
		rc = __verify_no_self_reference_in_expr(set->cats->datum_expr, stack);
		cil_stack_pop(stack);
		break;
	}
	default:
		break;
	}

	return rc;
}

int __cil_verify_ranges(struct cil_list *list)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *curr;
	struct cil_list_item *range = NULL;

	if (list == NULL || list->head == NULL) {
		goto exit;
	}

	cil_list_for_each(curr, list) {
		/* range */
		if (curr->flavor == CIL_LIST) {
			range = ((struct cil_list*)curr->data)->head;
			if (range == NULL || range->next == NULL || range->next->next != NULL) {
				goto exit;
			}
		}
	}

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR,"Invalid Range syntax\n");
	return rc;
}

struct cil_args_verify_order {
	uint32_t *flavor;
};

int __cil_verify_ordered_node_helper(struct cil_tree_node *node, __attribute__((unused)) uint32_t *finished, void *extra_args)
{
	struct cil_args_verify_order *args = extra_args;
	uint32_t *flavor = args->flavor;

	if (node->flavor == *flavor) {
		if (node->flavor == CIL_SID) {
			struct cil_sid *sid = node->data;
			if (sid->ordered == CIL_FALSE) {
				cil_tree_log(node, CIL_ERR, "SID %s not in sidorder statement", sid->datum.name);
				return SEPOL_ERR;
			}
		} else if (node->flavor == CIL_CLASS) {
			struct cil_class *class = node->data;
			if (class->ordered == CIL_FALSE) {
				cil_tree_log(node, CIL_ERR, "Class %s not in classorder statement", class->datum.name);
				return SEPOL_ERR;
			}
		} else if (node->flavor == CIL_CAT) {
			struct cil_cat *cat = node->data;
			if (cat->ordered == CIL_FALSE) {
				cil_tree_log(node, CIL_ERR, "Category %s not in categoryorder statement", cat->datum.name);
				return SEPOL_ERR;
			}
		} else if (node->flavor == CIL_SENS) {
			struct cil_sens *sens = node->data;
			if (sens->ordered == CIL_FALSE) {
				cil_tree_log(node, CIL_ERR, "Sensitivity %s not in sensitivityorder statement", sens->datum.name);
				return SEPOL_ERR;
			}
		}
	}

	return SEPOL_OK;
}

int __cil_verify_ordered(struct cil_tree_node *current, enum cil_flavor flavor)
{
	struct cil_args_verify_order extra_args;
	int rc = SEPOL_ERR;

	extra_args.flavor = &flavor;

	rc = cil_tree_walk(current, __cil_verify_ordered_node_helper, NULL, NULL, &extra_args);

	return rc;
}

int __cil_verify_initsids(struct cil_list *sids)
{
	int rc = SEPOL_OK;
	struct cil_list_item *i;

	if (sids->head == NULL) {
		cil_log(CIL_ERR, "At least one initial sid must be defined in the policy\n");
		return SEPOL_ERR;
	}

	cil_list_for_each(i, sids) {
		struct cil_sid *sid = i->data;
		if (sid->context == NULL) {
			struct cil_tree_node *node = sid->datum.nodes->head->data;
			cil_tree_log(node, CIL_INFO, "No context assigned to SID %s, omitting from policy",sid->datum.name);
		}
	}

	return rc;
}

static int __cil_is_cat_in_cats(struct cil_cat *cat, struct cil_cats *cats)
{
	struct cil_list_item *i;

	cil_list_for_each(i, cats->datum_expr) {
		struct cil_cat *c = i->data;
		if (c == cat) {
			return CIL_TRUE;
		}
	}

	return CIL_FALSE;
}


static int __cil_verify_cat_in_cats(struct cil_cat *cat, struct cil_cats *cats)
{
	if (__cil_is_cat_in_cats(cat, cats) != CIL_TRUE) {
		cil_log(CIL_ERR, "Failed to find category %s in category list\n", cat->datum.name);
		return SEPOL_ERR;
	}

	return SEPOL_OK;
}

static int __cil_verify_cats_associated_with_sens(struct cil_sens *sens, struct cil_cats *cats)
{
	int rc = SEPOL_OK;
	struct cil_list_item *i, *j;

	if (!cats) {
		return SEPOL_OK;
	}

	if (!sens->cats_list) {
		cil_log(CIL_ERR, "No categories can be used with sensitivity %s\n", sens->datum.name);
		return SEPOL_ERR;
	}

	cil_list_for_each(i, cats->datum_expr) {
		struct cil_cat *cat = i->data;
		int ok = CIL_FALSE;
		cil_list_for_each(j, sens->cats_list) {
			if (__cil_is_cat_in_cats(cat, j->data) == CIL_TRUE) {
				ok = CIL_TRUE;
				break;
			}
		}

		if (ok != CIL_TRUE) {
			cil_log(CIL_ERR, "Category %s cannot be used with sensitivity %s\n", 
					cat->datum.name, sens->datum.name);
			rc = SEPOL_ERR;
		}
	}

	return rc;
}

static int __cil_verify_levelrange_sensitivity(struct cil_db *db, struct cil_sens *low, struct cil_sens *high)
{
	struct cil_list_item *curr;
	int found = CIL_FALSE;
	int rc = SEPOL_ERR;

	cil_list_for_each(curr, db->sensitivityorder) {
		if (curr->data == low) {
			found = CIL_TRUE;
		}

		if ((found == CIL_TRUE) && (curr->data == high)) {
			break;
		}
	}

	if (found != CIL_TRUE || curr == NULL) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR, "Sensitivity %s does not dominate %s\n", 
		high->datum.name, low->datum.name);
	return rc;

}

static int __cil_verify_levelrange_cats(struct cil_cats *low, struct cil_cats *high)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *item;

	if (low == NULL || (low == NULL && high == NULL)) {
		return SEPOL_OK;
	}

	if (high == NULL) {
		rc = SEPOL_ERR;
		goto exit;
	}

	cil_list_for_each(item, low->datum_expr) {
		rc = __cil_verify_cat_in_cats(item->data, high);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR, "Low level category set must be a subset of the high level category set\n");
	return rc;
}

static int __cil_verify_levelrange(struct cil_db *db, struct cil_levelrange *lr)
{
	int rc = SEPOL_ERR;

	rc = __cil_verify_levelrange_sensitivity(db, lr->low->sens, lr->high->sens);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	rc = __cil_verify_levelrange_cats(lr->low->cats, lr->high->cats);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	rc = __cil_verify_cats_associated_with_sens(lr->low->sens, lr->low->cats);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Low level sensitivity and categories are not associated\n");
		goto exit;
	}

	rc = __cil_verify_cats_associated_with_sens(lr->high->sens, lr->high->cats);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "High level sensitivity and categories are not associated\n");
		goto exit;
	}
	
	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_verify_named_levelrange(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_levelrange *lr = node->data;

	rc = __cil_verify_levelrange(db, lr);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid named range");
	return rc;
}

static int __cil_verify_user_pre_eval(struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_user *user = node->data;

	if (user->dftlevel == NULL) {
		cil_log(CIL_ERR, "User %s does not have a default level\n", user->datum.name);
		goto exit;
	} else if (user->range == NULL) {
		cil_log(CIL_ERR, "User %s does not have a level range\n", user->datum.name);
		goto exit;
	} else if (user->bounds != NULL) {
		int steps = 0;
		int limit = 2;
		struct cil_user *u1 = user;
		struct cil_user *u2 = user->bounds;

		while (u2 != NULL) {
			if (u1 == u2) {
				cil_log(CIL_ERR, "Circular bounds found for user %s\n", u1->datum.name);
				goto exit;
			}

			if (steps == limit) {
				steps = 0;
				limit *= 2;
				u1 = u2;
			}

			u2 = u2->bounds;
			steps++;
		}
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid user");
	return rc;
}

static int __cil_verify_user_post_eval(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_user *user = node->data;

	/* Verify user range only if anonymous */
	if (user->range->datum.name == NULL) {
		rc = __cil_verify_levelrange(db, user->range);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid user");
	return rc;
}

static int __cil_verify_role(struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_role *role = node->data;
	int steps = 0;
	int limit = 2;
	struct cil_role *r1 = role;
	struct cil_role *r2 = role->bounds;

	while (r2 != NULL) {
		if (r1 == r2) {
			cil_log(CIL_ERR, "Circular bounds found for role %s\n", r1->datum.name);
			goto exit;
		}

		if (steps == limit) {
			steps = 0;
			limit *= 2;
			r1 = r2;
		}

		r2 = r2->bounds;
		steps++;
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid role");
	return rc;
}

static int __cil_verify_type(struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_type *type = node->data;
	int steps = 0;
	int limit = 2;
	struct cil_type *t1 = type;
	struct cil_type *t2 = type->bounds;

	while (t2 != NULL) {
		if (t1 == t2) {
			cil_log(CIL_ERR, "Circular bounds found for type %s\n", t1->datum.name);
			goto exit;
		}

		if (steps == limit) {
			steps = 0;
			limit *= 2;
			t1 = t2;
		}

		t2 = t2->bounds;
		steps++;
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid type");
	return rc;
}

static int __cil_verify_context(struct cil_db *db, struct cil_context *ctx)
{
	int rc = SEPOL_ERR;
	struct cil_user *user = ctx->user;
	struct cil_role *role = ctx->role;
	struct cil_type *type = ctx->type;
	struct cil_level *user_low = user->range->low;
	struct cil_level *user_high = user->range->high;
	struct cil_level *ctx_low = ctx->range->low;
	struct cil_level *ctx_high = ctx->range->high;
	struct cil_list *sensitivityorder = db->sensitivityorder;
	struct cil_list_item *curr;
	int found = CIL_FALSE;

	if (user->roles != NULL) {
		if (!ksu_ebitmap_get_bit(user->roles, role->value)) {
			cil_log(CIL_ERR, "Role %s is invalid for user %s\n", ctx->role_str, ctx->user_str);
			rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		cil_log(CIL_ERR, "No roles given to the user %s\n", ctx->user_str);
		rc = SEPOL_ERR;
		goto exit;
	}

	if (role->types != NULL) {
		if (!ksu_ebitmap_get_bit(role->types, type->value)) {
			cil_log(CIL_ERR, "Type %s is invalid for role %s\n", ctx->type_str, ctx->role_str);
			rc = SEPOL_ERR;
			goto exit;
		}
	} else {
		cil_log(CIL_ERR, "No types associated with role %s\n", ctx->role_str);
		rc = SEPOL_ERR;
		goto exit;
	}

	/* Verify range only when anonymous */
	if (ctx->range->datum.name == NULL) {
		rc = __cil_verify_levelrange(db, ctx->range);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	for (curr = sensitivityorder->head; curr != NULL; curr = curr->next) {
		struct cil_sens *sens = curr->data;

		if (found == CIL_FALSE) {
			if (sens == user_low->sens) {
				found = CIL_TRUE;
			} else if (sens == ctx_low->sens) {
				cil_log(CIL_ERR, "Range %s is invalid for user %s\n", 
					ctx->range_str, ctx->user_str);
				rc = SEPOL_ERR;
				goto exit;
			}
		}

		if (found == CIL_TRUE) {
			if (sens == ctx_high->sens) {
				break;
			} else if (sens == user_high->sens) {
				cil_log(CIL_ERR, "Range %s is invalid for user %s\n", 
					ctx->range_str, ctx->user_str);
				rc = SEPOL_ERR;
				goto exit;
			}
		}
	}

	return SEPOL_OK;
exit:
	cil_log(CIL_ERR, "Invalid context\n");
	return rc;
}

static int __cil_verify_named_context(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_context *ctx = node->data;

	rc = __cil_verify_context(db, ctx);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid named context");
	return rc;
}

/*
static int __cil_verify_rule(struct cil_tree_node *node, struct cil_complex_symtab *symtab)
{

	int rc = SEPOL_ERR;
	struct cil_type_rule *typerule = NULL;
	struct cil_roletransition *roletrans = NULL;
	struct cil_complex_symtab_key ckey;

	switch (node->flavor) {
	case CIL_ROLETRANSITION: {
		roletrans = node->data;
		ckey.key1 = (intptr_t)roletrans->src;
		ckey.key2 = (intptr_t)roletrans->tgt;
		ckey.key3 = (intptr_t)roletrans->obj;
		ckey.key4 = CIL_ROLETRANSITION;
		break;
	}
	case CIL_TYPE_RULE: {
		typerule = node->data;
		ckey.key1 = (intptr_t)typerule->src;
		ckey.key2 = (intptr_t)typerule->tgt;
		ckey.key3 = (intptr_t)typerule->obj;
		ckey.key4 = (intptr_t)typerule->rule_kind;
		break;
	}
	default:
		break;
	}


	rc = cil_complex_symtab_insert(symtab, &ckey, NULL);
	if (rc == SEPOL_EEXIST) {
		struct cil_complex_symtab_datum *datum = NULL;
		cil_complex_symtab_search(symtab, &ckey, &datum);
		if (datum == NULL) {
			cil_tree_log(node, CIL_ERR, "Duplicate rule defined");
			rc = SEPOL_ERR;
			goto exit;
		}
	}

	return SEPOL_OK;
exit:
	cil_tree_log(node, CIL_ERR, "Invalid rule");
	return rc;
}
*/

static int __cil_verify_booleanif_helper(struct cil_tree_node *node, __attribute__((unused)) uint32_t *finished, __attribute__((unused)) void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_tree_node *rule_node = node;
	struct cil_booleanif *bif = node->parent->parent->data;

	switch (rule_node->flavor) {
	case CIL_AVRULE: {
		struct cil_avrule *avrule = NULL;
		avrule = rule_node->data;
		if (avrule->rule_kind == CIL_AVRULE_NEVERALLOW) {
			if (bif->preserved_tunable) {
				cil_tree_log(node, CIL_ERR, "Neverallow found in tunableif block (treated as a booleanif due to preserve-tunables)");
			} else {
				cil_tree_log(node, CIL_ERR, "Neverallow found in booleanif block");
			}
			rc = SEPOL_ERR;
			goto exit;
		}
		break;
	}
	case CIL_TYPE_RULE: /*
	struct cil_type_rule *typerule = NULL;
	struct cil_tree_node *temp_node = NULL;
	struct cil_complex_symtab *symtab = extra_args;
	struct cil_complex_symtab_key ckey;
	struct cil_complex_symtab_datum datum;
		typerule = rule_node->data;

		ckey.key1 = (intptr_t)typerule->src;
		ckey.key2 = (intptr_t)typerule->tgt;
		ckey.key3 = (intptr_t)typerule->obj;
		ckey.key4 = (intptr_t)typerule->rule_kind;

		datum.data = node;

		rc = cil_complex_symtab_insert(symtab, &ckey, &datum);
		if (rc != SEPOL_OK) {
			goto exit;
		}

		for (temp_node = rule_node->next;
			temp_node != NULL;
			temp_node = temp_node->next) {

			if (temp_node->flavor == CIL_TYPE_RULE) {
				typerule = temp_node->data;
				if ((intptr_t)typerule->src == ckey.key1 &&
					(intptr_t)typerule->tgt == ckey.key2 &&
					(intptr_t)typerule->obj == ckey.key3 &&
					(intptr_t)typerule->rule_kind == ckey.key4) {
					cil_log(CIL_ERR, "Duplicate type rule found (line: %d)\n", node->line);
					rc = SEPOL_ERR;
					goto exit;
				}
			}
		}
		break;*/

		//TODO Fix duplicate type_rule detection
		break;
	case CIL_CALL:
		//Fall through to check content of call
		break;
	case CIL_TUNABLEIF:
		//Fall through
		break;
	case CIL_NAMETYPETRANSITION:
		/* While type transitions with file component are not allowed in
		   booleanif statements if they don't have "*" as the file. We
		   can't check that here. Or at least we won't right now. */
		break;
	default: {
		const char * flavor = cil_node_to_string(node);
		if (bif->preserved_tunable) {
			cil_tree_log(node, CIL_ERR, "Invalid %s statement in tunableif (treated as a booleanif due to preserve-tunables)", flavor);
		} else {
			cil_tree_log(node, CIL_ERR, "Invalid %s statement in booleanif", flavor);
		}
		goto exit;
	}
	}

	rc = SEPOL_OK;
exit:
	return rc;
}

static int __cil_verify_booleanif(struct cil_tree_node *node, struct cil_complex_symtab *symtab)
{
	int rc = SEPOL_ERR;
	struct cil_booleanif *bif = (struct cil_booleanif*)node->data;
	struct cil_tree_node *cond_block = node->cl_head;

	while (cond_block != NULL) {
		rc = cil_tree_walk(cond_block, __cil_verify_booleanif_helper, NULL, NULL, symtab);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		cond_block = cond_block->next;
	}

	return SEPOL_OK;
exit:
	if (bif->preserved_tunable) {
		cil_tree_log(node, CIL_ERR, "Invalid tunableif (treated as a booleanif due to preserve-tunables)");
	} else {
		cil_tree_log(node, CIL_ERR, "Invalid booleanif");
	}
	return rc;
}

static int __cil_verify_netifcon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_netifcon *netif = node->data;
	struct cil_context *if_ctx = netif->if_context;
	struct cil_context *pkt_ctx = netif->packet_context;

	/* Verify only when anonymous */
	if (if_ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, if_ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	/* Verify only when anonymous */
	if (pkt_ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, pkt_ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid netifcon");
	return rc;
}

static int __cil_verify_ibendportcon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_ibendportcon *ib_end_port = node->data;
	struct cil_context *ctx = ib_end_port->context;

	/* Verify only when anonymous */
	if (!ctx->datum.name) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK)
			goto exit;
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid ibendportcon");
	return rc;
}

static int __cil_verify_genfscon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_genfscon *genfs = node->data;
	struct cil_context *ctx = genfs->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid genfscon");
	return rc;
}

static int __cil_verify_filecon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_filecon *file = node->data;
	struct cil_context *ctx = file->context;

	if (ctx == NULL) {
		rc = SEPOL_OK;
		goto exit;
	}

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			cil_tree_log(node, CIL_ERR, "Invalid filecon");
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_verify_nodecon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_nodecon *nodecon = node->data;
	struct cil_context *ctx = nodecon->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid nodecon");
	return rc;
}

static int __cil_verify_ibpkeycon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_ibpkeycon *pkey = node->data;
	struct cil_context *ctx = pkey->context;

	/* Verify only when anonymous */
	if (!ctx->datum.name) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK)
			goto exit;
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid ibpkeycon");
	return rc;
}

static int __cil_verify_portcon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_portcon *port = node->data;
	struct cil_context *ctx = port->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid portcon");
	return rc;
}

static int __cil_verify_pirqcon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_pirqcon *pirq = node->data;
	struct cil_context *ctx = pirq->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid pirqcon");
	return rc;
}

static int __cil_verify_iomemcon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_iomemcon *iomem = node->data;
	struct cil_context *ctx = iomem->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid iomemcon");
	return rc;
}

static int __cil_verify_ioportcon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_ioportcon *ioport = node->data;
	struct cil_context *ctx = ioport->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid ioportcon");
	return rc;
}

static int __cil_verify_pcidevicecon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_pcidevicecon *pcidev = node->data;
	struct cil_context *ctx = pcidev->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid pcidevicecon");
	return rc;
}

static int __cil_verify_devicetreecon(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_devicetreecon *dt = node->data;
	struct cil_context *ctx = dt->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid devicetreecon");
	return rc;
}

static int __cil_verify_fsuse(struct cil_db *db, struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_fsuse *fsuse = node->data;
	struct cil_context *ctx = fsuse->context;

	/* Verify only when anonymous */
	if (ctx->datum.name == NULL) {
		rc = __cil_verify_context(db, ctx);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid fsuse");
	return rc;
}

static int __cil_verify_permissionx(struct cil_permissionx *permx, struct cil_tree_node *node)
{
	int rc;
	struct cil_list *classes = NULL;
	struct cil_list_item *item;
	struct cil_class *class;
	struct cil_symtab_datum *perm_datum;
	char *kind_str;

	switch (permx->kind) {
		case CIL_PERMX_KIND_IOCTL:
			kind_str = CIL_KEY_IOCTL;
			break;
		default:
			cil_tree_log(node, CIL_ERR, "Invalid permissionx kind (%d)", permx->kind);
			rc = SEPOL_ERR;
			goto exit;
	}

	classes = cil_expand_class(permx->obj);

	cil_list_for_each(item, classes) {
		class = item->data;
		rc = cil_symtab_get_datum(&class->perms, kind_str, &perm_datum);
		if (rc == SEPOL_ENOENT) {
			if (class->common != NULL) {
				rc = cil_symtab_get_datum(&class->common->perms, kind_str, &perm_datum);
			}

			if (rc == SEPOL_ENOENT) {
				cil_tree_log(node, CIL_ERR, "Invalid permissionx: %s is not a permission of class %s", kind_str, class->datum.name);
				rc = SEPOL_ERR;
				goto exit;
			}
		}
	}

	rc = SEPOL_OK;

exit:
	if (classes != NULL) {
		cil_list_destroy(&classes, CIL_FALSE);
	}

	return rc;
}

static int __cil_verify_avrulex(struct cil_tree_node *node)
{
	struct cil_avrule *avrulex = node->data;
	return __cil_verify_permissionx(avrulex->perms.x.permx, node);
}

static int __cil_verify_class(struct cil_tree_node *node)
{
	int rc = SEPOL_ERR;
	struct cil_class *class = node->data;

	if (class->common != NULL) {
		struct cil_class *common = class->common;
		struct cil_tree_node *common_node = common->datum.nodes->head->data;
		struct cil_tree_node *curr_com_perm = NULL;

		for (curr_com_perm = common_node->cl_head;
			curr_com_perm != NULL;
			curr_com_perm = curr_com_perm->next) {
			struct cil_perm *com_perm = curr_com_perm->data;
			struct cil_tree_node *curr_class_perm = NULL;

			for (curr_class_perm = node->cl_head;
				curr_class_perm != NULL;
				curr_class_perm = curr_class_perm->next) {
				struct cil_perm *class_perm = curr_class_perm->data;

				if (com_perm->datum.name == class_perm->datum.name) {
					cil_log(CIL_ERR, "Duplicate permissions between %s common and class declarations\n", class_perm->datum.name);
					goto exit;
				}
			}
		}
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid class");
	return rc;
}

static int __cil_verify_policycap(struct cil_tree_node *node)
{
	int rc;
	struct cil_policycap *polcap = node->data;

	rc = sepol_polcap_getnum((const char*)polcap->datum.name);
	if (rc == SEPOL_ERR) {
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_tree_log(node, CIL_ERR, "Invalid policycap (%s)", (const char*)polcap->datum.name);
	return rc;
}

int __cil_verify_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	int *avrule_cnt = 0;
	int *handleunknown;
	int *mls;
	int *nseuserdflt = 0;
	int *pass = 0;
	struct cil_args_verify *args = extra_args;
	struct cil_complex_symtab *csymtab = NULL;
	struct cil_db *db = NULL;

	if (node == NULL || extra_args == NULL) {
		goto exit;
	}

	db = args->db;
	avrule_cnt = args->avrule_cnt;
	handleunknown = args->handleunknown;
	mls = args->mls;
	nseuserdflt = args->nseuserdflt;
	csymtab = args->csymtab;
	pass = args->pass;

	if (node->flavor == CIL_MACRO) {
		*finished = CIL_TREE_SKIP_HEAD;
		rc = SEPOL_OK;
		goto exit;
	} else if (node->flavor == CIL_BLOCK) {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		rc = SEPOL_OK;
		goto exit;
	}

	switch (*pass) {
	case 0: {
		switch (node->flavor) {
		case CIL_USER:
			rc = __cil_verify_user_post_eval(db, node);
			break;
		case CIL_SELINUXUSERDEFAULT:
			(*nseuserdflt)++;
			rc = SEPOL_OK;
			break;
		case CIL_ROLE:
			rc = __cil_verify_role(node);
			break;
		case CIL_TYPE:
			rc = __cil_verify_type(node);
			break;
		case CIL_AVRULE:
			(*avrule_cnt)++;
			rc = SEPOL_OK;
			break;
		case CIL_HANDLEUNKNOWN:
			if (*handleunknown != -1) {
				cil_log(CIL_ERR, "Policy can not have more than one handleunknown\n");
				rc = SEPOL_ERR;
			} else {
				*handleunknown = ((struct cil_handleunknown*)node->data)->handle_unknown;
				rc = SEPOL_OK;
			}
			break;
		case CIL_MLS:
			if (*mls != -1) {
				cil_log(CIL_ERR, "Policy can not have more than one mls\n");
				rc = SEPOL_ERR;
			} else {
				*mls = ((struct cil_mls*)node->data)->value;
				rc = SEPOL_OK;
			}
			break;
		case CIL_ROLETRANSITION:
			rc = SEPOL_OK; //TODO __cil_verify_rule doesn't work quite right
			//rc = __cil_verify_rule(node, csymtab);
			break;
		case CIL_TYPE_RULE:
			rc = SEPOL_OK; //TODO __cil_verify_rule doesn't work quite right
			//rc = __cil_verify_rule(node, csymtab);
			break;
		case CIL_BOOLEANIF:
			rc = __cil_verify_booleanif(node, csymtab);
			*finished = CIL_TREE_SKIP_HEAD;
			break;
		case CIL_LEVELRANGE:
			rc = __cil_verify_named_levelrange(db, node);
			break;
		case CIL_CLASS:
			rc = __cil_verify_class(node);
			break;
		case CIL_POLICYCAP:
			rc = __cil_verify_policycap(node);
			break;
		default:
			rc = SEPOL_OK;
			break;
		}
		break;
	}
	case 1:	{
		switch (node->flavor) {
		case CIL_CONTEXT:
			rc = __cil_verify_named_context(db, node);
			break;
		case CIL_NETIFCON:
			rc = __cil_verify_netifcon(db, node);
			break;
		case CIL_GENFSCON:
			rc = __cil_verify_genfscon(db, node);
			break;
		case CIL_FILECON:
			rc = __cil_verify_filecon(db, node);
			break;
		case CIL_NODECON:
			rc = __cil_verify_nodecon(db, node);
			break;
		case CIL_IBPKEYCON:
			rc = __cil_verify_ibpkeycon(db, node);
			break;
		case CIL_IBENDPORTCON:
			rc = __cil_verify_ibendportcon(db, node);
			break;
		case CIL_PORTCON:
			rc = __cil_verify_portcon(db, node);
			break;
		case CIL_PIRQCON:
			rc = __cil_verify_pirqcon(db, node);
			break;
		case CIL_IOMEMCON:
			rc = __cil_verify_iomemcon(db, node);
			break;
		case CIL_IOPORTCON:
			rc = __cil_verify_ioportcon(db, node);
			break;
		case CIL_PCIDEVICECON:
			rc = __cil_verify_pcidevicecon(db, node);
			break;
		case CIL_DEVICETREECON:
			rc = __cil_verify_devicetreecon(db, node);
			break;
		case CIL_FSUSE:
			rc = __cil_verify_fsuse(db, node);
			break;
		case CIL_AVRULEX:
			rc = __cil_verify_avrulex(node);
			break;
		case CIL_PERMISSIONX:
			rc = __cil_verify_permissionx(node->data, node);
			break;
		case CIL_RANGETRANSITION:
			rc = SEPOL_OK;
			break;
		default:
			rc = SEPOL_OK;
			break;
		}
		break;
	}
	default:
		rc = SEPOL_ERR;
	}

exit:
	return rc;
}

static int __add_perm_to_list(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct cil_list *perm_list = (struct cil_list *)args;

	cil_list_append(perm_list, CIL_DATUM, d);

	return SEPOL_OK;
}

static int __cil_verify_classperms(struct cil_list *classperms,
				   struct cil_symtab_datum *orig,
				   struct cil_symtab_datum *parent,
				   struct cil_symtab_datum *cur,
				   enum cil_flavor flavor,
				   unsigned steps, unsigned limit)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *curr;

	if (classperms == NULL) {
		if (flavor == CIL_MAP_PERM) {
			cil_tree_log(NODE(cur), CIL_ERR, "Map class %s does not have a classmapping for %s", parent->name, cur->name);
		} else {
			cil_tree_log(NODE(cur), CIL_ERR, "Classpermission %s does not have a classpermissionset", cur->name);
		}
		goto exit;
	}

	if (steps > 0 && orig == cur) {
		if (flavor == CIL_MAP_PERM) {
			cil_tree_log(NODE(cur), CIL_ERR, "Found circular class permissions involving the map class %s and permission %s", parent->name, cur->name);
		} else {
			cil_tree_log(NODE(cur), CIL_ERR, "Found circular class permissions involving the set %s", cur->name);
		}
		goto exit;
	} else {
		steps++;
		if (steps > limit) {
			steps = 1;
			limit *= 2;
			orig = cur;
		}
	}

	cil_list_for_each(curr, classperms) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) != CIL_CLASS) { /* MAP */
				struct cil_list_item *i = NULL;
				cil_list_for_each(i, cp->perms) {
					if (i->flavor != CIL_OP) {
						struct cil_perm *cmp = i->data;
						rc = __cil_verify_classperms(cmp->classperms, orig, &cp->class->datum, &cmp->datum, CIL_MAP_PERM, steps, limit);
						if (rc != SEPOL_OK) {
							goto exit;
						}
					} else {
						enum cil_flavor op = (enum cil_flavor)(uintptr_t)i->data;
						if (op == CIL_ALL) {
							struct cil_class *mc = cp->class;
							struct cil_list *perm_list;
							struct cil_list_item *j = NULL;

							cil_list_init(&perm_list, CIL_MAP_PERM);
							cil_symtab_map(&mc->perms, __add_perm_to_list, perm_list);
							cil_list_for_each(j, perm_list) {
								struct cil_perm *cmp = j->data;
								rc = __cil_verify_classperms(cmp->classperms, orig, &cp->class->datum, &cmp->datum, CIL_MAP_PERM, steps, limit);
								if (rc != SEPOL_OK) {
									cil_list_destroy(&perm_list, CIL_FALSE);
									goto exit;
								}
							}
							cil_list_destroy(&perm_list, CIL_FALSE);
						}
					}
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			rc = __cil_verify_classperms(cp->classperms, orig, NULL, &cp->datum, CIL_CLASSPERMISSION, steps, limit);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

static int __cil_verify_classpermission(struct cil_tree_node *node)
{
	struct cil_classpermission *cp = node->data;

	return __cil_verify_classperms(cp->classperms, &cp->datum, NULL, &cp->datum, CIL_CLASSPERMISSION, 0, 2);
}

struct cil_verify_map_args {
	struct cil_class *class;
	struct cil_tree_node *node;
	int rc;
};

static int __verify_map_perm_classperms(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct cil_verify_map_args *map_args = args;
	struct cil_perm *cmp = (struct cil_perm *)d;
	int rc;

	rc = __cil_verify_classperms(cmp->classperms, &cmp->datum, &map_args->class->datum, &cmp->datum, CIL_MAP_PERM, 0, 2);
	if (rc != SEPOL_OK) {
		map_args->rc = rc;
	}

	return SEPOL_OK;
}

static int __cil_verify_map_class(struct cil_tree_node *node)
{
	struct cil_class *mc = node->data;
	struct cil_verify_map_args map_args;

	map_args.class = mc;
	map_args.node = node;
	map_args.rc = SEPOL_OK;

	cil_symtab_map(&mc->perms, __verify_map_perm_classperms, &map_args);

	if (map_args.rc != SEPOL_OK) {
		return SEPOL_ERR;
	}

	return SEPOL_OK;
}

int __cil_pre_verify_helper(struct cil_tree_node *node, uint32_t *finished, __attribute__((unused)) void *extra_args)
{
	int rc = SEPOL_OK;

	switch (node->flavor) {
	case CIL_MACRO: {
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_USER:
		rc = __cil_verify_user_pre_eval(node);
		break;
	case CIL_MAP_CLASS:
		rc = __cil_verify_map_class(node);
		break;
	case CIL_CLASSPERMISSION:
		rc = __cil_verify_classpermission(node);
		break;
	case CIL_USERATTRIBUTE:
	case CIL_ROLEATTRIBUTE:
	case CIL_TYPEATTRIBUTE:
	case CIL_CATSET: {
		struct cil_stack *stack;
		cil_stack_init(&stack);
		rc = cil_verify_no_self_reference(node->flavor, node->data, stack);
		cil_stack_destroy(&stack);
		break;
	}
	default:
		rc = SEPOL_OK;
		break;
	}

	return rc;
}

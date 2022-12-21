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
#include <inttypes.h>

#include <sepol/policydb/conditional.h>
#include <sepol/errcodes.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_find.h"
#include "cil_mem.h"
#include "cil_policy.h"
#include "cil_tree.h"
#include "cil_list.h"
#include "cil_symtab.h"


enum cil_statement_list {
	CIL_LIST_COMMON = 1,
	CIL_LIST_DEFAULT_USER,
	CIL_LIST_DEFAULT_ROLE,
	CIL_LIST_DEFAULT_TYPE,
	CIL_LIST_DEFAULT_RANGE,
	CIL_LIST_SENSALIAS,
	CIL_LIST_CATALIAS,
	CIL_LIST_MLSCONSTRAIN,
	CIL_LIST_MLSVALIDATETRANS,
	CIL_LIST_POLICYCAP,
	CIL_LIST_TYPEATTRIBUTE,
	CIL_LIST_ROLEATTRIBUTE,
	CIL_LIST_BOOL,
	CIL_LIST_TYPE,
	CIL_LIST_TYPEALIAS,
	CIL_LIST_ROLE,
	CIL_LIST_ROLEALLOW,
	CIL_LIST_ROLETRANSITION,
	CIL_LIST_USER,
	CIL_LIST_CONSTRAINT,
	CIL_LIST_VALIDATETRANS,
	CIL_LIST_NUM_LISTS
};

static int __cil_gather_statements_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct cil_list **lists;
	int kind = 0;

	lists = (struct cil_list **)extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_BOOLEANIF:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_COMMON:
		kind = CIL_LIST_COMMON;
		break;
	case CIL_DEFAULTUSER:
		kind = CIL_LIST_DEFAULT_USER;
		break;
	case CIL_DEFAULTROLE:
		kind = CIL_LIST_DEFAULT_ROLE;
		break;
	case CIL_DEFAULTTYPE:
		kind = CIL_LIST_DEFAULT_TYPE;
		break;
	case CIL_DEFAULTRANGE:
		kind = CIL_LIST_DEFAULT_RANGE;
		break;
	case CIL_SENSALIAS:
		kind = CIL_LIST_SENSALIAS;
		break;
	case CIL_CATALIAS:
		kind = CIL_LIST_CATALIAS;
		break;
	case CIL_MLSCONSTRAIN:
		kind = CIL_LIST_MLSCONSTRAIN;
		break;
	case CIL_MLSVALIDATETRANS:
		kind = CIL_LIST_MLSVALIDATETRANS;
		break;
	case CIL_POLICYCAP:
		kind = CIL_LIST_POLICYCAP;
		break;
	case CIL_TYPEATTRIBUTE: {
		struct cil_typeattribute *attr = node->data;
		if (strcmp(attr->datum.fqn, "cil_gen_require") != 0) {
			kind = CIL_LIST_TYPEATTRIBUTE;
		}
		break;
	}
	case CIL_ROLEATTRIBUTE: {
		struct cil_roleattribute *attr = node->data;
		if (strcmp(attr->datum.fqn, "cil_gen_require") != 0) {
			kind = CIL_LIST_ROLEATTRIBUTE;
		}
		break;
	}
	case CIL_BOOL:
		kind = CIL_LIST_BOOL;
		break;
	case CIL_TYPE:
		kind = CIL_LIST_TYPE;
		break;
	case CIL_TYPEALIAS:
		kind = CIL_LIST_TYPEALIAS;
		break;
	case CIL_ROLE: {
		struct cil_role *role = node->data;
		if (strcmp(role->datum.fqn, "object_r") != 0) {
			kind = CIL_LIST_ROLE;
		}
		break;
	}
	case CIL_ROLEALLOW:
		kind = CIL_LIST_ROLEALLOW;
		break;
	case CIL_ROLETRANSITION:
		kind = CIL_LIST_ROLETRANSITION;
		break;
	case CIL_USER:
		kind = CIL_LIST_USER;
		break;
	case CIL_CONSTRAIN:
		kind = CIL_LIST_CONSTRAINT;
		break;
	case CIL_VALIDATETRANS:
		kind = CIL_LIST_VALIDATETRANS;
		break;
	default:
		break;
	}

	if (kind > 0) {
		cil_list_append(lists[kind], node->flavor, node->data);
	}

	return SEPOL_OK;
}

static void cil_gather_statements(struct cil_tree_node *start, struct cil_list *lists[])
{
	cil_tree_walk(start, __cil_gather_statements_helper, NULL, NULL, lists);
}

static void cil_simple_rules_to_policy(FILE *out, struct cil_list *rules, const char *kind)
{
	struct cil_list_item *i1;

	cil_list_for_each(i1, rules) {
		fprintf(out, "%s %s;\n", kind, DATUM(i1->data)->fqn);
	}
}

static void cil_cats_to_policy(FILE *out, struct cil_cats *cats)
{
	const char *lead = "";
	struct cil_cat *first = NULL, *last = NULL, *cat;
	struct cil_list_item *i1;

	cil_list_for_each(i1, cats->datum_expr) {
		cat = i1->data;
		if (first == NULL) {
			first = cat;
		} else if (last == NULL) {
			if (cat->value == first->value + 1) {
				last = cat;
			} else {
				fprintf(out, "%s%s", lead, DATUM(first)->fqn);
				lead = ",";
				first = cat;
			}
		} else if (cat->value == last->value + 1) {
			last = cat;
		} else {
			fprintf(out, "%s%s", lead, DATUM(first)->fqn);
			lead = ",";
			if (last->value >= first->value + 1) {
				fprintf(out, ".");
			} else {
				fprintf(out, ",");
			}
			fprintf(out, "%s", DATUM(last)->fqn);
			first = cat;
			last = NULL;
		}
	}
	if (first) {
		fprintf(out, "%s%s", lead, DATUM(first)->fqn);
		if (last != NULL) {
			if (last->value >= first->value + 1) {
				fprintf(out, ".");
			} else {
				fprintf(out, ",");
			}
			fprintf(out, "%s", DATUM(last)->fqn);
		}
	}
}

static void cil_level_to_policy(FILE *out, struct cil_level *level)
{
	fprintf(out, "%s", DATUM(level->sens)->fqn);
	if (level->cats != NULL) {
		fprintf(out, ":");
		cil_cats_to_policy(out, level->cats);
	}
}

static int cil_levels_simple_and_equal(struct cil_level *l1, struct cil_level *l2)
{
	/* Mostly just want to detect s0 - s0 ranges */
	if (l1 == l2)
		return CIL_TRUE;

	if (l1->sens == l2->sens && (l1->cats == NULL && l2->cats == NULL))
		return CIL_TRUE;

	return CIL_FALSE;
}

static void cil_levelrange_to_policy(FILE *out, struct cil_levelrange *lvlrange)
{
	cil_level_to_policy(out, lvlrange->low);
	if (cil_levels_simple_and_equal(lvlrange->low, lvlrange->high) == CIL_FALSE) {
		fprintf(out, " - ");
		cil_level_to_policy(out, lvlrange->high);
	}
}

static void cil_context_to_policy(FILE *out, struct cil_context *context, int mls)
{
	fprintf(out, "%s:", DATUM(context->user)->fqn);
	fprintf(out, "%s:", DATUM(context->role)->fqn);
	fprintf(out, "%s", DATUM(context->type)->fqn);
	if (mls) {
		fprintf(out, ":");
		cil_levelrange_to_policy(out, context->range);
	}
}

static void cil_cond_expr_to_policy(FILE *out, struct cil_list *expr, int first)
{
	struct cil_list_item *i1 = expr->head;

	if (i1->flavor == CIL_OP) {
		enum cil_flavor op = (enum cil_flavor)(uintptr_t)i1->data;
		fprintf(out, "(");
		switch (op) {
		case CIL_NOT:
			fprintf(out, "! ");
			cil_cond_expr_to_policy(out, i1->next->data, CIL_FALSE);
			break;
		case CIL_OR:
			cil_cond_expr_to_policy(out, i1->next->data, CIL_FALSE);
			fprintf(out, " || ");
			cil_cond_expr_to_policy(out, i1->next->next->data, CIL_FALSE);
			break;
		case CIL_AND:
			cil_cond_expr_to_policy(out, i1->next->data, CIL_FALSE);
			fprintf(out, " && ");
			cil_cond_expr_to_policy(out, i1->next->next->data, CIL_FALSE);
			break;
		case CIL_XOR:
			cil_cond_expr_to_policy(out, i1->next->data, CIL_FALSE);
			fprintf(out, " ^ ");
			cil_cond_expr_to_policy(out, i1->next->next->data, CIL_FALSE);
			break;
		case CIL_EQ:
			cil_cond_expr_to_policy(out, i1->next->data, CIL_FALSE);
			fprintf(out, " == ");
			cil_cond_expr_to_policy(out, i1->next->next->data, CIL_FALSE);
			break;
		case CIL_NEQ:
			cil_cond_expr_to_policy(out, i1->next->data, CIL_FALSE);
			fprintf(out, " != ");
			cil_cond_expr_to_policy(out, i1->next->next->data, CIL_FALSE);
			break;
		default:
			fprintf(out, "???");
			break;
		}
		fprintf(out, ")");
	} else if (i1->flavor == CIL_DATUM) {
		if (first == CIL_TRUE) {
			fprintf(out, "(");
		}
		fprintf(out, "%s", DATUM(i1->data)->fqn);
		if (first == CIL_TRUE) {
			fprintf(out, ")");
		}
	} else if (i1->flavor == CIL_LIST) {
		cil_cond_expr_to_policy(out, i1->data, CIL_FALSE);
	} else {
		fprintf(out, "???");
	}
}

static size_t __cil_userattribute_len(struct cil_db *db, struct cil_userattribute *attr)
{
	ebitmap_node_t *unode;
	unsigned int i;
	size_t len = 0;

	ebitmap_for_each_positive_bit(attr->users, unode, i) {
		len += strlen(DATUM(db->val_to_user[i])->fqn);
		len++;
	}

	return len;
}

static size_t __cil_cons_leaf_operand_len(struct cil_db *db, struct cil_list_item *operand)
{
	struct cil_list_item *i1;
	enum cil_flavor flavor = operand->flavor;
	size_t len = 0;

	if (flavor == CIL_CONS_OPERAND) {
		len = 2;
	} else if (flavor == CIL_DATUM) {
		struct cil_tree_node *node = NODE(operand->data);
		if (node->flavor == CIL_USERATTRIBUTE) {
			len = __cil_userattribute_len(db, operand->data);
			len++; /* "{" */
		} else {
			len = strlen(DATUM(operand->data)->fqn);
		}
	} else if (flavor == CIL_LIST) {
		len = 1; /* "{" */
		cil_list_for_each(i1, (struct cil_list *)operand->data) {
			struct cil_tree_node *node = NODE(operand->data);
			if (node->flavor == CIL_USERATTRIBUTE) {
				len = __cil_userattribute_len(db, operand->data);
			} else {
				len += strlen(DATUM(operand->data)->fqn);
				len++; /* " " or "}" */
			}
		}
	}

	return len;
}

static size_t __cil_cons_leaf_op_len(struct cil_list_item *op)
{
	enum cil_flavor flavor = (enum cil_flavor)(uintptr_t)op->data;
	size_t len;

	switch (flavor) {
	case CIL_EQ:
		len = 4; /* " == " */
		break;
	case CIL_NEQ:
		len = 4; /* " != " */
		break;
	case CIL_CONS_DOM:
		len = 5; /* " dom " */
		break;
	case CIL_CONS_DOMBY:
		len = 7; /* " domby " */
		break;
	case CIL_CONS_INCOMP:
		len = 8; /* " incomp " */
		break;
	default:
		/* Should be impossible to be here */
		len = 5; /* " ??? " */
	}

	return len;
}

static size_t cil_cons_expr_len(struct cil_db *db, struct cil_list *cons_expr)
{
	struct cil_list_item *i1;
	enum cil_flavor op;
	size_t len;

	i1 = cons_expr->head;

	op = (enum cil_flavor)(uintptr_t)i1->data;
	switch (op) {
	case CIL_NOT:
		len = 6; /* "(not )" */
		len += cil_cons_expr_len(db, i1->next->data);
		break;
	case CIL_AND:
		len = 7; /* "( and )" */
		len += cil_cons_expr_len(db, i1->next->data);
		len += cil_cons_expr_len(db, i1->next->next->data);
		break;
	case CIL_OR:
		len = 6; /* "( or )" */
		len += cil_cons_expr_len(db, i1->next->data);
		len += cil_cons_expr_len(db, i1->next->next->data);
		break;
	default:
		len = 2; /* "()" */
		len += __cil_cons_leaf_operand_len(db, i1->next);
		len += __cil_cons_leaf_op_len(i1);
		len += __cil_cons_leaf_operand_len(db, i1->next->next);
	}

	return len;
}

static char *__cil_userattribute_to_string(struct cil_db *db, struct cil_userattribute *attr, char *new)
{
	ebitmap_node_t *unode;
	unsigned int i;
	char *str;
	size_t len;

	ebitmap_for_each_positive_bit(attr->users, unode, i) {
		str = DATUM(db->val_to_user[i])->fqn;
		len = strlen(str);
		memcpy(new, str, len);
		new += len;
		*new++ = ' ';
	}

	return new;
}

static char *__cil_cons_leaf_operand_to_string(struct cil_db *db, struct cil_list_item *operand, char *new)
{
	struct cil_list_item *i1;
	enum cil_flavor flavor = operand->flavor;
	const char *o_str;
	size_t o_len;

	if (flavor == CIL_CONS_OPERAND) {
		enum cil_flavor o_flavor = (enum cil_flavor)(uintptr_t)operand->data;
		switch (o_flavor) {
		case CIL_CONS_U1:
			o_str = "u1";
			break;
		case CIL_CONS_U2:
			o_str = "u2";
			break;
		case CIL_CONS_U3:
			o_str = "u3";
			break;
		case CIL_CONS_R1:
			o_str = "r1";
			break;
		case CIL_CONS_R2:
			o_str = "r2";
			break;
		case CIL_CONS_R3:
			o_str = "r3";
			break;
		case CIL_CONS_T1:
			o_str = "t1";
			break;
		case CIL_CONS_T2:
			o_str = "t2";
			break;
		case CIL_CONS_T3:
			o_str = "t3";
			break;
		case CIL_CONS_L1:
			o_str = "l1";
			break;
		case CIL_CONS_L2:
			o_str = "l2";
			break;
		case CIL_CONS_H1:
			o_str = "h1";
			break;
		case CIL_CONS_H2:
			o_str = "h2";
			break;
		default:
			/* Impossible */
			o_str = "??";
		}
		strcpy(new, o_str);
		new += 2;
	} else if (flavor == CIL_DATUM) {
		struct cil_tree_node *node = NODE(operand->data);
		if (node->flavor == CIL_USERATTRIBUTE) {
			*new++ = '{';
			new = __cil_userattribute_to_string(db, operand->data, new);
			new--;
			*new++ = '}';
		} else {
			o_str = DATUM(operand->data)->fqn;
			o_len = strlen(o_str);
			memcpy(new, o_str, o_len);
			new += o_len;
		}
	} else if (flavor == CIL_LIST) {
		*new++ = '{';
		cil_list_for_each(i1, (struct cil_list *)operand->data) {
			struct cil_tree_node *node = NODE(operand->data);
			if (node->flavor == CIL_USERATTRIBUTE) {
				new = __cil_userattribute_to_string(db, operand->data, new);
			} else {
				o_str = DATUM(operand->data)->fqn;
				o_len = strlen(o_str);
				memcpy(new, o_str, o_len);
				new += o_len;
				*new++ = ' ';
			}
		}
		new--;
		*new++ = '}';
	}

	return new;
}

static char *__cil_cons_leaf_op_to_string(struct cil_list_item *op, char *new)
{
	enum cil_flavor flavor = (enum cil_flavor)(uintptr_t)op->data;
	const char *op_str;
	size_t len;

	switch (flavor) {
	case CIL_EQ:
		op_str = " == ";
		len = 4;
		break;
	case CIL_NEQ:
		op_str = " != ";
		len = 4;
		break;
	case CIL_CONS_DOM:
		op_str = " dom ";
		len = 5;
		break;
	case CIL_CONS_DOMBY:
		op_str = " domby ";
		len = 7;
		break;
	case CIL_CONS_INCOMP:
		op_str = " incomp ";
		len = 8;
		break;
	default:
		/* Should be impossible to be here */
		op_str = " ??? ";
		len = 5;
	}

	strcpy(new, op_str);
	new += len;

	return new;
}

static char *__cil_cons_expr_to_string(struct cil_db *db, struct cil_list *cons_expr, char *new)
{
	struct cil_list_item *i1;
	enum cil_flavor op;

	i1 = cons_expr->head;

	op = (enum cil_flavor)(uintptr_t)i1->data;
	switch (op) {
	case CIL_NOT:
		*new++ = '(';
		strcpy(new, "not ");
		new += 4;
		new = __cil_cons_expr_to_string(db, i1->next->data, new);
		*new++ = ')';
		break;
	case CIL_AND:
		*new++ = '(';
		new = __cil_cons_expr_to_string(db, i1->next->data, new);
		strcpy(new, " and ");
		new += 5;
		new = __cil_cons_expr_to_string(db, i1->next->next->data, new);
		*new++ = ')';
		break;
	case CIL_OR:
		*new++ = '(';
		new = __cil_cons_expr_to_string(db, i1->next->data, new);
		strcpy(new, " or ");
		new += 4;
		new = __cil_cons_expr_to_string(db, i1->next->next->data, new);
		*new++ = ')';
		break;
	default:
		*new++ = '(';
		new = __cil_cons_leaf_operand_to_string(db, i1->next, new);
		new = __cil_cons_leaf_op_to_string(i1, new);
		new = __cil_cons_leaf_operand_to_string(db, i1->next->next, new);
		*new++ = ')';
	}

	return new;
}

static char *cil_cons_expr_to_string(struct cil_db *db, struct cil_list *cons_expr)
{
	char *new, *tail;
	size_t len = cil_cons_expr_len(db, cons_expr);

	new = cil_malloc(len+1);
	tail = __cil_cons_expr_to_string(db, cons_expr, new);
	*tail = '\0';

	return new;
}

static void cil_classperms_to_string(struct cil_classperms *classperms, struct cil_list *classperms_strs)
{
	struct cil_list_item *i1;
	size_t len = 0;
	char *new, *curr;

	len += strlen(DATUM(classperms->class)->fqn) + 1;
	cil_list_for_each(i1, classperms->perms) {
		len += strlen(DATUM(i1->data)->fqn) + 1;
	}
	len += 4; /* for "{ " and " }" */

	new = cil_malloc(len);
	curr = new;

	curr[len-1] = '\0';

	len = strlen(DATUM(classperms->class)->fqn);
	memcpy(curr, DATUM(classperms->class)->fqn, len);
	curr += len;
	*curr++ = ' ';

	*curr++ = '{';
	*curr++ = ' ';
	cil_list_for_each(i1, classperms->perms) {
		len = strlen(DATUM(i1->data)->fqn);
		memcpy(curr, DATUM(i1->data)->fqn, len);
		curr += len;
		*curr++ = ' ';
	}
	*curr++ = '}';

	cil_list_append(classperms_strs, CIL_STRING, new);
}

static void cil_classperms_to_strings(struct cil_list *classperms, struct cil_list *classperms_strs)
{
	struct cil_list_item *i1;

	cil_list_for_each(i1, classperms) {
		if (i1->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = i1->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				cil_classperms_to_string(cp, classperms_strs);
			} else { /* MAP */
				struct cil_list_item *i2 = NULL;
				cil_list_for_each(i2, cp->perms) {
					struct cil_perm *cmp = i2->data;
					cil_classperms_to_strings(cmp->classperms, classperms_strs);
				}
			}
		} else { /* SET */
			struct cil_classperms_set *cp_set = i1->data;
			struct cil_classpermission *cp = cp_set->set;
			cil_classperms_to_strings(cp->classperms, classperms_strs);
		}
	}
}

static void cil_class_decls_to_policy(FILE *out, struct cil_list *classorder)
{
	struct cil_list_item *i1;

	cil_list_for_each(i1, classorder) {
		fprintf(out, "class %s\n", DATUM(i1->data)->fqn);
	}
}

static void cil_sid_decls_to_policy(FILE *out, struct cil_list *sidorder)
{
	struct cil_list_item *i1;

	cil_list_for_each(i1, sidorder) {
		fprintf(out, "sid %s\n", DATUM(i1->data)->fqn);
	}
}

static void cil_commons_to_policy(FILE *out, struct cil_list *commons)
{
	struct cil_list_item *i1;
	struct cil_class* common;
	struct cil_tree_node *node;
	struct cil_tree_node *perm;

	cil_list_for_each(i1, commons) {
		common = i1->data;
		node = NODE(&common->datum);
		perm = node->cl_head;

		fprintf(out, "common %s {", common->datum.fqn);
		while (perm != NULL) {
			fprintf(out, "%s ", DATUM(perm->data)->fqn);
			perm = perm->next;
		}
		fprintf(out, "}\n");
	}
}

static void cil_classes_to_policy(FILE *out, struct cil_list *classorder)
{
	struct cil_list_item *i1;
	struct cil_class *class;
	struct cil_tree_node *node;

	cil_list_for_each(i1, classorder) {
		class = i1->data;
		node = NODE(&class->datum);

		fprintf(out, "class %s", class->datum.fqn);
		if (class->common != NULL) {
			fprintf(out, " inherits %s", class->common->datum.fqn);
		}
		if (node->cl_head != NULL) {
			struct cil_tree_node *perm = node->cl_head;
			fprintf(out, " {");
			while (perm != NULL) {
				fprintf(out, " %s", DATUM(perm->data)->fqn);
				perm = perm->next;
			}
			fprintf(out, " }");
		}
		fprintf(out, "\n");
	}
}

static void cil_defaults_to_policy(FILE *out, struct cil_list *defaults, const char *kind)
{
	struct cil_list_item *i1, *i2, *i3;
	struct cil_default *def;
	struct cil_list *class_list;

	cil_list_for_each(i1, defaults) {
		def = i1->data;
		fprintf(out, "%s {",kind);
		cil_list_for_each(i2, def->class_datums) {
			class_list = cil_expand_class(i2->data);
			cil_list_for_each(i3, class_list) {
				fprintf(out, " %s", DATUM(i3->data)->fqn);
			}
			cil_list_destroy(&class_list, CIL_FALSE);
		}
		fprintf(out, " }");
		if (def->object == CIL_DEFAULT_SOURCE) {
			fprintf(out," %s",CIL_KEY_SOURCE);
		} else if (def->object == CIL_DEFAULT_TARGET) {
			fprintf(out," %s",CIL_KEY_TARGET);
		}
		fprintf(out,";\n");
	}
}

static void cil_default_ranges_to_policy(FILE *out, struct cil_list *defaults)
{
	struct cil_list_item *i1, *i2, *i3;
	struct cil_defaultrange *def;
	struct cil_list *class_list;

	cil_list_for_each(i1, defaults) {
		def = i1->data;
		fprintf(out, "default_range {");
		cil_list_for_each(i2, def->class_datums) {
			class_list = cil_expand_class(i2->data);
			cil_list_for_each(i3, class_list) {
				fprintf(out, " %s", DATUM(i3->data)->fqn);
			}
			cil_list_destroy(&class_list, CIL_FALSE);
		}
		fprintf(out, " }");

		switch (def->object_range) {
		case CIL_DEFAULT_SOURCE_LOW:
			fprintf(out," %s %s", CIL_KEY_SOURCE, CIL_KEY_LOW);
			break;
		case CIL_DEFAULT_SOURCE_HIGH:
			fprintf(out," %s %s", CIL_KEY_SOURCE, CIL_KEY_HIGH);
			break;
		case CIL_DEFAULT_SOURCE_LOW_HIGH:
			fprintf(out," %s %s", CIL_KEY_SOURCE, CIL_KEY_LOW_HIGH);
			break;
		case CIL_DEFAULT_TARGET_LOW:
			fprintf(out," %s %s", CIL_KEY_TARGET, CIL_KEY_LOW);
			break;
		case CIL_DEFAULT_TARGET_HIGH:
			fprintf(out," %s %s", CIL_KEY_TARGET, CIL_KEY_HIGH);
			break;
		case CIL_DEFAULT_TARGET_LOW_HIGH:
			fprintf(out," %s %s", CIL_KEY_TARGET, CIL_KEY_LOW_HIGH);
			break;
		case CIL_DEFAULT_GLBLUB:
			fprintf(out," %s", CIL_KEY_GLBLUB);
			break;
		default:
			break;
		}
		fprintf(out,";\n");
	}
}

static void cil_sensitivities_to_policy(FILE *out, struct cil_list *sensorder, struct cil_list *all_aliases)
{
	struct cil_list_item *i1, *i2;
	struct cil_sens *sens;
	struct cil_list *aliases = NULL;
	struct cil_alias *alias;
	struct cil_sens *actual;
	int num_aliases;

	cil_list_for_each(i1, sensorder) {
		sens = i1->data;
		num_aliases = 0;
		cil_list_for_each(i2, all_aliases) {
			alias = i2->data;
			actual = alias->actual;
			if (sens == actual) {
				if (num_aliases == 0) {
					cil_list_init(&aliases, CIL_LIST);
				}
				cil_list_append(aliases, CIL_SENSALIAS, alias);
				num_aliases++;
			}
		}
		fprintf(out, "sensitivity %s", sens->datum.fqn);
		if (num_aliases > 0) {
			fprintf(out, " alias");
			if (num_aliases > 1) {
				fprintf(out, " {");
			}
			cil_list_for_each(i2, aliases) {
				alias = i2->data;
				fprintf(out, " %s", alias->datum.fqn);
			}
			if (num_aliases > 1) {
				fprintf(out, " }");
			}
			cil_list_destroy(&aliases, CIL_FALSE);
		}
		fprintf(out, ";\n");
	}
}

static void cil_dominance_to_policy(FILE *out, struct cil_list *sensorder)
{
	struct cil_list_item *item;
	struct cil_sens *sens;

	fprintf(out, "dominance {");
	cil_list_for_each(item, sensorder) {
		sens = item->data;
		fprintf(out, " %s", sens->datum.fqn);
	}
	fprintf(out, " }\n");
}

static void cil_categories_to_policy(FILE *out, struct cil_list *catorder, struct cil_list *all_aliases)
{
	struct cil_list_item *i1, *i2;
	struct cil_sens *cat;
	struct cil_list *aliases = NULL;
	struct cil_alias *alias;
	struct cil_sens *actual;
	int num_aliases;

	cil_list_for_each(i1, catorder) {
		cat = i1->data;
		num_aliases = 0;
		cil_list_for_each(i2, all_aliases) {
			alias = i2->data;
			actual = alias->actual;
			if (cat == actual) {
				if (num_aliases == 0) {
					cil_list_init(&aliases, CIL_LIST);
				}
				cil_list_append(aliases, CIL_CATALIAS, alias);
				num_aliases++;
			}
		}
		fprintf(out, "category %s",cat->datum.fqn);
		if (num_aliases > 0) {
			fprintf(out, " alias");
			if (num_aliases > 1) {
				fprintf(out, " { ");
			}
			cil_list_for_each(i2, aliases) {
				alias = i2->data;
				fprintf(out, " %s", alias->datum.fqn);
			}
			if (num_aliases > 1) {
				fprintf(out, " }");
			}
			cil_list_destroy(&aliases, CIL_FALSE);
		}
		fprintf(out, ";\n");
	}
}

static void cil_levels_to_policy(FILE *out, struct cil_list *sensorder)
{
	struct cil_list_item *i1, *i2;
	struct cil_sens *sens;

	cil_list_for_each(i1, sensorder) {
		sens = i1->data;
		if (sens->cats_list) {
			cil_list_for_each(i2, sens->cats_list) {
				fprintf(out, "level %s:",sens->datum.fqn);
				cil_cats_to_policy(out, i2->data);
				fprintf(out,";\n");
			}
		} else {
			fprintf(out, "level %s;\n",sens->datum.fqn);
		}
	}
}

static void cil_mlsconstrains_to_policy(FILE *out, struct cil_db *db, struct cil_list *mlsconstrains)
{
	struct cil_list_item *i1, *i2;
	struct cil_constrain *cons;
	struct cil_list *classperms_strs;
	char *cp_str;
	char *expr_str;

	cil_list_for_each(i1, mlsconstrains) {
		cons = i1->data;
		cil_list_init(&classperms_strs, CIL_LIST);
		cil_classperms_to_strings(cons->classperms, classperms_strs);
		expr_str = cil_cons_expr_to_string(db, cons->datum_expr);
		cil_list_for_each(i2, classperms_strs) {
			cp_str = i2->data;
			fprintf(out, "mlsconstrain %s %s;\n", cp_str, expr_str);
			free(cp_str);
		}
		free(expr_str);
		cil_list_destroy(&classperms_strs, CIL_FALSE);
	}
}

static void cil_validatetrans_to_policy(FILE *out, struct cil_db *db, struct cil_list *validatetrans, char *kind)
{
	struct cil_list_item *i1, *i2;
	struct cil_validatetrans *trans;
	struct cil_list *class_list;
	struct cil_class *class;
	char *expr_str;

	cil_list_for_each(i1, validatetrans) {
		trans = i1->data;
		class_list = cil_expand_class(trans->class);
		expr_str = cil_cons_expr_to_string(db, trans->datum_expr);
		cil_list_for_each(i2, class_list) {
			class = i2->data;
			fprintf(out, "%s %s %s;\n", kind, class->datum.fqn, expr_str);
		}
		free(expr_str);
		cil_list_destroy(&class_list, CIL_FALSE);
	}
}

static void cil_bools_to_policy(FILE *out, struct cil_list *bools)
{
	struct cil_list_item *i1;
	struct cil_bool *bool;
	const char *value;

	cil_list_for_each(i1, bools) {
		bool = i1->data;
		value = bool->value ? "true" : "false";
		fprintf(out, "bool %s %s;\n", bool->datum.fqn, value);
	}
}

static void cil_typealiases_to_policy(FILE *out, struct cil_list *types, struct cil_list *all_aliases)
{
	struct cil_list_item *i1, *i2;
	struct cil_type *type;
	struct cil_list *aliases = NULL;
	struct cil_alias *alias;
	struct cil_type *actual;
	int num_aliases;

	cil_list_for_each(i1, types) {
		type = i1->data;
		num_aliases = 0;
		cil_list_for_each(i2, all_aliases) {
			alias = i2->data;
			actual = alias->actual;
			if (type == actual) {
				if (num_aliases == 0) {
					cil_list_init(&aliases, CIL_LIST);
				}
				cil_list_append(aliases, CIL_TYPEALIAS, alias);
				num_aliases++;
			}
		}
		if (num_aliases > 0) {
			fprintf(out, "typealias %s alias", type->datum.fqn);
			if (num_aliases > 1) {
				fprintf(out, " {");
			}
			cil_list_for_each(i2, aliases) {
				alias = i2->data;
				fprintf(out, " %s", alias->datum.fqn);
			}
			if (num_aliases > 1) {
				fprintf(out, " }");
			}
			fprintf(out, ";\n");
			cil_list_destroy(&aliases, CIL_FALSE);
		}
	}
}

static void cil_typebounds_to_policy(FILE *out, struct cil_list *types)
{
	struct cil_list_item *i1;
	struct cil_type *child;
	struct cil_type *parent;

	cil_list_for_each(i1, types) {
		child = i1->data;
		if (child->bounds != NULL) {
			parent = child->bounds;
			fprintf(out, "typebounds %s %s;\n", parent->datum.fqn, child->datum.fqn);
		}
	}
}

static void cil_typeattributes_to_policy(FILE *out, struct cil_list *types, struct cil_list *attributes)
{
	struct cil_list_item *i1, *i2;
	struct cil_type *type;
	struct cil_typeattribute *attribute;
	int first = CIL_TRUE;

	cil_list_for_each(i1, types) {
		type = i1->data;
		cil_list_for_each(i2, attributes) {
			attribute = i2->data;
			if (!attribute->keep)
				continue;
			if (ksu_ebitmap_get_bit(attribute->types, type->value)) {
				if (first) {
					fprintf(out, "typeattribute %s %s", type->datum.fqn, attribute->datum.fqn);
					first = CIL_FALSE;
				} else {
					fprintf(out, ", %s", attribute->datum.fqn);
				}
			}
		}
		if (!first) {
			fprintf(out, ";\n");
			first = CIL_TRUE;
		}
	}
}

static void cil_xperms_to_policy(FILE *out, struct cil_permissionx *permx)
{
	ebitmap_node_t *node;
	unsigned int i, first = 0, last = 0;
	int need_first = CIL_TRUE, need_last = CIL_TRUE;
	const char *kind;

	if (permx->kind == CIL_PERMX_KIND_IOCTL) {
		kind = "ioctl";
	} else {
		kind = "???";
	}

	fprintf(out, "%s %s {", DATUM(permx->obj)->fqn, kind);

	ebitmap_for_each_positive_bit(permx->perms, node, i) {
		if (need_first == CIL_TRUE) {
			first = i;
			need_first = CIL_FALSE;
		} else if (need_last == CIL_TRUE) {
			if (i == first+1) {
				last = i;
				need_last = CIL_FALSE;
			} else {
				fprintf(out, " 0x%x", first);
				first = i;
			}
		} else if (i == last+1) {
			last = i;
		} else {
			if (last > first+1) {
				fprintf(out, " 0x%x-0x%x", first, last);
			} else {
				fprintf(out, " 0x%x 0x%x", first, last);
			}
			first = i;
			need_last = CIL_TRUE;
		}
	}
	if (need_first == CIL_FALSE) {
		if (need_last == CIL_FALSE) {
			fprintf(out, " 0x%x-0x%x", first, last);
		} else {
			fprintf(out, " 0x%x", first);
		}
	}
	fprintf(out," }");
}

static void cil_av_rulex_to_policy(FILE *out, struct cil_avrule *rule)
{
	const char *kind;
	struct cil_symtab_datum *src, *tgt;

	src = rule->src;
	tgt = rule->tgt;

	switch (rule->rule_kind) {
	case CIL_AVRULE_ALLOWED:
		kind = "allowxperm";
		break;
	case CIL_AVRULE_AUDITALLOW:
		kind = "auditallowxperm";
		break;
	case CIL_AVRULE_DONTAUDIT:
		kind = "dontauditxperm";
		break;
	case CIL_AVRULE_NEVERALLOW:
		kind = "neverallowxperm";
		break;
	default:
		kind = "???";
		break;
	}

	fprintf(out, "%s %s %s : ", kind, src->fqn, tgt->fqn);
	cil_xperms_to_policy(out, rule->perms.x.permx);
	fprintf(out, ";\n");
}

static void cil_av_rule_to_policy(FILE *out, struct cil_avrule *rule)
{
	const char *kind;
	struct cil_symtab_datum *src, *tgt;
	struct cil_list *classperms_strs;
	struct cil_list_item *i1;

	src = rule->src;
	tgt = rule->tgt;

	switch (rule->rule_kind) {
	case CIL_AVRULE_ALLOWED:
		kind = "allow";
		break;
	case CIL_AVRULE_AUDITALLOW:
		kind = "auditallow";
		break;
	case CIL_AVRULE_DONTAUDIT:
		kind = "dontaudit";
		break;
	case CIL_AVRULE_NEVERALLOW:
		kind = "neverallow";
		break;
	default:
		kind = "???";
		break;
	}

	cil_list_init(&classperms_strs, CIL_LIST);
	cil_classperms_to_strings(rule->perms.classperms, classperms_strs);
	cil_list_for_each(i1, classperms_strs) {
		char *cp_str = i1->data;
		fprintf(out, "%s %s %s : %s;\n", kind, src->fqn, tgt->fqn, cp_str);
		free(cp_str);
	}
	cil_list_destroy(&classperms_strs, CIL_FALSE);
}

static void cil_type_rule_to_policy(FILE *out, struct cil_type_rule *rule)
{
	const char *kind;
	struct cil_symtab_datum *src, *tgt, *res;
	struct cil_list *class_list;
	struct cil_list_item *i1;

	src = rule->src;
	tgt = rule->tgt;
	res = rule->result;

	switch (rule->rule_kind) {
	case CIL_TYPE_TRANSITION:
		kind = "type_transition";
		break;
	case CIL_TYPE_MEMBER:
		kind = "type_member";
		break;
	case CIL_TYPE_CHANGE:
		kind = "type_change";
		break;
	default:
		kind = "???";
		break;
	}

	class_list = cil_expand_class(rule->obj);
	cil_list_for_each(i1, class_list) {
		fprintf(out, "%s %s %s : %s %s;\n", kind, src->fqn, tgt->fqn, DATUM(i1->data)->fqn, res->fqn);
	}
	cil_list_destroy(&class_list, CIL_FALSE);
}

static void cil_nametypetransition_to_policy(FILE *out, struct cil_nametypetransition *trans)
{
	struct cil_symtab_datum *src, *tgt, *res;
	struct cil_name *name;
	struct cil_list *class_list;
	struct cil_list_item *i1;

	src = trans->src;
	tgt = trans->tgt;
	name = trans->name;
	res = trans->result;

	class_list = cil_expand_class(trans->obj);
	cil_list_for_each(i1, class_list) {
		fprintf(out, "type_transition %s %s : %s %s \"%s\";\n", src->fqn, tgt->fqn, DATUM(i1->data)->fqn, res->fqn, name->datum.fqn);
	}
	cil_list_destroy(&class_list, CIL_FALSE);
}

static void cil_rangetransition_to_policy(FILE *out, struct cil_rangetransition *trans)
{
	struct cil_symtab_datum *src, *exec;
	struct cil_list *class_list;
	struct cil_list_item *i1;

	src = trans->src;
	exec = trans->exec;

	class_list = cil_expand_class(trans->obj);
	cil_list_for_each(i1, class_list) {
		fprintf(out, "range_transition %s %s : %s ", src->fqn, exec->fqn, DATUM(i1->data)->fqn);
		cil_levelrange_to_policy(out, trans->range);
		fprintf(out, ";\n");
	}
	cil_list_destroy(&class_list, CIL_FALSE);
}

static void cil_typepermissive_to_policy(FILE *out, struct cil_typepermissive *rule)
{
	fprintf(out, "permissive %s;\n", DATUM(rule->type)->fqn);
}

struct block_te_rules_extra {
	FILE *out;
	enum cil_flavor flavor;
	uint32_t rule_kind;
};

static int __cil_block_te_rules_to_policy_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct block_te_rules_extra *args = extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_BOOLEANIF:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_AVRULE:
	case CIL_AVRULEX:
		if (args->flavor == node->flavor) {
			struct cil_avrule *rule = node->data;
			if (args->rule_kind == rule->rule_kind) {
				if (rule->is_extended) {
					cil_av_rulex_to_policy(args->out, rule);
				} else {
					cil_av_rule_to_policy(args->out, rule);
				}
			}
		}
		break;
	case CIL_TYPE_RULE:
		if (args->flavor == node->flavor) {
			struct cil_type_rule *rule = node->data;
			if (args->rule_kind == rule->rule_kind) {
				cil_type_rule_to_policy(args->out, rule);
			}
		}

		break;
	case CIL_NAMETYPETRANSITION:
		if (args->flavor == node->flavor) {
			cil_nametypetransition_to_policy(args->out, node->data);
		}
		break;
	case CIL_RANGETRANSITION:
		if (args->flavor == node->flavor) {
			cil_rangetransition_to_policy(args->out, node->data);
		}

		break;
	case CIL_TYPEPERMISSIVE:
		if (args->flavor == node->flavor) {
			cil_typepermissive_to_policy(args->out, node->data);
		}
		break;
	default:
		break;
	}

	return SEPOL_OK;
}

static void cil_block_te_rules_to_policy(FILE *out, struct cil_tree_node *start, int mls)
{
	struct block_te_rules_extra args;

	args.out = out;

	args.flavor = CIL_TYPEPERMISSIVE;
	args.rule_kind = 0;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);

	args.flavor = CIL_AVRULE;
	args.rule_kind = CIL_AVRULE_ALLOWED;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_AUDITALLOW;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_DONTAUDIT;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_NEVERALLOW;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);

	args.flavor = CIL_AVRULEX;
	args.rule_kind = CIL_AVRULE_ALLOWED;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_AUDITALLOW;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_DONTAUDIT;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_NEVERALLOW;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);

	args.flavor = CIL_TYPE_RULE;
	args.rule_kind = CIL_TYPE_TRANSITION;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_TYPE_MEMBER;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_TYPE_CHANGE;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	args.rule_kind = CIL_AVRULE_TYPE;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);

	args.flavor = CIL_NAMETYPETRANSITION;
	args.rule_kind = 0;
	cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);

	if (mls == CIL_TRUE) {
		args.flavor = CIL_RANGETRANSITION;
		args.rule_kind = 0;
		cil_tree_walk(start, __cil_block_te_rules_to_policy_helper, NULL, NULL, &args);
	}
}

struct te_rules_extra {
	FILE *out;
	int mls;
};

static int __cil_te_rules_to_policy_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct te_rules_extra *args = extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_BOOLEANIF: {
		struct cil_booleanif *bool = node->data;
		struct cil_tree_node *n;
		struct cil_condblock *cb;

		fprintf(args->out, "if ");
		cil_cond_expr_to_policy(args->out, bool->datum_expr, CIL_TRUE);
		fprintf(args->out," {\n");
		n = node->cl_head;
		cb = n != NULL ? n->data : NULL;
		if (cb && cb->flavor == CIL_CONDTRUE) {
			cil_block_te_rules_to_policy(args->out, n, args->mls);
			n = n->next;
			cb = n != NULL ? n->data : NULL;
		}
		if (cb && cb->flavor == CIL_CONDFALSE) {
			fprintf(args->out,"} else {\n");
			cil_block_te_rules_to_policy(args->out, n, args->mls);
		}
		fprintf(args->out,"}\n");
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;
}

static void cil_te_rules_to_policy(FILE *out, struct cil_tree_node *head, int mls)
{
	struct te_rules_extra args;

	args.out = out;
	args.mls = mls;

	cil_block_te_rules_to_policy(out, head, mls);
	cil_tree_walk(head, __cil_te_rules_to_policy_helper, NULL, NULL, &args);
}

static void cil_roles_to_policy(FILE *out, struct cil_list *rules)
{
	struct cil_list_item *i1;
	struct cil_role *role;

	cil_list_for_each(i1, rules) {
		role = i1->data;
		if (strcmp(role->datum.fqn,"object_r") == 0)
			continue;
		fprintf(out, "role %s;\n", role->datum.fqn);
	}
}

static void cil_role_types_to_policy(FILE *out, struct cil_list *roles, struct cil_list *types)
{
	struct cil_list_item *i1, *i2;
	struct cil_role *role;
	struct cil_type *type;
	int first = CIL_TRUE;

	cil_list_for_each(i1, roles) {
		role = i1->data;
		if (strcmp(role->datum.fqn,"object_r") == 0)
			continue;
		if (role->types) {
			cil_list_for_each(i2, types) {
				type = i2->data;
				if (ksu_ebitmap_get_bit(role->types, type->value)) {
					if (first) {
						fprintf(out, "role %s types { %s", role->datum.fqn, type->datum.fqn);
						first = CIL_FALSE;
					} else {
						fprintf(out, " %s", type->datum.fqn);
					}
				}
			}
			if (!first) {
				fprintf(out, " }");
				first = CIL_TRUE;
			}
			fprintf(out, ";\n");
		}
	}
}

static void cil_roleattributes_to_policy(FILE *out, struct cil_list *roles, struct cil_list *attributes)
{
	struct cil_list_item *i1, *i2;
	struct cil_role *role;
	struct cil_roleattribute *attribute;
	int first = CIL_TRUE;

	cil_list_for_each(i1, roles) {
		role = i1->data;
		if (strcmp(role->datum.fqn,"object_r") == 0)
			continue;
		cil_list_for_each(i2, attributes) {
			attribute = i2->data;
			if (ksu_ebitmap_get_bit(attribute->roles, role->value)) {
				if (first) {
					fprintf(out, "roleattribute %s %s", role->datum.fqn, attribute->datum.fqn);
					first = CIL_FALSE;
				} else {
					fprintf(out, ", %s", attribute->datum.fqn);
				}
			}
		}
		if (!first) {
			fprintf(out, ";\n");
			first = CIL_TRUE;
		}
	}
}

static void cil_roleallows_to_policy(FILE *out, struct cil_list *roleallows)
{
	struct cil_list_item *i1;
	struct cil_roleallow *allow;

	cil_list_for_each(i1, roleallows) {
		allow = i1->data;
		fprintf(out, "allow %s %s;\n", DATUM(allow->src)->fqn, DATUM(allow->tgt)->fqn);
	}
}

static void cil_roletransitions_to_policy(FILE *out, struct cil_list *roletransitions)
{
	struct cil_list_item *i1, *i2;
	struct cil_list *class_list;
	struct cil_roletransition *trans;


	cil_list_for_each(i1, roletransitions) {
		trans = i1->data;
		class_list = cil_expand_class(trans->obj);
		cil_list_for_each(i2, class_list) {
			fprintf(out, "role_transition %s %s : %s %s;\n", DATUM(trans->src)->fqn, DATUM(trans->tgt)->fqn, DATUM(i2->data)->fqn, DATUM(trans->result)->fqn);
		}
		cil_list_destroy(&class_list, CIL_FALSE);
	}
}

static void cil_users_to_policy(FILE *out, int mls, struct cil_list *users, struct cil_list *all_roles)
{
	struct cil_list_item *i1, *i2;
	struct cil_user *user;
	struct cil_list *roles = NULL;
	struct cil_role *role;
	int num_roles;

	cil_list_for_each(i1, users) {
		user = i1->data;
		num_roles = 0;
		fprintf(out, "user %s",user->datum.fqn);
		cil_list_for_each(i2, all_roles) {
			role = i2->data;
			if (ksu_ebitmap_get_bit(user->roles, role->value)) {
				if (num_roles == 0) {
					cil_list_init(&roles, CIL_LIST);
				}
				cil_list_append(roles, CIL_ROLE, role);
				num_roles++;
			}
		}
		if (num_roles > 0) {
			fprintf(out, " roles");
			if (num_roles > 1) {
				fprintf(out, " {");
			}
			cil_list_for_each(i2, roles) {
				role = i2->data;
				fprintf(out, " %s", role->datum.fqn);
			}
			if (num_roles > 1) {
				fprintf(out, " }");
			}
			cil_list_destroy(&roles, CIL_FALSE);
		}

		if (mls == CIL_TRUE && user->dftlevel != NULL) {
			fprintf(out, " level ");
			cil_level_to_policy(out, user->dftlevel);
		}

		if (mls == CIL_TRUE && user->range != NULL) {
			fprintf(out, " range ");
			cil_levelrange_to_policy(out, user->range);
		}

		fprintf(out,";\n");
	}
}

static void cil_constrains_to_policy(FILE *out, struct cil_db *db, struct cil_list *constrains)
{
	struct cil_list_item *i1, *i2;
	struct cil_constrain *cons;
	struct cil_list *classperms_strs;
	char *cp_str;
	char *expr_str;

	cil_list_for_each(i1, constrains) {
		cons = i1->data;
		cil_list_init(&classperms_strs, CIL_LIST);
		cil_classperms_to_strings(cons->classperms, classperms_strs);
		expr_str = cil_cons_expr_to_string(db, cons->datum_expr);
		cil_list_for_each(i2, classperms_strs) {
			cp_str = i2->data;
			fprintf(out, "constrain %s %s;\n",cp_str, expr_str);
			free(cp_str);
		}
		free(expr_str);
		cil_list_destroy(&classperms_strs, CIL_FALSE);
	}
}

static void cil_sid_contexts_to_policy(FILE *out, struct cil_list *sids, int mls)
{
	struct cil_list_item *i1;
	struct cil_sid *sid;

	cil_list_for_each(i1, sids) {
		sid = i1->data;
		if (sid->context) {
			fprintf(out, "sid %s ", sid->datum.fqn);
			cil_context_to_policy(out, sid->context, mls);
			fprintf(out,"\n");
		}
	}
}

static void cil_fsuses_to_policy(FILE *out, struct cil_sort *fsuses, int mls)
{
	unsigned i;
	struct cil_fsuse *fsuse;

	for (i=0; i<fsuses->count; i++) {
		fsuse = fsuses->array[i];
		if (fsuse->type == CIL_FSUSE_XATTR) {
			fprintf(out, "fs_use_xattr %s ", fsuse->fs_str);
			cil_context_to_policy(out, fsuse->context, mls);
			fprintf(out,";\n");
		}
	}

	for (i=0; i<fsuses->count; i++) {
		fsuse = fsuses->array[i];
		if (fsuse->type == CIL_FSUSE_TASK) {
			fprintf(out, "fs_use_task %s ", fsuse->fs_str);
			cil_context_to_policy(out, fsuse->context, mls);
			fprintf(out,";\n");
		}
	}

	for (i=0; i<fsuses->count; i++) {
		fsuse = fsuses->array[i];
		if (fsuse->type == CIL_FSUSE_TRANS) {
			fprintf(out, "fs_use_trans %s ", fsuse->fs_str);
			cil_context_to_policy(out, fsuse->context, mls);
			fprintf(out,";\n");
		}
	}
}

static void cil_genfscons_to_policy(FILE *out, struct cil_sort *genfscons, int mls)
{
	unsigned i;
	struct cil_genfscon *genfscon;

	for (i=0; i<genfscons->count; i++) {
		genfscon = genfscons->array[i];
		fprintf(out, "genfscon %s %s ", genfscon->fs_str, genfscon->path_str);
		cil_context_to_policy(out, genfscon->context, mls);
		fprintf(out, "\n");
	}
}

static void cil_ibpkeycons_to_policy(FILE *out, struct cil_sort *ibpkeycons, int mls)
{
	uint32_t i = 0;

	for (i = 0; i < ibpkeycons->count; i++) {
		struct cil_ibpkeycon *ibpkeycon = (struct cil_ibpkeycon *)ibpkeycons->array[i];

		fprintf(out, "ibpkeycon %s ", ibpkeycon->subnet_prefix_str);
		fprintf(out, "%d ", ibpkeycon->pkey_low);
		fprintf(out, "%d ", ibpkeycon->pkey_high);
		cil_context_to_policy(out, ibpkeycon->context, mls);
		fprintf(out, "\n");
	}
}

static void cil_ibendportcons_to_policy(FILE *out, struct cil_sort *ibendportcons, int mls)
{
	uint32_t i;

	for (i = 0; i < ibendportcons->count; i++) {
		struct cil_ibendportcon *ibendportcon = (struct cil_ibendportcon *)ibendportcons->array[i];

		fprintf(out, "ibendportcon %s ", ibendportcon->dev_name_str);
		fprintf(out, "%u ", ibendportcon->port);
		cil_context_to_policy(out, ibendportcon->context, mls);
		fprintf(out, "\n");
	}
}

static void cil_portcons_to_policy(FILE *out, struct cil_sort *portcons, int mls)
{
	unsigned i;
	struct cil_portcon *portcon;

	for (i=0; i<portcons->count; i++) {
		portcon = portcons->array[i];
		fprintf(out, "portcon ");
		if (portcon->proto == CIL_PROTOCOL_UDP) {
			fprintf(out, "udp ");
		} else if (portcon->proto == CIL_PROTOCOL_TCP) {
			fprintf(out, "tcp ");
		} else if (portcon->proto == CIL_PROTOCOL_DCCP) {
			fprintf(out, "dccp ");
		} else if (portcon->proto == CIL_PROTOCOL_SCTP) {
			fprintf(out, "sctp ");
		}
		if (portcon->port_low == portcon->port_high) {
			fprintf(out, "%d ", portcon->port_low);
		} else {
			fprintf(out, "%d-%d ", portcon->port_low, portcon->port_high);
		}
		cil_context_to_policy(out, portcon->context, mls);
		fprintf(out, "\n");
	}
}

static void cil_netifcons_to_policy(FILE *out, struct cil_sort *netifcons, int mls)
{
	unsigned i;
	struct cil_netifcon *netifcon;

	for (i=0; i<netifcons->count; i++) {
		netifcon = netifcons->array[i];
		fprintf(out, "netifcon %s ", netifcon->interface_str);
		cil_context_to_policy(out, netifcon->if_context, mls);
		fprintf(out, " ");
		cil_context_to_policy(out, netifcon->packet_context, mls);
		fprintf(out, "\n");
	}
}

static void cil_nodecons_to_policy(FILE *out, struct cil_sort *nodecons, int mls)
{
	unsigned i;
	struct cil_nodecon *nodecon;
	char *addr, *mask;

	for (i=0; i<nodecons->count; i++) {
		nodecon = nodecons->array[i];
		fprintf(out, "nodecon ");

		if (nodecon->addr->family == AF_INET) {
			errno = 0;
			addr = cil_malloc(INET_ADDRSTRLEN);
			inet_ntop(nodecon->addr->family, &nodecon->addr->ip.v4, addr, INET_ADDRSTRLEN);
			if (errno == 0) {
				fprintf(out, "%s ",addr);
			} else {
				fprintf(out, "[INVALID] ");
			}
			free(addr);

			errno = 0;
			mask = cil_malloc(INET_ADDRSTRLEN);
			inet_ntop(nodecon->mask->family, &nodecon->mask->ip.v4, mask, INET_ADDRSTRLEN);
			if (errno == 0) {
				fprintf(out, "%s ",mask);
			} else {
				fprintf(out, "[INVALID] ");
			}
			free(mask);
		} else {
			errno = 0;
			addr = cil_malloc(INET6_ADDRSTRLEN);
			inet_ntop(nodecon->addr->family, &nodecon->addr->ip.v6, addr, INET6_ADDRSTRLEN);
			if (errno == 0) {
				fprintf(out, "%s ",addr);
			} else {
				fprintf(out, "[INVALID] ");
			}
			free(addr);

			errno = 0;
			mask = cil_malloc(INET6_ADDRSTRLEN);
			inet_ntop(nodecon->mask->family, &nodecon->mask->ip.v6, mask, INET6_ADDRSTRLEN);
			if (errno == 0) {
				fprintf(out, "%s ",mask);
			} else {
				fprintf(out, "[INVALID] ");
			}
			free(mask);
		}

		cil_context_to_policy(out, nodecon->context, mls);
		fprintf(out, "\n");
	}
}

static void cil_pirqcons_to_policy(FILE *out, struct cil_sort *pirqcons, int mls)
{
	unsigned i;
	struct cil_pirqcon *pirqcon;

	for (i = 0; i<pirqcons->count; i++) {
		pirqcon = pirqcons->array[i];
		fprintf(out, "pirqcon %d ", pirqcon->pirq);
		cil_context_to_policy(out, pirqcon->context, mls);
		fprintf(out, ";\n");
	}
}

static void cil_iomemcons_to_policy(FILE *out, struct cil_sort *iomemcons, int mls)
{
	unsigned i;
	struct cil_iomemcon *iomemcon;

	for (i = 0; i<iomemcons->count; i++) {
		iomemcon = iomemcons->array[i];
		if (iomemcon->iomem_low == iomemcon->iomem_high) {
			fprintf(out, "iomemcon %"PRIx64" ", iomemcon->iomem_low);
		} else {
			fprintf(out, "iomemcon %"PRIx64"-%"PRIx64" ", iomemcon->iomem_low, iomemcon->iomem_high);
		}
		cil_context_to_policy(out, iomemcon->context, mls);
		fprintf(out, ";\n");
	}
}

static void cil_ioportcons_to_policy(FILE *out, struct cil_sort *ioportcons, int mls)
{
	unsigned i;
	struct cil_ioportcon *ioportcon;

	for (i = 0; i < ioportcons->count; i++) {
		ioportcon = ioportcons->array[i];
		fprintf(out, "ioportcon 0x%x-0x%x ", ioportcon->ioport_low, ioportcon->ioport_high);
		cil_context_to_policy(out, ioportcon->context, mls);
		fprintf(out, ";\n");
	}
}

static void cil_pcidevicecons_to_policy(FILE *out, struct cil_sort *pcidevicecons, int mls)
{
	unsigned i;
	struct cil_pcidevicecon *pcidevicecon;

	for (i = 0; i < pcidevicecons->count; i++) {
		pcidevicecon = pcidevicecons->array[i];
		fprintf(out, "pcidevicecon 0x%x ", pcidevicecon->dev);
		cil_context_to_policy(out, pcidevicecon->context, mls);
		fprintf(out, ";\n");
	}
}

static void cil_devicetreecons_to_policy(FILE *out, struct cil_sort *devicetreecons, int mls)
{
	unsigned i;
	struct cil_devicetreecon *devicetreecon;

	for (i = 0; i < devicetreecons->count; i++) {
		devicetreecon = devicetreecons->array[i];
		fprintf(out, "devicetreecon %s ", devicetreecon->path);
		cil_context_to_policy(out, devicetreecon->context, mls);
		fprintf(out, ";\n");
	}
}

void cil_gen_policy(FILE *out, struct cil_db *db)
{
	unsigned i;
	struct cil_tree_node *head = db->ast->root;
	struct cil_list *lists[CIL_LIST_NUM_LISTS];

	for (i=0; i<CIL_LIST_NUM_LISTS; i++) {
		cil_list_init(&lists[i], CIL_LIST);
	}

	cil_gather_statements(head, lists);

	cil_class_decls_to_policy(out, db->classorder);

	cil_sid_decls_to_policy(out, db->sidorder);

	cil_commons_to_policy(out, lists[CIL_LIST_COMMON]);
	cil_classes_to_policy(out, db->classorder);

	cil_defaults_to_policy(out, lists[CIL_LIST_DEFAULT_USER], "default_user");
	cil_defaults_to_policy(out, lists[CIL_LIST_DEFAULT_ROLE], "default_role");
	cil_defaults_to_policy(out, lists[CIL_LIST_DEFAULT_TYPE], "default_type");

	if (db->mls == CIL_TRUE) {
		cil_default_ranges_to_policy(out, lists[CIL_LIST_DEFAULT_RANGE]);
		cil_sensitivities_to_policy(out, db->sensitivityorder, lists[CIL_LIST_SENSALIAS]);
		cil_dominance_to_policy(out, db->sensitivityorder);
		cil_categories_to_policy(out, db->catorder, lists[CIL_LIST_CATALIAS]);
		cil_levels_to_policy(out, db->sensitivityorder);
		cil_mlsconstrains_to_policy(out, db, lists[CIL_LIST_MLSCONSTRAIN]);
		cil_validatetrans_to_policy(out, db, lists[CIL_LIST_MLSVALIDATETRANS], CIL_KEY_MLSVALIDATETRANS);
	}

	cil_simple_rules_to_policy(out, lists[CIL_LIST_POLICYCAP], CIL_KEY_POLICYCAP);

	cil_simple_rules_to_policy(out, lists[CIL_LIST_TYPEATTRIBUTE], "attribute");
	cil_simple_rules_to_policy(out, lists[CIL_LIST_ROLEATTRIBUTE], "attribute_role");

	cil_bools_to_policy(out, lists[CIL_LIST_BOOL]);

	cil_simple_rules_to_policy(out, lists[CIL_LIST_TYPE], "type");
	cil_typealiases_to_policy(out, lists[CIL_LIST_TYPE], lists[CIL_LIST_TYPEALIAS]);
	cil_typebounds_to_policy(out, lists[CIL_LIST_TYPE]);
	cil_typeattributes_to_policy(out, lists[CIL_LIST_TYPE], lists[CIL_LIST_TYPEATTRIBUTE]);
	cil_te_rules_to_policy(out, head, db->mls);

	cil_roles_to_policy(out, lists[CIL_LIST_ROLE]);
	cil_role_types_to_policy(out, lists[CIL_LIST_ROLE], lists[CIL_LIST_TYPE]);
	cil_roleattributes_to_policy(out, lists[CIL_LIST_ROLE], lists[CIL_LIST_ROLEATTRIBUTE]);
	cil_roleallows_to_policy(out, lists[CIL_LIST_ROLEALLOW]);
	cil_roletransitions_to_policy(out, lists[CIL_LIST_ROLETRANSITION]);

	cil_users_to_policy(out, db->mls, lists[CIL_LIST_USER], lists[CIL_LIST_ROLE]);

	cil_constrains_to_policy(out, db, lists[CIL_LIST_CONSTRAINT]);
	cil_validatetrans_to_policy(out, db, lists[CIL_LIST_VALIDATETRANS], CIL_KEY_VALIDATETRANS);

	cil_sid_contexts_to_policy(out, db->sidorder, db->mls);
	cil_fsuses_to_policy(out, db->fsuse, db->mls);
	cil_genfscons_to_policy(out, db->genfscon, db->mls);
	cil_portcons_to_policy(out, db->portcon, db->mls);
	cil_netifcons_to_policy(out, db->netifcon, db->mls);
	cil_ibpkeycons_to_policy(out, db->ibpkeycon, db->mls);
	cil_ibendportcons_to_policy(out, db->ibendportcon, db->mls);
	cil_nodecons_to_policy(out, db->nodecon, db->mls);
	cil_pirqcons_to_policy(out, db->pirqcon, db->mls);
	cil_iomemcons_to_policy(out, db->iomemcon, db->mls);
	cil_ioportcons_to_policy(out, db->ioportcon, db->mls);
	cil_pcidevicecons_to_policy(out, db->pcidevicecon, db->mls);
	cil_devicetreecons_to_policy(out, db->devicetreecon, db->mls);

	for (i=0; i<CIL_LIST_NUM_LISTS; i++) {
		cil_list_destroy(&lists[i], CIL_FALSE);
	}

}

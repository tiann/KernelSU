/*
 * Author : Stephen Smalley, <sds@tycho.nsa.gov>
 */
/*
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 *
 *	Support for enhanced MLS infrastructure.
 *
 * Updated: Frank Mayer <mayerf@tresys.com>
 *          and Karl MacMillan <kmacmillan@tresys.com>
 *
 * 	Added conditional policy language extensions
 *
 * Updated: Red Hat, Inc.  James Morris <jmorris@redhat.com>
 *
 *      Fine-grained netlink support
 *      IPv6 support
 *      Code cleanup
 *
 * Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
 * Copyright (C) 2003 - 2004 Tresys Technology, LLC
 * Copyright (C) 2003 - 2004 Red Hat, Inc.
 * Copyright (C) 2017 Mellanox Technologies Inc.
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

/* FLASK */

/*
 * Implementation of the security services.
 */

/* Initial sizes malloc'd for sepol_compute_av_reason_buffer() support */
#define REASON_BUF_SIZE 2048
#define EXPR_BUF_SIZE 1024
#define STACK_LEN 32

// #include <stdlib.h>
#include <linux/types.h>
#include <linux/socket.h>
// #include <netinet/in.h>
#include <linux/in.h>
// #include <arpa/inet.h>
#include <linux/inet.h>

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/sidtab.h>
#include <sepol/policydb/services.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/util.h>
#include <sepol/sepol.h>

#include "debug.h"
#include "private.h"
#include "context.h"
#include "mls.h"
#include "flask.h"

#ifndef BUG
#define BUG() do { ERR(NULL, "Badness at %s:%d", __FILE__, __LINE__); } while (0)
#endif

static int selinux_enforcing = 1;

static sidtab_t mysidtab, *sidtab = &mysidtab;
static policydb_t mypolicydb, *policydb = &mypolicydb;

/* Used by sepol_compute_av_reason_buffer() to keep track of entries */
static int reason_buf_used;
static int reason_buf_len;

/* Stack services for RPN to infix conversion. */
static char **stack;
static int stack_len;
static int next_stack_entry;

static void push(char *expr_ptr)
{
	if (next_stack_entry >= stack_len) {
		char **new_stack;
		int new_stack_len;

		if (stack_len == 0)
			new_stack_len = STACK_LEN;
		else
			new_stack_len = stack_len * 2;

		new_stack = reallocarray(stack, new_stack_len, sizeof(*stack));
		if (!new_stack) {
			ERR(NULL, "unable to allocate stack space");
			return;
		}
		stack_len = new_stack_len;
		stack = new_stack;
	}
	stack[next_stack_entry] = expr_ptr;
	next_stack_entry++;
}

static char *pop(void)
{
	next_stack_entry--;
	if (next_stack_entry < 0) {
		next_stack_entry = 0;
		ERR(NULL, "pop called with no stack entries");
		return NULL;
	}
	return stack[next_stack_entry];
}
/* End Stack services */

int sepol_set_sidtab(sidtab_t * s)
{
	sidtab = s;
	return 0;
}

int sepol_set_policydb(policydb_t * p)
{
	policydb = p;
	return 0;
}

int sepol_set_policydb_from_file(FILE * fp)
{
	struct policy_file pf;

	policy_file_init(&pf);
	pf.fp = fp;
	pf.type = PF_USE_STDIO;
	if (mypolicydb.policy_type)
		ksu_policydb_destroy(&mypolicydb);
	if (policydb_init(&mypolicydb)) {
		ERR(NULL, "Out of memory!");
		return -1;
	}
	if (ksu_policydb_read(&mypolicydb, &pf, 0)) {
		ksu_policydb_destroy(&mypolicydb);
		ERR(NULL, "can't read binary policy: %m");
		return -1;
	}
	policydb = &mypolicydb;
	return sepol_sidtab_init(sidtab);
}

/*
 * The largest sequence number that has been used when
 * providing an access decision to the access vector cache.
 * The sequence number only changes when a policy change
 * occurs.
 */
static uint32_t latest_granting = 0;

/*
 * cat_expr_buf adds a string to an expression buffer and handles
 * realloc's if buffer is too small. The array of expression text
 * buffer pointers and its counter are globally defined here as
 * constraint_expr_eval_reason() sets them up and cat_expr_buf
 * updates the e_buf pointer.
 */
static int expr_counter;
static char **expr_list;
static int expr_buf_used;
static int expr_buf_len;

static void cat_expr_buf(char *e_buf, const char *string)
{
	int len, new_buf_len;
	char *p, *new_buf;

	while (1) {
		p = e_buf + expr_buf_used;
		len = snprintf(p, expr_buf_len - expr_buf_used, "%s", string);
		if (len < 0 || len >= expr_buf_len - expr_buf_used) {
			new_buf_len = expr_buf_len + EXPR_BUF_SIZE;
			new_buf = realloc(e_buf, new_buf_len);
			if (!new_buf) {
				ERR(NULL, "failed to realloc expr buffer");
				return;
			}
			/* Update new ptr in expr list and locally + new len */
			expr_list[expr_counter] = new_buf;
			e_buf = new_buf;
			expr_buf_len = new_buf_len;
		} else {
			expr_buf_used += len;
			return;
		}
	}
}

/*
 * If the POLICY_KERN version is >= POLICYDB_VERSION_CONSTRAINT_NAMES,
 * then for 'types' only, read the types_names->types list as it will
 * contain a list of types and attributes that were defined in the
 * policy source.
 * For user and role plus types (for policy vers <
 * POLICYDB_VERSION_CONSTRAINT_NAMES) just read the e->names list.
 */
static void get_name_list(constraint_expr_t *e, int type,
							const char *src, const char *op, int failed)
{
	ebitmap_t *types;
	int rc = 0;
	unsigned int i;
	char tmp_buf[128];
	int counter = 0;

	if (policydb->policy_type == POLICY_KERN &&
			policydb->policyvers >= POLICYDB_VERSION_CONSTRAINT_NAMES &&
			type == CEXPR_TYPE)
		types = &e->type_names->types;
	else
		types = &e->names;

	/* Find out how many entries */
	for (i = ebitmap_startbit(types); i < ebitmap_length(types); i++) {
		rc = ksu_ebitmap_get_bit(types, i);
		if (rc == 0)
			continue;
		else
			counter++;
	}
	snprintf(tmp_buf, sizeof(tmp_buf), "(%s%s", src, op);
	cat_expr_buf(expr_list[expr_counter], tmp_buf);

	if (counter == 0)
		cat_expr_buf(expr_list[expr_counter], "<empty_set> ");
	if (counter > 1)
		cat_expr_buf(expr_list[expr_counter], " {");
	if (counter >= 1) {
		for (i = ebitmap_startbit(types); i < ebitmap_length(types); i++) {
			rc = ksu_ebitmap_get_bit(types, i);
			if (rc == 0)
				continue;

			/* Collect entries */
			switch (type) {
			case CEXPR_USER:
				snprintf(tmp_buf, sizeof(tmp_buf), " %s",
							policydb->p_user_val_to_name[i]);
				break;
			case CEXPR_ROLE:
				snprintf(tmp_buf, sizeof(tmp_buf), " %s",
							policydb->p_role_val_to_name[i]);
				break;
			case CEXPR_TYPE:
				snprintf(tmp_buf, sizeof(tmp_buf), " %s",
							policydb->p_type_val_to_name[i]);
				break;
			}
			cat_expr_buf(expr_list[expr_counter], tmp_buf);
		}
	}
	if (counter > 1)
		cat_expr_buf(expr_list[expr_counter], " }");
	if (failed)
		cat_expr_buf(expr_list[expr_counter], " -Fail-) ");
	else
		cat_expr_buf(expr_list[expr_counter], ") ");

	return;
}

static void msgcat(const char *src, const char *tgt, const char *op, int failed)
{
	char tmp_buf[128];
	if (failed)
		snprintf(tmp_buf, sizeof(tmp_buf), "(%s %s %s -Fail-) ",
				src, op, tgt);
	else
		snprintf(tmp_buf, sizeof(tmp_buf), "(%s %s %s) ",
				src, op, tgt);
	cat_expr_buf(expr_list[expr_counter], tmp_buf);
}

/* Returns a buffer with class, statement type and permissions */
static char *get_class_info(sepol_security_class_t tclass,
							constraint_node_t *constraint,
							context_struct_t *xcontext)
{
	constraint_expr_t *e;
	int mls, state_num;
	/* Determine statement type */
	const char *statements[] = {
		"constrain ",			/* 0 */
		"mlsconstrain ",		/* 1 */
		"validatetrans ",		/* 2 */
		"mlsvalidatetrans ",	/* 3 */
		0 };
	size_t class_buf_len = 0;
	size_t new_class_buf_len;
	size_t buf_used;
	int len;
	char *class_buf = NULL, *p;
	char *new_class_buf = NULL;

	/* Find if MLS statement or not */
	mls = 0;
	for (e = constraint->expr; e; e = e->next) {
		if (e->attr >= CEXPR_L1L2) {
			mls = 1;
			break;
		}
	}

	if (xcontext == NULL)
		state_num = mls + 0;
	else
		state_num = mls + 2;

	while (1) {
		new_class_buf_len = class_buf_len + EXPR_BUF_SIZE;
		new_class_buf = realloc(class_buf, new_class_buf_len);
		if (!new_class_buf) {
			free(class_buf);
			return NULL;
		}
		class_buf_len = new_class_buf_len;
		class_buf = new_class_buf;
		buf_used = 0;
		p = class_buf;

		/* Add statement type */
		len = snprintf(p, class_buf_len - buf_used, "%s", statements[state_num]);
		if (len < 0 || (size_t)len >= class_buf_len - buf_used)
			continue;

		/* Add class entry */
		p += len;
		buf_used += len;
		len = snprintf(p, class_buf_len - buf_used, "%s ",
				policydb->p_class_val_to_name[tclass - 1]);
		if (len < 0 || (size_t)len >= class_buf_len - buf_used)
			continue;

		/* Add permission entries (validatetrans does not have perms) */
		p += len;
		buf_used += len;
		if (state_num < 2) {
			len = snprintf(p, class_buf_len - buf_used, "{%s } (",
			sepol_av_to_string(policydb, tclass,
				constraint->permissions));
		} else {
			len = snprintf(p, class_buf_len - buf_used, "(");
		}
		if (len < 0 || (size_t)len >= class_buf_len - buf_used)
			continue;
		break;
	}
	return class_buf;
}

/*
 * Modified version of constraint_expr_eval that will process each
 * constraint as before but adds the information to text buffers that
 * will hold various components. The expression will be in RPN format,
 * therefore there is a stack based RPN to infix converter to produce
 * the final readable constraint.
 *
 * Return the boolean value of a constraint expression
 * when it is applied to the specified source and target
 * security contexts.
 *
 * xcontext is a special beast...  It is used by the validatetrans rules
 * only.  For these rules, scontext is the context before the transition,
 * tcontext is the context after the transition, and xcontext is the
 * context of the process performing the transition.  All other callers
 * of constraint_expr_eval_reason should pass in NULL for xcontext.
 *
 * This function will also build a buffer as the constraint is processed
 * for analysis. If this option is not required, then:
 *      'tclass' should be '0' and r_buf MUST be NULL.
 */
static int constraint_expr_eval_reason(context_struct_t *scontext,
				context_struct_t *tcontext,
				context_struct_t *xcontext,
				sepol_security_class_t tclass,
				constraint_node_t *constraint,
				char **r_buf,
				unsigned int flags)
{
	uint32_t val1, val2;
	context_struct_t *c;
	role_datum_t *r1, *r2;
	mls_level_t *l1, *l2;
	constraint_expr_t *e;
	int s[CEXPR_MAXDEPTH];
	int sp = -1;
	char tmp_buf[128];

/*
 * Define the s_t_x_num values that make up r1, t2 etc. in text strings
 * Set 1 = source, 2 = target, 3 = xcontext for validatetrans
 */
#define SOURCE  1
#define TARGET  2
#define XTARGET 3

	int s_t_x_num;

	/* Set 0 = fail, u = CEXPR_USER, r = CEXPR_ROLE, t = CEXPR_TYPE */
	int u_r_t = 0;

	char *src = NULL;
	char *tgt = NULL;
	int rc = 0, x;
	char *class_buf = NULL;
	int expr_list_len = 0;
	int expr_count;

	/*
	 * The array of expression answer buffer pointers and counter.
	 */
	char **answer_list = NULL;
	int answer_counter = 0;

	/* The pop operands */
	char *a;
	char *b;
	int a_len, b_len;

	class_buf = get_class_info(tclass, constraint, xcontext);
	if (!class_buf) {
		ERR(NULL, "failed to allocate class buffer");
		return -ENOMEM;
	}

	/* Original function but with buffer support */
	expr_counter = 0;
	expr_list = NULL;
	for (e = constraint->expr; e; e = e->next) {
		/* Allocate a stack to hold expression buffer entries */
		if (expr_counter >= expr_list_len) {
			char **new_expr_list;
			int new_expr_list_len;

			if (expr_list_len == 0)
				new_expr_list_len = STACK_LEN;
			else
				new_expr_list_len = expr_list_len * 2;

			new_expr_list = reallocarray(expr_list,
					new_expr_list_len, sizeof(*expr_list));
			if (!new_expr_list) {
				ERR(NULL, "failed to allocate expr buffer stack");
				rc = -ENOMEM;
				goto out;
			}
			expr_list_len = new_expr_list_len;
			expr_list = new_expr_list;
		}

		/*
		 * malloc a buffer to store each expression text component. If
		 * buffer is too small cat_expr_buf() will realloc extra space.
		 */
		expr_buf_len = EXPR_BUF_SIZE;
		expr_list[expr_counter] = malloc(expr_buf_len);
		if (!expr_list[expr_counter]) {
			ERR(NULL, "failed to allocate expr buffer");
			rc = -ENOMEM;
			goto out;
		}
		expr_buf_used = 0;

		/* Now process each expression of the constraint */
		switch (e->expr_type) {
		case CEXPR_NOT:
			if (sp < 0) {
				BUG();
				rc = -EINVAL;
				goto out;
			}
			s[sp] = !s[sp];
			cat_expr_buf(expr_list[expr_counter], "not");
			break;
		case CEXPR_AND:
			if (sp < 1) {
				BUG();
				rc = -EINVAL;
				goto out;
			}
			sp--;
			s[sp] &= s[sp + 1];
			cat_expr_buf(expr_list[expr_counter], "and");
			break;
		case CEXPR_OR:
			if (sp < 1) {
				BUG();
				rc = -EINVAL;
				goto out;
			}
			sp--;
			s[sp] |= s[sp + 1];
			cat_expr_buf(expr_list[expr_counter], "or");
			break;
		case CEXPR_ATTR:
			if (sp == (CEXPR_MAXDEPTH - 1))
				goto out;

			switch (e->attr) {
			case CEXPR_USER:
				val1 = scontext->user;
				val2 = tcontext->user;
				free(src); src = strdup("u1");
				free(tgt); tgt = strdup("u2");
				break;
			case CEXPR_TYPE:
				val1 = scontext->type;
				val2 = tcontext->type;
				free(src); src = strdup("t1");
				free(tgt); tgt = strdup("t2");
				break;
			case CEXPR_ROLE:
				val1 = scontext->role;
				val2 = tcontext->role;
				r1 = policydb->role_val_to_struct[val1 - 1];
				r2 = policydb->role_val_to_struct[val2 - 1];
				free(src); src = strdup("r1");
				free(tgt); tgt = strdup("r2");

				switch (e->op) {
				case CEXPR_DOM:
					s[++sp] = ksu_ebitmap_get_bit(&r1->dominates, val2 - 1);
					msgcat(src, tgt, "dom", s[sp] == 0);
					expr_counter++;
					continue;
				case CEXPR_DOMBY:
					s[++sp] = ksu_ebitmap_get_bit(&r2->dominates, val1 - 1);
					msgcat(src, tgt, "domby", s[sp] == 0);
					expr_counter++;
					continue;
				case CEXPR_INCOMP:
					s[++sp] = (!ksu_ebitmap_get_bit(&r1->dominates, val2 - 1)
						 && !ksu_ebitmap_get_bit(&r2->dominates, val1 - 1));
					msgcat(src, tgt, "incomp", s[sp] == 0);
					expr_counter++;
					continue;
				default:
					break;
				}
				break;
			case CEXPR_L1L2:
				l1 = &(scontext->range.level[0]);
				l2 = &(tcontext->range.level[0]);
				free(src); src = strdup("l1");
				free(tgt); tgt = strdup("l2");
				goto mls_ops;
			case CEXPR_L1H2:
				l1 = &(scontext->range.level[0]);
				l2 = &(tcontext->range.level[1]);
				free(src); src = strdup("l1");
				free(tgt); tgt = strdup("h2");
				goto mls_ops;
			case CEXPR_H1L2:
				l1 = &(scontext->range.level[1]);
				l2 = &(tcontext->range.level[0]);
				free(src); src = strdup("h1");
				free(tgt); tgt = strdup("l2");
				goto mls_ops;
			case CEXPR_H1H2:
				l1 = &(scontext->range.level[1]);
				l2 = &(tcontext->range.level[1]);
				free(src); src = strdup("h1");
				free(tgt); tgt = strdup("h2");
				goto mls_ops;
			case CEXPR_L1H1:
				l1 = &(scontext->range.level[0]);
				l2 = &(scontext->range.level[1]);
				free(src); src = strdup("l1");
				free(tgt); tgt = strdup("h1");
				goto mls_ops;
			case CEXPR_L2H2:
				l1 = &(tcontext->range.level[0]);
				l2 = &(tcontext->range.level[1]);
				free(src); src = strdup("l2");
				free(tgt); tgt = strdup("h2");
mls_ops:
				switch (e->op) {
				case CEXPR_EQ:
					s[++sp] = mls_level_eq(l1, l2);
					msgcat(src, tgt, "eq", s[sp] == 0);
					expr_counter++;
					continue;
				case CEXPR_NEQ:
					s[++sp] = !mls_level_eq(l1, l2);
					msgcat(src, tgt, "!=", s[sp] == 0);
					expr_counter++;
					continue;
				case CEXPR_DOM:
					s[++sp] = mls_level_dom(l1, l2);
					msgcat(src, tgt, "dom", s[sp] == 0);
					expr_counter++;
					continue;
				case CEXPR_DOMBY:
					s[++sp] = mls_level_dom(l2, l1);
					msgcat(src, tgt, "domby", s[sp] == 0);
					expr_counter++;
					continue;
				case CEXPR_INCOMP:
					s[++sp] = mls_level_incomp(l2, l1);
					msgcat(src, tgt, "incomp", s[sp] == 0);
					expr_counter++;
					continue;
				default:
					BUG();
					goto out;
				}
				break;
			default:
				BUG();
				goto out;
			}

			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = (val1 == val2);
				msgcat(src, tgt, "==", s[sp] == 0);
				break;
			case CEXPR_NEQ:
				s[++sp] = (val1 != val2);
				msgcat(src, tgt, "!=", s[sp] == 0);
				break;
			default:
				BUG();
				goto out;
			}
			break;
		case CEXPR_NAMES:
			if (sp == (CEXPR_MAXDEPTH - 1))
				goto out;
			s_t_x_num = SOURCE;
			c = scontext;
			if (e->attr & CEXPR_TARGET) {
				s_t_x_num = TARGET;
				c = tcontext;
			} else if (e->attr & CEXPR_XTARGET) {
				s_t_x_num = XTARGET;
				c = xcontext;
			}
			if (!c) {
				BUG();
				goto out;
			}
			if (e->attr & CEXPR_USER) {
				u_r_t = CEXPR_USER;
				val1 = c->user;
				snprintf(tmp_buf, sizeof(tmp_buf), "u%d ", s_t_x_num);
				free(src); src = strdup(tmp_buf);
			} else if (e->attr & CEXPR_ROLE) {
				u_r_t = CEXPR_ROLE;
				val1 = c->role;
				snprintf(tmp_buf, sizeof(tmp_buf), "r%d ", s_t_x_num);
				free(src); src = strdup(tmp_buf);
			} else if (e->attr & CEXPR_TYPE) {
				u_r_t = CEXPR_TYPE;
				val1 = c->type;
				snprintf(tmp_buf, sizeof(tmp_buf), "t%d ", s_t_x_num);
				free(src); src = strdup(tmp_buf);
			} else {
				BUG();
				goto out;
			}

			switch (e->op) {
			case CEXPR_EQ:
				s[++sp] = ksu_ebitmap_get_bit(&e->names, val1 - 1);
				get_name_list(e, u_r_t, src, "==", s[sp] == 0);
				break;

			case CEXPR_NEQ:
				s[++sp] = !ksu_ebitmap_get_bit(&e->names, val1 - 1);
				get_name_list(e, u_r_t, src, "!=", s[sp] == 0);
				break;
			default:
				BUG();
				goto out;
			}
			break;
		default:
			BUG();
			goto out;
		}
		expr_counter++;
	}

	/*
	 * At this point each expression of the constraint is in
	 * expr_list[n+1] and in RPN format. Now convert to 'infix'
	 */

	/*
	 * Save expr count but zero expr_counter to detect if
	 * 'BUG(); goto out;' was called as we need to release any used
	 * expr_list malloc's. Normally they are released by the RPN to
	 * infix code.
	 */
	expr_count = expr_counter;
	expr_counter = 0;

	/*
	 * Generate the same number of answer buffer entries as expression
	 * buffers (as there will never be more).
	 */
	answer_list = calloc(expr_count, sizeof(*answer_list));
	if (!answer_list) {
		ERR(NULL, "failed to allocate answer stack");
		rc = -ENOMEM;
		goto out;
	}

	/* Convert constraint from RPN to infix notation. */
	for (x = 0; x != expr_count; x++) {
		if (strncmp(expr_list[x], "and", 3) == 0 || strncmp(expr_list[x],
					"or", 2) == 0) {
			b = pop();
			b_len = strlen(b);
			a = pop();
			a_len = strlen(a);

			/* get a buffer to hold the answer */
			answer_list[answer_counter] = malloc(a_len + b_len + 8);
			if (!answer_list[answer_counter]) {
				ERR(NULL, "failed to allocate answer buffer");
				rc = -ENOMEM;
				goto out;
			}
			memset(answer_list[answer_counter], '\0', a_len + b_len + 8);

			sprintf(answer_list[answer_counter], "%s %s %s", a,
					expr_list[x], b);
			push(answer_list[answer_counter++]);
			free(a);
			free(b);
			free(expr_list[x]);
		} else if (strncmp(expr_list[x], "not", 3) == 0) {
			b = pop();
			b_len = strlen(b);

			answer_list[answer_counter] = malloc(b_len + 8);
			if (!answer_list[answer_counter]) {
				ERR(NULL, "failed to allocate answer buffer");
				rc = -ENOMEM;
				goto out;
			}
			memset(answer_list[answer_counter], '\0', b_len + 8);

			if (strncmp(b, "not", 3) == 0)
				sprintf(answer_list[answer_counter], "%s (%s)",
						expr_list[x], b);
			else
				sprintf(answer_list[answer_counter], "%s%s",
						expr_list[x], b);
			push(answer_list[answer_counter++]);
			free(b);
			free(expr_list[x]);
		} else {
			push(expr_list[x]);
		}
	}
	/* Get the final answer from tos and build constraint text */
	a = pop();

	/* validatetrans / constraint calculation:
				rc = 0 is denied, rc = 1 is granted */
	sprintf(tmp_buf, "%s %s\n",
			xcontext ? "Validatetrans" : "Constraint",
			s[0] ? "GRANTED" : "DENIED");

	/*
	 * This will add the constraints to the callers reason buffer (who is
	 * responsible for freeing the memory). It will handle any realloc's
	 * should the buffer be too short.
	 * The reason_buf_used and reason_buf_len counters are defined
	 * globally as multiple constraints can be in the buffer.
	 */

	if (r_buf && ((s[0] == 0) || ((s[0] == 1 &&
				(flags & SHOW_GRANTED) == SHOW_GRANTED)))) {
		int len, new_buf_len;
		char *p, **new_buf = r_buf;
		/*
		* These contain the constraint components that are added to the
		* callers reason buffer.
		*/
		const char *buffers[] = { class_buf, a, "); ", tmp_buf, 0 };

		for (x = 0; buffers[x] != NULL; x++) {
			while (1) {
				p = *r_buf ? (*r_buf + reason_buf_used) : NULL;
				len = snprintf(p, reason_buf_len - reason_buf_used,
						"%s", buffers[x]);
				if (len < 0 || len >= reason_buf_len - reason_buf_used) {
					new_buf_len = reason_buf_len + REASON_BUF_SIZE;
					*new_buf = realloc(*r_buf, new_buf_len);
					if (!*new_buf) {
						ERR(NULL, "failed to realloc reason buffer");
						goto out1;
					}
					**r_buf = **new_buf;
					reason_buf_len = new_buf_len;
					continue;
				} else {
					reason_buf_used += len;
					break;
				}
			}
		}
	}

out1:
	rc = s[0];
	free(a);

out:
	free(class_buf);
	free(src);
	free(tgt);

	if (expr_counter) {
		for (x = 0; expr_list[x] != NULL; x++)
			free(expr_list[x]);
	}
	free(answer_list);
	free(expr_list);
	return rc;
}

/* Forward declaration */
static int context_struct_compute_av(context_struct_t * scontext,
				     context_struct_t * tcontext,
				     sepol_security_class_t tclass,
				     sepol_access_vector_t requested,
				     struct sepol_av_decision *avd,
				     unsigned int *reason,
				     char **r_buf,
				     unsigned int flags);

static void type_attribute_bounds_av(context_struct_t *scontext,
				     context_struct_t *tcontext,
				     sepol_security_class_t tclass,
				     sepol_access_vector_t requested,
				     struct sepol_av_decision *avd,
				     unsigned int *reason)
{
	context_struct_t lo_scontext;
	context_struct_t lo_tcontext, *tcontextp = tcontext;
	struct sepol_av_decision lo_avd;
	type_datum_t *source;
	type_datum_t *target;
	sepol_access_vector_t masked = 0;

	source = policydb->type_val_to_struct[scontext->type - 1];
	if (!source->bounds)
		return;

	target = policydb->type_val_to_struct[tcontext->type - 1];

	memset(&lo_avd, 0, sizeof(lo_avd));

	memcpy(&lo_scontext, scontext, sizeof(lo_scontext));
	lo_scontext.type = source->bounds;

	if (target->bounds) {
		memcpy(&lo_tcontext, tcontext, sizeof(lo_tcontext));
		lo_tcontext.type = target->bounds;
		tcontextp = &lo_tcontext;
	}

	context_struct_compute_av(&lo_scontext,
				  tcontextp,
				  tclass,
				  requested,
				  &lo_avd,
				  NULL, /* reason intentionally omitted */
				  NULL,
				  0);

	masked = ~lo_avd.allowed & avd->allowed;

	if (!masked)
		return;		/* no masked permission */

	/* mask violated permissions */
	avd->allowed &= ~masked;

	*reason |= SEPOL_COMPUTEAV_BOUNDS;
}

/*
 * Compute access vectors based on a context structure pair for
 * the permissions in a particular class.
 */
static int context_struct_compute_av(context_struct_t * scontext,
				     context_struct_t * tcontext,
				     sepol_security_class_t tclass,
				     sepol_access_vector_t requested,
				     struct sepol_av_decision *avd,
				     unsigned int *reason,
				     char **r_buf,
				     unsigned int flags)
{
	constraint_node_t *constraint;
	struct role_allow *ra;
	avtab_key_t avkey;
	class_datum_t *tclass_datum;
	avtab_ptr_t node;
	ebitmap_t *sattr, *tattr;
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;

	if (!tclass || tclass > policydb->p_classes.nprim) {
		ERR(NULL, "unrecognized class %d", tclass);
		return -EINVAL;
	}
	tclass_datum = policydb->class_val_to_struct[tclass - 1];

	/* 
	 * Initialize the access vectors to the default values.
	 */
	avd->allowed = 0;
	avd->decided = 0xffffffff;
	avd->auditallow = 0;
	avd->auditdeny = 0xffffffff;
	avd->seqno = latest_granting;
	if (reason)
		*reason = 0;

	/*
	 * If a specific type enforcement rule was defined for
	 * this permission check, then use it.
	 */
	avkey.target_class = tclass;
	avkey.specified = AVTAB_AV;
	sattr = &policydb->type_attr_map[scontext->type - 1];
	tattr = &policydb->type_attr_map[tcontext->type - 1];
	ebitmap_for_each_positive_bit(sattr, snode, i) {
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			avkey.source_type = i + 1;
			avkey.target_type = j + 1;
			for (node =
			     ksu_avtab_search_node(&policydb->te_avtab, &avkey);
			     node != NULL;
			     node =
			     ksu_avtab_search_node_next(node, avkey.specified)) {
				if (node->key.specified == AVTAB_ALLOWED)
					avd->allowed |= node->datum.data;
				else if (node->key.specified ==
					 AVTAB_AUDITALLOW)
					avd->auditallow |= node->datum.data;
				else if (node->key.specified == AVTAB_AUDITDENY)
					avd->auditdeny &= node->datum.data;
			}

			/* Check conditional av table for additional permissions */
			ksu_cond_compute_av(&policydb->te_cond_avtab, &avkey, avd);

		}
	}

	if (requested & ~avd->allowed) {
		if (reason)
			*reason |= SEPOL_COMPUTEAV_TE;
		requested &= avd->allowed;
	}

	/* 
	 * Remove any permissions prohibited by a constraint (this includes
	 * the MLS policy).
	 */
	constraint = tclass_datum->constraints;
	while (constraint) {
		if ((constraint->permissions & (avd->allowed)) &&
		    !constraint_expr_eval_reason(scontext, tcontext, NULL,
					  tclass, constraint, r_buf, flags)) {
			avd->allowed =
			    (avd->allowed) & ~(constraint->permissions);
		}
		constraint = constraint->next;
	}

	if (requested & ~avd->allowed) {
		if (reason)
			*reason |= SEPOL_COMPUTEAV_CONS;
		requested &= avd->allowed;
	}

	/* 
	 * If checking process transition permission and the
	 * role is changing, then check the (current_role, new_role) 
	 * pair.
	 */
	if (tclass == policydb->process_class &&
	    (avd->allowed & policydb->process_trans_dyntrans) &&
	    scontext->role != tcontext->role) {
		for (ra = policydb->role_allow; ra; ra = ra->next) {
			if (scontext->role == ra->role &&
			    tcontext->role == ra->new_role)
				break;
		}
		if (!ra)
			avd->allowed &= ~policydb->process_trans_dyntrans;
	}

	if (requested & ~avd->allowed) {
		if (reason)
			*reason |= SEPOL_COMPUTEAV_RBAC;
		requested &= avd->allowed;
	}

	type_attribute_bounds_av(scontext, tcontext, tclass, requested, avd,
				 reason);
	return 0;
}

/*
 * sepol_validate_transition_reason_buffer - the reason buffer is realloc'd
 * in the constraint_expr_eval_reason() function.
 */
int sepol_validate_transition_reason_buffer(sepol_security_id_t oldsid,
				     sepol_security_id_t newsid,
				     sepol_security_id_t tasksid,
				     sepol_security_class_t tclass,
				     char **reason_buf,
				     unsigned int flags)
{
	context_struct_t *ocontext;
	context_struct_t *ncontext;
	context_struct_t *tcontext;
	class_datum_t *tclass_datum;
	constraint_node_t *constraint;

	if (!tclass || tclass > policydb->p_classes.nprim) {
		ERR(NULL, "unrecognized class %d", tclass);
		return -EINVAL;
	}
	tclass_datum = policydb->class_val_to_struct[tclass - 1];

	ocontext = sepol_sidtab_search(sidtab, oldsid);
	if (!ocontext) {
		ERR(NULL, "unrecognized SID %d", oldsid);
		return -EINVAL;
	}

	ncontext = sepol_sidtab_search(sidtab, newsid);
	if (!ncontext) {
		ERR(NULL, "unrecognized SID %d", newsid);
		return -EINVAL;
	}

	tcontext = sepol_sidtab_search(sidtab, tasksid);
	if (!tcontext) {
		ERR(NULL, "unrecognized SID %d", tasksid);
		return -EINVAL;
	}

	/*
	 * Set the buffer to NULL as mls/validatetrans may not be processed.
	 * If a buffer is required, then the routines in
	 * constraint_expr_eval_reason will realloc in REASON_BUF_SIZE
	 * chunks (as it gets called for each mls/validatetrans processed).
	 * We just make sure these start from zero.
	 */
	*reason_buf = NULL;
	reason_buf_used = 0;
	reason_buf_len = 0;
	constraint = tclass_datum->validatetrans;
	while (constraint) {
		if (!constraint_expr_eval_reason(ocontext, ncontext, tcontext,
				tclass, constraint, reason_buf, flags)) {
			return -EPERM;
		}
		constraint = constraint->next;
	}
	return 0;
}

int sepol_compute_av_reason(sepol_security_id_t ssid,
				   sepol_security_id_t tsid,
				   sepol_security_class_t tclass,
				   sepol_access_vector_t requested,
				   struct sepol_av_decision *avd,
				   unsigned int *reason)
{
	context_struct_t *scontext = 0, *tcontext = 0;
	int rc = 0;

	scontext = sepol_sidtab_search(sidtab, ssid);
	if (!scontext) {
		ERR(NULL, "unrecognized source SID %d", ssid);
		rc = -EINVAL;
		goto out;
	}
	tcontext = sepol_sidtab_search(sidtab, tsid);
	if (!tcontext) {
		ERR(NULL, "unrecognized target SID %d", tsid);
		rc = -EINVAL;
		goto out;
	}

	rc = context_struct_compute_av(scontext, tcontext, tclass,
					requested, avd, reason, NULL, 0);
      out:
	return rc;
}

/*
 * sepol_compute_av_reason_buffer - the reason buffer is malloc'd to
 * REASON_BUF_SIZE. If the buffer size is exceeded, then it is realloc'd
 * in the constraint_expr_eval_reason() function.
 */
int sepol_compute_av_reason_buffer(sepol_security_id_t ssid,
				   sepol_security_id_t tsid,
				   sepol_security_class_t tclass,
				   sepol_access_vector_t requested,
				   struct sepol_av_decision *avd,
				   unsigned int *reason,
				   char **reason_buf,
				   unsigned int flags)
{
	context_struct_t *scontext = 0, *tcontext = 0;
	int rc = 0;

	scontext = sepol_sidtab_search(sidtab, ssid);
	if (!scontext) {
		ERR(NULL, "unrecognized source SID %d", ssid);
		rc = -EINVAL;
		goto out;
	}
	tcontext = sepol_sidtab_search(sidtab, tsid);
	if (!tcontext) {
		ERR(NULL, "unrecognized target SID %d", tsid);
		rc = -EINVAL;
		goto out;
	}

	/*
	 * Set the buffer to NULL as constraints may not be processed.
	 * If a buffer is required, then the routines in
	 * constraint_expr_eval_reason will realloc in REASON_BUF_SIZE
	 * chunks (as it gets called for each constraint processed).
	 * We just make sure these start from zero.
	 */
	*reason_buf = NULL;
	reason_buf_used = 0;
	reason_buf_len = 0;

	rc = context_struct_compute_av(scontext, tcontext, tclass,
					   requested, avd, reason, reason_buf, flags);
out:
	return rc;
}

int sepol_compute_av(sepol_security_id_t ssid,
			    sepol_security_id_t tsid,
			    sepol_security_class_t tclass,
			    sepol_access_vector_t requested,
			    struct sepol_av_decision *avd)
{
	unsigned int reason = 0;
	return sepol_compute_av_reason(ssid, tsid, tclass, requested, avd,
				       &reason);
}

/*
 * Return a class ID associated with the class string specified by
 * class_name.
 */
int sepol_string_to_security_class(const char *class_name,
			sepol_security_class_t *tclass)
{
	class_datum_t *tclass_datum;

	tclass_datum = hashtab_search(policydb->p_classes.table,
				      class_name);
	if (!tclass_datum) {
		ERR(NULL, "unrecognized class %s", class_name);
		return STATUS_ERR;
	}
	*tclass = tclass_datum->s.value;
	return STATUS_SUCCESS;
}

/*
 * Return access vector bit associated with the class ID and permission
 * string.
 */
int sepol_string_to_av_perm(sepol_security_class_t tclass,
					const char *perm_name,
					sepol_access_vector_t *av)
{
	class_datum_t *tclass_datum;
	perm_datum_t *perm_datum;

	if (!tclass || tclass > policydb->p_classes.nprim) {
		ERR(NULL, "unrecognized class %d", tclass);
		return -EINVAL;
	}
	tclass_datum = policydb->class_val_to_struct[tclass - 1];

	/* Check for unique perms then the common ones (if any) */
	perm_datum = (perm_datum_t *)
			hashtab_search(tclass_datum->permissions.table,
			perm_name);
	if (perm_datum != NULL) {
		*av = UINT32_C(1) << (perm_datum->s.value - 1);
		return STATUS_SUCCESS;
	}

	if (tclass_datum->comdatum == NULL)
		goto out;

	perm_datum = (perm_datum_t *)
			hashtab_search(tclass_datum->comdatum->permissions.table,
			perm_name);

	if (perm_datum != NULL) {
		*av = UINT32_C(1) << (perm_datum->s.value - 1);
		return STATUS_SUCCESS;
	}
out:
	ERR(NULL, "could not convert %s to av bit", perm_name);
	return STATUS_ERR;
}

 const char *sepol_av_perm_to_string(sepol_security_class_t tclass,
					sepol_access_vector_t av)
{
	return sepol_av_to_string(policydb, tclass, av);
}

/*
 * Write the security context string representation of 
 * the context associated with `sid' into a dynamically
 * allocated string of the correct size.  Set `*scontext'
 * to point to this string and set `*scontext_len' to
 * the length of the string.
 */
int sepol_sid_to_context(sepol_security_id_t sid,
				sepol_security_context_t * scontext,
				size_t * scontext_len)
{
	context_struct_t *context;
	int rc = 0;

	context = sepol_sidtab_search(sidtab, sid);
	if (!context) {
		ERR(NULL, "unrecognized SID %d", sid);
		rc = -EINVAL;
		goto out;
	}
	rc = context_to_string(NULL, policydb, context, scontext, scontext_len);
      out:
	return rc;

}

/*
 * Return a SID associated with the security context that
 * has the string representation specified by `scontext'.
 */
int sepol_context_to_sid(sepol_const_security_context_t scontext,
				size_t scontext_len, sepol_security_id_t * sid)
{

	context_struct_t *context = NULL;

	/* First, create the context */
	if (context_from_string(NULL, policydb, &context,
				scontext, scontext_len) < 0)
		goto err;

	/* Obtain the new sid */
	if (sid && (sepol_sidtab_context_to_sid(sidtab, context, sid) < 0))
		goto err;

	context_destroy(context);
	free(context);
	return STATUS_SUCCESS;

      err:
	if (context) {
		context_destroy(context);
		free(context);
	}
	ERR(NULL, "could not convert %s to sid", scontext);
	return STATUS_ERR;
}

static inline int compute_sid_handle_invalid_context(context_struct_t *
						     scontext,
						     context_struct_t *
						     tcontext,
						     sepol_security_class_t
						     tclass,
						     context_struct_t *
						     newcontext)
{
	if (selinux_enforcing) {
		return -EACCES;
	} else {
		sepol_security_context_t s, t, n;
		size_t slen, tlen, nlen;

		context_to_string(NULL, policydb, scontext, &s, &slen);
		context_to_string(NULL, policydb, tcontext, &t, &tlen);
		context_to_string(NULL, policydb, newcontext, &n, &nlen);
		ERR(NULL, "invalid context %s for "
		    "scontext=%s tcontext=%s tclass=%s",
		    n, s, t, policydb->p_class_val_to_name[tclass - 1]);
		free(s);
		free(t);
		free(n);
		return 0;
	}
}

static int sepol_compute_sid(sepol_security_id_t ssid,
			     sepol_security_id_t tsid,
			     sepol_security_class_t tclass,
			     uint32_t specified, sepol_security_id_t * out_sid)
{
	struct class_datum *cladatum = NULL;
	context_struct_t *scontext = 0, *tcontext = 0, newcontext;
	struct role_trans *roletr = 0;
	avtab_key_t avkey;
	avtab_datum_t *avdatum;
	avtab_ptr_t node;
	int rc = 0;

	scontext = sepol_sidtab_search(sidtab, ssid);
	if (!scontext) {
		ERR(NULL, "unrecognized SID %d", ssid);
		rc = -EINVAL;
		goto out;
	}
	tcontext = sepol_sidtab_search(sidtab, tsid);
	if (!tcontext) {
		ERR(NULL, "unrecognized SID %d", tsid);
		rc = -EINVAL;
		goto out;
	}

	if (tclass && tclass <= policydb->p_classes.nprim)
		cladatum = policydb->class_val_to_struct[tclass - 1];

	context_init(&newcontext);

	/* Set the user identity. */
	switch (specified) {
	case AVTAB_TRANSITION:
	case AVTAB_CHANGE:
		if (cladatum && cladatum->default_user == DEFAULT_TARGET) {
			newcontext.user = tcontext->user;
		} else {
			/* notice this gets both DEFAULT_SOURCE and unset */
			/* Use the process user identity. */
			newcontext.user = scontext->user;
		}
		break;
	case AVTAB_MEMBER:
		/* Use the related object owner. */
		newcontext.user = tcontext->user;
		break;
	}

	/* Set the role to default values. */
	if (cladatum && cladatum->default_role == DEFAULT_SOURCE) {
		newcontext.role = scontext->role;
	} else if (cladatum && cladatum->default_role == DEFAULT_TARGET) {
		newcontext.role = tcontext->role;
	} else {
		if (tclass == policydb->process_class)
			newcontext.role = scontext->role;
		else
			newcontext.role = OBJECT_R_VAL;
	}

	/* Set the type to default values. */
	if (cladatum && cladatum->default_type == DEFAULT_SOURCE) {
		newcontext.type = scontext->type;
	} else if (cladatum && cladatum->default_type == DEFAULT_TARGET) {
		newcontext.type = tcontext->type;
	} else {
		if (tclass == policydb->process_class) {
			/* Use the type of process. */
			newcontext.type = scontext->type;
		} else {
			/* Use the type of the related object. */
			newcontext.type = tcontext->type;
		}
	}

	/* Look for a type transition/member/change rule. */
	avkey.source_type = scontext->type;
	avkey.target_type = tcontext->type;
	avkey.target_class = tclass;
	avkey.specified = specified;
	avdatum = ksu_avtab_search(&policydb->te_avtab, &avkey);

	/* If no permanent rule, also check for enabled conditional rules */
	if (!avdatum) {
		node = ksu_avtab_search_node(&policydb->te_cond_avtab, &avkey);
		for (; node != NULL;
		     node = ksu_avtab_search_node_next(node, specified)) {
			if (node->key.specified & AVTAB_ENABLED) {
				avdatum = &node->datum;
				break;
			}
		}
	}

	if (avdatum) {
		/* Use the type from the type transition/member/change rule. */
		newcontext.type = avdatum->data;
	}

	/* Check for class-specific changes. */
	if (specified & AVTAB_TRANSITION) {
		/* Look for a role transition rule. */
		for (roletr = policydb->role_tr; roletr;
		     roletr = roletr->next) {
			if (roletr->role == scontext->role &&
			    roletr->type == tcontext->type &&
			    roletr->tclass == tclass) {
				/* Use the role transition rule. */
				newcontext.role = roletr->new_role;
				break;
			}
		}
	}

	/* Set the MLS attributes.
	   This is done last because it may allocate memory. */
	rc = ksu_mls_compute_sid(policydb, scontext, tcontext, tclass, specified,
			     &newcontext);
	if (rc)
		goto out;

	/* Check the validity of the context. */
	if (!ksu_policydb_context_isvalid(policydb, &newcontext)) {
		rc = compute_sid_handle_invalid_context(scontext,
							tcontext,
							tclass, &newcontext);
		if (rc)
			goto out;
	}
	/* Obtain the sid for the context. */
	rc = sepol_sidtab_context_to_sid(sidtab, &newcontext, out_sid);
      out:
	context_destroy(&newcontext);
	return rc;
}

/*
 * Compute a SID to use for labeling a new object in the 
 * class `tclass' based on a SID pair.  
 */
int sepol_transition_sid(sepol_security_id_t ssid,
				sepol_security_id_t tsid,
				sepol_security_class_t tclass,
				sepol_security_id_t * out_sid)
{
	return sepol_compute_sid(ssid, tsid, tclass, AVTAB_TRANSITION, out_sid);
}

/*
 * Compute a SID to use when selecting a member of a 
 * polyinstantiated object of class `tclass' based on 
 * a SID pair.
 */
int sepol_member_sid(sepol_security_id_t ssid,
			    sepol_security_id_t tsid,
			    sepol_security_class_t tclass,
			    sepol_security_id_t * out_sid)
{
	return sepol_compute_sid(ssid, tsid, tclass, AVTAB_MEMBER, out_sid);
}

/*
 * Compute a SID to use for relabeling an object in the 
 * class `tclass' based on a SID pair.  
 */
int sepol_change_sid(sepol_security_id_t ssid,
			    sepol_security_id_t tsid,
			    sepol_security_class_t tclass,
			    sepol_security_id_t * out_sid)
{
	return sepol_compute_sid(ssid, tsid, tclass, AVTAB_CHANGE, out_sid);
}

/*
 * Verify that each permission that is defined under the
 * existing policy is still defined with the same value
 * in the new policy.
 */
static int validate_perm(hashtab_key_t key, hashtab_datum_t datum, void *p)
{
	hashtab_t h;
	perm_datum_t *perdatum, *perdatum2;

	h = (hashtab_t) p;
	perdatum = (perm_datum_t *) datum;

	perdatum2 = (perm_datum_t *) hashtab_search(h, key);
	if (!perdatum2) {
		ERR(NULL, "permission %s disappeared", key);
		return -1;
	}
	if (perdatum->s.value != perdatum2->s.value) {
		ERR(NULL, "the value of permissions %s changed", key);
		return -1;
	}
	return 0;
}

/*
 * Verify that each class that is defined under the
 * existing policy is still defined with the same 
 * attributes in the new policy.
 */
static int validate_class(hashtab_key_t key, hashtab_datum_t datum, void *p)
{
	policydb_t *newp;
	class_datum_t *cladatum, *cladatum2;

	newp = (policydb_t *) p;
	cladatum = (class_datum_t *) datum;

	cladatum2 =
	    (class_datum_t *) hashtab_search(newp->p_classes.table, key);
	if (!cladatum2) {
		ERR(NULL, "class %s disappeared", key);
		return -1;
	}
	if (cladatum->s.value != cladatum2->s.value) {
		ERR(NULL, "the value of class %s changed", key);
		return -1;
	}
	if ((cladatum->comdatum && !cladatum2->comdatum) ||
	    (!cladatum->comdatum && cladatum2->comdatum)) {
		ERR(NULL, "the inherits clause for the access "
		    "vector definition for class %s changed", key);
		return -1;
	}
	if (cladatum->comdatum) {
		if (ksu_hashtab_map
		    (cladatum->comdatum->permissions.table, validate_perm,
		     cladatum2->comdatum->permissions.table)) {
			ERR(NULL,
			    " in the access vector definition "
			    "for class %s", key);
			return -1;
		}
	}
	if (ksu_hashtab_map(cladatum->permissions.table, validate_perm,
			cladatum2->permissions.table)) {
		ERR(NULL, " in access vector definition for class %s", key);
		return -1;
	}
	return 0;
}

/* Clone the SID into the new SID table. */
static int clone_sid(sepol_security_id_t sid,
		     context_struct_t * context, void *arg)
{
	sidtab_t *s = arg;

	return sepol_sidtab_insert(s, sid, context);
}

static inline int convert_context_handle_invalid_context(context_struct_t *
							 context)
{
	if (selinux_enforcing) {
		return -EINVAL;
	} else {
		sepol_security_context_t s;
		size_t len;

		context_to_string(NULL, policydb, context, &s, &len);
		ERR(NULL, "context %s is invalid", s);
		free(s);
		return 0;
	}
}

typedef struct {
	policydb_t *oldp;
	policydb_t *newp;
} convert_context_args_t;

/*
 * Convert the values in the security context
 * structure `c' from the values specified
 * in the policy `p->oldp' to the values specified
 * in the policy `p->newp'.  Verify that the
 * context is valid under the new policy.
 */
static int convert_context(sepol_security_id_t key __attribute__ ((unused)),
			   context_struct_t * c, void *p)
{
	convert_context_args_t *args;
	context_struct_t oldc;
	role_datum_t *role;
	type_datum_t *typdatum;
	user_datum_t *usrdatum;
	sepol_security_context_t s;
	size_t len;
	int rc = -EINVAL;

	args = (convert_context_args_t *) p;

	if (context_cpy(&oldc, c))
		return -ENOMEM;

	/* Convert the user. */
	usrdatum = (user_datum_t *) hashtab_search(args->newp->p_users.table,
						   args->oldp->
						   p_user_val_to_name[c->user -
								      1]);

	if (!usrdatum) {
		goto bad;
	}
	c->user = usrdatum->s.value;

	/* Convert the role. */
	role = (role_datum_t *) hashtab_search(args->newp->p_roles.table,
					       args->oldp->
					       p_role_val_to_name[c->role - 1]);
	if (!role) {
		goto bad;
	}
	c->role = role->s.value;

	/* Convert the type. */
	typdatum = (type_datum_t *)
	    hashtab_search(args->newp->p_types.table,
			   args->oldp->p_type_val_to_name[c->type - 1]);
	if (!typdatum) {
		goto bad;
	}
	c->type = typdatum->s.value;

	rc = ksu_mls_convert_context(args->oldp, args->newp, c);
	if (rc)
		goto bad;

	/* Check the validity of the new context. */
	if (!ksu_policydb_context_isvalid(args->newp, c)) {
		rc = convert_context_handle_invalid_context(&oldc);
		if (rc)
			goto bad;
	}

	context_destroy(&oldc);
	return 0;

      bad:
	context_to_string(NULL, policydb, &oldc, &s, &len);
	context_destroy(&oldc);
	ERR(NULL, "invalidating context %s", s);
	free(s);
	return rc;
}

/* Reading from a policy "file". */
int next_entry(void *buf, struct policy_file *fp, size_t bytes)
{
	// size_t nread;

	switch (fp->type) {
	case PF_USE_STDIO:
#if 0
		nread = fread(buf, bytes, 1, fp->fp);

		if (nread != 1)
			return -1;
		break;
#else
		// don't support read from 'stdio'
		return -1;
#endif
	case PF_USE_MEMORY:
		if (bytes > fp->len) {
			// errno = EOVERFLOW;
			return -1;
		}
		memcpy(buf, fp->data, bytes);
		fp->data += bytes;
		fp->len -= bytes;
		break;
	default:
		// errno = EINVAL;
		return -1;
	}
	return 0;
}

size_t put_entry(const void *ptr, size_t size, size_t n,
			struct policy_file *fp)
{
	size_t bytes = size * n;

	switch (fp->type) {
	case PF_USE_STDIO:
		// return fwrite(ptr, size, n, fp->fp);
		return -1;
	case PF_USE_MEMORY:
		if (bytes > fp->len) {
			// errno = ENOSPC;
			return 0;
		}

		memcpy(fp->data, ptr, bytes);
		fp->data += bytes;
		fp->len -= bytes;
		return n;
	case PF_LEN:
		fp->len += bytes;
		return n;
	default:
		return 0;
	}
	return 0;
}

/*
 * Reads a string and null terminates it from the policy file.
 * This is a port of str_read from the SE Linux kernel code.
 *
 * It returns:
 *   0 - Success
 *  -1 - Failure with errno set
 */
int str_read(char **strp, struct policy_file *fp, size_t len)
{
	int rc;
	char *str;

	if (zero_or_saturated(len)) {
		// errno = EINVAL;
		return -1;
	}

	str = malloc(len + 1);
	if (!str)
		return -1;

	/* it's expected the caller should free the str */
	*strp = str;

	/* next_entry sets errno */
	rc = next_entry(str, fp, len);
	if (rc)
		return rc;

	str[len] = '\0';
	return 0;
}

/*
 * Read a new set of configuration data from 
 * a policy database binary representation file.
 *
 * Verify that each class that is defined under the
 * existing policy is still defined with the same 
 * attributes in the new policy.  
 *
 * Convert the context structures in the SID table to the
 * new representation and verify that all entries
 * in the SID table are valid under the new policy. 
 *
 * Change the active policy database to use the new 
 * configuration data.  
 *
 * Reset the access vector cache.
 */
int sepol_load_policy(void *data, size_t len)
{
	policydb_t oldpolicydb, newpolicydb;
	sidtab_t oldsidtab, newsidtab;
	convert_context_args_t args;
	int rc = 0;
	struct policy_file file, *fp;

	policy_file_init(&file);
	file.type = PF_USE_MEMORY;
	file.data = data;
	file.len = len;
	fp = &file;

	if (policydb_init(&newpolicydb))
		return -ENOMEM;

	if (ksu_policydb_read(&newpolicydb, fp, 1)) {
		ksu_policydb_destroy(&mypolicydb);
		return -EINVAL;
	}

	sepol_sidtab_init(&newsidtab);

	/* Verify that the existing classes did not change. */
	if (ksu_hashtab_map
	    (policydb->p_classes.table, validate_class, &newpolicydb)) {
		ERR(NULL, "the definition of an existing class changed");
		rc = -EINVAL;
		goto err;
	}

	/* Clone the SID table. */
	sepol_sidtab_shutdown(sidtab);
	if (sepol_sidtab_map(sidtab, clone_sid, &newsidtab)) {
		rc = -ENOMEM;
		goto err;
	}

	/* Convert the internal representations of contexts 
	   in the new SID table and remove invalid SIDs. */
	args.oldp = policydb;
	args.newp = &newpolicydb;
	sepol_sidtab_map_remove_on_error(&newsidtab, convert_context, &args);

	/* Save the old policydb and SID table to free later. */
	memcpy(&oldpolicydb, policydb, sizeof *policydb);
	sepol_sidtab_set(&oldsidtab, sidtab);

	/* Install the new policydb and SID table. */
	memcpy(policydb, &newpolicydb, sizeof *policydb);
	sepol_sidtab_set(sidtab, &newsidtab);

	/* Free the old policydb and SID table. */
	ksu_policydb_destroy(&oldpolicydb);
	sepol_sidtab_destroy(&oldsidtab);

	return 0;

      err:
	sepol_sidtab_destroy(&newsidtab);
	ksu_policydb_destroy(&newpolicydb);
	return rc;

}

/*
 * Return the SIDs to use for an unlabeled file system
 * that is being mounted from the device with the
 * the kdevname `name'.  The `fs_sid' SID is returned for 
 * the file system and the `file_sid' SID is returned
 * for all files within that file system.
 */
int sepol_fs_sid(char *name,
			sepol_security_id_t * fs_sid,
			sepol_security_id_t * file_sid)
{
	int rc = 0;
	ocontext_t *c;

	c = policydb->ocontexts[OCON_FS];
	while (c) {
		if (strcmp(c->u.name, name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0] || !c->sid[1]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[1],
							 &c->sid[1]);
			if (rc)
				goto out;
		}
		*fs_sid = c->sid[0];
		*file_sid = c->sid[1];
	} else {
		*fs_sid = SECINITSID_FS;
		*file_sid = SECINITSID_FILE;
	}

      out:
	return rc;
}

/*
 * Return the SID of the ibpkey specified by
 * `subnet prefix', and `pkey number'.
 */
int sepol_ibpkey_sid(uint64_t subnet_prefix,
			    uint16_t pkey, sepol_security_id_t *out_sid)
{
	ocontext_t *c;
	int rc = 0;

	c = policydb->ocontexts[OCON_IBPKEY];
	while (c) {
		if (c->u.ibpkey.low_pkey <= pkey &&
		    c->u.ibpkey.high_pkey >= pkey &&
		    subnet_prefix == c->u.ibpkey.subnet_prefix)
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
		}
		*out_sid = c->sid[0];
	} else {
		*out_sid = SECINITSID_UNLABELED;
	}

out:
	return rc;
}

/*
 * Return the SID of the subnet management interface specified by
 * `device name', and `port'.
 */
int sepol_ibendport_sid(char *dev_name,
			       uint8_t port,
			       sepol_security_id_t *out_sid)
{
	ocontext_t *c;
	int rc = 0;

	c = policydb->ocontexts[OCON_IBENDPORT];
	while (c) {
		if (c->u.ibendport.port == port &&
		    !strcmp(dev_name, c->u.ibendport.dev_name))
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
		}
		*out_sid = c->sid[0];
	} else {
		*out_sid = SECINITSID_UNLABELED;
	}

out:
	return rc;
}


/*
 * Return the SID of the port specified by
 * `domain', `type', `protocol', and `port'.
 */
int sepol_port_sid(uint16_t domain __attribute__ ((unused)),
			  uint16_t type __attribute__ ((unused)),
			  uint8_t protocol,
			  uint16_t port, sepol_security_id_t * out_sid)
{
	ocontext_t *c;
	int rc = 0;

	c = policydb->ocontexts[OCON_PORT];
	while (c) {
		if (c->u.port.protocol == protocol &&
		    c->u.port.low_port <= port && c->u.port.high_port >= port)
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
		}
		*out_sid = c->sid[0];
	} else {
		*out_sid = SECINITSID_PORT;
	}

      out:
	return rc;
}

/*
 * Return the SIDs to use for a network interface
 * with the name `name'.  The `if_sid' SID is returned for 
 * the interface and the `msg_sid' SID is returned as 
 * the default SID for messages received on the
 * interface.
 */
int sepol_netif_sid(char *name,
			   sepol_security_id_t * if_sid,
			   sepol_security_id_t * msg_sid)
{
	int rc = 0;
	ocontext_t *c;

	c = policydb->ocontexts[OCON_NETIF];
	while (c) {
		if (strcmp(name, c->u.name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		if (!c->sid[0] || !c->sid[1]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[1],
							 &c->sid[1]);
			if (rc)
				goto out;
		}
		*if_sid = c->sid[0];
		*msg_sid = c->sid[1];
	} else {
		*if_sid = SECINITSID_NETIF;
		*msg_sid = SECINITSID_NETMSG;
	}

      out:
	return rc;
}

static int match_ipv6_addrmask(uint32_t * input, uint32_t * addr,
			       uint32_t * mask)
{
	int i, fail = 0;

	for (i = 0; i < 4; i++)
		if (addr[i] != (input[i] & mask[i])) {
			fail = 1;
			break;
		}

	return !fail;
}

/*
 * Return the SID of the node specified by the address
 * `addrp' where `addrlen' is the length of the address
 * in bytes and `domain' is the communications domain or
 * address family in which the address should be interpreted.
 */
int sepol_node_sid(uint16_t domain,
			  void *addrp,
			  size_t addrlen, sepol_security_id_t * out_sid)
{
	int rc = 0;
	ocontext_t *c;

	switch (domain) {
	case AF_INET:{
			uint32_t addr;

			if (addrlen != sizeof(uint32_t)) {
				rc = -EINVAL;
				goto out;
			}

			addr = *((uint32_t *) addrp);

			c = policydb->ocontexts[OCON_NODE];
			while (c) {
				if (c->u.node.addr == (addr & c->u.node.mask))
					break;
				c = c->next;
			}
			break;
		}

	case AF_INET6:
		if (addrlen != sizeof(uint64_t) * 2) {
			rc = -EINVAL;
			goto out;
		}

		c = policydb->ocontexts[OCON_NODE6];
		while (c) {
			if (match_ipv6_addrmask(addrp, c->u.node6.addr,
						c->u.node6.mask))
				break;
			c = c->next;
		}
		break;

	default:
		*out_sid = SECINITSID_NODE;
		goto out;
	}

	if (c) {
		if (!c->sid[0]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
		}
		*out_sid = c->sid[0];
	} else {
		*out_sid = SECINITSID_NODE;
	}

      out:
	return rc;
}

/*
 * Generate the set of SIDs for legal security contexts
 * for a given user that can be reached by `fromsid'.
 * Set `*sids' to point to a dynamically allocated 
 * array containing the set of SIDs.  Set `*nel' to the
 * number of elements in the array.
 */
#define SIDS_NEL 25

int sepol_get_user_sids(sepol_security_id_t fromsid,
			       char *username,
			       sepol_security_id_t ** sids, uint32_t * nel)
{
	context_struct_t *fromcon, usercon;
	sepol_security_id_t *mysids, *mysids2, sid;
	uint32_t mynel = 0, maxnel = SIDS_NEL;
	user_datum_t *user;
	role_datum_t *role;
	struct sepol_av_decision avd;
	int rc = 0;
	unsigned int i, j, reason;
	ebitmap_node_t *rnode, *tnode;

	fromcon = sepol_sidtab_search(sidtab, fromsid);
	if (!fromcon) {
		rc = -EINVAL;
		goto out;
	}

	user = (user_datum_t *) hashtab_search(policydb->p_users.table,
					       username);
	if (!user) {
		rc = -EINVAL;
		goto out;
	}
	usercon.user = user->s.value;

	mysids = calloc(maxnel, sizeof(sepol_security_id_t));
	if (!mysids) {
		rc = -ENOMEM;
		goto out;
	}

	ebitmap_for_each_positive_bit(&user->roles.roles, rnode, i) {
		role = policydb->role_val_to_struct[i];
		usercon.role = i + 1;
		ebitmap_for_each_positive_bit(&role->types.types, tnode, j) {
			usercon.type = j + 1;
			if (usercon.type == fromcon->type)
				continue;

			if (ksu_mls_setup_user_range
			    (fromcon, user, &usercon, policydb->mls))
				continue;

			rc = context_struct_compute_av(fromcon, &usercon,
						       policydb->process_class,
						       policydb->process_trans,
						       &avd, &reason, NULL, 0);
			if (rc || !(avd.allowed & policydb->process_trans))
				continue;
			rc = sepol_sidtab_context_to_sid(sidtab, &usercon,
							 &sid);
			if (rc) {
				free(mysids);
				goto out;
			}
			if (mynel < maxnel) {
				mysids[mynel++] = sid;
			} else {
				maxnel += SIDS_NEL;
				mysids2 = calloc(maxnel, sizeof(sepol_security_id_t));
				if (!mysids2) {
					rc = -ENOMEM;
					free(mysids);
					goto out;
				}
				memcpy(mysids2, mysids,
				       mynel * sizeof(sepol_security_id_t));
				free(mysids);
				mysids = mysids2;
				mysids[mynel++] = sid;
			}
		}
	}

	*sids = mysids;
	*nel = mynel;

      out:
	return rc;
}

/*
 * Return the SID to use for a file in a filesystem
 * that cannot support a persistent label mapping or use another
 * fixed labeling behavior like transition SIDs or task SIDs.
 */
int sepol_genfs_sid(const char *fstype,
			   const char *path,
			   sepol_security_class_t sclass,
			   sepol_security_id_t * sid)
{
	size_t len;
	genfs_t *genfs;
	ocontext_t *c;
	int rc = 0, cmp = 0;

	for (genfs = policydb->genfs; genfs; genfs = genfs->next) {
		cmp = strcmp(fstype, genfs->fstype);
		if (cmp <= 0)
			break;
	}

	if (!genfs || cmp) {
		*sid = SECINITSID_UNLABELED;
		rc = -ENOENT;
		goto out;
	}

	for (c = genfs->head; c; c = c->next) {
		len = strlen(c->u.name);
		if ((!c->v.sclass || sclass == c->v.sclass) &&
		    (strncmp(c->u.name, path, len) == 0))
			break;
	}

	if (!c) {
		*sid = SECINITSID_UNLABELED;
		rc = -ENOENT;
		goto out;
	}

	if (!c->sid[0]) {
		rc = sepol_sidtab_context_to_sid(sidtab,
						 &c->context[0], &c->sid[0]);
		if (rc)
			goto out;
	}

	*sid = c->sid[0];
      out:
	return rc;
}

int sepol_fs_use(const char *fstype,
			unsigned int *behavior, sepol_security_id_t * sid)
{
	int rc = 0;
	ocontext_t *c;

	c = policydb->ocontexts[OCON_FSUSE];
	while (c) {
		if (strcmp(fstype, c->u.name) == 0)
			break;
		c = c->next;
	}

	if (c) {
		*behavior = c->v.behavior;
		if (!c->sid[0]) {
			rc = sepol_sidtab_context_to_sid(sidtab,
							 &c->context[0],
							 &c->sid[0]);
			if (rc)
				goto out;
		}
		*sid = c->sid[0];
	} else {
		rc = sepol_genfs_sid(fstype, "/", policydb->dir_class, sid);
		if (rc) {
			*behavior = SECURITY_FS_USE_NONE;
			rc = 0;
		} else {
			*behavior = SECURITY_FS_USE_GENFS;
		}
	}

      out:
	return rc;
}

/* FLASK */

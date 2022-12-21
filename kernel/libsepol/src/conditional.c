/* Authors: Karl MacMillan <kmacmillan@tresys.com>
 *          Frank Mayer <mayerf@tresys.com>
 *          David Caplan <dac@tresys.com>
 *
 * Copyright (C) 2003 - 2005 Tresys Technology, LLC
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

// #include <stdlib.h>

#include <sepol/policydb/flask_types.h>
#include <sepol/policydb/conditional.h>

#include "private.h"
#include "debug.h"

/* move all type rules to top of t/f lists to help kernel on evaluation */
static void cond_optimize(cond_av_list_t ** l)
{
	cond_av_list_t *top, *p, *cur;

	top = p = cur = *l;

	while (cur) {
		if ((cur->node->key.specified & AVTAB_TYPE) && (top != cur)) {
			p->next = cur->next;
			cur->next = top;
			top = cur;
			cur = p->next;
		} else {
			p = cur;
			cur = cur->next;
		}
	}
	*l = top;
}

/* reorder t/f lists for kernel */
void cond_optimize_lists(cond_list_t * cl)
{
	cond_list_t *n;

	for (n = cl; n != NULL; n = n->next) {
		cond_optimize(&n->true_list);
		cond_optimize(&n->false_list);
	}
}

static int bool_present(unsigned int target, unsigned int bools[],
			unsigned int num_bools)
{
	unsigned int i = 0;
	int ret = 1;

	if (num_bools > COND_MAX_BOOLS) {
		return 0;
	}
	while (i < num_bools && target != bools[i])
		i++;
	if (i == num_bools)
		ret = 0;	/* got to end w/o match */
	return ret;
}

static int same_bools(cond_node_t * a, cond_node_t * b)
{
	unsigned int i, x;

	x = a->nbools;

	/* same number of bools? */
	if (x != b->nbools)
		return 0;

	/* make sure all the bools in a are also in b */
	for (i = 0; i < x; i++)
		if (!bool_present(a->bool_ids[i], b->bool_ids, x))
			return 0;
	return 1;
}

/*
 * Determine if two conditional expressions are equal. 
 */
int cond_expr_equal(cond_node_t * a, cond_node_t * b)
{
	cond_expr_t *cur_a, *cur_b;

	if (a == NULL || b == NULL)
		return 0;

	if (a->nbools != b->nbools)
		return 0;

	/* if exprs have <= COND_MAX_BOOLS we can check the precompute values
	 * for the expressions.
	 */
	if (a->nbools <= COND_MAX_BOOLS && b->nbools <= COND_MAX_BOOLS) {
		if (!same_bools(a, b))
			return 0;
		return (a->expr_pre_comp == b->expr_pre_comp);
	}

	/* for long expressions we check for exactly the same expression */
	cur_a = a->expr;
	cur_b = b->expr;
	while (1) {
		if (cur_a == NULL && cur_b == NULL)
			return 1;
		else if (cur_a == NULL || cur_b == NULL)
			return 0;
		if (cur_a->expr_type != cur_b->expr_type)
			return 0;
		if (cur_a->expr_type == COND_BOOL) {
			if (cur_a->bool != cur_b->bool)
				return 0;
		}
		cur_a = cur_a->next;
		cur_b = cur_b->next;
	}
	return 1;
}

/* Create a new conditional node, optionally copying
 * the conditional expression from an existing node.
 * If node is NULL then a new node will be created
 * with no conditional expression.
 */
cond_node_t *cond_node_create(policydb_t * p, cond_node_t * node)
{
	cond_node_t *new_node;
	unsigned int i;

	new_node = (cond_node_t *)malloc(sizeof(cond_node_t));
	if (!new_node) {
		return NULL;
	}
	memset(new_node, 0, sizeof(cond_node_t));

	if (node) {
		new_node->expr = cond_copy_expr(node->expr);
		if (!new_node->expr) {
			free(new_node);
			return NULL;
		}
		new_node->cur_state = cond_evaluate_expr(p, new_node->expr);
		new_node->nbools = node->nbools;
		for (i = 0; i < min(node->nbools, COND_MAX_BOOLS); i++)
			new_node->bool_ids[i] = node->bool_ids[i];
		new_node->expr_pre_comp = node->expr_pre_comp;
		new_node->flags = node->flags;
	}

	return new_node;
}

/* Find a conditional (the needle) within a list of existing ones (the
 * haystack) that has a matching expression.  If found, return a
 * pointer to the existing node, setting 'was_created' to 0.
 * Otherwise create a new one and return it, setting 'was_created' to
 * 1. */
cond_node_t *cond_node_find(policydb_t * p,
			    cond_node_t * needle, cond_node_t * haystack,
			    int *was_created)
{
	while (haystack) {
		if (cond_expr_equal(needle, haystack)) {
			*was_created = 0;
			return haystack;
		}
		haystack = haystack->next;
	}
	*was_created = 1;

	return cond_node_create(p, needle);
}

/* return either a pre-existing matching node or create a new node */
cond_node_t *cond_node_search(policydb_t * p, cond_node_t * list,
			      cond_node_t * cn)
{
	int was_created;
	cond_node_t *result = cond_node_find(p, cn, list, &was_created);
	if (result != NULL && was_created) {
		/* add conditional node to policy list */
		result->next = p->cond_list;
		p->cond_list = result;
	}
	return result;
}

/*
 * cond_evaluate_expr evaluates a conditional expr
 * in reverse polish notation. It returns true (1), false (0),
 * or undefined (-1). Undefined occurs when the expression
 * exceeds the stack depth of COND_EXPR_MAXDEPTH.
 */
int cond_evaluate_expr(policydb_t * p, cond_expr_t * expr)
{

	cond_expr_t *cur;
	int s[COND_EXPR_MAXDEPTH];
	int sp = -1;

	s[0] = -1;

	for (cur = expr; cur != NULL; cur = cur->next) {
		switch (cur->expr_type) {
		case COND_BOOL:
			if (sp == (COND_EXPR_MAXDEPTH - 1))
				return -1;
			sp++;
			s[sp] = p->bool_val_to_struct[cur->bool - 1]->state;
			break;
		case COND_NOT:
			if (sp < 0)
				return -1;
			s[sp] = !s[sp];
			break;
		case COND_OR:
			if (sp < 1)
				return -1;
			sp--;
			s[sp] |= s[sp + 1];
			break;
		case COND_AND:
			if (sp < 1)
				return -1;
			sp--;
			s[sp] &= s[sp + 1];
			break;
		case COND_XOR:
			if (sp < 1)
				return -1;
			sp--;
			s[sp] ^= s[sp + 1];
			break;
		case COND_EQ:
			if (sp < 1)
				return -1;
			sp--;
			s[sp] = (s[sp] == s[sp + 1]);
			break;
		case COND_NEQ:
			if (sp < 1)
				return -1;
			sp--;
			s[sp] = (s[sp] != s[sp + 1]);
			break;
		default:
			return -1;
		}
	}
	return s[0];
}

cond_expr_t *cond_copy_expr(cond_expr_t * expr)
{
	cond_expr_t *cur, *head, *tail, *new_expr;
	tail = head = NULL;
	cur = expr;
	while (cur) {
		new_expr = (cond_expr_t *) malloc(sizeof(cond_expr_t));
		if (!new_expr)
			goto free_head;
		memset(new_expr, 0, sizeof(cond_expr_t));

		new_expr->expr_type = cur->expr_type;
		new_expr->bool = cur->bool;

		if (!head)
			head = new_expr;
		if (tail)
			tail->next = new_expr;
		tail = new_expr;
		cur = cur->next;
	}
	return head;

      free_head:
	while (head) {
		tail = head->next;
		free(head);
		head = tail;
	}
	return NULL;
}

/*
 * evaluate_cond_node evaluates the conditional stored in
 * a cond_node_t and if the result is different than the
 * current state of the node it sets the rules in the true/false
 * list appropriately. If the result of the expression is undefined
 * all of the rules are disabled for safety.
 */
static int evaluate_cond_node(policydb_t * p, cond_node_t * node)
{
	int new_state;
	cond_av_list_t *cur;

	new_state = cond_evaluate_expr(p, node->expr);
	if (new_state != node->cur_state) {
		node->cur_state = new_state;
		if (new_state == -1)
			WARN(NULL, "expression result was undefined - disabling all rules.");
		/* turn the rules on or off */
		for (cur = node->true_list; cur != NULL; cur = cur->next) {
			if (new_state <= 0) {
				cur->node->key.specified &= ~AVTAB_ENABLED;
			} else {
				cur->node->key.specified |= AVTAB_ENABLED;
			}
		}

		for (cur = node->false_list; cur != NULL; cur = cur->next) {
			/* -1 or 1 */
			if (new_state) {
				cur->node->key.specified &= ~AVTAB_ENABLED;
			} else {
				cur->node->key.specified |= AVTAB_ENABLED;
			}
		}
	}
	return 0;
}

/* precompute and simplify an expression if possible.  If left with !expression, change 
 * to expression and switch t and f. precompute expression for expressions with limited
 * number of bools.
 */
int cond_normalize_expr(policydb_t * p, cond_node_t * cn)
{
	cond_expr_t *ne, *e;
	cond_av_list_t *tmp;
	unsigned int i, j, orig_value[COND_MAX_BOOLS];
	int k;
	uint32_t test = 0x0;
	avrule_t *tmp2;

	cn->nbools = 0;

	memset(cn->bool_ids, 0, sizeof(cn->bool_ids));
	cn->expr_pre_comp = 0x0;

	/* take care of !expr case */
	ne = NULL;
	e = cn->expr;

	/* because it's RPN look at last element */
	while (e->next != NULL) {
		ne = e;
		e = e->next;
	}
	if (e->expr_type == COND_NOT) {
		if (ne) {
			ne->next = NULL;
		} else {	/* ne should never be NULL */
			ERR(NULL, "Found expr with no bools and only a ! - this should never happen.");
			return -1;
		}
		/* swap the true and false lists */
		tmp = cn->true_list;
		cn->true_list = cn->false_list;
		cn->false_list = tmp;
		tmp2 = cn->avtrue_list;
		cn->avtrue_list = cn->avfalse_list;
		cn->avfalse_list = tmp2;

		/* free the "not" node in the list */
		free(e);
	}

	/* find all the bools in the expression */
	for (e = cn->expr; e != NULL; e = e->next) {
		switch (e->expr_type) {
		case COND_BOOL:
			/* see if we've already seen this bool */
			if (!bool_present(e->bool, cn->bool_ids, cn->nbools)) {
				/* count em all but only record up to COND_MAX_BOOLS */
				if (cn->nbools < COND_MAX_BOOLS)
					cn->bool_ids[cn->nbools++] = e->bool;
				else
					cn->nbools++;
			}
			break;
		default:
			break;
		}
	}

	/* only precompute for exprs with <= COND_AX_BOOLS */
	if (cn->nbools <= COND_MAX_BOOLS) {
		/* save the default values for the bools so we can play with them */
		for (i = 0; i < cn->nbools; i++) {
			orig_value[i] =
			    p->bool_val_to_struct[cn->bool_ids[i] - 1]->state;
		}

		/* loop through all possible combinations of values for bools in expression */
		for (test = 0x0; test < (UINT32_C(1) << cn->nbools); test++) {
			/* temporarily set the value for all the bools in the
			 * expression using the corr.  bit in test */
			for (j = 0; j < cn->nbools; j++) {
				p->bool_val_to_struct[cn->bool_ids[j] -
						      1]->state =
				    (test & (UINT32_C(1) << j)) ? 1 : 0;
			}
			k = cond_evaluate_expr(p, cn->expr);
			if (k == -1) {
				ERR(NULL, "While testing expression, expression result "
				     "was undefined - this should never happen.");
				return -1;
			}
			/* set the bit if expression evaluates true */
			if (k)
				cn->expr_pre_comp |= UINT32_C(1) << test;
		}

		/* restore bool default values */
		for (i = 0; i < cn->nbools; i++)
			p->bool_val_to_struct[cn->bool_ids[i] - 1]->state =
			    orig_value[i];
	}
	return 0;
}

int evaluate_conds(policydb_t * p)
{
	int ret;
	cond_node_t *cur;

	for (cur = p->cond_list; cur != NULL; cur = cur->next) {
		ret = evaluate_cond_node(p, cur);
		if (ret)
			return ret;
	}
	return 0;
}

int ksu_cond_policydb_init(policydb_t * p)
{
	p->bool_val_to_struct = NULL;
	p->cond_list = NULL;
	if (ksu_avtab_init(&p->te_cond_avtab))
		return -1;

	return 0;
}

void cond_av_list_destroy(cond_av_list_t * list)
{
	cond_av_list_t *cur, *next;
	for (cur = list; cur != NULL; cur = next) {
		next = cur->next;
		/* the avtab_ptr_t node is destroy by the avtab */
		free(cur);
	}
}

void cond_expr_destroy(cond_expr_t * expr)
{
	cond_expr_t *cur_expr, *next_expr;

	if (!expr)
		return;

	for (cur_expr = expr; cur_expr != NULL; cur_expr = next_expr) {
		next_expr = cur_expr->next;
		free(cur_expr);
	}
}

void cond_node_destroy(cond_node_t * node)
{
	if (!node)
		return;

	cond_expr_destroy(node->expr);
	avrule_list_destroy(node->avtrue_list);
	avrule_list_destroy(node->avfalse_list);
	cond_av_list_destroy(node->true_list);
	cond_av_list_destroy(node->false_list);
}

void cond_list_destroy(cond_list_t * list)
{
	cond_node_t *next, *cur;

	if (list == NULL)
		return;

	for (cur = list; cur != NULL; cur = next) {
		next = cur->next;
		cond_node_destroy(cur);
		free(cur);
	}
}

void ksu_cond_policydb_destroy(policydb_t * p)
{
	if (p->bool_val_to_struct != NULL)
		free(p->bool_val_to_struct);
	ksu_avtab_destroy(&p->te_cond_avtab);
	cond_list_destroy(p->cond_list);
}

int ksu_cond_init_bool_indexes(policydb_t * p)
{
	if (p->bool_val_to_struct)
		free(p->bool_val_to_struct);
	p->bool_val_to_struct = (cond_bool_datum_t **)
	    calloc(p->p_bools.nprim, sizeof(cond_bool_datum_t *));
	if (!p->bool_val_to_struct)
		return -1;
	return 0;
}

int ksu_cond_destroy_bool(hashtab_key_t key, hashtab_datum_t datum, void *p
		      __attribute__ ((unused)))
{
	if (key)
		free(key);
	free(datum);
	return 0;
}

int ksu_cond_index_bool(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	cond_bool_datum_t *booldatum;

	booldatum = datum;
	p = datap;

	if (!booldatum->s.value || booldatum->s.value > p->p_bools.nprim)
		return -EINVAL;

	if (p->p_bool_val_to_name[booldatum->s.value - 1] != NULL)
		return -EINVAL;

	p->p_bool_val_to_name[booldatum->s.value - 1] = key;
	p->bool_val_to_struct[booldatum->s.value - 1] = booldatum;

	return 0;
}

static int bool_isvalid(cond_bool_datum_t * b)
{
	if (!(b->state == 0 || b->state == 1))
		return 0;
	return 1;
}

int ksu_cond_read_bool(policydb_t * p,
		   hashtab_t h,
		   struct policy_file *fp)
{
	char *key = 0;
	cond_bool_datum_t *booldatum;
	uint32_t buf[3], len;
	int rc;

	booldatum = malloc(sizeof(cond_bool_datum_t));
	if (!booldatum)
		return -1;
	memset(booldatum, 0, sizeof(cond_bool_datum_t));

	rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
	if (rc < 0)
		goto err;

	booldatum->s.value = le32_to_cpu(buf[0]);
	booldatum->state = le32_to_cpu(buf[1]);

	if (!bool_isvalid(booldatum))
		goto err;

	len = le32_to_cpu(buf[2]);
	if (str_read(&key, fp, len))
		goto err;

	if (p->policy_type != POLICY_KERN &&
	    p->policyvers >= MOD_POLICYDB_VERSION_TUNABLE_SEP) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto err;
		booldatum->flags = le32_to_cpu(buf[0]);
	}

	if (hashtab_insert(h, key, booldatum))
		goto err;

	return 0;
      err:
	ksu_cond_destroy_bool(key, booldatum, 0);
	return -1;
}

struct cond_insertf_data {
	struct policydb *p;
	cond_av_list_t *other;
	cond_av_list_t *head;
	cond_av_list_t *tail;
};

static int cond_insertf(avtab_t * a
			__attribute__ ((unused)), avtab_key_t * k,
			avtab_datum_t * d, void *ptr)
{
	struct cond_insertf_data *data = ptr;
	struct policydb *p = data->p;
	cond_av_list_t *other = data->other, *list, *cur;
	avtab_ptr_t node_ptr;
	uint8_t found;

	/*
	 * For type rules we have to make certain there aren't any
	 * conflicting rules by searching the te_avtab and the
	 * cond_te_avtab.
	 */
	if (k->specified & AVTAB_TYPE) {
		if (ksu_avtab_search(&p->te_avtab, k)) {
			WARN(NULL, "security: type rule already exists outside of a conditional.");
			return -1;
		}
		/*
		 * If we are reading the false list other will be a pointer to
		 * the true list. We can have duplicate entries if there is only
		 * 1 other entry and it is in our true list.
		 *
		 * If we are reading the true list (other == NULL) there shouldn't
		 * be any other entries.
		 */
		if (other) {
			node_ptr = ksu_avtab_search_node(&p->te_cond_avtab, k);
			if (node_ptr) {
				if (ksu_avtab_search_node_next
				    (node_ptr, k->specified)) {
					ERR(NULL, "security: too many conflicting type rules.");
					return -1;
				}
				found = 0;
				for (cur = other; cur != NULL; cur = cur->next) {
					if (cur->node == node_ptr) {
						found = 1;
						break;
					}
				}
				if (!found) {
					ERR(NULL, "security: conflicting type rules.");
					return -1;
				}
			}
		} else {
			if (ksu_avtab_search(&p->te_cond_avtab, k)) {
				ERR(NULL, "security: conflicting type rules when adding type rule for true.");
				return -1;
			}
		}
	}

	node_ptr = ksu_avtab_insert_nonunique(&p->te_cond_avtab, k, d);
	if (!node_ptr) {
		ERR(NULL, "security: could not insert rule.");
		return -1;
	}
	node_ptr->parse_context = (void *)1;

	list = malloc(sizeof(cond_av_list_t));
	if (!list)
		return -1;
	memset(list, 0, sizeof(cond_av_list_t));

	list->node = node_ptr;
	if (!data->head)
		data->head = list;
	else
		data->tail->next = list;
	data->tail = list;
	return 0;
}

static int cond_read_av_list(policydb_t * p, void *fp,
			     cond_av_list_t ** ret_list, cond_av_list_t * other)
{
	unsigned int i;
	int rc;
	uint32_t buf[1], len;
	struct cond_insertf_data data;

	*ret_list = NULL;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;

	len = le32_to_cpu(buf[0]);
	if (len == 0) {
		return 0;
	}

	data.p = p;
	data.other = other;
	data.head = NULL;
	data.tail = NULL;
	for (i = 0; i < len; i++) {
		rc = ksu_avtab_read_item(fp, p->policyvers, &p->te_cond_avtab,
				     cond_insertf, &data);
		if (rc) {
			cond_av_list_destroy(data.head);
			return rc;
		}

	}

	*ret_list = data.head;
	return 0;
}

static int expr_isvalid(policydb_t * p, cond_expr_t * expr)
{
	if (expr->expr_type <= 0 || expr->expr_type > COND_LAST) {
		WARN(NULL, "security: conditional expressions uses unknown operator.");
		return 0;
	}

	if (expr->bool > p->p_bools.nprim) {
		WARN(NULL, "security: conditional expressions uses unknown bool.");
		return 0;
	}
	return 1;
}

static int cond_read_node(policydb_t * p, cond_node_t * node, void *fp)
{
	uint32_t buf[2];
	int len, i, rc;
	cond_expr_t *expr = NULL, *last = NULL;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		goto err;

	node->cur_state = le32_to_cpu(buf[0]);

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		goto err;

	/* expr */
	len = le32_to_cpu(buf[0]);

	for (i = 0; i < len; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
		if (rc < 0)
			goto err;

		expr = malloc(sizeof(cond_expr_t));
		if (!expr) {
			goto err;
		}
		memset(expr, 0, sizeof(cond_expr_t));

		expr->expr_type = le32_to_cpu(buf[0]);
		expr->bool = le32_to_cpu(buf[1]);

		if (!expr_isvalid(p, expr)) {
			free(expr);
			goto err;
		}

		if (i == 0) {
			node->expr = expr;
		} else {
			last->next = expr;
		}
		last = expr;
	}

	if (p->policy_type == POLICY_KERN) {
		if (cond_read_av_list(p, fp, &node->true_list, NULL) != 0)
			goto err;
		if (cond_read_av_list(p, fp, &node->false_list, node->true_list)
		    != 0)
			goto err;
	} else {
		if (avrule_read_list(p, &node->avtrue_list, fp))
			goto err;
		if (avrule_read_list(p, &node->avfalse_list, fp))
			goto err;
	}

	if (p->policy_type != POLICY_KERN &&
	    p->policyvers >= MOD_POLICYDB_VERSION_TUNABLE_SEP) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto err;
		node->flags = le32_to_cpu(buf[0]);
	}

	return 0;
      err:
	cond_node_destroy(node);
	free(node);
	return -1;
}

int ksu_cond_read_list(policydb_t * p, cond_list_t ** list, void *fp)
{
	cond_node_t *node, *last = NULL;
	uint32_t buf[1];
	int i, len, rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;

	len = le32_to_cpu(buf[0]);

	rc = ksu_avtab_alloc(&p->te_cond_avtab, p->te_avtab.nel);
	if (rc)
		goto err;

	for (i = 0; i < len; i++) {
		node = malloc(sizeof(cond_node_t));
		if (!node)
			goto err;
		memset(node, 0, sizeof(cond_node_t));

		if (cond_read_node(p, node, fp) != 0)
			goto err;

		if (i == 0) {
			*list = node;
		} else {
			last->next = node;
		}
		last = node;
	}
	return 0;
      err:
	return -1;
}

/* Determine whether additional permissions are granted by the conditional
 * av table, and if so, add them to the result 
 */
void ksu_cond_compute_av(avtab_t * ctab, avtab_key_t * key,
		     struct sepol_av_decision *avd)
{
	avtab_ptr_t node;

	if (!ctab || !key || !avd)
		return;

	for (node = ksu_avtab_search_node(ctab, key); node != NULL;
	     node = ksu_avtab_search_node_next(node, key->specified)) {
		if ((uint16_t) (AVTAB_ALLOWED | AVTAB_ENABLED) ==
		    (node->key.specified & (AVTAB_ALLOWED | AVTAB_ENABLED)))
			avd->allowed |= node->datum.data;
		if ((uint16_t) (AVTAB_AUDITDENY | AVTAB_ENABLED) ==
		    (node->key.specified & (AVTAB_AUDITDENY | AVTAB_ENABLED)))
			/* Since a '0' in an auditdeny mask represents a 
			 * permission we do NOT want to audit (dontaudit), we use
			 * the '&' operand to ensure that all '0's in the mask
			 * are retained (much unlike the allow and auditallow cases).
			 */
			avd->auditdeny &= node->datum.data;
		if ((uint16_t) (AVTAB_AUDITALLOW | AVTAB_ENABLED) ==
		    (node->key.specified & (AVTAB_AUDITALLOW | AVTAB_ENABLED)))
			avd->auditallow |= node->datum.data;
	}
	return;
}

avtab_datum_t *cond_av_list_search(avtab_key_t * key,
				   cond_av_list_t * cond_list)
{

	cond_av_list_t *cur_av;

	for (cur_av = cond_list; cur_av != NULL; cur_av = cur_av->next) {

		if (cur_av->node->key.source_type == key->source_type &&
		    cur_av->node->key.target_type == key->target_type &&
		    cur_av->node->key.target_class == key->target_class)

			return &cur_av->node->datum;

	}
	return NULL;

}

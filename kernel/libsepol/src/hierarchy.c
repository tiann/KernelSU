/* Authors: Joshua Brindle <jbrindle@tresys.com>
 * 	    Jason Tang <jtang@tresys.com>
 *
 * Updates: KaiGai Kohei <kaigai@ak.jp.nec.com>
 *          adds checks based on newer boundary facility.
 *
 * A set of utility functions that aid policy decision when dealing
 * with hierarchal namespaces.
 *
 * Copyright (C) 2005 Tresys Technology, LLC
 *
 * Copyright (c) 2008 NEC Corporation
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

#include <linux/string.h>
// #include <stdlib.h>
// #include <assert.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/hierarchy.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/util.h>

#include "debug.h"

#define BOUNDS_AVTAB_SIZE 1024

static int bounds_insert_helper(sepol_handle_t *handle, avtab_t *avtab,
				avtab_key_t *avtab_key, avtab_datum_t *datum)
{
	int rc = avtab_insert(avtab, avtab_key, datum);
	if (rc) {
		if (rc == SEPOL_ENOMEM)
			ERR(handle, "Insufficient memory");
		else
			ERR(handle, "Unexpected error (%d)", rc);
	}
	return rc;
}


static int bounds_insert_rule(sepol_handle_t *handle, avtab_t *avtab,
			      avtab_t *global, avtab_t *other,
			      avtab_key_t *avtab_key, avtab_datum_t *datum)
{
	int rc = 0;
	avtab_datum_t *dup = ksu_avtab_search(avtab, avtab_key);

	if (!dup) {
		rc = bounds_insert_helper(handle, avtab, avtab_key, datum);
		if (rc) goto exit;
	} else {
		dup->data |= datum->data;
	}

	if (other) {
		/* Search the other conditional avtab for the key and
		 * add any common permissions to the global avtab
		 */
		uint32_t data = 0;
		dup = ksu_avtab_search(other, avtab_key);
		if (dup) {
			data = dup->data & datum->data;
			if (data) {
				dup = ksu_avtab_search(global, avtab_key);
				if (!dup) {
					avtab_datum_t d;
					d.data = data;
					rc = bounds_insert_helper(handle, global,
								  avtab_key, &d);
					if (rc) goto exit;
				} else {
					dup->data |= data;
				}
			}
		}
	}

exit:
	return rc;
}

static int bounds_expand_rule(sepol_handle_t *handle, policydb_t *p,
			      avtab_t *avtab, avtab_t *global, avtab_t *other,
			      uint32_t parent, uint32_t src, uint32_t tgt,
			      uint32_t class, uint32_t data)
{
	int rc = 0;
	avtab_key_t avtab_key;
	avtab_datum_t datum;
	ebitmap_node_t *tnode;
	unsigned int i;

	avtab_key.specified = AVTAB_ALLOWED;
	avtab_key.target_class = class;
	datum.data = data;

	if (ksu_ebitmap_get_bit(&p->attr_type_map[src - 1], parent - 1)) {
		avtab_key.source_type = parent;
		ebitmap_for_each_positive_bit(&p->attr_type_map[tgt - 1], tnode, i) {
			avtab_key.target_type = i + 1;
			rc = bounds_insert_rule(handle, avtab, global, other,
						&avtab_key, &datum);
			if (rc) goto exit;
		}
	}

exit:
	return rc;
}

static int bounds_expand_cond_rules(sepol_handle_t *handle, policydb_t *p,
				    cond_av_list_t *cur, avtab_t *avtab,
				    avtab_t *global, avtab_t *other,
				    uint32_t parent)
{
	int rc = 0;

	for (; cur; cur = cur->next) {
		avtab_ptr_t n = cur->node;
		rc = bounds_expand_rule(handle, p, avtab, global, other, parent,
					n->key.source_type, n->key.target_type,
					n->key.target_class, n->datum.data);
		if (rc) goto exit;
	}

exit:
	return rc;
}

struct bounds_expand_args {
	sepol_handle_t *handle;
	policydb_t *p;
	avtab_t *avtab;
	uint32_t parent;
};

static int bounds_expand_rule_callback(avtab_key_t *k, avtab_datum_t *d,
				       void *args)
{
	struct bounds_expand_args *a = (struct bounds_expand_args *)args;

	if (!(k->specified & AVTAB_ALLOWED))
		return 0;

	return bounds_expand_rule(a->handle, a->p, a->avtab, NULL, NULL,
				  a->parent, k->source_type, k->target_type,
				  k->target_class, d->data);
}

struct bounds_cond_info {
	avtab_t true_avtab;
	avtab_t false_avtab;
	cond_list_t *cond_list;
	struct bounds_cond_info *next;
};

static void bounds_destroy_cond_info(struct bounds_cond_info *cur)
{
	struct bounds_cond_info *next;

	for (; cur; cur = next) {
		next = cur->next;
		ksu_avtab_destroy(&cur->true_avtab);
		ksu_avtab_destroy(&cur->false_avtab);
		cur->next = NULL;
		free(cur);
	}
}

static int bounds_expand_parent_rules(sepol_handle_t *handle, policydb_t *p,
				      avtab_t *global_avtab,
				      struct bounds_cond_info **cond_info,
				      uint32_t parent)
{
	int rc = 0;
	struct bounds_expand_args args;
	cond_list_t *cur;

	ksu_avtab_init(global_avtab);
	rc = ksu_avtab_alloc(global_avtab, BOUNDS_AVTAB_SIZE);
	if (rc) goto oom;

	args.handle = handle;
	args.p = p;
	args.avtab = global_avtab;
	args.parent = parent;
	rc = avtab_map(&p->te_avtab, bounds_expand_rule_callback, &args);
	if (rc) goto exit;

	*cond_info = NULL;
	for (cur = p->cond_list; cur; cur = cur->next) {
		struct bounds_cond_info *ci;
		ci = malloc(sizeof(struct bounds_cond_info));
		if (!ci) goto oom;
		ksu_avtab_init(&ci->true_avtab);
		ksu_avtab_init(&ci->false_avtab);
		ci->cond_list = cur;
		ci->next = *cond_info;
		*cond_info = ci;
		if (cur->true_list) {
			rc = ksu_avtab_alloc(&ci->true_avtab, BOUNDS_AVTAB_SIZE);
			if (rc) goto oom;
			rc = bounds_expand_cond_rules(handle, p, cur->true_list,
						      &ci->true_avtab, NULL,
						      NULL, parent);
			if (rc) goto exit;
		}
		if (cur->false_list) {
			rc = ksu_avtab_alloc(&ci->false_avtab, BOUNDS_AVTAB_SIZE);
			if (rc) goto oom;
			rc = bounds_expand_cond_rules(handle, p, cur->false_list,
						      &ci->false_avtab,
						      global_avtab,
						      &ci->true_avtab, parent);
			if (rc) goto exit;
		}
	}

	return 0;

oom:
	ERR(handle, "Insufficient memory");

exit:
	ERR(handle,"Failed to expand parent rules");
	ksu_avtab_destroy(global_avtab);
	bounds_destroy_cond_info(*cond_info);
	*cond_info = NULL;
	return rc;
}

static int bounds_not_covered(avtab_t *global_avtab, avtab_t *cur_avtab,
			      avtab_key_t *avtab_key, uint32_t data)
{
	avtab_datum_t *datum = ksu_avtab_search(cur_avtab, avtab_key);
	if (datum)
		data &= ~datum->data;
	if (global_avtab && data) {
		datum = ksu_avtab_search(global_avtab, avtab_key);
		if (datum)
			data &= ~datum->data;
	}

	return data;
}

static int bounds_add_bad(sepol_handle_t *handle, uint32_t src, uint32_t tgt,
			  uint32_t class, uint32_t data, avtab_ptr_t *bad)
{
	struct avtab_node *new = malloc(sizeof(struct avtab_node));
	if (new == NULL) {
		ERR(handle, "Insufficient memory");
		return SEPOL_ENOMEM;
	}
	memset(new, 0, sizeof(struct avtab_node));
	new->key.source_type = src;
	new->key.target_type = tgt;
	new->key.target_class = class;
	new->datum.data = data;
	new->next = *bad;
	*bad = new;

	return 0;
}

static int bounds_check_rule(sepol_handle_t *handle, policydb_t *p,
			     avtab_t *global_avtab, avtab_t *cur_avtab,
			     uint32_t child, uint32_t parent, uint32_t src,
			     uint32_t tgt, uint32_t class, uint32_t data,
			     avtab_ptr_t *bad, int *numbad)
{
	int rc = 0;
	avtab_key_t avtab_key;
	type_datum_t *td;
	ebitmap_node_t *tnode;
	unsigned int i;
	uint32_t d;

	avtab_key.specified = AVTAB_ALLOWED;
	avtab_key.target_class = class;

	if (ksu_ebitmap_get_bit(&p->attr_type_map[src - 1], child - 1)) {
		avtab_key.source_type = parent;
		ebitmap_for_each_positive_bit(&p->attr_type_map[tgt - 1], tnode, i) {
			td = p->type_val_to_struct[i];
			if (td && td->bounds) {
				avtab_key.target_type = td->bounds;
				d = bounds_not_covered(global_avtab, cur_avtab,
						       &avtab_key, data);
			} else {
				avtab_key.target_type = i + 1;
				d = bounds_not_covered(global_avtab, cur_avtab,
						       &avtab_key, data);
			}
			if (d) {
				(*numbad)++;
				rc = bounds_add_bad(handle, child, i+1, class, d, bad);
				if (rc) goto exit;
			}
		}
	}

exit:
	return rc;
}

static int bounds_check_cond_rules(sepol_handle_t *handle, policydb_t *p,
				   avtab_t *global_avtab, avtab_t *cond_avtab,
				   cond_av_list_t *rules, uint32_t child,
				   uint32_t parent, avtab_ptr_t *bad,
				   int *numbad)
{
	int rc = 0;
	cond_av_list_t *cur;

	for (cur = rules; cur; cur = cur->next) {
		avtab_ptr_t ap = cur->node;
		avtab_key_t *key = &ap->key;
		avtab_datum_t *datum = &ap->datum;
		if (!(key->specified & AVTAB_ALLOWED))
			continue;
		rc = bounds_check_rule(handle, p, global_avtab, cond_avtab,
				       child, parent, key->source_type,
				       key->target_type, key->target_class,
				       datum->data, bad, numbad);
		if (rc) goto exit;
	}

exit:
	return rc;
}

struct bounds_check_args {
	sepol_handle_t *handle;
	policydb_t *p;
	avtab_t *cur_avtab;
	uint32_t child;
	uint32_t parent;
	avtab_ptr_t bad;
	int numbad;
};

static int bounds_check_rule_callback(avtab_key_t *k, avtab_datum_t *d,
				      void *args)
{
	struct bounds_check_args *a = (struct bounds_check_args *)args;

	if (!(k->specified & AVTAB_ALLOWED))
		return 0;

	return bounds_check_rule(a->handle, a->p, NULL, a->cur_avtab, a->child,
				 a->parent, k->source_type, k->target_type,
				 k->target_class, d->data, &a->bad, &a->numbad);
}

static int bounds_check_child_rules(sepol_handle_t *handle, policydb_t *p,
				    avtab_t *global_avtab,
				    struct bounds_cond_info *cond_info,
				    uint32_t child, uint32_t parent,
				    avtab_ptr_t *bad, int *numbad)
{
	int rc;
	struct bounds_check_args args;
	struct bounds_cond_info *cur;

	args.handle = handle;
	args.p = p;
	args.cur_avtab = global_avtab;
	args.child = child;
	args.parent = parent;
	args.bad = NULL;
	args.numbad = 0;
	rc = avtab_map(&p->te_avtab, bounds_check_rule_callback, &args);
	if (rc) goto exit;

	for (cur = cond_info; cur; cur = cur->next) {
		cond_list_t *node = cur->cond_list;
		rc = bounds_check_cond_rules(handle, p, global_avtab,
					     &cur->true_avtab,
					     node->true_list, child, parent,
					     &args.bad, &args.numbad);
		if (rc) goto exit;

		rc = bounds_check_cond_rules(handle, p, global_avtab,
					     &cur->false_avtab,
					     node->false_list, child, parent,
					     &args.bad, &args.numbad);
		if (rc) goto exit;
	}

	*numbad += args.numbad;
	*bad = args.bad;

exit:
	return rc;
}

int bounds_check_type(sepol_handle_t *handle, policydb_t *p, uint32_t child,
		      uint32_t parent, avtab_ptr_t *bad, int *numbad)
{
	int rc = 0;
	avtab_t global_avtab;
	struct bounds_cond_info *cond_info = NULL;

	rc = bounds_expand_parent_rules(handle, p, &global_avtab, &cond_info, parent);
	if (rc) goto exit;

	rc = bounds_check_child_rules(handle, p, &global_avtab, cond_info,
				      child, parent, bad, numbad);

	bounds_destroy_cond_info(cond_info);
	ksu_avtab_destroy(&global_avtab);

exit:
	return rc;
}

struct bounds_args {
	sepol_handle_t *handle;
	policydb_t *p;
	int numbad;
};

static void bounds_report(sepol_handle_t *handle, policydb_t *p, uint32_t child,
			  uint32_t parent, avtab_ptr_t cur)
{
	ERR(handle, "Child type %s exceeds bounds of parent %s in the following rules:",
	    p->p_type_val_to_name[child - 1],
	    p->p_type_val_to_name[parent - 1]);
	for (; cur; cur = cur->next) {
		ERR(handle, "    %s %s : %s { %s }",
		    p->p_type_val_to_name[cur->key.source_type - 1],
		    p->p_type_val_to_name[cur->key.target_type - 1],
		    p->p_class_val_to_name[cur->key.target_class - 1],
		    sepol_av_to_string(p, cur->key.target_class,
				       cur->datum.data));
	}
}

void bounds_destroy_bad(avtab_ptr_t cur)
{
	avtab_ptr_t next;

	for (; cur; cur = next) {
		next = cur->next;
		cur->next = NULL;
		free(cur);
	}
}

static int bounds_check_type_callback(hashtab_key_t k __attribute__ ((unused)),
				      hashtab_datum_t d, void *args)
{
	int rc = 0;
	struct bounds_args *a = (struct bounds_args *)args;
	type_datum_t *t = (type_datum_t *)d;
	avtab_ptr_t bad = NULL;

	if (t->bounds) {
		rc = bounds_check_type(a->handle, a->p, t->s.value, t->bounds,
				       &bad, &a->numbad);
		if (bad) {
			bounds_report(a->handle, a->p, t->s.value, t->bounds,
				      bad);
			bounds_destroy_bad(bad);
		}
	}

	return rc;
}

int bounds_check_types(sepol_handle_t *handle, policydb_t *p)
{
	int rc;
	struct bounds_args args;

	args.handle = handle;
	args.p = p;
	args.numbad = 0;

	rc = ksu_hashtab_map(p->p_types.table, bounds_check_type_callback, &args);
	if (rc) goto exit;

	if (args.numbad > 0) {
		ERR(handle, "%d errors found during type bounds check",
		    args.numbad);
		rc = SEPOL_ERR;
	}

exit:
	return rc;
}

/* The role bounds is defined as: a child role cannot have a type that
 * its parent doesn't have.
 */
static int bounds_check_role_callback(hashtab_key_t k,
				      hashtab_datum_t d, void *args)
{
	struct bounds_args *a = (struct bounds_args *)args;
	role_datum_t *r = (role_datum_t *) d;
	role_datum_t *rp = NULL;

	if (!r->bounds)
		return 0;

	rp = a->p->role_val_to_struct[r->bounds - 1];

	if (rp && !ksu_ebitmap_contains(&rp->types.types, &r->types.types)) {
		ERR(a->handle, "Role bounds violation, %s exceeds %s",
		    (char *)k, a->p->p_role_val_to_name[rp->s.value - 1]);
		a->numbad++;
	}

	return 0;
}

int bounds_check_roles(sepol_handle_t *handle, policydb_t *p)
{
	struct bounds_args args;

	args.handle = handle;
	args.p = p;
	args.numbad = 0;

	ksu_hashtab_map(p->p_roles.table, bounds_check_role_callback, &args);

	if (args.numbad > 0) {
		ERR(handle, "%d errors found during role bounds check",
		    args.numbad);
		return SEPOL_ERR;
	}

	return 0;
}

/* The user bounds is defined as: a child user cannot have a role that
 * its parent doesn't have.
 */
static int bounds_check_user_callback(hashtab_key_t k,
				      hashtab_datum_t d, void *args)
{
	struct bounds_args *a = (struct bounds_args *)args;
	user_datum_t *u = (user_datum_t *) d;
	user_datum_t *up = NULL;

	if (!u->bounds)
		return 0;

	up = a->p->user_val_to_struct[u->bounds - 1];

	if (up && !ksu_ebitmap_contains(&up->roles.roles, &u->roles.roles)) {
		ERR(a->handle, "User bounds violation, %s exceeds %s",
		    (char *) k, a->p->p_user_val_to_name[up->s.value - 1]);
		a->numbad++;
	}

	return 0;
}

int bounds_check_users(sepol_handle_t *handle, policydb_t *p)
{
	struct bounds_args args;

	args.handle = handle;
	args.p = p;
	args.numbad = 0;

	ksu_hashtab_map(p->p_users.table, bounds_check_user_callback, &args);

	if (args.numbad > 0) {
		ERR(handle, "%d errors found during user bounds check",
		    args.numbad);
		return SEPOL_ERR;
	}

	return 0;
}

#define add_hierarchy_callback_template(prefix)				\
	int hierarchy_add_##prefix##_callback(hashtab_key_t k __attribute__ ((unused)), \
					    hashtab_datum_t d, void *args) \
{								\
	struct bounds_args *a = (struct bounds_args *)args;		\
	sepol_handle_t *handle = a->handle;				\
	policydb_t *p = a->p;						\
	prefix##_datum_t *datum = (prefix##_datum_t *)d;		\
	prefix##_datum_t *parent;					\
	char *parent_name, *datum_name, *tmp;				\
									\
	if (!datum->bounds) {						\
		datum_name = p->p_##prefix##_val_to_name[datum->s.value - 1]; \
									\
		tmp = strrchr(datum_name, '.');				\
		/* no '.' means it has no parent */			\
		if (!tmp) return 0;					\
									\
		parent_name = strdup(datum_name);			\
		if (!parent_name) {					\
			ERR(handle, "Insufficient memory");		\
			return SEPOL_ENOMEM;				\
		}							\
		parent_name[tmp - datum_name] = '\0';			\
									\
		parent = hashtab_search(p->p_##prefix##s.table, parent_name); \
		if (!parent) {						\
			/* Orphan type/role/user */			\
			ERR(handle, "%s doesn't exist, %s is an orphan",\
			    parent_name,				\
			    p->p_##prefix##_val_to_name[datum->s.value - 1]); \
			free(parent_name);				\
			a->numbad++;					\
			return 0;					\
		}							\
		datum->bounds = parent->s.value;			\
		free(parent_name);					\
	}								\
									\
	return 0;							\
}								\

static add_hierarchy_callback_template(type)
static add_hierarchy_callback_template(role)
static add_hierarchy_callback_template(user)

int hierarchy_add_bounds(sepol_handle_t *handle, policydb_t *p)
{
	int rc = 0;
	struct bounds_args args;

	args.handle = handle;
	args.p = p;
	args.numbad = 0;

	rc = ksu_hashtab_map(p->p_users.table, hierarchy_add_user_callback, &args);
	if (rc) goto exit;

	rc = ksu_hashtab_map(p->p_roles.table, hierarchy_add_role_callback, &args);
	if (rc) goto exit;

	rc = ksu_hashtab_map(p->p_types.table, hierarchy_add_type_callback, &args);
	if (rc) goto exit;

	if (args.numbad > 0) {
		ERR(handle, "%d errors found while adding hierarchies",
		    args.numbad);
		rc = SEPOL_ERR;
	}

exit:
	return rc;
}

int hierarchy_check_constraints(sepol_handle_t * handle, policydb_t * p)
{
	int rc = 0;
	int violation = 0;

	rc = hierarchy_add_bounds(handle, p);
	if (rc) goto exit;

	rc = bounds_check_users(handle, p);
	if (rc)
		violation = 1;

	rc = bounds_check_roles(handle, p);
	if (rc)
		violation = 1;

	rc = bounds_check_types(handle, p);
	if (rc) {
		if (rc == SEPOL_ERR)
			violation = 1;
		else
			goto exit;
	}

	if (violation)
		rc = SEPOL_ERR;

exit:
	return rc;
}

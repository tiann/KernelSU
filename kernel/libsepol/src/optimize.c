/*
 * Author: Ondrej Mosnacek <omosnacek@gmail.com>
 *
 * Copyright (C) 2019 Red Hat Inc.
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

/*
 * Binary policy optimization.
 *
 * Defines the policydb_optimize() function, which finds and removes
 * redundant rules from the binary policy to reduce its size and potentially
 * improve rule matching times. Only rules that are already covered by a
 * more general rule are removed. The resulting policy is functionally
 * equivalent to the original one.
 */

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>

#include "debug.h"
#include "private.h"

#define TYPE_VEC_INIT_SIZE 16

struct type_vec {
	uint32_t *types;
	unsigned int count, capacity;
};

static int type_vec_init(struct type_vec *v)
{
	v->capacity = TYPE_VEC_INIT_SIZE;
	v->count = 0;
	v->types = calloc(v->capacity, sizeof(*v->types));
	if (!v->types)
		return -1;
	return 0;
}

static void type_vec_destroy(struct type_vec *v)
{
	free(v->types);
}

static int type_vec_append(struct type_vec *v, uint32_t type)
{
	if (v->capacity == v->count) {
		unsigned int new_capacity = v->capacity * 2;
		uint32_t *new_types = reallocarray(v->types,
						   new_capacity,
						   sizeof(*v->types));
		if (!new_types)
			return -1;

		v->types = new_types;
		v->capacity = new_capacity;
	}

	v->types[v->count++] = type;
	return 0;
}

static int type_vec_contains(const struct type_vec *v, uint32_t type)
{
	unsigned int s = 0, e = v->count;

	while (s != e) {
		unsigned int mid = (s + e) / 2;

		if (v->types[mid] == type)
			return 1;

		if (v->types[mid] < type)
			s = mid + 1;
		else
			e = mid;
	}
	return 0;
}

/* builds map: type/attribute -> {all attributes that are a superset of it} */
static struct type_vec *build_type_map(const policydb_t *p)
{
	unsigned int i, k;
	ebitmap_node_t *n;
	struct type_vec *map = calloc(p->p_types.nprim, sizeof(*map));
	if (!map)
		return NULL;

	for (i = 0; i < p->p_types.nprim; i++) {
		if (type_vec_init(&map[i]))
			goto err;

		if (!p->type_val_to_struct[i])
			continue;

		if (p->type_val_to_struct[i]->flavor != TYPE_ATTRIB) {
			ebitmap_for_each_positive_bit(&p->type_attr_map[i],
						      n, k) {
				if (type_vec_append(&map[i], k))
					goto err;
			}
		} else {
			ebitmap_t *types_i = &p->attr_type_map[i];

			for (k = 0; k < p->p_types.nprim; k++) {
				const ebitmap_t *types_k;

				if (!p->type_val_to_struct[k] || p->type_val_to_struct[k]->flavor != TYPE_ATTRIB)
					continue;

				types_k = &p->attr_type_map[k];

				if (ksu_ebitmap_contains(types_k, types_i)) {
					if (type_vec_append(&map[i], k))
						goto err;
				}
			}
		}
	}
	return map;
err:
	for (k = 0; k <= i; k++)
		type_vec_destroy(&map[k]);
	free(map);
	return NULL;
}

static void destroy_type_map(const policydb_t *p, struct type_vec *type_map)
{
	unsigned int i;
	for (i = 0; i < p->p_types.nprim; i++)
		type_vec_destroy(&type_map[i]);
	free(type_map);
}

static int process_xperms(uint32_t *p1, const uint32_t *p2)
{
	size_t i;
	int ret = 1;

	for (i = 0; i < EXTENDED_PERMS_LEN; i++) {
		p1[i] &= ~p2[i];
		if (p1[i] != 0)
			ret = 0;
	}
	return ret;
}

static int process_avtab_datum(uint16_t specified,
			       avtab_datum_t *d1, const avtab_datum_t *d2)
{
	/* inverse logic needed for AUDITDENY rules */
	if (specified & AVTAB_AUDITDENY)
		return (d1->data |= ~d2->data) == UINT32_C(0xFFFFFFFF);

	if (specified & AVTAB_AV)
		return (d1->data &= ~d2->data) == 0;

	if (specified & AVTAB_XPERMS) {
		avtab_extended_perms_t *x1 = d1->xperms;
		const avtab_extended_perms_t *x2 = d2->xperms;

		if (x1->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
			if (x2->specified == AVTAB_XPERMS_IOCTLFUNCTION) {
				if (x1->driver != x2->driver)
					return 0;
				return process_xperms(x1->perms, x2->perms);
			}
			if (x2->specified == AVTAB_XPERMS_IOCTLDRIVER)
				return xperm_test(x1->driver, x2->perms);
		} else if (x1->specified == AVTAB_XPERMS_IOCTLDRIVER) {
			if (x2->specified == AVTAB_XPERMS_IOCTLFUNCTION)
				return 0;

			if (x2->specified == AVTAB_XPERMS_IOCTLDRIVER)
				return process_xperms(x1->perms, x2->perms);
		}
		return 0;
	}
	return 0;
}

/* checks if avtab contains a rule that covers the given rule */
static int is_avrule_redundant(avtab_ptr_t entry, avtab_t *tab,
			       const struct type_vec *type_map,
			       unsigned char not_cond)
{
	unsigned int i, k, s_idx, t_idx;
	uint32_t st, tt;
	avtab_datum_t *d1, *d2;
	avtab_key_t key;

	/* we only care about AV rules */
	if (!(entry->key.specified & (AVTAB_AV|AVTAB_XPERMS)))
		return 0;

	s_idx = entry->key.source_type - 1;
	t_idx = entry->key.target_type - 1;

	key.target_class = entry->key.target_class;
	key.specified    = entry->key.specified;

	d1 = &entry->datum;

	for (i = 0; i < type_map[s_idx].count; i++) {
		st = type_map[s_idx].types[i];
		key.source_type = st + 1;

		for (k = 0; k < type_map[t_idx].count; k++) {
			tt = type_map[t_idx].types[k];

			if (not_cond && s_idx == st && t_idx == tt)
				continue;

			key.target_type = tt + 1;

			d2 = ksu_avtab_search(tab, &key);
			if (!d2)
				continue;

			if (process_avtab_datum(key.specified, d1, d2))
				return 1;
		}
	}
	return 0;
}

static int is_type_attr(policydb_t *p, unsigned int id)
{
	return p->type_val_to_struct[id]->flavor == TYPE_ATTRIB;
}

static int is_avrule_with_attr(avtab_ptr_t entry, policydb_t *p)
{
	unsigned int s_idx = entry->key.source_type - 1;
	unsigned int t_idx = entry->key.target_type - 1;

	return is_type_attr(p, s_idx) || is_type_attr(p, t_idx);
}

/* checks if conditional list contains a rule that covers the given rule */
static int is_cond_rule_redundant(avtab_ptr_t e1, cond_av_list_t *list,
				  const struct type_vec *type_map)
{
	unsigned int s1, t1, c1, k1, s2, t2, c2, k2;

	/* we only care about AV rules */
	if (!(e1->key.specified & (AVTAB_AV|AVTAB_XPERMS)))
		return 0;

	s1 = e1->key.source_type - 1;
	t1 = e1->key.target_type - 1;
	c1 = e1->key.target_class;
	k1 = e1->key.specified;

	for (; list; list = list->next) {
		avtab_ptr_t e2 = list->node;

		s2 = e2->key.source_type - 1;
		t2 = e2->key.target_type - 1;
		c2 = e2->key.target_class;
		k2 = e2->key.specified;

		if (k1 != k2 || c1 != c2)
			continue;

		if (s1 == s2 && t1 == t2)
			continue;
		if (!type_vec_contains(&type_map[s1], s2))
			continue;
		if (!type_vec_contains(&type_map[t1], t2))
			continue;

		if (process_avtab_datum(k1, &e1->datum, &e2->datum))
			return 1;
	}
	return 0;
}

static void optimize_avtab(policydb_t *p, const struct type_vec *type_map)
{
	avtab_t *tab = &p->te_avtab;
	unsigned int i;
	avtab_ptr_t *cur;

	for (i = 0; i < tab->nslot; i++) {
		cur = &tab->htable[i];
		while (*cur) {
			if (is_avrule_redundant(*cur, tab, type_map, 1)) {
				/* redundant rule -> remove it */
				avtab_ptr_t tmp = *cur;

				*cur = tmp->next;
				if (tmp->key.specified & AVTAB_XPERMS)
					free(tmp->datum.xperms);
				free(tmp);

				tab->nel--;
			} else {
				/* rule not redundant -> move to next rule */
				cur = &(*cur)->next;
			}
		}
	}
}

/* find redundant rules in (*cond) and put them into (*del) */
static void optimize_cond_av_list(cond_av_list_t **cond, cond_av_list_t **del,
				  policydb_t *p, const struct type_vec *type_map)
{
	cond_av_list_t **listp = cond;
	cond_av_list_t *pcov = NULL;
	cond_av_list_t **pcov_cur;

	/*
	 * Separate out all "potentially covering" rules (src or tgt is an attr)
	 * and move them to the end of the list. This is needed to avoid
	 * polynomial complexity when almost all rules are expanded.
	 */
	while (*cond) {
		if (is_avrule_with_attr((*cond)->node, p)) {
			cond_av_list_t *tmp = *cond;

			*cond = tmp->next;
			tmp->next = pcov;
			pcov = tmp;
		} else {
			cond = &(*cond)->next;
		}
	}
	/* link the "potentially covering" rules to the end of the list */
	*cond = pcov;

	/* now go through the list and find the redundant rules */
	cond = listp;
	pcov_cur = &pcov;
	while (*cond) {
		/* needed because pcov itself may get deleted */
		if (*cond == pcov)
			pcov_cur = cond;
		/*
		 * First check if covered by an unconditional rule, then also
		 * check if covered by another rule in the same list.
		 */
		if (is_avrule_redundant((*cond)->node, &p->te_avtab, type_map, 0) ||
		    is_cond_rule_redundant((*cond)->node, *pcov_cur, type_map)) {
			cond_av_list_t *tmp = *cond;

			*cond = tmp->next;
			tmp->next = *del;
			*del = tmp;
		} else {
			cond = &(*cond)->next;
		}
	}
}

static void optimize_cond_avtab(policydb_t *p, const struct type_vec *type_map)
{
	avtab_t *tab = &p->te_cond_avtab;
	unsigned int i;
	avtab_ptr_t *cur;
	cond_node_t **cond;
	cond_av_list_t **avcond, *del = NULL;

	/* First go through all conditionals and collect redundant rules. */
	cond = &p->cond_list;
	while (*cond) {
		optimize_cond_av_list(&(*cond)->true_list,  &del, p, type_map);
		optimize_cond_av_list(&(*cond)->false_list, &del, p, type_map);
		/* TODO: maybe also check for rules present in both lists */

		/* nothing left in both lists -> remove the whole conditional */
		if (!(*cond)->true_list && !(*cond)->false_list) {
			cond_node_t *cond_tmp = *cond;

			*cond = cond_tmp->next;
			cond_node_destroy(cond_tmp);
			free(cond_tmp);
		} else {
			cond = &(*cond)->next;
		}
	}

	if (!del)
		return;

	/*
	 * Now go through the whole cond_avtab and remove all rules that are
	 * found in the 'del' list.
	 */
	for (i = 0; i < tab->nslot; i++) {
		cur = &tab->htable[i];
		while (*cur) {
			int redundant = 0;
			avcond = &del;
			while (*avcond) {
				if ((*avcond)->node == *cur) {
					cond_av_list_t *cond_tmp = *avcond;

					*avcond = cond_tmp->next;
					free(cond_tmp);
					redundant = 1;
					break;
				} else {
					avcond = &(*avcond)->next;
				}
			}
			if (redundant) {
				avtab_ptr_t tmp = *cur;

				*cur = tmp->next;
				if (tmp->key.specified & AVTAB_XPERMS)
					free(tmp->datum.xperms);
				free(tmp);

				tab->nel--;
			} else {
				cur = &(*cur)->next;
			}
		}
	}
}

int policydb_optimize(policydb_t *p)
{
	struct type_vec *type_map;

	if (p->policy_type != POLICY_KERN)
		return -1;

	if (p->policyvers >= POLICYDB_VERSION_AVTAB && p->policyvers <= POLICYDB_VERSION_PERMISSIVE) {
		/*
		 * For policy versions between 20 and 23, attributes exist in the policy,
		 * but only in the type_attr_map. This means that there are gaps in both
		 * the type_val_to_struct and p_type_val_to_name arrays and policy rules
		 * can refer to those gaps.
		 */
		ERR(NULL, "Optimizing policy versions between 20 and 23 is not supported");
		return -1;
	}

	type_map = build_type_map(p);
	if (!type_map)
		return -1;

	optimize_avtab(p, type_map);
	optimize_cond_avtab(p, type_map);

	destroy_type_map(p, type_map);
	return 0;
}

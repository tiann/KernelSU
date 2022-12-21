
/* Author : Stephen Smalley, <sds@tycho.nsa.gov> */

/*
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 *
 *	Support for enhanced MLS infrastructure.
 *
 * Updated: Frank Mayer <mayerf@tresys.com> and Karl MacMillan <kmacmillan@tresys.com>
 *
 * 	Added conditional policy language extensions
 * 
 * Updated: Joshua Brindle <jbrindle@tresys.com> and Jason Tang <jtang@tresys.org>
 *
 *	Module writing support
 *
 * Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
 * Copyright (C) 2003-2005 Tresys Technology, LLC
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
// #include <assert.h>
// #include <stdlib.h>

#include <sepol/policydb/ebitmap.h>
#include <sepol/policydb/avtab.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/expand.h>

#include "debug.h"
#include "private.h"
#include "mls.h"

#define glblub_version ((p->policy_type == POLICY_KERN && \
		     p->policyvers >= POLICYDB_VERSION_GLBLUB) || \
		    (p->policy_type == POLICY_BASE && \
		     p->policyvers >= MOD_POLICYDB_VERSION_GLBLUB))

struct policy_data {
	struct policy_file *fp;
	struct policydb *p;
};

static int avrule_write_list(policydb_t *p,
			     avrule_t * avrules, struct policy_file *fp);

static int ebitmap_write(ebitmap_t * e, struct policy_file *fp)
{
	ebitmap_node_t *n;
	uint32_t buf[32], bit, count;
	uint64_t map;
	size_t items;

	buf[0] = cpu_to_le32(MAPSIZE);
	buf[1] = cpu_to_le32(e->highbit);

	count = 0;
	for (n = e->node; n; n = n->next)
		count++;
	buf[2] = cpu_to_le32(count);

	items = put_entry(buf, sizeof(uint32_t), 3, fp);
	if (items != 3)
		return POLICYDB_ERROR;

	for (n = e->node; n; n = n->next) {
		bit = cpu_to_le32(n->startbit);
		items = put_entry(&bit, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		map = cpu_to_le64(n->map);
		items = put_entry(&map, sizeof(uint64_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;

	}

	return POLICYDB_SUCCESS;
}

/* Ordering of datums in the original avtab format in the policy file. */
static uint16_t spec_order[] = {
	AVTAB_ALLOWED,
	AVTAB_AUDITDENY,
	AVTAB_AUDITALLOW,
	AVTAB_TRANSITION,
	AVTAB_CHANGE,
	AVTAB_MEMBER
};

static int avtab_write_item(policydb_t * p,
			    avtab_ptr_t cur, struct policy_file *fp,
			    unsigned merge, unsigned commit, uint32_t * nel)
{
	avtab_ptr_t node;
	uint8_t buf8;
	uint16_t buf16[4];
	uint32_t buf32[10], lookup, val;
	size_t items, items2;
	unsigned set;
	unsigned int oldvers = (p->policy_type == POLICY_KERN
				&& p->policyvers < POLICYDB_VERSION_AVTAB);
	unsigned int i;

	if (oldvers) {
		/* Generate the old avtab format.
		   Requires merging similar entries if uncond avtab. */
		if (merge) {
			if (cur->merged)
				return POLICYDB_SUCCESS;	/* already merged by prior merge */
		}

		items = 1;	/* item 0 is used for the item count */
		val = cur->key.source_type;
		buf32[items++] = cpu_to_le32(val);
		val = cur->key.target_type;
		buf32[items++] = cpu_to_le32(val);
		val = cur->key.target_class;
		buf32[items++] = cpu_to_le32(val);

		val = cur->key.specified & ~AVTAB_ENABLED;
		if (cur->key.specified & AVTAB_ENABLED)
			val |= AVTAB_ENABLED_OLD;
		set = 1;

		if (merge) {
			/* Merge specifier values for all similar (av or type)
			   entries that have the same key. */
			if (val & AVTAB_AV)
				lookup = AVTAB_AV;
			else if (val & AVTAB_TYPE)
				lookup = AVTAB_TYPE;
			else
				return POLICYDB_ERROR;
			for (node = ksu_avtab_search_node_next(cur, lookup);
			     node;
			     node = ksu_avtab_search_node_next(node, lookup)) {
				val |= (node->key.specified & ~AVTAB_ENABLED);
				set++;
				if (node->key.specified & AVTAB_ENABLED)
					val |= AVTAB_ENABLED_OLD;
			}
		}

		if (!(val & (AVTAB_AV | AVTAB_TYPE))) {
			ERR(fp->handle, "null entry");
			return POLICYDB_ERROR;
		}
		if ((val & AVTAB_AV) && (val & AVTAB_TYPE)) {
			ERR(fp->handle, "entry has both access "
			    "vectors and types");
			return POLICYDB_ERROR;
		}

		buf32[items++] = cpu_to_le32(val);

		if (merge) {
			/* Include datums for all similar (av or type)
			   entries that have the same key. */
			for (i = 0;
			     i < (sizeof(spec_order) / sizeof(spec_order[0]));
			     i++) {
				if (val & spec_order[i]) {
					if (cur->key.specified & spec_order[i])
						node = cur;
					else {
						node =
						    ksu_avtab_search_node_next(cur,
									   spec_order
									   [i]);
						if (nel)
							(*nel)--;	/* one less node */
					}

					if (!node) {
						ERR(fp->handle, "missing node");
						return POLICYDB_ERROR;
					}
					buf32[items++] =
					    cpu_to_le32(node->datum.data);
					set--;
					node->merged = 1;
				}
			}
		} else {
			buf32[items++] = cpu_to_le32(cur->datum.data);
			cur->merged = 1;
			set--;
		}

		if (set) {
			ERR(fp->handle, "data count wrong");
			return POLICYDB_ERROR;
		}

		buf32[0] = cpu_to_le32(items - 1);

		if (commit) {
			/* Commit this item to the policy file. */
			items2 = put_entry(buf32, sizeof(uint32_t), items, fp);
			if (items != items2)
				return POLICYDB_ERROR;
		}

		return POLICYDB_SUCCESS;
	}

	/* Generate the new avtab format. */
	buf16[0] = cpu_to_le16(cur->key.source_type);
	buf16[1] = cpu_to_le16(cur->key.target_type);
	buf16[2] = cpu_to_le16(cur->key.target_class);
	buf16[3] = cpu_to_le16(cur->key.specified);
	items = put_entry(buf16, sizeof(uint16_t), 4, fp);
	if (items != 4)
		return POLICYDB_ERROR;
	if ((p->policyvers < POLICYDB_VERSION_XPERMS_IOCTL) &&
			(cur->key.specified & AVTAB_XPERMS)) {
		ERR(fp->handle, "policy version %u does not support ioctl extended"
				"permissions rules and one was specified", p->policyvers);
		return POLICYDB_ERROR;
	}

	if (p->target_platform != SEPOL_TARGET_SELINUX &&
			(cur->key.specified & AVTAB_XPERMS)) {
		ERR(fp->handle, "Target platform %s does not support ioctl "
				"extended permissions rules and one was specified",
				policydb_target_strings[p->target_platform]);
		return POLICYDB_ERROR;
	}

	if (cur->key.specified & AVTAB_XPERMS) {
		buf8 = cur->datum.xperms->specified;
		items = put_entry(&buf8, sizeof(uint8_t),1,fp);
		if (items != 1)
			return POLICYDB_ERROR;
		buf8 = cur->datum.xperms->driver;
		items = put_entry(&buf8, sizeof(uint8_t),1,fp);
		if (items != 1)
			return POLICYDB_ERROR;
		for (i = 0; i < ARRAY_SIZE(cur->datum.xperms->perms); i++)
			buf32[i] = cpu_to_le32(cur->datum.xperms->perms[i]);
		items = put_entry(buf32, sizeof(uint32_t),8,fp);
		if (items != 8)
			return POLICYDB_ERROR;
	} else {
		buf32[0] = cpu_to_le32(cur->datum.data);
		items = put_entry(buf32, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

static inline void avtab_reset_merged(avtab_t * a)
{
	unsigned int i;
	avtab_ptr_t cur;
	for (i = 0; i < a->nslot; i++) {
		for (cur = a->htable[i]; cur; cur = cur->next)
			cur->merged = 0;
	}
}

static int avtab_write(struct policydb *p, avtab_t * a, struct policy_file *fp)
{
	unsigned int i;
	int rc;
	avtab_t expa;
	avtab_ptr_t cur;
	uint32_t nel;
	size_t items;
	unsigned int oldvers = (p->policy_type == POLICY_KERN
				&& p->policyvers < POLICYDB_VERSION_AVTAB);

	if (oldvers) {
		/* Old avtab format.
		   First, we need to expand attributes.  Then, we need to
		   merge similar entries, so we need to track merged nodes 
		   and compute the final nel. */
		if (ksu_avtab_init(&expa))
			return POLICYDB_ERROR;
		if (expand_avtab(p, a, &expa)) {
			rc = -1;
			goto out;
		}
		a = &expa;
		avtab_reset_merged(a);
		nel = a->nel;
	} else {
		/* New avtab format.  nel is good to go. */
		nel = cpu_to_le32(a->nel);
		items = put_entry(&nel, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}

	for (i = 0; i < a->nslot; i++) {
		for (cur = a->htable[i]; cur; cur = cur->next) {
			/* If old format, compute final nel.
			   If new format, write out the items. */
			if (avtab_write_item(p, cur, fp, 1, !oldvers, &nel)) {
				rc = -1;
				goto out;
			}
		}
	}

	if (oldvers) {
		/* Old avtab format.
		   Write the computed nel value, then write the items. */
		nel = cpu_to_le32(nel);
		items = put_entry(&nel, sizeof(uint32_t), 1, fp);
		if (items != 1) {
			rc = -1;
			goto out;
		}
		avtab_reset_merged(a);
		for (i = 0; i < a->nslot; i++) {
			for (cur = a->htable[i]; cur; cur = cur->next) {
				if (avtab_write_item(p, cur, fp, 1, 1, NULL)) {
					rc = -1;
					goto out;
				}
			}
		}
	}

	rc = 0;
      out:
	if (oldvers)
		ksu_avtab_destroy(&expa);
	return rc;
}

/*
 * Write a semantic MLS level structure to a policydb binary 
 * representation file.
 */
static int mls_write_semantic_level_helper(mls_semantic_level_t * l,
					   struct policy_file *fp)
{
	uint32_t buf[2], ncat = 0;
	size_t items;
	mls_semantic_cat_t *cat;

	for (cat = l->cat; cat; cat = cat->next)
		ncat++;

	buf[0] = cpu_to_le32(l->sens);
	buf[1] = cpu_to_le32(ncat);
	items = put_entry(buf, sizeof(uint32_t), 2, fp);
	if (items != 2)
		return POLICYDB_ERROR;

	for (cat = l->cat; cat; cat = cat->next) {
		buf[0] = cpu_to_le32(cat->low);
		buf[1] = cpu_to_le32(cat->high);
		items = put_entry(buf, sizeof(uint32_t), 2, fp);
		if (items != 2)
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

/*
 * Read a semantic MLS range structure to a policydb binary 
 * representation file.
 */
static int mls_write_semantic_range_helper(mls_semantic_range_t * r,
					   struct policy_file *fp)
{
	int rc;

	rc = mls_write_semantic_level_helper(&r->level[0], fp);
	if (rc)
		return rc;

	rc = mls_write_semantic_level_helper(&r->level[1], fp);

	return rc;
}

/*
 * Write a MLS level structure to a policydb binary 
 * representation file.
 */
static int mls_write_level(mls_level_t * l, struct policy_file *fp)
{
	uint32_t sens;
	size_t items;

	sens = cpu_to_le32(l->sens);
	items = put_entry(&sens, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	if (ebitmap_write(&l->cat, fp))
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

/*
 * Write a MLS range structure to a policydb binary 
 * representation file.
 */
static int mls_write_range_helper(mls_range_t * r, struct policy_file *fp)
{
	uint32_t buf[3];
	size_t items, items2;
	int eq;

	eq = mls_level_eq(&r->level[1], &r->level[0]);

	items = 1;		/* item 0 is used for the item count */
	buf[items++] = cpu_to_le32(r->level[0].sens);
	if (!eq)
		buf[items++] = cpu_to_le32(r->level[1].sens);
	buf[0] = cpu_to_le32(items - 1);

	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items2 != items)
		return POLICYDB_ERROR;

	if (ebitmap_write(&r->level[0].cat, fp))
		return POLICYDB_ERROR;
	if (!eq)
		if (ebitmap_write(&r->level[1].cat, fp))
			return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int sens_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	level_datum_t *levdatum;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;

	levdatum = (level_datum_t *) datum;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(levdatum->isalias);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	if (mls_write_level(levdatum->level, fp))
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int cat_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	cat_datum_t *catdatum;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;

	catdatum = (cat_datum_t *) datum;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(catdatum->s.value);
	buf[items++] = cpu_to_le32(catdatum->isalias);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int role_trans_write(policydb_t *p, struct policy_file *fp)
{
	role_trans_t *r = p->role_tr;
	role_trans_t *tr;
	uint32_t buf[3];
	size_t nel, items;
	int new_roletr = (p->policy_type == POLICY_KERN &&
			  p->policyvers >= POLICYDB_VERSION_ROLETRANS);
	int warning_issued = 0;

	nel = 0;
	for (tr = r; tr; tr = tr->next)
		if(new_roletr || tr->tclass == p->process_class)
			nel++;

	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;
	for (tr = r; tr; tr = tr->next) {
		if (!new_roletr && tr->tclass != p->process_class) {
			if (!warning_issued)
				WARN(fp->handle, "Discarding role_transition "
				     "rules for security classes other than "
				     "\"process\"");
			warning_issued = 1;
			continue;
		}
		buf[0] = cpu_to_le32(tr->role);
		buf[1] = cpu_to_le32(tr->type);
		buf[2] = cpu_to_le32(tr->new_role);
		items = put_entry(buf, sizeof(uint32_t), 3, fp);
		if (items != 3)
			return POLICYDB_ERROR;
		if (new_roletr) {
			buf[0] = cpu_to_le32(tr->tclass);
			items = put_entry(buf, sizeof(uint32_t), 1, fp);
			if (items != 1)
				return POLICYDB_ERROR;
		}
	}

	return POLICYDB_SUCCESS;
}

static int role_allow_write(role_allow_t * r, struct policy_file *fp)
{
	role_allow_t *ra;
	uint32_t buf[2];
	size_t nel, items;

	nel = 0;
	for (ra = r; ra; ra = ra->next)
		nel++;
	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;
	for (ra = r; ra; ra = ra->next) {
		buf[0] = cpu_to_le32(ra->role);
		buf[1] = cpu_to_le32(ra->new_role);
		items = put_entry(buf, sizeof(uint32_t), 2, fp);
		if (items != 2)
			return POLICYDB_ERROR;
	}
	return POLICYDB_SUCCESS;
}

static int filename_write_one_compat(hashtab_key_t key, void *data, void *ptr)
{
	uint32_t bit, buf[4];
	size_t items, len;
	filename_trans_key_t *ft = (filename_trans_key_t *)key;
	filename_trans_datum_t *datum = data;
	ebitmap_node_t *node;
	void *fp = ptr;

	len = strlen(ft->name);
	do {
		ebitmap_for_each_positive_bit(&datum->stypes, node, bit) {
			buf[0] = cpu_to_le32(len);
			items = put_entry(buf, sizeof(uint32_t), 1, fp);
			if (items != 1)
				return POLICYDB_ERROR;

			items = put_entry(ft->name, sizeof(char), len, fp);
			if (items != len)
				return POLICYDB_ERROR;

			buf[0] = cpu_to_le32(bit + 1);
			buf[1] = cpu_to_le32(ft->ttype);
			buf[2] = cpu_to_le32(ft->tclass);
			buf[3] = cpu_to_le32(datum->otype);
			items = put_entry(buf, sizeof(uint32_t), 4, fp);
			if (items != 4)
				return POLICYDB_ERROR;
		}

		datum = datum->next;
	} while (datum);

	return 0;
}

static int filename_write_one(hashtab_key_t key, void *data, void *ptr)
{
	uint32_t buf[3];
	size_t items, len, ndatum;
	filename_trans_key_t *ft = (filename_trans_key_t *)key;
	filename_trans_datum_t *datum;
	void *fp = ptr;

	len = strlen(ft->name);
	buf[0] = cpu_to_le32(len);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	items = put_entry(ft->name, sizeof(char), len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	ndatum = 0;
	datum = data;
	do {
		ndatum++;
		datum = datum->next;
	} while (datum);

	buf[0] = cpu_to_le32(ft->ttype);
	buf[1] = cpu_to_le32(ft->tclass);
	buf[2] = cpu_to_le32(ndatum);
	items = put_entry(buf, sizeof(uint32_t), 3, fp);
	if (items != 3)
		return POLICYDB_ERROR;

	datum = data;
	do {
		if (ebitmap_write(&datum->stypes, fp))
			return POLICYDB_ERROR;

		buf[0] = cpu_to_le32(datum->otype);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;

		datum = datum->next;
	} while (datum);

	return 0;
}

static int filename_trans_write(struct policydb *p, void *fp)
{
	size_t items;
	uint32_t buf[1];
	int rc;

	if (p->policyvers < POLICYDB_VERSION_FILENAME_TRANS)
		return 0;

	if (p->policyvers < POLICYDB_VERSION_COMP_FTRANS) {
		buf[0] = cpu_to_le32(p->filename_trans_count);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;

		rc = ksu_hashtab_map(p->filename_trans, filename_write_one_compat,
				 fp);
	} else {
		buf[0] = cpu_to_le32(p->filename_trans->nel);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;

		rc = ksu_hashtab_map(p->filename_trans, filename_write_one, fp);
	}
	return rc;
}

static int role_set_write(role_set_t * x, struct policy_file *fp)
{
	size_t items;
	uint32_t buf[1];

	if (ebitmap_write(&x->roles, fp))
		return POLICYDB_ERROR;

	buf[0] = cpu_to_le32(x->flags);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int type_set_write(type_set_t * x, struct policy_file *fp)
{
	size_t items;
	uint32_t buf[1];

	if (ebitmap_write(&x->types, fp))
		return POLICYDB_ERROR;
	if (ebitmap_write(&x->negset, fp))
		return POLICYDB_ERROR;

	buf[0] = cpu_to_le32(x->flags);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int cond_write_bool(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	cond_bool_datum_t *booldatum;
	uint32_t buf[3], len;
	unsigned int items, items2;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;
	struct policydb *p = pd->p;

	booldatum = (cond_bool_datum_t *) datum;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(booldatum->s.value);
	buf[items++] = cpu_to_le32(booldatum->state);
	buf[items++] = cpu_to_le32(len);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;
	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	if (p->policy_type != POLICY_KERN &&
	    p->policyvers >= MOD_POLICYDB_VERSION_TUNABLE_SEP) {
		buf[0] = cpu_to_le32(booldatum->flags);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

/*
 * cond_write_cond_av_list doesn't write out the av_list nodes.
 * Instead it writes out the key/value pairs from the avtab. This
 * is necessary because there is no way to uniquely identifying rules
 * in the avtab so it is not possible to associate individual rules
 * in the avtab with a conditional without saving them as part of
 * the conditional. This means that the avtab with the conditional
 * rules will not be saved but will be rebuilt on policy load.
 */
static int cond_write_av_list(policydb_t * p,
			      cond_av_list_t * list, struct policy_file *fp)
{
	uint32_t buf[4];
	cond_av_list_t *cur_list, *new_list = NULL;
	avtab_t expa;
	uint32_t len, items;
	unsigned int oldvers = (p->policy_type == POLICY_KERN
				&& p->policyvers < POLICYDB_VERSION_AVTAB);
	int rc = -1;

	if (oldvers) {
		if (ksu_avtab_init(&expa))
			return POLICYDB_ERROR;
		if (expand_cond_av_list(p, list, &new_list, &expa))
			goto out;
		list = new_list;
	}

	len = 0;
	for (cur_list = list; cur_list != NULL; cur_list = cur_list->next) {
		if (cur_list->node->parse_context)
			len++;
	}

	buf[0] = cpu_to_le32(len);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		goto out;

	if (len == 0) {
		rc = 0;
		goto out;
	}

	for (cur_list = list; cur_list != NULL; cur_list = cur_list->next) {
		if (cur_list->node->parse_context)
			if (avtab_write_item(p, cur_list->node, fp, 0, 1, NULL))
				goto out;
	}

	rc = 0;
      out:
	if (oldvers) {
		cond_av_list_destroy(new_list);
		ksu_avtab_destroy(&expa);
	}

	return rc;
}

static int cond_write_node(policydb_t * p,
			   cond_node_t * node, struct policy_file *fp)
{
	cond_expr_t *cur_expr;
	uint32_t buf[2];
	uint32_t items, items2, len;

	buf[0] = cpu_to_le32(node->cur_state);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	/* expr */
	len = 0;
	for (cur_expr = node->expr; cur_expr != NULL; cur_expr = cur_expr->next)
		len++;

	buf[0] = cpu_to_le32(len);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	for (cur_expr = node->expr; cur_expr != NULL; cur_expr = cur_expr->next) {
		items = 0;
		buf[items++] = cpu_to_le32(cur_expr->expr_type);
		buf[items++] = cpu_to_le32(cur_expr->bool);
		items2 = put_entry(buf, sizeof(uint32_t), items, fp);
		if (items2 != items)
			return POLICYDB_ERROR;
	}

	if (p->policy_type == POLICY_KERN) {
		if (cond_write_av_list(p, node->true_list, fp) != 0)
			return POLICYDB_ERROR;
		if (cond_write_av_list(p, node->false_list, fp) != 0)
			return POLICYDB_ERROR;
	} else {
		if (avrule_write_list(p, node->avtrue_list, fp))
			return POLICYDB_ERROR;
		if (avrule_write_list(p, node->avfalse_list, fp))
			return POLICYDB_ERROR;
	}

	if (p->policy_type != POLICY_KERN &&
	    p->policyvers >= MOD_POLICYDB_VERSION_TUNABLE_SEP) {
		buf[0] = cpu_to_le32(node->flags);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

static int cond_write_list(policydb_t * p, cond_list_t * list,
			   struct policy_file *fp)
{
	cond_node_t *cur;
	uint32_t len, items;
	uint32_t buf[1];

	len = 0;
	for (cur = list; cur != NULL; cur = cur->next)
		len++;
	buf[0] = cpu_to_le32(len);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	for (cur = list; cur != NULL; cur = cur->next) {
		if (cond_write_node(p, cur, fp) != 0)
			return POLICYDB_ERROR;
	}
	return POLICYDB_SUCCESS;
}

/*
 * Write a security context structure
 * to a policydb binary representation file.
 */
static int context_write(struct policydb *p, context_struct_t * c,
			 struct policy_file *fp)
{
	uint32_t buf[32];
	size_t items, items2;

	items = 0;
	buf[items++] = cpu_to_le32(c->user);
	buf[items++] = cpu_to_le32(c->role);
	buf[items++] = cpu_to_le32(c->type);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items2 != items)
		return POLICYDB_ERROR;
	if ((p->policyvers >= POLICYDB_VERSION_MLS
	     && p->policy_type == POLICY_KERN)
	    || (p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policy_type == POLICY_BASE))
		if (mls_write_range_helper(&c->range, fp))
			return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

/*
 * The following *_write functions are used to
 * write the symbol data to a policy database
 * binary representation file.
 */

static int perm_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	perm_datum_t *perdatum;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;

	perdatum = (perm_datum_t *) datum;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(perdatum->s.value);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int common_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	common_datum_t *comdatum;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;

	comdatum = (common_datum_t *) datum;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(comdatum->s.value);
	buf[items++] = cpu_to_le32(comdatum->permissions.nprim);
	buf[items++] = cpu_to_le32(comdatum->permissions.table->nel);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	if (ksu_hashtab_map(comdatum->permissions.table, perm_write, pd))
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int write_cons_helper(policydb_t * p,
			     constraint_node_t * node, int allowxtarget,
			     struct policy_file *fp)
{
	constraint_node_t *c;
	constraint_expr_t *e;
	uint32_t buf[3], nexpr;
	int items;

	for (c = node; c; c = c->next) {
		nexpr = 0;
		for (e = c->expr; e; e = e->next) {
			nexpr++;
		}
		buf[0] = cpu_to_le32(c->permissions);
		buf[1] = cpu_to_le32(nexpr);
		items = put_entry(buf, sizeof(uint32_t), 2, fp);
		if (items != 2)
			return POLICYDB_ERROR;
		for (e = c->expr; e; e = e->next) {
			buf[0] = cpu_to_le32(e->expr_type);
			buf[1] = cpu_to_le32(e->attr);
			buf[2] = cpu_to_le32(e->op);
			items = put_entry(buf, sizeof(uint32_t), 3, fp);
			if (items != 3)
				return POLICYDB_ERROR;

			switch (e->expr_type) {
			case CEXPR_NAMES:
				if (!allowxtarget && (e->attr & CEXPR_XTARGET))
					return POLICYDB_ERROR;
				if (ebitmap_write(&e->names, fp)) {
					return POLICYDB_ERROR;
				}
				if ((p->policy_type != POLICY_KERN &&
						type_set_write(e->type_names, fp)) ||
						(p->policy_type == POLICY_KERN &&
						(p->policyvers >= POLICYDB_VERSION_CONSTRAINT_NAMES) &&
						type_set_write(e->type_names, fp))) {
					return POLICYDB_ERROR;
				}
				break;
			default:
				break;
			}
		}
	}

	return POLICYDB_SUCCESS;
}

static int class_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	class_datum_t *cladatum;
	constraint_node_t *c;
	uint32_t buf[32], ncons;
	size_t items, items2, len, len2;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;
	struct policydb *p = pd->p;

	cladatum = (class_datum_t *) datum;

	len = strlen(key);
	if (cladatum->comkey)
		len2 = strlen(cladatum->comkey);
	else
		len2 = 0;

	ncons = 0;
	for (c = cladatum->constraints; c; c = c->next) {
		ncons++;
	}

	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(len2);
	buf[items++] = cpu_to_le32(cladatum->s.value);
	buf[items++] = cpu_to_le32(cladatum->permissions.nprim);
	if (cladatum->permissions.table)
		buf[items++] = cpu_to_le32(cladatum->permissions.table->nel);
	else
		buf[items++] = 0;
	buf[items++] = cpu_to_le32(ncons);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	if (cladatum->comkey) {
		items = put_entry(cladatum->comkey, 1, len2, fp);
		if (items != len2)
			return POLICYDB_ERROR;
	}
	if (ksu_hashtab_map(cladatum->permissions.table, perm_write, pd))
		return POLICYDB_ERROR;

	if (write_cons_helper(p, cladatum->constraints, 0, fp))
		return POLICYDB_ERROR;

	if ((p->policy_type == POLICY_KERN
	     && p->policyvers >= POLICYDB_VERSION_VALIDATETRANS)
	    || (p->policy_type == POLICY_BASE
		&& p->policyvers >= MOD_POLICYDB_VERSION_VALIDATETRANS)) {
		/* write out the validatetrans rule */
		ncons = 0;
		for (c = cladatum->validatetrans; c; c = c->next) {
			ncons++;
		}
		buf[0] = cpu_to_le32(ncons);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		if (write_cons_helper(p, cladatum->validatetrans, 1, fp))
			return POLICYDB_ERROR;
	}

	if ((p->policy_type == POLICY_KERN &&
	     p->policyvers >= POLICYDB_VERSION_NEW_OBJECT_DEFAULTS) ||
	    (p->policy_type == POLICY_BASE &&
	     p->policyvers >= MOD_POLICYDB_VERSION_NEW_OBJECT_DEFAULTS)) {
		buf[0] = cpu_to_le32(cladatum->default_user);
		buf[1] = cpu_to_le32(cladatum->default_role);
		if (!glblub_version && cladatum->default_range == DEFAULT_GLBLUB) {
			WARN(fp->handle,
                             "class %s default_range set to GLBLUB but policy version is %d (%d required), discarding",
                             p->p_class_val_to_name[cladatum->s.value - 1], p->policyvers,
                             p->policy_type == POLICY_KERN? POLICYDB_VERSION_GLBLUB:MOD_POLICYDB_VERSION_GLBLUB);
                        cladatum->default_range = 0;
                }
		buf[2] = cpu_to_le32(cladatum->default_range);
		items = put_entry(buf, sizeof(uint32_t), 3, fp);
		if (items != 3)
			return POLICYDB_ERROR;
	}

	if ((p->policy_type == POLICY_KERN &&
	     p->policyvers >= POLICYDB_VERSION_DEFAULT_TYPE) ||
	    (p->policy_type == POLICY_BASE &&
	     p->policyvers >= MOD_POLICYDB_VERSION_DEFAULT_TYPE)) {
		buf[0] = cpu_to_le32(cladatum->default_type);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

static int role_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	role_datum_t *role;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;
	struct policydb *p = pd->p;

	role = (role_datum_t *) datum;

	/*
	 * Role attributes are redundant for policy.X, skip them
	 * when writing the roles symbol table. They are also skipped
	 * when pp is downgraded.
	 *
	 * Their numbers would be deducted in ksu_policydb_write().
	 */
	if ((role->flavor == ROLE_ATTRIB) &&
	    ((p->policy_type == POLICY_KERN) ||
	     (p->policy_type != POLICY_KERN &&
	      p->policyvers < MOD_POLICYDB_VERSION_ROLEATTRIB)))
		return POLICYDB_SUCCESS;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(role->s.value);
	if (policydb_has_boundary_feature(p))
		buf[items++] = cpu_to_le32(role->bounds);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	if (ebitmap_write(&role->dominates, fp))
		return POLICYDB_ERROR;
	if (p->policy_type == POLICY_KERN) {
		if (role->s.value == OBJECT_R_VAL) {
			/*
			 * CIL populates object_r's types map
			 * rather than handling it as a special case.
			 * However, this creates an inconsistency with
			 * the kernel policy read from /sys/fs/selinux/policy
			 * because the kernel ignores everything except for
			 * object_r's value from the policy file.
			 * Make them consistent by writing an empty
			 * ebitmap instead.
			 */
			ebitmap_t empty;
			ebitmap_init(&empty);
			if (ebitmap_write(&empty, fp))
				return POLICYDB_ERROR;
		} else {
			if (ebitmap_write(&role->types.types, fp))
				return POLICYDB_ERROR;
		}
	} else {
		if (type_set_write(&role->types, fp))
			return POLICYDB_ERROR;
	}

	if (p->policy_type != POLICY_KERN &&
	    p->policyvers >= MOD_POLICYDB_VERSION_ROLEATTRIB) {
		buf[0] = cpu_to_le32(role->flavor);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;

		if (ebitmap_write(&role->roles, fp))
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

static int type_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	type_datum_t *typdatum;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;
	struct policydb *p = pd->p;

	typdatum = (type_datum_t *) datum;

	/*
	 * The kernel policy version less than 24 (= POLICYDB_VERSION_BOUNDARY)
	 * does not support to load entries of attribute, so we skip to write it.
	 */
	if (p->policy_type == POLICY_KERN
	    && p->policyvers < POLICYDB_VERSION_BOUNDARY
	    && typdatum->flavor == TYPE_ATTRIB)
		return POLICYDB_SUCCESS;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(typdatum->s.value);
	if (policydb_has_boundary_feature(p)) {
		uint32_t properties = 0;

		if (p->policy_type != POLICY_KERN
		    && p->policyvers >= MOD_POLICYDB_VERSION_BOUNDARY_ALIAS) {
			buf[items++] = cpu_to_le32(typdatum->primary);
		}

		if (typdatum->primary)
			properties |= TYPEDATUM_PROPERTY_PRIMARY;

		if (typdatum->flavor == TYPE_ATTRIB) {
			properties |= TYPEDATUM_PROPERTY_ATTRIBUTE;
		} else if (typdatum->flavor == TYPE_ALIAS
			   && p->policy_type != POLICY_KERN)
			properties |= TYPEDATUM_PROPERTY_ALIAS;

		if (typdatum->flags & TYPE_FLAGS_PERMISSIVE
		    && p->policy_type != POLICY_KERN)
			properties |= TYPEDATUM_PROPERTY_PERMISSIVE;

		buf[items++] = cpu_to_le32(properties);
		buf[items++] = cpu_to_le32(typdatum->bounds);
	} else {
		buf[items++] = cpu_to_le32(typdatum->primary);

		if (p->policy_type != POLICY_KERN) {
			buf[items++] = cpu_to_le32(typdatum->flavor);

			if (p->policyvers >= MOD_POLICYDB_VERSION_PERMISSIVE)
				buf[items++] = cpu_to_le32(typdatum->flags);
			else if (typdatum->flags & TYPE_FLAGS_PERMISSIVE)
				WARN(fp->handle, "Warning! Module policy "
				     "version %d cannot support permissive "
				     "types, but one was defined",
				     p->policyvers);
		}
	}
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	if (p->policy_type != POLICY_KERN) {
		if (ebitmap_write(&typdatum->types, fp))
			return POLICYDB_ERROR;
	}

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	return POLICYDB_SUCCESS;
}

static int user_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	user_datum_t *usrdatum;
	uint32_t buf[32];
	size_t items, items2, len;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;
	struct policydb *p = pd->p;

	usrdatum = (user_datum_t *) datum;

	len = strlen(key);
	items = 0;
	buf[items++] = cpu_to_le32(len);
	buf[items++] = cpu_to_le32(usrdatum->s.value);
	if (policydb_has_boundary_feature(p))
		buf[items++] = cpu_to_le32(usrdatum->bounds);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	items = put_entry(key, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	if (p->policy_type == POLICY_KERN) {
		if (ebitmap_write(&usrdatum->roles.roles, fp))
			return POLICYDB_ERROR;
	} else {
		if (role_set_write(&usrdatum->roles, fp))
			return POLICYDB_ERROR;
	}

	if ((p->policyvers >= POLICYDB_VERSION_MLS
	     && p->policy_type == POLICY_KERN)
	    || (p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policyvers < MOD_POLICYDB_VERSION_MLS_USERS
		&& p->policy_type == POLICY_MOD)
	    || (p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policyvers < MOD_POLICYDB_VERSION_MLS_USERS
		&& p->policy_type == POLICY_BASE)) {
		if (mls_write_range_helper(&usrdatum->exp_range, fp))
			return POLICYDB_ERROR;
		if (mls_write_level(&usrdatum->exp_dfltlevel, fp))
			return POLICYDB_ERROR;
	} else if ((p->policyvers >= MOD_POLICYDB_VERSION_MLS_USERS
		    && p->policy_type == POLICY_MOD)
		   || (p->policyvers >= MOD_POLICYDB_VERSION_MLS_USERS
		       && p->policy_type == POLICY_BASE)) {
		if (mls_write_semantic_range_helper(&usrdatum->range, fp))
			return -1;
		if (mls_write_semantic_level_helper(&usrdatum->dfltlevel, fp))
			return -1;
	}

	return POLICYDB_SUCCESS;
}

static int (*write_f[SYM_NUM]) (hashtab_key_t key, hashtab_datum_t datum,
				void *datap) = {
common_write, class_write, role_write, type_write, user_write,
	    cond_write_bool, sens_write, cat_write,};

static int ocontext_write_xen(const struct policydb_compat_info *info, policydb_t *p,
			  struct policy_file *fp)
{
	unsigned int i, j;
	size_t nel, items, len;
	uint32_t buf[32];
	ocontext_t *c;
	for (i = 0; i < info->ocon_num; i++) {
		nel = 0;
		for (c = p->ocontexts[i]; c; c = c->next) {
			if (i == OCON_XEN_ISID && !c->context[0].user) {
				INFO(fp->handle,
				     "No context assigned to SID %s, omitting from policy",
				     c->u.name);
				continue;
			}
			nel++;
		}
		buf[0] = cpu_to_le32(nel);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		for (c = p->ocontexts[i]; c; c = c->next) {
			switch (i) {
			case OCON_XEN_ISID:
				if (!c->context[0].user)
					break;
				buf[0] = cpu_to_le32(c->sid[0]);
				items = put_entry(buf, sizeof(uint32_t), 1, fp);
				if (items != 1)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_XEN_PIRQ:
				buf[0] = cpu_to_le32(c->u.pirq);
				items = put_entry(buf, sizeof(uint32_t), 1, fp);
				if (items != 1)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_XEN_IOPORT:
				buf[0] = c->u.ioport.low_ioport;
				buf[1] = c->u.ioport.high_ioport;
				for (j = 0; j < 2; j++)
					buf[j] = cpu_to_le32(buf[j]);
				items = put_entry(buf, sizeof(uint32_t), 2, fp);
				if (items != 2)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_XEN_IOMEM:
				if (p->policyvers >= POLICYDB_VERSION_XEN_DEVICETREE) {
					uint64_t b64[2];
					b64[0] = c->u.iomem.low_iomem;
					b64[1] = c->u.iomem.high_iomem;
					for (j = 0; j < 2; j++)
						b64[j] = cpu_to_le64(b64[j]);
					items = put_entry(b64, sizeof(uint64_t), 2, fp);
					if (items != 2)
						return POLICYDB_ERROR;
				} else {
					if (c->u.iomem.high_iomem > 0xFFFFFFFFULL) {
						ERR(fp->handle, "policy version %d"
							" cannot represent IOMEM addresses over 16TB",
							p->policyvers);
						return POLICYDB_ERROR;
					}

					buf[0] = c->u.iomem.low_iomem;
					buf[1] = c->u.iomem.high_iomem;
					for (j = 0; j < 2; j++)
						buf[j] = cpu_to_le32(buf[j]);
					items = put_entry(buf, sizeof(uint32_t), 2, fp);
					if (items != 2)
						return POLICYDB_ERROR;
				}
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_XEN_PCIDEVICE:
				buf[0] = cpu_to_le32(c->u.device);
				items = put_entry(buf, sizeof(uint32_t), 1, fp);
				if (items != 1)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_XEN_DEVICETREE:
				len = strlen(c->u.name);
				buf[0] = cpu_to_le32(len);
				items = put_entry(buf, sizeof(uint32_t), 1, fp);
				if (items != 1)
					return POLICYDB_ERROR;
				items = put_entry(c->u.name, 1, len, fp);
				if (items != len)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			}
		}
	}
	return POLICYDB_SUCCESS;
}

static int ocontext_write_selinux(const struct policydb_compat_info *info,
	policydb_t *p, struct policy_file *fp)
{
	unsigned int i, j;
	size_t nel, items, len;
	uint32_t buf[32];
	ocontext_t *c;
	for (i = 0; i < info->ocon_num; i++) {
		nel = 0;
		for (c = p->ocontexts[i]; c; c = c->next) {
			if (i == OCON_ISID && !c->context[0].user) {
				INFO(fp->handle,
				     "No context assigned to SID %s, omitting from policy",
				     c->u.name);
				continue;
			}
			nel++;
		}
		buf[0] = cpu_to_le32(nel);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		for (c = p->ocontexts[i]; c; c = c->next) {
			switch (i) {
			case OCON_ISID:
				if (!c->context[0].user)
					break;
				buf[0] = cpu_to_le32(c->sid[0]);
				items = put_entry(buf, sizeof(uint32_t), 1, fp);
				if (items != 1)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_FS:
			case OCON_NETIF:
				len = strlen(c->u.name);
				buf[0] = cpu_to_le32(len);
				items = put_entry(buf, sizeof(uint32_t), 1, fp);
				if (items != 1)
					return POLICYDB_ERROR;
				items = put_entry(c->u.name, 1, len, fp);
				if (items != len)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[1], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_IBPKEY:
				 /* The subnet prefix is in network order */
				memcpy(buf, &c->u.ibpkey.subnet_prefix,
				       sizeof(c->u.ibpkey.subnet_prefix));

				buf[2] = cpu_to_le32(c->u.ibpkey.low_pkey);
				buf[3] = cpu_to_le32(c->u.ibpkey.high_pkey);

				items = put_entry(buf, sizeof(uint32_t), 4, fp);
				if (items != 4)
					return POLICYDB_ERROR;

				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_IBENDPORT:
				len = strlen(c->u.ibendport.dev_name);
				buf[0] = cpu_to_le32(len);
				buf[1] = cpu_to_le32(c->u.ibendport.port);
				items = put_entry(buf, sizeof(uint32_t), 2, fp);
				if (items != 2)
					return POLICYDB_ERROR;
				items = put_entry(c->u.ibendport.dev_name, 1, len, fp);
				if (items != len)
					return POLICYDB_ERROR;

				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_PORT:
				buf[0] = c->u.port.protocol;
				buf[1] = c->u.port.low_port;
				buf[2] = c->u.port.high_port;
				for (j = 0; j < 3; j++) {
					buf[j] = cpu_to_le32(buf[j]);
				}
				items = put_entry(buf, sizeof(uint32_t), 3, fp);
				if (items != 3)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_NODE:
				buf[0] = c->u.node.addr; /* network order */
				buf[1] = c->u.node.mask; /* network order */
				items = put_entry(buf, sizeof(uint32_t), 2, fp);
				if (items != 2)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_FSUSE:
				buf[0] = cpu_to_le32(c->v.behavior);
				len = strlen(c->u.name);
				buf[1] = cpu_to_le32(len);
				items = put_entry(buf, sizeof(uint32_t), 2, fp);
				if (items != 2)
					return POLICYDB_ERROR;
				items = put_entry(c->u.name, 1, len, fp);
				if (items != len)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			case OCON_NODE6:
				for (j = 0; j < 4; j++)
					buf[j] = c->u.node6.addr[j]; /* network order */
				for (j = 0; j < 4; j++)
					buf[j + 4] = c->u.node6.mask[j]; /* network order */
				items = put_entry(buf, sizeof(uint32_t), 8, fp);
				if (items != 8)
					return POLICYDB_ERROR;
				if (context_write(p, &c->context[0], fp))
					return POLICYDB_ERROR;
				break;
			}
		}
	}
	return POLICYDB_SUCCESS;
}

static int ocontext_write(const struct policydb_compat_info *info, policydb_t * p,
	struct policy_file *fp)
{
	int rc = POLICYDB_ERROR;
	switch (p->target_platform) {
	case SEPOL_TARGET_SELINUX:
		rc = ocontext_write_selinux(info, p, fp);
		break;
	case SEPOL_TARGET_XEN:
		rc = ocontext_write_xen(info, p, fp);
		break;
	}
	return rc;
}

static int genfs_write(policydb_t * p, struct policy_file *fp)
{
	genfs_t *genfs;
	ocontext_t *c;
	size_t nel = 0, items, len;
	uint32_t buf[32];

	for (genfs = p->genfs; genfs; genfs = genfs->next)
		nel++;
	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;
	for (genfs = p->genfs; genfs; genfs = genfs->next) {
		len = strlen(genfs->fstype);
		buf[0] = cpu_to_le32(len);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		items = put_entry(genfs->fstype, 1, len, fp);
		if (items != len)
			return POLICYDB_ERROR;
		nel = 0;
		for (c = genfs->head; c; c = c->next)
			nel++;
		buf[0] = cpu_to_le32(nel);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		for (c = genfs->head; c; c = c->next) {
			len = strlen(c->u.name);
			buf[0] = cpu_to_le32(len);
			items = put_entry(buf, sizeof(uint32_t), 1, fp);
			if (items != 1)
				return POLICYDB_ERROR;
			items = put_entry(c->u.name, 1, len, fp);
			if (items != len)
				return POLICYDB_ERROR;
			buf[0] = cpu_to_le32(c->v.sclass);
			items = put_entry(buf, sizeof(uint32_t), 1, fp);
			if (items != 1)
				return POLICYDB_ERROR;
			if (context_write(p, &c->context[0], fp))
				return POLICYDB_ERROR;
		}
	}
	return POLICYDB_SUCCESS;
}


struct rangetrans_write_args {
	size_t nel;
	int new_rangetr;
	struct policy_file *fp;
	struct policydb *p;
};

static int rangetrans_count(hashtab_key_t key,
			    void *data __attribute__ ((unused)),
			    void *ptr)
{
	struct range_trans *rt = (struct range_trans *)key;
	struct rangetrans_write_args *args = ptr;
	struct policydb *p = args->p;

	/* all range_transitions are written for the new format, only
	   process related range_transitions are written for the old
	   format, so count accordingly */
	if (args->new_rangetr || rt->target_class == p->process_class)
		args->nel++;
	return 0;
}

static int range_write_helper(hashtab_key_t key, void *data, void *ptr)
{
	uint32_t buf[2];
	struct range_trans *rt = (struct range_trans *)key;
	struct mls_range *r = data;
	struct rangetrans_write_args *args = ptr;
	struct policy_file *fp = args->fp;
	struct policydb *p = args->p;
	int new_rangetr = args->new_rangetr;
	size_t items;
	static int warning_issued = 0;
	int rc;

	if (!new_rangetr && rt->target_class != p->process_class) {
		if (!warning_issued)
			WARN(fp->handle, "Discarding range_transition "
			     "rules for security classes other than "
			     "\"process\"");
		warning_issued = 1;
		return 0;
	}

	buf[0] = cpu_to_le32(rt->source_type);
	buf[1] = cpu_to_le32(rt->target_type);
	items = put_entry(buf, sizeof(uint32_t), 2, fp);
	if (items != 2)
		return POLICYDB_ERROR;
	if (new_rangetr) {
		buf[0] = cpu_to_le32(rt->target_class);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}
	rc = mls_write_range_helper(r, fp);
	if (rc)
		return rc;

	return 0;
}

static int range_write(policydb_t * p, struct policy_file *fp)
{
	size_t items;
	uint32_t buf[2];
	int new_rangetr = (p->policy_type == POLICY_KERN &&
			   p->policyvers >= POLICYDB_VERSION_RANGETRANS);
	struct rangetrans_write_args args;
	int rc;

	args.nel = 0;
	args.new_rangetr = new_rangetr;
	args.fp = fp;
	args.p = p;
	rc = ksu_hashtab_map(p->range_tr, rangetrans_count, &args);
	if (rc)
		return rc;

	buf[0] = cpu_to_le32(args.nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	return ksu_hashtab_map(p->range_tr, range_write_helper, &args);
}

/************** module writing functions below **************/

static int avrule_write(policydb_t *p, avrule_t * avrule,
			struct policy_file *fp)
{
	size_t items, items2;
	uint32_t buf[32], len;
	class_perm_node_t *cur;

	if (p->policyvers < MOD_POLICYDB_VERSION_SELF_TYPETRANS &&
	    (avrule->specified & AVRULE_TYPE) &&
	    (avrule->flags & RULE_SELF)) {
		ERR(fp->handle,
		    "Module contains a self rule not supported by the target module policy version");
		return POLICYDB_ERROR;
	}

	items = 0;
	buf[items++] = cpu_to_le32(avrule->specified);
	buf[items++] = cpu_to_le32(avrule->flags);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items2 != items)
		return POLICYDB_ERROR;

	if (type_set_write(&avrule->stypes, fp))
		return POLICYDB_ERROR;

	if (type_set_write(&avrule->ttypes, fp))
		return POLICYDB_ERROR;

	cur = avrule->perms;
	len = 0;
	while (cur) {
		len++;
		cur = cur->next;
	}
	items = 0;
	buf[items++] = cpu_to_le32(len);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items2 != items)
		return POLICYDB_ERROR;
	cur = avrule->perms;
	while (cur) {
		items = 0;
		buf[items++] = cpu_to_le32(cur->tclass);
		buf[items++] = cpu_to_le32(cur->data);
		items2 = put_entry(buf, sizeof(uint32_t), items, fp);
		if (items2 != items)
			return POLICYDB_ERROR;

		cur = cur->next;
	}

	if (avrule->specified & AVRULE_XPERMS) {
		size_t nel = ARRAY_SIZE(avrule->xperms->perms);
		uint32_t buf32[nel];
		uint8_t buf8;
		unsigned int i;

		if (p->policyvers < MOD_POLICYDB_VERSION_XPERMS_IOCTL) {
			ERR(fp->handle,
			    "module policy version %u does not support ioctl"
			    " extended permissions rules and one was specified",
			    p->policyvers);
			return POLICYDB_ERROR;
		}

		if (p->target_platform != SEPOL_TARGET_SELINUX) {
			ERR(fp->handle,
			    "Target platform %s does not support ioctl"
			    " extended permissions rules and one was specified",
			    policydb_target_strings[p->target_platform]);
			return POLICYDB_ERROR;
		}

		buf8 = avrule->xperms->specified;
		items = put_entry(&buf8, sizeof(uint8_t),1,fp);
		if (items != 1)
			return POLICYDB_ERROR;
		buf8 = avrule->xperms->driver;
		items = put_entry(&buf8, sizeof(uint8_t),1,fp);
		if (items != 1)
			return POLICYDB_ERROR;
		for (i = 0; i < nel; i++)
			buf32[i] = cpu_to_le32(avrule->xperms->perms[i]);
		items = put_entry(buf32, sizeof(uint32_t), nel, fp);
		if (items != nel)
			return POLICYDB_ERROR;
	}

	return POLICYDB_SUCCESS;
}

static int avrule_write_list(policydb_t *p, avrule_t * avrules,
			     struct policy_file *fp)
{
	uint32_t buf[32], len;
	avrule_t *avrule;

	avrule = avrules;
	len = 0;
	while (avrule) {
		len++;
		avrule = avrule->next;
	}

	buf[0] = cpu_to_le32(len);
	if (put_entry(buf, sizeof(uint32_t), 1, fp) != 1)
		return POLICYDB_ERROR;

	avrule = avrules;
	while (avrule) {
		if (avrule_write(p, avrule, fp))
			return POLICYDB_ERROR;
		avrule = avrule->next;
	}

	return POLICYDB_SUCCESS;
}

static int only_process(ebitmap_t *in, struct policydb *p)
{
	unsigned int i, value;
	ebitmap_node_t *node;

	if (!p->process_class)
		return 0;

	value = p->process_class - 1;

	ebitmap_for_each_positive_bit(in, node, i) {
		if (i != value)
			return 0;
	}
	return 1;
}

static int role_trans_rule_write(policydb_t *p, role_trans_rule_t * t,
				 struct policy_file *fp)
{
	int nel = 0;
	size_t items;
	uint32_t buf[1];
	role_trans_rule_t *tr;
	int warned = 0;
	int new_role = p->policyvers >= MOD_POLICYDB_VERSION_ROLETRANS;

	for (tr = t; tr; tr = tr->next)
		if (new_role || only_process(&tr->classes, p))
			nel++;

	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;
	for (tr = t; tr; tr = tr->next) {
		if (!new_role && !only_process(&tr->classes, p)) {
			if (!warned)
				WARN(fp->handle, "Discarding role_transition "
					"rules for security classes other than "
					"\"process\"");
			warned = 1;
			continue;
		}
		if (role_set_write(&tr->roles, fp))
			return POLICYDB_ERROR;
		if (type_set_write(&tr->types, fp))
			return POLICYDB_ERROR;
		if (new_role)
			if (ebitmap_write(&tr->classes, fp))
				return POLICYDB_ERROR;
		buf[0] = cpu_to_le32(tr->new_role);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
	}
	return POLICYDB_SUCCESS;
}

static int role_allow_rule_write(role_allow_rule_t * r, struct policy_file *fp)
{
	int nel = 0;
	size_t items;
	uint32_t buf[1];
	role_allow_rule_t *ra;

	for (ra = r; ra; ra = ra->next)
		nel++;
	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;
	for (ra = r; ra; ra = ra->next) {
		if (role_set_write(&ra->roles, fp))
			return POLICYDB_ERROR;
		if (role_set_write(&ra->new_roles, fp))
			return POLICYDB_ERROR;
	}
	return POLICYDB_SUCCESS;
}

static int filename_trans_rule_write(policydb_t *p, filename_trans_rule_t *t,
				     struct policy_file *fp)
{
	int nel = 0;
	size_t items, entries;
	uint32_t buf[3], len;
	filename_trans_rule_t *ftr;

	for (ftr = t; ftr; ftr = ftr->next)
		nel++;

	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;

	for (ftr = t; ftr; ftr = ftr->next) {
		len = strlen(ftr->name);
		buf[0] = cpu_to_le32(len);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;

		items = put_entry(ftr->name, sizeof(char), len, fp);
		if (items != len)
			return POLICYDB_ERROR;

		if (type_set_write(&ftr->stypes, fp))
			return POLICYDB_ERROR;
		if (type_set_write(&ftr->ttypes, fp))
			return POLICYDB_ERROR;

		buf[0] = cpu_to_le32(ftr->tclass);
		buf[1] = cpu_to_le32(ftr->otype);
		buf[2] = cpu_to_le32(ftr->flags);

		if (p->policyvers >= MOD_POLICYDB_VERSION_SELF_TYPETRANS) {
			entries = 3;
		} else if (!(ftr->flags & RULE_SELF)) {
			entries = 2;
		} else {
			ERR(fp->handle,
			    "Module contains a self rule not supported by the target module policy version");
			return POLICYDB_ERROR;
		}

		items = put_entry(buf, sizeof(uint32_t), entries, fp);
		if (items != entries)
			return POLICYDB_ERROR;
	}
	return POLICYDB_SUCCESS;
}

static int range_trans_rule_write(range_trans_rule_t * t,
				  struct policy_file *fp)
{
	int nel = 0;
	size_t items;
	uint32_t buf[1];
	range_trans_rule_t *rt;

	for (rt = t; rt; rt = rt->next)
		nel++;
	buf[0] = cpu_to_le32(nel);
	items = put_entry(buf, sizeof(uint32_t), 1, fp);
	if (items != 1)
		return POLICYDB_ERROR;
	for (rt = t; rt; rt = rt->next) {
		if (type_set_write(&rt->stypes, fp))
			return POLICYDB_ERROR;
		if (type_set_write(&rt->ttypes, fp))
			return POLICYDB_ERROR;
		if (ebitmap_write(&rt->tclasses, fp))
			return POLICYDB_ERROR;
		if (mls_write_semantic_range_helper(&rt->trange, fp))
			return POLICYDB_ERROR;
	}
	return POLICYDB_SUCCESS;
}

static int scope_index_write(scope_index_t * scope_index,
			     unsigned int num_scope_syms,
			     struct policy_file *fp)
{
	unsigned int i;
	uint32_t buf[1];
	for (i = 0; i < num_scope_syms; i++) {
		if (ebitmap_write(scope_index->scope + i, fp) == -1) {
			return POLICYDB_ERROR;
		}
	}
	buf[0] = cpu_to_le32(scope_index->class_perms_len);
	if (put_entry(buf, sizeof(uint32_t), 1, fp) != 1) {
		return POLICYDB_ERROR;
	}
	for (i = 0; i < scope_index->class_perms_len; i++) {
		if (ebitmap_write(scope_index->class_perms_map + i, fp) == -1) {
			return POLICYDB_ERROR;
		}
	}
	return POLICYDB_SUCCESS;
}

static int avrule_decl_write(avrule_decl_t * decl, int num_scope_syms,
			     policydb_t * p, struct policy_file *fp)
{
	struct policy_data pd;
	uint32_t buf[2];
	int i;
	buf[0] = cpu_to_le32(decl->decl_id);
	buf[1] = cpu_to_le32(decl->enabled);
	if (put_entry(buf, sizeof(uint32_t), 2, fp) != 2) {
		return POLICYDB_ERROR;
	}
	if (cond_write_list(p, decl->cond_list, fp) == -1 ||
	    avrule_write_list(p, decl->avrules, fp) == -1 ||
	    role_trans_rule_write(p, decl->role_tr_rules, fp) == -1 ||
	    role_allow_rule_write(decl->role_allow_rules, fp) == -1) {
		return POLICYDB_ERROR;
	}

	if (p->policyvers >= MOD_POLICYDB_VERSION_FILENAME_TRANS &&
	    filename_trans_rule_write(p, decl->filename_trans_rules, fp))
		return POLICYDB_ERROR;

	if (p->policyvers >= MOD_POLICYDB_VERSION_RANGETRANS &&
	    range_trans_rule_write(decl->range_tr_rules, fp) == -1) {
		return POLICYDB_ERROR;
	}
	if (scope_index_write(&decl->required, num_scope_syms, fp) == -1 ||
	    scope_index_write(&decl->declared, num_scope_syms, fp) == -1) {
		return POLICYDB_ERROR;
	}
	pd.fp = fp;
	pd.p = p;
	for (i = 0; i < num_scope_syms; i++) {
		buf[0] = cpu_to_le32(decl->symtab[i].nprim);
		buf[1] = cpu_to_le32(decl->symtab[i].table->nel);
		if (put_entry(buf, sizeof(uint32_t), 2, fp) != 2) {
			return POLICYDB_ERROR;
		}
		if (ksu_hashtab_map(decl->symtab[i].table, write_f[i], &pd)) {
			return POLICYDB_ERROR;
		}
	}
	return POLICYDB_SUCCESS;
}

static int avrule_block_write(avrule_block_t * block, int num_scope_syms,
			      policydb_t * p, struct policy_file *fp)
{
	/* first write a count of the total number of blocks */
	uint32_t buf[1], num_blocks = 0;
	avrule_block_t *cur;
	for (cur = block; cur != NULL; cur = cur->next) {
		num_blocks++;
	}
	buf[0] = cpu_to_le32(num_blocks);
	if (put_entry(buf, sizeof(uint32_t), 1, fp) != 1) {
		return POLICYDB_ERROR;
	}

	/* now write each block */
	for (cur = block; cur != NULL; cur = cur->next) {
		uint32_t num_decls = 0;
		avrule_decl_t *decl;
		/* write a count of number of branches */
		for (decl = cur->branch_list; decl != NULL; decl = decl->next) {
			num_decls++;
		}
		buf[0] = cpu_to_le32(num_decls);
		if (put_entry(buf, sizeof(uint32_t), 1, fp) != 1) {
			return POLICYDB_ERROR;
		}
		for (decl = cur->branch_list; decl != NULL; decl = decl->next) {
			if (avrule_decl_write(decl, num_scope_syms, p, fp) ==
			    -1) {
				return POLICYDB_ERROR;
			}
		}
	}
	return POLICYDB_SUCCESS;
}

static int scope_write(hashtab_key_t key, hashtab_datum_t datum, void *ptr)
{
	scope_datum_t *scope = (scope_datum_t *) datum;
	struct policy_data *pd = ptr;
	struct policy_file *fp = pd->fp;
	uint32_t static_buf[32], *dyn_buf = NULL, *buf;
	size_t key_len = strlen(key);
	unsigned int items = 2 + scope->decl_ids_len, i;
	int rc;

	buf = static_buf;
	if (items >= (sizeof(static_buf) / 4)) {
		/* too many things required, so dynamically create a
		 * buffer.  this would have been easier with C99's
		 * dynamic arrays... */
		rc = POLICYDB_ERROR;
		dyn_buf = calloc(items, sizeof(*dyn_buf));
		if (!dyn_buf)
			goto err;
		buf = dyn_buf;
	}
	buf[0] = cpu_to_le32(key_len);

	rc = POLICYDB_ERROR;
	if (put_entry(buf, sizeof(*buf), 1, fp) != 1 ||
	    put_entry(key, 1, key_len, fp) != key_len)
		goto err;
	buf[0] = cpu_to_le32(scope->scope);
	buf[1] = cpu_to_le32(scope->decl_ids_len);

	for (i = 0; i < scope->decl_ids_len; i++)
		buf[2 + i] = cpu_to_le32(scope->decl_ids[i]);

	rc = POLICYDB_ERROR;
	if (put_entry(buf, sizeof(*buf), items, fp) != items)
		goto err;
	rc = POLICYDB_SUCCESS;
err:
	free(dyn_buf);
	return rc;
}

static int type_attr_uncount(hashtab_key_t key __attribute__ ((unused)),
			     hashtab_datum_t datum, void *args)
{
	type_datum_t *typdatum = datum;
	uint32_t *p_nel = args;

	if (typdatum->flavor == TYPE_ATTRIB) {
		/* uncount attribute from total number of types */
		(*p_nel)--;
	}
	return 0;
}

static int role_attr_uncount(hashtab_key_t key __attribute__ ((unused)),
			     hashtab_datum_t datum, void *args)
{
	role_datum_t *role = datum;
	uint32_t *p_nel = args;

	if (role->flavor == ROLE_ATTRIB) {
		/* uncount attribute from total number of roles */
		(*p_nel)--;
	}
	return 0;
}

/*
 * Write the configuration data in a policy database
 * structure to a policy database binary representation
 * file.
 */
int ksu_policydb_write(policydb_t * p, struct policy_file *fp)
{
	unsigned int i, num_syms;
	uint32_t buf[32], config;
	size_t items, items2, len;
	const struct policydb_compat_info *info;
	struct policy_data pd;
	const char *policydb_str;

	if (p->unsupported_format)
		return POLICYDB_UNSUPPORTED;

	pd.fp = fp;
	pd.p = p;

	config = 0;
	if (p->mls) {
		if ((p->policyvers < POLICYDB_VERSION_MLS &&
		    p->policy_type == POLICY_KERN) ||
		    (p->policyvers < MOD_POLICYDB_VERSION_MLS &&
		    p->policy_type == POLICY_BASE) ||
		    (p->policyvers < MOD_POLICYDB_VERSION_MLS &&
		    p->policy_type == POLICY_MOD)) {
			ERR(fp->handle, "policy version %d cannot support MLS",
			    p->policyvers);
			return POLICYDB_ERROR;
		}
		config |= POLICYDB_CONFIG_MLS;
	}

	config |= (POLICYDB_CONFIG_UNKNOWN_MASK & p->handle_unknown);

	/* Write the magic number and string identifiers. */
	items = 0;
	if (p->policy_type == POLICY_KERN) {
		buf[items++] = cpu_to_le32(POLICYDB_MAGIC);
		len = strlen(policydb_target_strings[p->target_platform]);
		policydb_str = policydb_target_strings[p->target_platform];
	} else {
		buf[items++] = cpu_to_le32(POLICYDB_MOD_MAGIC);
		len = strlen(POLICYDB_MOD_STRING);
		policydb_str = POLICYDB_MOD_STRING;
	}
	buf[items++] = cpu_to_le32(len);
	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;
	items = put_entry(policydb_str, 1, len, fp);
	if (items != len)
		return POLICYDB_ERROR;

	/* Write the version, config, and table sizes. */
	items = 0;
	info = policydb_lookup_compat(p->policyvers, p->policy_type,
					p->target_platform);
	if (!info) {
		ERR(fp->handle, "compatibility lookup failed for policy "
		    "version %d", p->policyvers);
		return POLICYDB_ERROR;
	}

	if (p->policy_type != POLICY_KERN) {
		buf[items++] = cpu_to_le32(p->policy_type);
	}
	buf[items++] = cpu_to_le32(p->policyvers);
	buf[items++] = cpu_to_le32(config);
	buf[items++] = cpu_to_le32(info->sym_num);
	buf[items++] = cpu_to_le32(info->ocon_num);

	items2 = put_entry(buf, sizeof(uint32_t), items, fp);
	if (items != items2)
		return POLICYDB_ERROR;

	if (p->policy_type == POLICY_MOD) {
		/* Write module name and version */
		len = strlen(p->name);
		buf[0] = cpu_to_le32(len);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		items = put_entry(p->name, 1, len, fp);
		if (items != len)
			return POLICYDB_ERROR;
		len = strlen(p->version);
		buf[0] = cpu_to_le32(len);
		items = put_entry(buf, sizeof(uint32_t), 1, fp);
		if (items != 1)
			return POLICYDB_ERROR;
		items = put_entry(p->version, 1, len, fp);
		if (items != len)
			return POLICYDB_ERROR;
	}

	if ((p->policyvers >= POLICYDB_VERSION_POLCAP &&
	     p->policy_type == POLICY_KERN) ||
	    (p->policyvers >= MOD_POLICYDB_VERSION_POLCAP &&
	     p->policy_type == POLICY_BASE) ||
	    (p->policyvers >= MOD_POLICYDB_VERSION_POLCAP &&
	     p->policy_type == POLICY_MOD)) {
		if (ebitmap_write(&p->policycaps, fp) == -1)
			return POLICYDB_ERROR;
	}

	if (p->policyvers < POLICYDB_VERSION_PERMISSIVE &&
	    p->policy_type == POLICY_KERN) {
		ebitmap_node_t *tnode;

		ebitmap_for_each_positive_bit(&p->permissive_map, tnode, i) {
			WARN(fp->handle, "Warning! Policy version %d cannot "
			     "support permissive types, but some were defined",
			     p->policyvers);
			break;
		}
	}

	if (p->policyvers >= POLICYDB_VERSION_PERMISSIVE &&
	    p->policy_type == POLICY_KERN) {
		if (ebitmap_write(&p->permissive_map, fp) == -1)
			return POLICYDB_ERROR;
	}

	num_syms = info->sym_num;
	for (i = 0; i < num_syms; i++) {
		buf[0] = cpu_to_le32(p->symtab[i].nprim);
		buf[1] = p->symtab[i].table->nel;

		/*
		 * A special case when writing type/attribute symbol table.
		 * The kernel policy version less than 24 does not support
		 * to load entries of attribute, so we have to re-calculate
		 * the actual number of types except for attributes.
		 */
		if (i == SYM_TYPES &&
		    p->policyvers < POLICYDB_VERSION_BOUNDARY &&
		    p->policy_type == POLICY_KERN) {
			ksu_hashtab_map(p->symtab[i].table, type_attr_uncount, &buf[1]);
		}

		/*
		 * Another special case when writing role/attribute symbol
		 * table, role attributes are redundant for policy.X, or
		 * when the pp's version is not big enough. So deduct
		 * their numbers from p_roles.table->nel.
		 */
		if ((i == SYM_ROLES) &&
		    ((p->policy_type == POLICY_KERN) ||
		     (p->policy_type != POLICY_KERN &&
		      p->policyvers < MOD_POLICYDB_VERSION_ROLEATTRIB)))
			(void)ksu_hashtab_map(p->symtab[i].table, role_attr_uncount, &buf[1]);

		buf[1] = cpu_to_le32(buf[1]);
		items = put_entry(buf, sizeof(uint32_t), 2, fp);
		if (items != 2)
			return POLICYDB_ERROR;
		if (ksu_hashtab_map(p->symtab[i].table, write_f[i], &pd))
			return POLICYDB_ERROR;
	}

	if (p->policy_type == POLICY_KERN) {
		if (avtab_write(p, &p->te_avtab, fp))
			return POLICYDB_ERROR;
		if (p->policyvers < POLICYDB_VERSION_BOOL) {
			if (p->p_bools.nprim)
				WARN(fp->handle, "Discarding "
				     "booleans and conditional rules");
		} else {
			if (cond_write_list(p, p->cond_list, fp))
				return POLICYDB_ERROR;
		}
		if (role_trans_write(p, fp))
			return POLICYDB_ERROR;
		if (role_allow_write(p->role_allow, fp))
			return POLICYDB_ERROR;
		if (p->policyvers >= POLICYDB_VERSION_FILENAME_TRANS) {
			if (filename_trans_write(p, fp))
				return POLICYDB_ERROR;
		} else {
			if (p->filename_trans)
				WARN(fp->handle, "Discarding filename type transition rules");
		}
	} else {
		if (avrule_block_write(p->global, num_syms, p, fp) == -1) {
			return POLICYDB_ERROR;
		}

		for (i = 0; i < num_syms; i++) {
			buf[0] = cpu_to_le32(p->scope[i].table->nel);
			if (put_entry(buf, sizeof(uint32_t), 1, fp) != 1) {
				return POLICYDB_ERROR;
			}
			if (ksu_hashtab_map(p->scope[i].table, scope_write, &pd))
				return POLICYDB_ERROR;
		}
	}

	if (ocontext_write(info, p, fp) == -1 || genfs_write(p, fp) == -1) {
		return POLICYDB_ERROR;
	}

	if ((p->policyvers >= POLICYDB_VERSION_MLS
	     && p->policy_type == POLICY_KERN)
	    || (p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policyvers < MOD_POLICYDB_VERSION_RANGETRANS
		&& p->policy_type == POLICY_BASE)) {
		if (range_write(p, fp)) {
			return POLICYDB_ERROR;
		}
	}

	if (p->policy_type == POLICY_KERN
	    && p->policyvers >= POLICYDB_VERSION_AVTAB) {
		for (i = 0; i < p->p_types.nprim; i++) {
			if (ebitmap_write(&p->type_attr_map[i], fp) == -1)
				return POLICYDB_ERROR;
		}
	}

	return POLICYDB_SUCCESS;
}

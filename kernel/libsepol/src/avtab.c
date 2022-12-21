
/* Author : Stephen Smalley, <sds@tycho.nsa.gov> */

/*
 * Updated: Yuichi Nakamura <ynakam@hitachisoft.jp>
 * 	Tuned number of hash slots for avtab to reduce memory usage
 */

/* Updated: Frank Mayer <mayerf@tresys.com>
 *          and Karl MacMillan <kmacmillan@mentalrootkit.com>
 *
 * 	Added conditional policy language extensions
 *
 * Updated: Red Hat, Inc.  James Morris <jmorris@redhat.com>
 *
 *      Code cleanup
 *
 * Updated: Karl MacMillan <kmacmillan@mentalrootkit.com>
 *
 * Copyright (C) 2003 Tresys Technology, LLC
 * Copyright (C) 2003,2007 Red Hat, Inc.
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
 * Implementation of the access vector table type.
 */

// #include <stdlib.h>
#include <sepol/policydb/avtab.h>
#include <sepol/policydb/policydb.h>
#include <sepol/errcodes.h>

#include "debug.h"
#include "private.h"
#include "kernel.h"

/* Based on MurmurHash3, written by Austin Appleby and placed in the
 * public domain.
 */
ignore_unsigned_overflow_
static inline int avtab_hash(struct avtab_key *keyp, uint32_t mask)
{
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	static const uint32_t r1 = 15;
	static const uint32_t r2 = 13;
	static const uint32_t m  = 5;
	static const uint32_t n  = 0xe6546b64;

	uint32_t hash = 0;

#define mix(input) do { \
	uint32_t v = input; \
	v *= c1; \
	v = (v << r1) | (v >> (32 - r1)); \
	v *= c2; \
	hash ^= v; \
	hash = (hash << r2) | (hash >> (32 - r2)); \
	hash = hash * m + n; \
} while (0)

	mix(keyp->target_class);
	mix(keyp->target_type);
	mix(keyp->source_type);

#undef mix

	hash ^= hash >> 16;
	hash *= 0x85ebca6b;
	hash ^= hash >> 13;
	hash *= 0xc2b2ae35;
	hash ^= hash >> 16;

	return hash & mask;
}

static avtab_ptr_t
avtab_insert_node(avtab_t * h, int hvalue, avtab_ptr_t prev, avtab_key_t * key,
		  avtab_datum_t * datum)
{
	avtab_ptr_t newnode;
	avtab_extended_perms_t *xperms;

	newnode = (avtab_ptr_t) malloc(sizeof(struct avtab_node));
	if (newnode == NULL)
		return NULL;
	memset(newnode, 0, sizeof(struct avtab_node));
	newnode->key = *key;

	if (key->specified & AVTAB_XPERMS) {
		xperms = calloc(1, sizeof(avtab_extended_perms_t));
		if (xperms == NULL) {
			free(newnode);
			return NULL;
		}
		if (datum->xperms) /* else caller populates xperms */
			*xperms = *(datum->xperms);

		newnode->datum.xperms = xperms;
		/* data is usually ignored with xperms, except in the case of
		 * neverallow checking, which requires permission bits to be set.
		 * So copy data so it is set in the avtab
		 */
		newnode->datum.data = datum->data;
	} else {
		newnode->datum = *datum;
	}

	if (prev) {
		newnode->next = prev->next;
		prev->next = newnode;
	} else {
		newnode->next = h->htable[hvalue];
		h->htable[hvalue] = newnode;
	}

	h->nel++;
	return newnode;
}

int avtab_insert(avtab_t * h, avtab_key_t * key, avtab_datum_t * datum)
{
	int hvalue;
	avtab_ptr_t prev, cur, newnode;
	uint16_t specified =
	    key->specified & ~(AVTAB_ENABLED | AVTAB_ENABLED_OLD);

	if (!h || !h->htable)
		return SEPOL_ENOMEM;

	hvalue = avtab_hash(key, h->mask);
	for (prev = NULL, cur = h->htable[hvalue];
	     cur; prev = cur, cur = cur->next) {
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class == cur->key.target_class &&
		    (specified & cur->key.specified)) {
			/* Extended permissions are not necessarily unique */
			if (specified & AVTAB_XPERMS)
				break;
			return SEPOL_EEXIST;
		}
		if (key->source_type < cur->key.source_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type < cur->key.target_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class < cur->key.target_class)
			break;
	}

	newnode = avtab_insert_node(h, hvalue, prev, key, datum);
	if (!newnode)
		return SEPOL_ENOMEM;

	return 0;
}

/* Unlike avtab_insert(), this function allow multiple insertions of the same 
 * key/specified mask into the table, as needed by the conditional avtab.  
 * It also returns a pointer to the node inserted.
 */
avtab_ptr_t
ksu_avtab_insert_nonunique(avtab_t * h, avtab_key_t * key, avtab_datum_t * datum)
{
	int hvalue;
	avtab_ptr_t prev, cur, newnode;
	uint16_t specified =
	    key->specified & ~(AVTAB_ENABLED | AVTAB_ENABLED_OLD);

	if (!h || !h->htable)
		return NULL;
	hvalue = avtab_hash(key, h->mask);
	for (prev = NULL, cur = h->htable[hvalue];
	     cur; prev = cur, cur = cur->next) {
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class == cur->key.target_class &&
		    (specified & cur->key.specified))
			break;
		if (key->source_type < cur->key.source_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type < cur->key.target_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class < cur->key.target_class)
			break;
	}
	newnode = avtab_insert_node(h, hvalue, prev, key, datum);

	return newnode;
}

avtab_datum_t *ksu_avtab_search(avtab_t * h, avtab_key_t * key)
{
	int hvalue;
	avtab_ptr_t cur;
	uint16_t specified =
	    key->specified & ~(AVTAB_ENABLED | AVTAB_ENABLED_OLD);

	if (!h || !h->htable)
		return NULL;

	hvalue = avtab_hash(key, h->mask);
	for (cur = h->htable[hvalue]; cur; cur = cur->next) {
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class == cur->key.target_class &&
		    (specified & cur->key.specified))
			return &cur->datum;

		if (key->source_type < cur->key.source_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type < cur->key.target_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class < cur->key.target_class)
			break;
	}

	return NULL;
}

/* This search function returns a node pointer, and can be used in
 * conjunction with avtab_search_next_node()
 */
avtab_ptr_t ksu_avtab_search_node(avtab_t * h, avtab_key_t * key)
{
	int hvalue;
	avtab_ptr_t cur;
	uint16_t specified =
	    key->specified & ~(AVTAB_ENABLED | AVTAB_ENABLED_OLD);

	if (!h || !h->htable)
		return NULL;

	hvalue = avtab_hash(key, h->mask);
	for (cur = h->htable[hvalue]; cur; cur = cur->next) {
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class == cur->key.target_class &&
		    (specified & cur->key.specified))
			return cur;

		if (key->source_type < cur->key.source_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type < cur->key.target_type)
			break;
		if (key->source_type == cur->key.source_type &&
		    key->target_type == cur->key.target_type &&
		    key->target_class < cur->key.target_class)
			break;
	}
	return NULL;
}

avtab_ptr_t ksu_avtab_search_node_next(avtab_ptr_t node, int specified)
{
	avtab_ptr_t cur;

	if (!node)
		return NULL;

	specified &= ~(AVTAB_ENABLED | AVTAB_ENABLED_OLD);
	for (cur = node->next; cur; cur = cur->next) {
		if (node->key.source_type == cur->key.source_type &&
		    node->key.target_type == cur->key.target_type &&
		    node->key.target_class == cur->key.target_class &&
		    (specified & cur->key.specified))
			return cur;

		if (node->key.source_type < cur->key.source_type)
			break;
		if (node->key.source_type == cur->key.source_type &&
		    node->key.target_type < cur->key.target_type)
			break;
		if (node->key.source_type == cur->key.source_type &&
		    node->key.target_type == cur->key.target_type &&
		    node->key.target_class < cur->key.target_class)
			break;
	}
	return NULL;
}

void ksu_avtab_destroy(avtab_t * h)
{
	unsigned int i;
	avtab_ptr_t cur, temp;

	if (!h || !h->htable)
		return;

	for (i = 0; i < h->nslot; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			if (cur->key.specified & AVTAB_XPERMS) {
				free(cur->datum.xperms);
			}
			temp = cur;
			cur = cur->next;
			free(temp);
		}
		h->htable[i] = NULL;
	}
	free(h->htable);
	h->htable = NULL;
	h->nslot = 0;
	h->mask = 0;
}

int avtab_map(avtab_t * h,
	      int (*apply) (avtab_key_t * k,
			    avtab_datum_t * d, void *args), void *args)
{
	unsigned int i;
	int ret;
	avtab_ptr_t cur;

	if (!h)
		return 0;

	for (i = 0; i < h->nslot; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			ret = apply(&cur->key, &cur->datum, args);
			if (ret)
				return ret;
			cur = cur->next;
		}
	}
	return 0;
}

int ksu_avtab_init(avtab_t * h)
{
	h->htable = NULL;
	h->nel = 0;
	return 0;
}

int ksu_avtab_alloc(avtab_t *h, uint32_t nrules)
{
	uint32_t mask = 0;
	uint32_t shift = 0;
	uint32_t work = nrules;
	uint32_t nslot = 0;

	if (nrules == 0)
		goto out;

	while (work) {
		work  = work >> 1;
		shift++;
	}
	if (shift > 2)
		shift = shift - 2;
	nslot = UINT32_C(1) << shift;
	if (nslot > MAX_AVTAB_HASH_BUCKETS)
		nslot = MAX_AVTAB_HASH_BUCKETS;
	mask = nslot - 1;

	h->htable = calloc(nslot, sizeof(avtab_ptr_t));
	if (!h->htable)
		return -1;
out:
	h->nel = 0;
	h->nslot = nslot;
	h->mask = mask;
	return 0;
}

void ksu_avtab_hash_eval(avtab_t * h, char *tag)
{
	unsigned int i, chain_len, slots_used, max_chain_len;
	avtab_ptr_t cur;

	slots_used = 0;
	max_chain_len = 0;
	for (i = 0; i < h->nslot; i++) {
		cur = h->htable[i];
		if (cur) {
			slots_used++;
			chain_len = 0;
			while (cur) {
				chain_len++;
				cur = cur->next;
			}

			if (chain_len > max_chain_len)
				max_chain_len = chain_len;
		}
	}

	printf
	    ("%s:  %d entries and %d/%d buckets used, longest chain length %d\n",
	     tag, h->nel, slots_used, h->nslot, max_chain_len);
}

/* Ordering of datums in the original avtab format in the policy file. */
static const uint16_t spec_order[] = {
	AVTAB_ALLOWED,
	AVTAB_AUDITDENY,
	AVTAB_AUDITALLOW,
	AVTAB_TRANSITION,
	AVTAB_CHANGE,
	AVTAB_MEMBER,
	AVTAB_XPERMS_ALLOWED,
	AVTAB_XPERMS_AUDITALLOW,
	AVTAB_XPERMS_DONTAUDIT
};

int ksu_avtab_read_item(struct policy_file *fp, uint32_t vers, avtab_t * a,
		    int (*insertf) (avtab_t * a, avtab_key_t * k,
				    avtab_datum_t * d, void *p), void *p)
{
	uint8_t buf8;
	uint16_t buf16[4], enabled;
	uint32_t buf32[8], items, items2, val;
	avtab_key_t key;
	avtab_datum_t datum;
	avtab_extended_perms_t xperms;
	unsigned set;
	unsigned int i;
	int rc;

	memset(&key, 0, sizeof(avtab_key_t));
	memset(&datum, 0, sizeof(avtab_datum_t));
	memset(&xperms, 0, sizeof(avtab_extended_perms_t));

	if (vers < POLICYDB_VERSION_AVTAB) {
		rc = next_entry(buf32, fp, sizeof(uint32_t));
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			return -1;
		}
		items2 = le32_to_cpu(buf32[0]);

		if (items2 < 5 || items2 > ARRAY_SIZE(buf32)) {
			ERR(fp->handle, "invalid item count");
			return -1;
		}

		rc = next_entry(buf32, fp, sizeof(uint32_t) * items2);
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			return -1;
		}

		items = 0;
		val = le32_to_cpu(buf32[items++]);
		key.source_type = (uint16_t) val;
		if (key.source_type != val) {
			ERR(fp->handle, "truncated source type");
			return -1;
		}
		val = le32_to_cpu(buf32[items++]);
		key.target_type = (uint16_t) val;
		if (key.target_type != val) {
			ERR(fp->handle, "truncated target type");
			return -1;
		}
		val = le32_to_cpu(buf32[items++]);
		key.target_class = (uint16_t) val;
		if (key.target_class != val) {
			ERR(fp->handle, "truncated target class");
			return -1;
		}

		val = le32_to_cpu(buf32[items++]);
		enabled = (val & AVTAB_ENABLED_OLD) ? AVTAB_ENABLED : 0;

		if (!(val & (AVTAB_AV | AVTAB_TYPE))) {
			ERR(fp->handle, "null entry");
			return -1;
		}
		if ((val & AVTAB_AV) && (val & AVTAB_TYPE)) {
			ERR(fp->handle, "entry has both access "
			    "vectors and types");
			return -1;
		}

		for (i = 0; i < ARRAY_SIZE(spec_order); i++) {
			if (val & spec_order[i]) {
				if (items >= items2) { /* items is index, items2 is total number */
					ERR(fp->handle, "entry has too many items (%d/%d)",
					    items + 1, items2);
					return -1;
				}
				key.specified = spec_order[i] | enabled;
				datum.data = le32_to_cpu(buf32[items++]);
				rc = insertf(a, &key, &datum, p);
				if (rc)
					return rc;
			}
		}

		if (items != items2) {
			ERR(fp->handle, "entry only had %d items, "
			    "expected %d", items2, items);
			return -1;
		}
		return 0;
	}

	rc = next_entry(buf16, fp, sizeof(uint16_t) * 4);
	if (rc < 0) {
		ERR(fp->handle, "truncated entry");
		return -1;
	}
	items = 0;
	key.source_type = le16_to_cpu(buf16[items++]);
	key.target_type = le16_to_cpu(buf16[items++]);
	key.target_class = le16_to_cpu(buf16[items++]);
	key.specified = le16_to_cpu(buf16[items++]);

	set = 0;
	for (i = 0; i < ARRAY_SIZE(spec_order); i++) {
		if (key.specified & spec_order[i])
			set++;
	}
	if (!set || set > 1) {
		ERR(fp->handle, "more than one specifier");
		return -1;
	}

	if ((vers < POLICYDB_VERSION_XPERMS_IOCTL) &&
			(key.specified & AVTAB_XPERMS)) {
		ERR(fp->handle, "policy version %u does not support extended "
				"permissions rules and one was specified", vers);
		return -1;
	} else if (key.specified & AVTAB_XPERMS) {
		rc = next_entry(&buf8, fp, sizeof(uint8_t));
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			return -1;
		}
		xperms.specified = buf8;
		rc = next_entry(&buf8, fp, sizeof(uint8_t));
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			return -1;
		}
		xperms.driver = buf8;
		rc = next_entry(buf32, fp, sizeof(uint32_t)*8);
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			return -1;
		}
		for (i = 0; i < ARRAY_SIZE(xperms.perms); i++)
			xperms.perms[i] = le32_to_cpu(buf32[i]);
		datum.xperms = &xperms;
	} else {
		rc = next_entry(buf32, fp, sizeof(uint32_t));
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			return -1;
		}
		datum.data = le32_to_cpu(*buf32);
	}
	return insertf(a, &key, &datum, p);
}

static int avtab_insertf(avtab_t * a, avtab_key_t * k, avtab_datum_t * d,
			 void *p __attribute__ ((unused)))
{
	return avtab_insert(a, k, d);
}

int ksu_avtab_read(avtab_t * a, struct policy_file *fp, uint32_t vers)
{
	unsigned int i;
	int rc;
	uint32_t buf[1];
	uint32_t nel;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0) {
		ERR(fp->handle, "truncated table");
		goto bad;
	}
	nel = le32_to_cpu(buf[0]);
	if (!nel) {
		ERR(fp->handle, "table is empty");
		goto bad;
	}

	rc = ksu_avtab_alloc(a, nel);
	if (rc) {
		ERR(fp->handle, "out of memory");
		goto bad;
	}

	for (i = 0; i < nel; i++) {
		rc = ksu_avtab_read_item(fp, vers, a, avtab_insertf, NULL);
		if (rc) {
			if (rc == SEPOL_ENOMEM)
				ERR(fp->handle, "out of memory");
			if (rc == SEPOL_EEXIST)
				ERR(fp->handle, "duplicate entry");
			ERR(fp->handle, "failed on entry %d of %u", i, nel);
			goto bad;
		}
	}

	return 0;

      bad:
	ksu_avtab_destroy(a);
	return -1;
}

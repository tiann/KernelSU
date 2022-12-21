
/* Author : Stephen Smalley, <sds@tycho.nsa.gov> */

/*
 * Updated : Karl MacMillan <kmacmillan@mentalrootkit.com>
 *
 * Copyright (C) 2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


/* FLASK */

/*
 * Implementation of the hash table type.
 */

// #include <stdlib.h>
#include <linux/string.h>
#include <sepol/policydb/hashtab.h>

#include "private.h"

hashtab_t hashtab_create(unsigned int (*hash_value) (hashtab_t h,
						     const_hashtab_key_t key),
			 int (*keycmp) (hashtab_t h,
					const_hashtab_key_t key1,
					const_hashtab_key_t key2),
			 unsigned int size)
{

	hashtab_t p;

	p = (hashtab_t) malloc(sizeof(hashtab_val_t));
	if (p == NULL)
		return p;

	memset(p, 0, sizeof(hashtab_val_t));
	p->size = size;
	p->nel = 0;
	p->hash_value = hash_value;
	p->keycmp = keycmp;
	p->htable = (hashtab_ptr_t *) calloc(size, sizeof(hashtab_ptr_t));
	if (p->htable == NULL) {
		free(p);
		return NULL;
	}

	return p;
}

static void hashtab_check_resize(hashtab_t h)
{
	unsigned int hvalue, i, old_size, new_size = h->size;
	hashtab_ptr_t *new_htable, *dst, cur, next;

	while (new_size <= h->nel && new_size * 2 != 0)
		new_size *= 2;

	if (h->size == new_size)
		return;

	new_htable = calloc(new_size, sizeof(*new_htable));
	if (!new_htable)
		return;

	old_size = h->size;
	h->size = new_size;

	/* Move all elements to the new htable */
	for (i = 0; i < old_size; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			hvalue = h->hash_value(h, cur->key);
			dst = &new_htable[hvalue];
			while (*dst && h->keycmp(h, cur->key, (*dst)->key) > 0)
				dst = &(*dst)->next;

			next = cur->next;

			cur->next = *dst;
			*dst = cur;

			cur = next;
		}
	}
	free(h->htable);
	h->htable = new_htable;
}

int hashtab_insert(hashtab_t h, hashtab_key_t key, hashtab_datum_t datum)
{
	int hvalue;
	hashtab_ptr_t prev, cur, newnode;

	if (!h)
		return SEPOL_ENOMEM;

	hashtab_check_resize(h);

	hvalue = h->hash_value(h, key);
	prev = NULL;
	cur = h->htable[hvalue];
	while (cur && h->keycmp(h, key, cur->key) > 0) {
		prev = cur;
		cur = cur->next;
	}

	if (cur && (h->keycmp(h, key, cur->key) == 0))
		return SEPOL_EEXIST;

	newnode = (hashtab_ptr_t) malloc(sizeof(hashtab_node_t));
	if (newnode == NULL)
		return SEPOL_ENOMEM;
	memset(newnode, 0, sizeof(struct hashtab_node));
	newnode->key = key;
	newnode->datum = datum;
	if (prev) {
		newnode->next = prev->next;
		prev->next = newnode;
	} else {
		newnode->next = h->htable[hvalue];
		h->htable[hvalue] = newnode;
	}

	h->nel++;
	return SEPOL_OK;
}

int hashtab_remove(hashtab_t h, hashtab_key_t key,
		   void (*destroy) (hashtab_key_t k,
				    hashtab_datum_t d, void *args), void *args)
{
	int hvalue;
	hashtab_ptr_t cur, last;

	if (!h)
		return SEPOL_ENOENT;

	hvalue = h->hash_value(h, key);
	last = NULL;
	cur = h->htable[hvalue];
	while (cur != NULL && h->keycmp(h, key, cur->key) > 0) {
		last = cur;
		cur = cur->next;
	}

	if (cur == NULL || (h->keycmp(h, key, cur->key) != 0))
		return SEPOL_ENOENT;

	if (last == NULL)
		h->htable[hvalue] = cur->next;
	else
		last->next = cur->next;

	if (destroy)
		destroy(cur->key, cur->datum, args);
	free(cur);
	h->nel--;
	return SEPOL_OK;
}

hashtab_datum_t hashtab_search(hashtab_t h, const_hashtab_key_t key)
{

	int hvalue;
	hashtab_ptr_t cur;

	if (!h)
		return NULL;

	hvalue = h->hash_value(h, key);
	cur = h->htable[hvalue];
	while (cur != NULL && h->keycmp(h, key, cur->key) > 0)
		cur = cur->next;

	if (cur == NULL || (h->keycmp(h, key, cur->key) != 0))
		return NULL;

	return cur->datum;
}

void ksu_hashtab_destroy(hashtab_t h)
{
	unsigned int i;
	hashtab_ptr_t cur, temp;

	if (!h)
		return;

	for (i = 0; i < h->size; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			temp = cur;
			cur = cur->next;
			free(temp);
		}
		h->htable[i] = NULL;
	}

	free(h->htable);
	h->htable = NULL;

	free(h);
}

int ksu_hashtab_map(hashtab_t h,
		int (*apply) (hashtab_key_t k,
			      hashtab_datum_t d, void *args), void *args)
{
	unsigned int i;
	hashtab_ptr_t cur;
	int ret;

	if (!h)
		return SEPOL_OK;

	for (i = 0; i < h->size; i++) {
		cur = h->htable[i];
		while (cur != NULL) {
			ret = apply(cur->key, cur->datum, args);
			if (ret)
				return ret;
			cur = cur->next;
		}
	}
	return SEPOL_OK;
}

void hashtab_hash_eval(hashtab_t h, char *tag)
{
	unsigned int i;
	int chain_len, slots_used, max_chain_len;
	hashtab_ptr_t cur;

	slots_used = 0;
	max_chain_len = 0;
	for (i = 0; i < h->size; i++) {
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
	     tag, h->nel, slots_used, h->size, max_chain_len);
}

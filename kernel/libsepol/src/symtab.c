
/* Author : Stephen Smalley, <sds@tycho.nsa.gov> */

/* FLASK */

/*
 * Implementation of the symbol table type.
 */

#include <linux/string.h>

#include "private.h"

#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/symtab.h>

ignore_unsigned_overflow_
static unsigned int symhash(hashtab_t h, const_hashtab_key_t key)
{
	const char *p, *keyp;
	size_t size;
	unsigned int val;

	val = 0;
	keyp = (const char *)key;
	size = strlen(keyp);
	for (p = keyp; ((size_t) (p - keyp)) < size; p++)
		val =
		    (val << 4 | (val >> (8 * sizeof(unsigned int) - 4))) ^ (*p);
	return val & (h->size - 1);
}

static int symcmp(hashtab_t h
		  __attribute__ ((unused)), const_hashtab_key_t key1,
		  const_hashtab_key_t key2)
{
	return strcmp(key1, key2);
}

int ksu_symtab_init(symtab_t * s, unsigned int size)
{
	s->table = hashtab_create(symhash, symcmp, size);
	if (!s->table)
		return -1;
	s->nprim = 0;
	return 0;
}

void symtab_destroy(symtab_t * s)
{
	if (!s)
		return;
	if (s->table)
		ksu_hashtab_destroy(s->table);
	return;
}
/* FLASK */

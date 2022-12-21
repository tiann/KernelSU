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

#ifndef __CIL_SYMTAB_H_
#define __CIL_SYMTAB_H_

#include <sepol/policydb/symtab.h>
#include <sepol/policydb/hashtab.h>

#include "cil_tree.h"

struct cil_symtab_datum {
	struct cil_list *nodes;
	char *name;
	char *fqn;
	symtab_t *symtab;
};

#define DATUM(d) ((struct cil_symtab_datum *)(d))
#define NODE(n) ((struct cil_tree_node *)(DATUM(n)->nodes->head->data))
#define FLAVOR(f) (NODE(f)->flavor)

struct cil_complex_symtab_key {
	intptr_t key1;
	intptr_t key2;
	intptr_t key3;
	intptr_t key4;
};

struct cil_complex_symtab_datum {
	void *data;
};

struct cil_complex_symtab_node {
	struct cil_complex_symtab_key *ckey;
	struct cil_complex_symtab_datum *datum;
	struct cil_complex_symtab_node *next;
};

struct cil_complex_symtab {
	struct cil_complex_symtab_node **htable;
	uint32_t nelems;
	uint32_t nslots;
	uint32_t mask;
};

void cil_symtab_init(symtab_t *symtab, unsigned int size);
void cil_symtab_datum_init(struct cil_symtab_datum *datum);
void cil_symtab_datum_destroy(struct cil_symtab_datum *datum);
void cil_symtab_datum_remove_node(struct cil_symtab_datum *datum, struct cil_tree_node *node);
int cil_symtab_insert(symtab_t *symtab, hashtab_key_t key, struct cil_symtab_datum *datum, struct cil_tree_node *node);
void cil_symtab_remove_datum(struct cil_symtab_datum *datum);
int cil_symtab_get_datum(symtab_t *symtab, char *key, struct cil_symtab_datum **datum);
int cil_symtab_map(symtab_t *symtab,
				   int (*apply) (hashtab_key_t k, hashtab_datum_t d, void *args),
				   void *args);
void cil_symtab_destroy(symtab_t *symtab);
void cil_complex_symtab_init(struct cil_complex_symtab *symtab, unsigned int size);
int cil_complex_symtab_insert(struct cil_complex_symtab *symtab, struct cil_complex_symtab_key *ckey, struct cil_complex_symtab_datum *datum);
void cil_complex_symtab_search(struct cil_complex_symtab *symtab, struct cil_complex_symtab_key *ckey, struct cil_complex_symtab_datum **out);
void cil_complex_symtab_destroy(struct cil_complex_symtab *symtab);

#endif

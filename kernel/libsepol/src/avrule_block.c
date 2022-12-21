/* Authors: Jason Tang <jtang@tresys.com>
 *
 * Functions that manipulate a logical block (conditional, optional,
 * or global scope) for a policy module.
 *
 * Copyright (C) 2005 Tresys Technology, LLC
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

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/avrule_block.h>

// #include <assert.h>
// #include <stdlib.h>
#include "kernel.h"

/* It is anticipated that there be less declarations within an avrule
 * block than the global policy.  Thus the symbol table sizes are
 * smaller than those listed in policydb.c */
static const unsigned int symtab_sizes[SYM_NUM] = {
	2,
	4,
	8,
	32,
	16,
	4,
	2,
	2,
};

avrule_block_t *avrule_block_create(void)
{
	avrule_block_t *block;
	if ((block = calloc(1, sizeof(*block))) == NULL) {
		return NULL;
	}
	return block;
}

avrule_decl_t *avrule_decl_create(uint32_t decl_id)
{
	avrule_decl_t *decl;
	int i;
	if ((decl = calloc(1, sizeof(*decl))) == NULL) {
		return NULL;
	}
	decl->decl_id = decl_id;
	for (i = 0; i < SYM_NUM; i++) {
		if (ksu_symtab_init(&decl->symtab[i], symtab_sizes[i])) {
			avrule_decl_destroy(decl);
			return NULL;
		}
	}

	for (i = 0; i < SYM_NUM; i++) {
		ebitmap_init(&decl->required.scope[i]);
		ebitmap_init(&decl->declared.scope[i]);
	}
	return decl;
}

/* note that unlike the other destroy functions, this one does /NOT/
 * destroy the pointer itself */
static void scope_index_destroy(scope_index_t * scope)
{
	unsigned int i;
	if (scope == NULL) {
		return;
	}
	for (i = 0; i < SYM_NUM; i++) {
		ksu_ebitmap_destroy(scope->scope + i);
	}
	if (scope->class_perms_map) {
		for (i = 0; i < scope->class_perms_len; i++) {
			ksu_ebitmap_destroy(scope->class_perms_map + i);
		}
	}
	free(scope->class_perms_map);
}

void avrule_decl_destroy(avrule_decl_t * x)
{
	if (x == NULL) {
		return;
	}
	cond_list_destroy(x->cond_list);
	avrule_list_destroy(x->avrules);
	role_trans_rule_list_destroy(x->role_tr_rules);
	filename_trans_rule_list_destroy(x->filename_trans_rules);
	role_allow_rule_list_destroy(x->role_allow_rules);
	range_trans_rule_list_destroy(x->range_tr_rules);
	scope_index_destroy(&x->required);
	scope_index_destroy(&x->declared);
	symtabs_destroy(x->symtab);
	free(x->module_name);
	free(x);
}

void avrule_block_destroy(avrule_block_t * x)
{
	avrule_decl_t *decl;
	if (x == NULL) {
		return;
	}
	decl = x->branch_list;
	while (decl != NULL) {
		avrule_decl_t *next_decl = decl->next;
		avrule_decl_destroy(decl);
		decl = next_decl;
	}
	free(x);
}

void avrule_block_list_destroy(avrule_block_t * x)
{
	while (x != NULL) {
		avrule_block_t *next = x->next;
		avrule_block_destroy(x);
		x = next;
	}
}

/* Get a conditional node from a avrule_decl with the same expression.
 * If that expression does not exist then create one. */
cond_list_t *get_decl_cond_list(policydb_t * p, avrule_decl_t * decl,
				cond_list_t * cond)
{
	cond_list_t *result;
	int was_created;
	result = cond_node_find(p, cond, decl->cond_list, &was_created);
	if (result != NULL && was_created) {
		result->next = decl->cond_list;
		decl->cond_list = result;
	}
	return result;
}

/* Look up an identifier in a policy's scoping table.  If it is there,
 * marked as SCOPE_DECL, and any of its declaring block has been enabled,
 * then return 1.  Otherwise return 0. Can only be called after the 
 * decl_val_to_struct index has been created */
int is_id_enabled(char *id, policydb_t * p, int symbol_table)
{
	scope_datum_t *scope =
	    (scope_datum_t *) hashtab_search(p->scope[symbol_table].table, id);
	avrule_decl_t *decl;
	uint32_t len;

	if (scope == NULL) {
		return 0;
	}
	if (scope->scope != SCOPE_DECL) {
		return 0;
	}

	len = scope->decl_ids_len;
	if (len < 1) {
		return 0;
	}

	if (symbol_table == SYM_ROLES || symbol_table == SYM_USERS) {
		uint32_t i;
		for (i = 0; i < len; i++) {
			decl = p->decl_val_to_struct[scope->decl_ids[i] - 1];
			if (decl != NULL && decl->enabled) {
				return 1;
			}
		}
	} else {
		decl = p->decl_val_to_struct[scope->decl_ids[len-1] - 1];
		if (decl != NULL && decl->enabled) {
			return 1;
		}
	}

	return 0;
}

/* Check if a particular permission is present within the given class,
 * and that the class is enabled.  Returns 1 if both conditions are
 * true, 0 if neither could be found or if the class id disabled. */
int is_perm_enabled(char *class_id, char *perm_id, policydb_t * p)
{
	class_datum_t *cladatum;
	perm_datum_t *perm;
	if (!is_id_enabled(class_id, p, SYM_CLASSES)) {
		return 0;
	}
	cladatum =
	    (class_datum_t *) hashtab_search(p->p_classes.table, class_id);
	if (cladatum == NULL) {
		return 0;
	}
	perm = hashtab_search(cladatum->permissions.table, perm_id);
	if (perm == NULL && cladatum->comdatum != 0) {
		/* permission was not in this class.  before giving
		 * up, check the class's parent */
		perm =
		    hashtab_search(cladatum->comdatum->permissions.table,
				   perm_id);
	}
	if (perm == NULL) {
		return 0;
	}
	return 1;
}

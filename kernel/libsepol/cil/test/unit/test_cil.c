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

#include <sepol/policydb/policydb.h>

#include "CuTest.h"
#include "test_cil.h"

#include "../../src/cil_internal.h"
#include "../../src/cil_tree.h"

void test_cil_symtab_array_init(CuTest *tc) {
	struct cil_db *test_new_db;
	test_new_db = malloc(sizeof(*test_new_db));

	cil_symtab_array_init(test_new_db->symtab, cil_sym_sizes[CIL_SYM_ARRAY_ROOT]);
	CuAssertPtrNotNull(tc, test_new_db->symtab);

	free(test_new_db);
}

void test_cil_db_init(CuTest *tc) {
	struct cil_db *test_db;

	cil_db_init(&test_db);

	CuAssertPtrNotNull(tc, test_db->ast);
	CuAssertPtrNotNull(tc, test_db->symtab);
	CuAssertPtrNotNull(tc, test_db->symtab);
}

// TODO: Reach SEPOL_ERR return in cil_db_init ( currently can't produce a method to do so )

void test_cil_get_symtab_block(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->parent->flavor = CIL_BLOCK;
	test_ast_node->line = 1;

	int rc = cil_get_symtab(test_db, test_ast_node->parent, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, symtab);
}

void test_cil_get_symtab_class(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->parent->flavor = CIL_CLASS;
	test_ast_node->line = 1;

	int rc = cil_get_symtab(test_db, test_ast_node->parent, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, symtab);
}

void test_cil_get_symtab_root(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->parent->flavor = CIL_ROOT;
	test_ast_node->line = 1;

	int rc = cil_get_symtab(test_db, test_ast_node->parent, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, symtab);
}

void test_cil_get_symtab_flavor_neg(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->parent->flavor = 1234567;
	test_ast_node->line = 1;

	int rc = cil_get_symtab(test_db, test_ast_node->parent, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertPtrEquals(tc, symtab, NULL);
}

void test_cil_get_symtab_null_neg(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = NULL;
	test_ast_node->line = 1;

	int rc = cil_get_symtab(test_db, test_ast_node->parent, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertPtrEquals(tc, symtab, NULL);
}

void test_cil_get_symtab_node_null_neg(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_get_symtab(test_db, test_ast_node, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertPtrEquals(tc, symtab, NULL);
	CuAssertPtrEquals(tc, test_ast_node, NULL);
}

void test_cil_get_symtab_parent_null_neg(CuTest *tc) {
	symtab_t *symtab = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = NULL;
	test_ast_node->line = 1;

	int rc = cil_get_symtab(test_db, test_ast_node->parent, &symtab, CIL_SYM_BLOCKS);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertPtrEquals(tc, symtab, NULL);
}


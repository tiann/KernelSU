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
#include "test_cil_symtab.h"

#include "../../src/cil_tree.h"
#include "../../src/cil_symtab.h"
#include "../../src/cil_internal.h"

void test_cil_symtab_insert(CuTest *tc) {
	symtab_t *test_symtab = NULL;
	char* test_name = "test";
	struct cil_block *test_block = malloc(sizeof(*test_block));

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);   

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_symtab_array_init(test_block->symtab, cil_sym_sizes[CIL_SYM_ARRAY_BLOCK]);

	test_block->is_abstract = 0;

	cil_get_symtab(test_db, test_ast_node->parent, &test_symtab, CIL_SYM_BLOCKS);

	int rc = cil_symtab_insert(test_symtab, (hashtab_key_t)test_name, (struct cil_symtab_datum*)test_block, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

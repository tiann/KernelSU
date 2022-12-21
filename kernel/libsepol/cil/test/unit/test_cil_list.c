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

#include "CuTest.h"
#include "CilTest.h"

#include "../../src/cil_internal.h"
#include "../../src/cil_build_ast.h"

void test_cil_list_init(CuTest *tc) {
	struct cil_avrule *test_avrule = malloc(sizeof(*test_avrule));

	cil_classpermset_init(&test_avrule->classpermset);
	cil_permset_init(&test_avrule->classpermset->permset);

	cil_list_init(&test_avrule->classpermset->permset->perms_list_str);
	CuAssertPtrNotNull(tc, test_avrule->classpermset->permset->perms_list_str);

	cil_destroy_avrule(test_avrule);
}

void test_cil_list_append_item(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_append_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_list_append_item_append(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_append_item(test_class_list, test_new_item);
	
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head->next;
	
	int rc2 = cil_list_append_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, SEPOL_OK, rc2);
}

void test_cil_list_append_item_append_extra(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", "process", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_append_item(test_class_list, test_new_item);
	
	cil_list_item_init(&test_new_item);
	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head->next;
	
	int rc2 = cil_list_append_item(test_class_list, test_new_item);
	
	cil_list_item_init(&test_new_item);
	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head->next->next;
	
	int rc3 = cil_list_append_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, SEPOL_OK, rc2);
	CuAssertIntEquals(tc, SEPOL_OK, rc3);
}

void test_cil_list_append_item_listnull_neg(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list = NULL;

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_append_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_list_append_item_itemnull_neg(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item = NULL;

	int rc = cil_list_append_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_list_prepend_item(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_prepend_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_list_prepend_item_prepend(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_prepend_item(test_class_list, test_new_item);
	
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_list_prepend_item_prepend_neg(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", "process", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	struct cil_list_item *test_new_item_next;
	cil_list_item_init(&test_new_item_next);
	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head->next;
	test_new_item->next = test_new_item_next;	
	
	int rc = cil_list_prepend_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_list_prepend_item_listnull_neg(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list = NULL;

	struct cil_list_item *test_new_item;
	cil_list_item_init(&test_new_item);

	test_new_item->flavor = CIL_CLASS;
	test_new_item->data = test_tree->root->cl_head->cl_head->next->cl_head;

	int rc = cil_list_prepend_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_list_prepend_item_itemnull_neg(CuTest *tc) {
        char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", "relabelto", ")", "(", "eq", "12", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_list *test_class_list;
	cil_list_init(&test_class_list);

	struct cil_list_item *test_new_item = NULL;

	int rc = cil_list_prepend_item(test_class_list, test_new_item);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

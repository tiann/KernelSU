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
#include "CilTest.h"
#include "test_cil_build_ast.h"

#include "../../src/cil_build_ast.h"

#include "../../src/cil_tree.h"

int __cil_build_ast_node_helper(struct cil_tree_node *, uint32_t *, void *);
int __cil_build_ast_last_child_helper(__attribute__((unused)) struct cil_tree_node *parse_current, void *);
//int __cil_build_constrain_tree(struct cil_tree_node *parse_current, struct cil_tree_node *expr_root);

struct cil_args_build {
	struct cil_tree_node *ast;
	struct cil_db *db;
	struct cil_tree_node *macro;
	struct cil_tree_node *tifstack;
};

struct cil_args_build *gen_build_args(struct cil_tree_node *node, struct cil_db *db, struct cil_tree_node * macro, struct cil_tree_node *tifstack)
{
	struct cil_args_build *args = cil_malloc(sizeof(*args));
	args->ast = node;
	args->db = db;
	args->macro = macro;
	args->tifstack = tifstack;

	return args;
}

// First seen in cil_gen_common
void test_cil_parse_to_list(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	struct cil_avrule *test_avrule;
	cil_avrule_init(&test_avrule);
	test_avrule->rule_kind = CIL_AVRULE_ALLOWED;
	test_avrule->src_str = cil_strdup(test_current->next->data);
	test_avrule->tgt_str = cil_strdup(test_current->next->next->data);

	cil_classpermset_init(&test_avrule->classpermset);

	test_avrule->classpermset->class_str = cil_strdup(test_current->next->next->next->cl_head->data);

	cil_permset_init(&test_avrule->classpermset->permset);

	cil_list_init(&test_avrule->classpermset->permset->perms_list_str);

	test_current = test_current->next->next->next->cl_head->next->cl_head;

	int rc = cil_parse_to_list(test_current, test_avrule->classpermset->permset->perms_list_str, CIL_AST_STR);
	CuAssertIntEquals(tc, SEPOL_OK, rc);

	cil_destroy_avrule(test_avrule);
}

void test_cil_parse_to_list_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	struct cil_avrule *test_avrule;
	cil_avrule_init(&test_avrule);
	test_avrule->rule_kind = CIL_AVRULE_ALLOWED;
	test_avrule->src_str = cil_strdup(test_current->next->data);
	test_avrule->tgt_str = cil_strdup(test_current->next->next->data);

	cil_classpermset_init(&test_avrule->classpermset);

	test_avrule->classpermset->class_str = cil_strdup(test_current->next->next->next->cl_head->data);

	cil_permset_init(&test_avrule->classpermset->permset);

	cil_list_init(&test_avrule->classpermset->permset->perms_list_str);

	test_current = NULL;

	int rc = cil_parse_to_list(test_current, test_avrule->classpermset->permset->perms_list_str, CIL_AST_STR);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);

	cil_destroy_avrule(test_avrule);
}

void test_cil_parse_to_list_listnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	struct cil_avrule *test_avrule;
	cil_avrule_init(&test_avrule);
	test_avrule->rule_kind = CIL_AVRULE_ALLOWED;
	test_avrule->src_str = cil_strdup(test_current->next->data);
	test_avrule->tgt_str = cil_strdup(test_current->next->next->data);

	cil_classpermset_init(&test_avrule->classpermset);

	test_avrule->classpermset->class_str = cil_strdup(test_current->next->next->next->cl_head->data);

	cil_permset_init(&test_avrule->classpermset->permset);

	test_current = test_current->next->next->next->cl_head->next->cl_head;

	int rc = cil_parse_to_list(test_current, test_avrule->classpermset->permset->perms_list_str, CIL_AST_STR);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);

	cil_destroy_avrule(test_avrule);
}

void test_cil_set_to_list(CuTest *tc) {
	char *line[] = {"(", "foo1", "foo2", "(", "foo3", ")", ")", NULL};

	struct cil_tree *test_tree;
	struct cil_list *cil_l = NULL;
	struct cil_list *sub_list = NULL;

	gen_test_tree(&test_tree, line);
	cil_list_init(&cil_l);

	int rc = cil_set_to_list(test_tree->root->cl_head, cil_l, 1);
	sub_list = (struct cil_list *)cil_l->head->next->next->data;

	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertStrEquals(tc, "foo1", (char*)cil_l->head->data);
	CuAssertStrEquals(tc, "foo2", (char*)cil_l->head->next->data);
	CuAssertStrEquals(tc, "foo3", (char*)sub_list->head->data);
}

void test_cil_set_to_list_tree_node_null_neg(CuTest *tc) {
	struct cil_list *cil_l = NULL;
	int rc = cil_set_to_list(NULL, cil_l, 1);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_set_to_list_cl_head_null_neg(CuTest *tc) {
	char *line[] = {"(", "foo", "bar", ")", NULL};

	struct cil_list *cil_l;
	struct cil_tree *test_tree = NULL;

	cil_list_init(&cil_l);
	gen_test_tree(&test_tree, line);
	test_tree->root->cl_head = NULL;

	int rc = cil_set_to_list(test_tree->root, cil_l, 1);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_set_to_list_listnull_neg(CuTest *tc) {
	char *line[] = {"(", "foo1", "foo2", "foo3", ")", NULL};

	struct cil_tree *test_tree = NULL;
	gen_test_tree(&test_tree, line);

	int rc = cil_set_to_list(test_tree->root, NULL, 1);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_block(CuTest *tc) {
	char *line[] = {"(", "block", "a", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, ((struct cil_block*)test_ast_node->data)->is_abstract, 0);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_BLOCK);
}

void test_cil_gen_block_justblock_neg(CuTest *tc) {
	char *line[] = {"(", "block", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_block_noname_neg(CuTest *tc) {
	char *line[] = {"(", "block", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_block_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "block", "foo", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_block_treenull_neg(CuTest *tc) {
	char *line[] = {"(", "block", "foo", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_tree->root->cl_head->cl_head = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_block_nodenull_neg(CuTest *tc) {
	char *line[] = {"(", "block", "foo", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_block_nodeparentnull_neg(CuTest *tc) {
	char *line[] = {"(", "block", "foo", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = NULL;
	test_ast_node->line = 1;

	int rc = cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_destroy_block(CuTest *tc) {
	char *line[] = {"(", "block", "a", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_gen_block(test_db, test_tree->root->cl_head->cl_head, test_ast_node, 0);

	cil_destroy_block((struct cil_block*)test_ast_node->data);
	CuAssertPtrEquals(tc, NULL,test_ast_node->data);
}

void test_cil_gen_blockinherit(CuTest *tc) {
	char *line[] = {"(", "blockinherit", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_blockinherit_namelist_neg(CuTest *tc) {
	char *line[] = {"(", "blockinherit", "(", "foo", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_blockinherit_namenull_neg(CuTest *tc) {
	char *line[] = {"(", "blockinherit", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_blockinherit_extra_neg(CuTest *tc) {
	char *line[] = {"(", "blockinherit", "foo", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_blockinherit_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "blockinherit", "foo", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_blockinherit_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_blockinherit_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "blockinherit", "foo", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_blockinherit(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_perm(CuTest *tc) {
	char *line[] = {"(", "class", "foo", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_class *new_node;
	cil_class_init(&new_node);

	struct cil_tree_node *new_tree_node;
	cil_tree_node_init(&new_tree_node);
	new_tree_node->data = new_node;
	new_tree_node->flavor = CIL_CLASS;

	test_ast_node->parent = new_tree_node;
	test_ast_node->line = 1;

	int rc = cil_gen_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node);
	int rc1 = cil_gen_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head->next, test_ast_node);
	int rc2 = cil_gen_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head->next->next, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, SEPOL_OK, rc1);
	CuAssertIntEquals(tc, SEPOL_OK, rc2);
}

void test_cil_gen_perm_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};

	int rc = 0;
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_current_perm = NULL;
	struct cil_tree_node *test_new_ast = NULL;
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	test_current_perm = test_tree->root->cl_head->cl_head->next->next->cl_head;

	cil_tree_node_init(&test_new_ast);
	test_new_ast->parent = test_ast_node;
	test_new_ast->line = test_current_perm->line;

	rc = cil_gen_perm(test_db, test_current_perm, test_new_ast);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_perm_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};

	int rc = 0;
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_current_perm = NULL;
	struct cil_tree_node *test_new_ast = NULL;
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_current_perm = NULL; 

	cil_tree_node_init(&test_new_ast);
	test_new_ast->parent = test_ast_node;

	rc = cil_gen_perm(test_db, test_current_perm, test_new_ast);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_perm_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "foo", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_class *new_node;
	cil_class_init(&new_node);

	struct cil_tree_node *new_tree_node;
	cil_tree_node_init(&new_tree_node);
	new_tree_node->data = new_node;
	new_tree_node->flavor = CIL_CLASS;

	int rc = cil_gen_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_perm_nodenull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};

	int rc = 0;
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_current_perm = NULL;
	struct cil_tree_node *test_new_ast = NULL;
	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_current_perm = test_tree->root->cl_head->cl_head->next->next->cl_head;

	cil_tree_node_init(&test_new_ast);
	test_new_ast->parent = test_ast_node;
	test_new_ast->line = test_current_perm->line;

	rc = cil_gen_perm(test_db, test_current_perm, test_new_ast);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_permset_noname_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "(", "foo", ")", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_noperms_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_emptyperms_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_extra_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_permset_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_permset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_perm_nodes(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	char *test_key = test_tree->root->cl_head->cl_head->next->data;
	struct cil_class *test_cls;
	cil_class_init(&test_cls);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_symtab_insert(&test_db->symtab[CIL_SYM_CLASSES], (hashtab_key_t)test_key, (struct cil_symtab_datum*)test_cls, test_ast_node);

	test_ast_node->data = test_cls;
	test_ast_node->flavor = CIL_CLASS;

	int rc = cil_gen_perm_nodes(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node, CIL_PERM);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_perm_nodes_failgen_neg(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	char *test_key = test_tree->root->cl_head->cl_head->next->data;
	struct cil_class *test_cls;
	cil_class_init(&test_cls);

	cil_symtab_destroy(&test_cls->perms);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_symtab_insert(&test_db->symtab[CIL_SYM_CLASSES], (hashtab_key_t)test_key, (struct cil_symtab_datum*)test_cls, test_ast_node);

	test_ast_node->data = test_cls;
	test_ast_node->flavor = CIL_CLASS;

	int rc = cil_gen_perm_nodes(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node, CIL_PERM);
	CuAssertIntEquals(tc, SEPOL_ENOMEM, rc);
}

void test_cil_gen_perm_nodes_inval_perm_neg(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "(", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	char *test_key = test_tree->root->cl_head->cl_head->next->data;
	struct cil_class *test_cls;
	cil_class_init(&test_cls);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_symtab_insert(&test_db->symtab[CIL_SYM_CLASSES], (hashtab_key_t)test_key, (struct cil_symtab_datum*)test_cls, test_ast_node);

	test_ast_node->data = test_cls;
	test_ast_node->flavor = CIL_CLASS;

	int rc = cil_gen_perm_nodes(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node, CIL_PERM);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_permset(CuTest *tc) { 
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_permset *permset;
	cil_permset_init(&permset);

	int rc = cil_fill_permset(test_tree->root->cl_head->cl_head->next->next->cl_head, permset);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_permset_sublist_neg(CuTest *tc) { 
	char *line[] = {"(", "permissionset", "foo", "(", "read", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_permset *permset;
	cil_permset_init(&permset);

	int rc = cil_fill_permset(test_tree->root->cl_head->cl_head->next->next->cl_head, permset);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_permset_startpermnull_neg(CuTest *tc) { 
	char *line[] = {"(", "permissionset", "foo", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_permset *permset;
	cil_permset_init(&permset);

	int rc = cil_fill_permset(test_tree->root->cl_head->cl_head->next->next->cl_head, permset);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_permset_permsetnull_neg(CuTest *tc) { 
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_permset *permset = NULL;

	int rc = cil_fill_permset(test_tree->root->cl_head->cl_head->next->next->cl_head, permset);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_in(CuTest *tc) { 
	char *line[] = {"(", "in", "foo", "(", "allow", "test", "baz", "(", "char", "(", "read", ")", ")", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_in(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_in_blockstrnull_neg(CuTest *tc) { 
	char *line[] = {"(", "in", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_in(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_in_extra_neg(CuTest *tc) { 
	char *line[] = {"(", "in", "foo", "(", "allow", "test", "baz", "(", "char", "(", "read", ")", ")", ")", "extra", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_in(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_in_dbnull_neg(CuTest *tc) { 
	char *line[] = {"(", "in", "foo", "(", "allow", "test", "baz", "(", "char", "(", "read", ")", ")", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_in(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_in_currnull_neg(CuTest *tc) { 
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_in(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_in_astnull_neg(CuTest *tc) { 
	char *line[] = {"(", "in", "foo", "(", "allow", "test", "baz", "(", "char", "(", "read", ")", ")", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_in(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class(CuTest *tc) { 
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->cl_tail);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_CLASS);
}

void test_cil_gen_class_noname_neg(CuTest *tc) { 
	char *line[] = {"(", "class", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_nodenull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "class", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_tree->root->cl_head->cl_head = NULL;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_noclass_neg(CuTest *tc) { 
	char *line[] = {"(", "test", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_noclassname_neg(CuTest *tc) { 
	char *line[] = {"(", "class", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_namesublist_neg(CuTest *tc) { 
	char *line[] = {"(", "class", "(", "foo", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_noperms(CuTest *tc) { 
	char *line[] = {"(", "class", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_class_permsnotinlist_neg(CuTest *tc) { 
	char *line[] = {"(", "class", "foo", "read", "write", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_extrapermlist_neg(CuTest *tc) { 
	char *line[] = {"(", "class", "foo", "(", "read", ")", "(", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_class_listinlist_neg(CuTest *tc) { 
        char *line[] = {"(", "class", "test", "(", "read", "(", "write", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_class(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_anonperms(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_classpermset_anonperms_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "(", "write", "(", "extra", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_namedperms(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "perms", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_classpermset_extra_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "(", "write", ")", "extra", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_emptypermslist_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "(", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_noperms_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_noclass_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_classnodenull_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps;
	cil_classpermset_init(&cps);

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_classpermset_cpsnull_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_classpermset *cps = NULL;

	int rc = cil_fill_classpermset(test_tree->root->cl_head->cl_head->next->next->cl_head, cps);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "char_w", "(", "char", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_classpermset_noname_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "(", "foo", ")", "(", "read", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_noclass_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_noperms_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", "(", "char", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_emptyperms_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_extra_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", "(", "read", "(", "write", ")", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", "(", "read", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classpermset_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", "(", "read", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_classpermset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_perm(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_classmap *map = NULL;
	cil_classmap_init(&map);	

	test_ast_node->flavor = CIL_CLASSMAP;
	test_ast_node->data = map;
	
	struct cil_tree_node *test_ast_node_a;
	cil_tree_node_init(&test_ast_node_a);

	test_ast_node_a->parent = test_ast_node;
	test_ast_node_a->line = test_tree->root->cl_head->cl_head->next->next->cl_head->line;
	test_ast_node_a->path = test_tree->root->cl_head->cl_head->next->next->cl_head->path;
	
	int rc = cil_gen_classmap_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node_a);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_classmap_perm_dupeperm_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	struct cil_tree_node *test_ast_node_a;
	cil_tree_node_init(&test_ast_node_a);

	test_ast_node_a->parent = test_ast_node;
	test_ast_node_a->line = test_tree->root->cl_head->cl_head->next->next->cl_head->line;
	test_ast_node_a->path = test_tree->root->cl_head->cl_head->next->next->cl_head->path; 
	
	int rc = cil_gen_classmap_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node_a);
	CuAssertIntEquals(tc, SEPOL_EEXIST, rc);
}

void test_cil_gen_classmap_perm_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	struct cil_tree_node *test_ast_node_a;
	cil_tree_node_init(&test_ast_node_a);

	test_ast_node_a->parent = test_ast_node;
	test_ast_node_a->line = test_tree->root->cl_head->cl_head->line;
	test_ast_node_a->path = test_tree->root->cl_head->cl_head->path;

	test_db = NULL;

	int rc = cil_gen_classmap_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node_a);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_perm_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	struct cil_tree_node *test_ast_node_a;
	cil_tree_node_init(&test_ast_node_a);

	test_ast_node_a->parent = test_ast_node;
	test_ast_node_a->line = test_tree->root->cl_head->cl_head->line;
	test_ast_node_a->path = test_tree->root->cl_head->cl_head->path;

	int rc = cil_gen_classmap_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node_a);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_perm_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	struct cil_tree_node *test_ast_node_a = NULL;

	int rc = cil_gen_classmap_perm(test_db, test_tree->root->cl_head->cl_head->next->next->cl_head, test_ast_node_a);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_classmap_extra_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_noname_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_emptyperms_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmap_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_classmap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_anonpermset(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", 
			"(", "file", "(", "open", "read", "getattr", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_classmapping_anonpermset_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", 
			"(", "file", "(", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_namedpermset(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", "char_w", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_classmapping_noclassmapname_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_noclassmapperm_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_nopermissionsets_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", 
			"(", "file", "(", "open", "read", "getattr", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classmapping_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", 
			"(", "file", "(", "open", "read", "getattr", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_classmapping(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common(CuTest *tc) {
	char *line[] = {"(", "common", "test", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_COMMON);
}

void test_cil_gen_common_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "common", "test", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "common", "test", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common_noname_neg(CuTest *tc) {
	char *line[] = {"(", "common", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common_twoperms_neg(CuTest *tc) {
	char *line[] = {"(", "common", "foo", "(", "write", ")", "(", "read", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common_permsublist_neg(CuTest *tc) {
        char *line[] = {"(", "common", "test", "(", "read", "(", "write", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_common_noperms_neg(CuTest *tc) {
        char *line[] = {"(", "common", "test", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_common(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
       
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sid(CuTest *tc) {
	char *line[] = {"(", "sid", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_sid_noname_neg(CuTest *tc) {
	char *line[] = {"(", "sid", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sid_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "sid", "(", "foo", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sid_extra_neg(CuTest *tc) {
	char *line[] = {"(", "sid", "foo", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sid_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sid", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sid_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sid_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "sid", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_sid(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "(", "blah", "blah", "blah", "(", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);


	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_SIDCONTEXT);
}

void test_cil_gen_sidcontext_namedcontext(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "something", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_SIDCONTEXT);
}

void test_cil_gen_sidcontext_halfcontext_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "(", "blah", "blah", "blah", "(", "(", "s0", "(", "c0", ")", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_noname_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "(", "blah", "blah", "blah", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_empty_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_dblname_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "test2", "(", "blah", "blah", "blah", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "(", "blah", "blah", "blah", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_pcurrnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sidcontext_astnodenull_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "(", "blah", "blah", "blah", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sidcontext(test_db, test_tree->root->cl_head->cl_head, NULL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type(CuTest *tc) {
	char *line[] = {"(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TYPE);
}

void test_cil_gen_type_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_type(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_type(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_type(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_extra_neg(CuTest *tc) {
	char *line[] = {"(", "type", "foo", "bar," ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattribute(CuTest *tc) {
	char *line[] = {"(", "typeattribute", "test", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattribute(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TYPEATTRIBUTE);
}

void test_cil_gen_typeattribute_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "typeattribute", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_typeattribute(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}


void test_cil_gen_typeattribute_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_typeattribute(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}


void test_cil_gen_typeattribute_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typeattribute", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_typeattribute(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattribute_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typeattribute", "foo", "bar," ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattribute(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds(CuTest *tc) {
	char *line[] = {"(", "typebounds", "type_a", "type_b", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_typebounds_notype1_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_type1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "(", "type_a", ")", "type_b", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_notype2_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "type_a", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_type2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "type_a", "(", "type_b", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "type_a", "type_b", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "type_a", "type_b", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typebounds_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "type_a", "type_b", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_typebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typepermissive(CuTest *tc) {
	char *line[] = {"(", "typepermissive", "type_a", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_typepermissive_noname_neg(CuTest *tc) {
	char *line[] = {"(", "typepermissive", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typepermissive_typeinparens_neg(CuTest *tc) {
	char *line[] = {"(", "typepermissive", "(", "type_a", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typepermissive_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typepermissive", "type_a", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typepermissive_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "typepermissive", "type_a", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;
	
	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typepermissive_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typepermissive_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typepermissive", "type_a", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_typepermissive(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_nametypetransition_strinparens_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "(", "str", ")", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_nostr_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_srcinparens_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "(", "foo", ")", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_nosrc_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_tgtinparens_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "(", "bar", ")", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_notgt_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_classinparens_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "(", "file", ")", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_noclass_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_destinparens_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", "(", "foobar", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_nodest_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}


void test_cil_gen_nametypetransition_extra_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", "foobar", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", "foobar", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nametypetransition_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_nametypetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_rangetransition_namedtransition(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "namedtrans", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_rangetransition_anon_low_l(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "(", "s0", "(", "c0", ")", ")", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_rangetransition_anon_low_l_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "(", "s0", "(", ")", ")", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_anon_high_l(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", "(", "s0", "(", "c0", ")",  ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_rangetransition_anon_high_l_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", "(", "s0", "(", ")",  ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_nofirsttype_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_firsttype_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "(", "type_a_t", ")", "type_b_t", "class", "(", "low_l", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_nosecondtype_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_secondtype_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "(", "type_b_t", ")", "class", "(", "low_l", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_noclass_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_class_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "(", "class", ")", "(", "low_l", "high_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_nolevel_l_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_nolevel_h_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rangetransition_extra_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a_t", "type_b_t", "class", "(", "low_l", "high_l", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rangetransition(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_and(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_or(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "or", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_xor(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "xor", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_not(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "not", "foo", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_not_noexpr_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "not", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_not_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "not", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_eq(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "eq", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_neq(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "neq", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_nested(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "or", "(","neq", "foo", "bar", ")", "(", "eq", "baz", "boo", ")", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_expr_stack_nested_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "(","neq", "foo", "bar", ")", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_nested_emptyargs_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "eq", "(", ")", "(", ")", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_nested_missingoperator_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "or", "(","foo", "bar", ")", "(", "eq", "baz", "boo", ")", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_arg1null_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "eq", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_arg2null_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "eq", "foo", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_extraarg_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "eq", "foo", "bar", "extra", ")",
			"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_booleanif *bif;
	cil_boolif_init(&bif);

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, &bif->expr_stack);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_expr_stack_stacknull_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "xor", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_expr_stack(test_tree->root->cl_head->cl_head->next, CIL_BOOL, NULL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_multiplebools_true(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "write", ")", ")", ")", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_boolif_multiplebools_false(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")",
			"(", "false", "(", "allow", "foo", "bar", "(", "write", ")", ")", ")", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_boolif_multiplebools_unknowncond_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")",
			"(", "dne", "(", "allow", "foo", "bar", "(", "write", ")", ")", ")", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_true(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_boolif_false(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "false", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_boolif_unknowncond_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "dne", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_nested(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "(", "or", "foo", "bar", ")", "baz", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_boolif_nested_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "(", "or", "foo", "bar", ")", "baz", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_extra_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "(", "or", "foo", "bar", ")", "baz", "beef", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_extra_parens_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "(", "or", "foo", "bar", ")", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_nocond(CuTest *tc) {
	char *line[] = {"(", "booleanif", "baz",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_boolif_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "**", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_nocond_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_notruelist_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_boolif_empty_cond_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_boolif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_multiplebools_true(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", 
			"(", "true", "(", "allow", "foo", "bar", "(", "write", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_tunif_multiplebools_false(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")",
			"(", "false", "(", "allow", "foo", "bar", "(", "write", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_tunif_multiplebools_unknowncond_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")",
			"(", "dne", "(", "allow", "foo", "bar", "(", "write", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_true(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_tunif_false(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "false", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_tunif_unknowncond_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "dne", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_nocond(CuTest *tc) {
	char *line[] = {"(", "tunableif", "baz",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_tunif_nested(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "(", "or", "foo", "bar", ")", "baz", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_tunif_nested_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "(", "or", "foo", "bar", ")", "baz", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_extra_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "(", "or", "foo", "bar", ")", "baz", "beef", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_extra_parens_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "(", "or", "foo", "bar", ")", ")",
			"(", "true", "(", "allow", "foo", "baz", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "**", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true", "(", "allow", "foo", "bar", "(", "read", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_nocond_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_tunif_notruelist_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_tunif(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_condblock_true(CuTest *tc) {
	char *line[] = {"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDTRUE);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_condblock_false(CuTest *tc) {
	char *line[] = {"(", "false", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDFALSE);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_condblock_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "false", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDFALSE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_condblock_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDFALSE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_condblock_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "false", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDFALSE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_condblock_nocond_neg(CuTest *tc) {
	char *line[] = {"(", "true", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDTRUE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_condblock_extra_neg(CuTest *tc) {
	char *line[] = {"(", "true", "(", "allow", "foo", "bar", "baz", "(", "read", ")", ")", "Extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_condblock(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_CONDTRUE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typealias(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", "type_t", ")", "(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertStrEquals(tc, ((struct cil_typealias*)test_ast_node->data)->type_str, test_tree->root->cl_head->cl_head->next->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TYPEALIAS);
}

void test_cil_gen_typealias_incomplete_neg(CuTest *tc) {
	char *line[] = {"(", "typealias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typealias_incomplete_neg2(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typealias_extratype_neg(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", "foo", "extra_t", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typealias_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", "type_t", ")", "(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typealias_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typealias_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", "type_t", ")", "(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_typealias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "test_t", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
}

void test_cil_gen_typeattributeset_and_two_types(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "and", "test_t", "test2_t", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
}

void test_cil_gen_typeattributeset_not(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "not", "notypes_t", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
}

void test_cil_gen_typeattributeset_exclude_attr(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "not", "attr", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
}

void test_cil_gen_typeattributeset_exclude_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "not", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "not", "type_t", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "not", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_noname_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "(", "filetypes", ")", "(", "test_t", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_emptylists_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_listinparens_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "(", "test_t", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_typeattributeset_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "test_t", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_typeattributeset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userbounds(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", "user2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_OK);
}

void test_cil_gen_userbounds_notype1_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_type1_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "(", "user1", ")", "user2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_notype2_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_type2_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", "(", "user2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_extra_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", "user2", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", "user2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_userbounds_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", "user2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_userbounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_role(CuTest *tc) {
	char *line[] = {"(", "role", "test_r", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_role(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_ROLE);
}

void test_cil_gen_role_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "role", "test_r", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_role(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_role_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_role(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_role_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "role", "test_r", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_role(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_role_extrarole_neg(CuTest *tc) {
	char *line[] = {"(", "role", "test_r", "extra_r", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_role(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_role_noname_neg(CuTest *tc) {
	char *line[] = {"(", "role", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_role(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletransition(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r",  "bar_t", "process",  "foobar_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_roletransition(test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_ROLETRANSITION);
}

void test_cil_gen_roletransition_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletransition(NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc); 	
}

void test_cil_gen_roletransition_astnull_neg (CuTest *tc) {
	char *line[] = {"(", "roletransition" "foo_r", "bar_t", "process", "foobar_r", ")", NULL};

	struct  cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_roletransition(test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletransition_srcnull_neg(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r", "bar_t", "process", "foobar_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletransition(test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletransition_tgtnull_neg(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r", "bar_t", "process", "foobar_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next->next = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletransition(test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletransition_resultnull_neg(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r", "bar_t", "process", "foobar_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next->next->next->next = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletransition(test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletransition_extra_neg(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r", "bar_t", "process", "foobar_r", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletransition(test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_true(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "true", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, ((struct cil_bool*)test_ast_node->data)->value, 1);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_BOOL);
}

void test_cil_gen_bool_tunable_true(CuTest *tc) {
	char *line[] = {"(", "tunable", "foo", "true", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_TUNABLE);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, ((struct cil_bool*)test_ast_node->data)->value, 1);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TUNABLE);
}

void test_cil_gen_bool_false(CuTest *tc) {
	char *line[] = {"(", "boolean", "bar", "false", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, ((struct cil_bool*)test_ast_node->data)->value, 0);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_BOOL);
}

void test_cil_gen_bool_tunable_false(CuTest *tc) {
	char *line[] = {"(", "tunable", "bar", "false", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_TUNABLE);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, ((struct cil_bool*)test_ast_node->data)->value, 0);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TUNABLE);
}

void test_cil_gen_bool_none_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_notbool_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_boolname_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_extraname_false_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "false", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_bool_extraname_true_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "true", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_bool(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_BOOL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_t1type(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t1", "type_t", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);

	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_t1t1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t1", "t1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_t2type(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t2", "type_t", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_t2t2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t2", "t2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_r1role(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r1", "role_r", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_r1r1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r1", "r1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_r2role(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r2", "role_r", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_r2r2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r2", "r2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_t1t2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t1", "t2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_r1r2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r1", "r2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_r1r2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r1", "r2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_u1u2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "u1", "u2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_u1user(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "u1", "user_u", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_u1u1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "u1", "u1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_u2user(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "u2", "user_u", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_u2u2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "u2", "u2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l2h2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l2", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l2", "h1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l1l2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "l2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l1h1(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "h1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l1h2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_h1l2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "h1", "l2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_h1h2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "h1", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq_h1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "h1", "l1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l1l1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "l1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_l1l2_constrain_neg(CuTest *tc) {
	char *line[] = {"(", "constrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "l2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_CONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_l1l2_constrain_neg(CuTest *tc) {
	char *line[] = {"(", "constrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "l2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_CONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_leftkeyword_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "h2", "h1", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_noexpr1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_expr1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "(", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_noexpr2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_expr2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "(", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "foo", "foo", "extra", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l2", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_noexpr1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_expr1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "(", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_noexpr2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_expr2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l1", "(", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_eq2_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "foo", "foo", "extra", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_noteq(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "neq", "l2", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_noteq_noexpr1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "neq", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_noteq_expr1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "neq", "(", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_noteq_noexpr2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "neq", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_noteq_expr2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "neq", "l1", "(", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_noteq_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "neq", "foo", "foo", "extra", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_not(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "not", "(", "neq", "l2", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);

	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_not_noexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "not", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_not_emptyparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "not", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_not_extraparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "not", "(", "neq", "l2", "h2", ")", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_or(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", 
			"(", "neq", "l1", "l2", ")", "(", "neq", "l1", "h1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_or_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", 
			"(", "foo", ")", "(", "neq", "l1", "h1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_or_noexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_or_emptyfirstparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_or_missingsecondexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", "(", "foo", ")", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_or_emptysecondparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", "(", "foo", ")", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_or_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "or", "(", "foo", ")", "(", "foo", ")", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_and(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", 
			"(", "neq", "l1", "l2", ")", "(", "neq", "l1", "h1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_and_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", 
			"(", "foo", ")", "(", "neq", "l1", "h1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_and_noexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_and_emptyfirstparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_and_missingsecondexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", "(", "foo", ")", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_and_emptysecondparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", "(", "foo", ")", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_and_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "and", "(", "foo", ")", "(", "foo", ")", "(", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_dom(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dom", "l2", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_dom_noexpr1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dom", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_dom_expr1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dom", "(", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_dom_noexpr2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dom", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_dom_expr2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dom", "l1", "(", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_dom_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dom", "foo", "foo", "extra", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_domby(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "domby", "l2", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_domby_noexpr1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "domby", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_domby_expr1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "domby", "(", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_domby_noexpr2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "domby", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_domby_expr2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "domby", "l1", "(", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_domby_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "domby", "foo", "foo", "extra", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_incomp(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "incomp", "l2", "h2", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_expr_stack_incomp_noexpr1_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "incomp", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_incomp_expr1inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "incomp", "(", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_incomp_noexpr2_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "incomp", "l1", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_incomp_expr2inparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "incomp", "l1", "(", "h2", ")", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_incomp_extraexpr_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "incomp", "foo", "foo", "extra", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_stacknull_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t1", "type_t", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, NULL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_operatorinparens_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "(", "eq", ")", "t1", "type_t", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expr_stack_incorrectcall_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "t1", "type_t", ")", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *parse_current = test_tree->root->cl_head->cl_head;

	struct cil_constrain *cons;
	cil_constrain_init(&cons);
	cil_classpermset_init(&cons->classpermset);
	cil_fill_classpermset(parse_current->next->cl_head, cons->classpermset);
	
	int rc = cil_gen_expr_stack(parse_current->next->next->cl_head->next->next, CIL_MLSCONSTRAIN, &cons->expr);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roleallow(CuTest *tc) {
	char *line[] = {"(", "roleallow", "staff_r", "sysadm_r", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_roleallow(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertStrEquals(tc, ((struct cil_roleallow*)test_ast_node->data)->src_str, test_current->next->data);
	CuAssertStrEquals(tc, ((struct cil_roleallow*)test_ast_node->data)->tgt_str, test_current->next->next->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_ROLEALLOW);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_roleallow_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "roleallow", "foo", "bar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_roleallow(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roleallow_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_roleallow(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc); 
}

void test_cil_gen_roleallow_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "roleallow", "foo", "bar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_roleallow(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roleallow_srcnull_neg(CuTest *tc) {
	char *line[] = {"(", "roleallow", "foo", "bar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roleallow(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roleallow_tgtnull_neg(CuTest *tc) {
	char *line[] = {"(", "roleallow", "foo", "bar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roleallow(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roleallow_extra_neg(CuTest *tc) {
	char *line[] = {"(", "roleallow", "foo", "bar", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roleallow(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_rolebounds(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", "role2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_OK);
}

void test_cil_gen_rolebounds_norole1_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_role1_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "(", "role1", ")", "role2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_norole2_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_role2_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", "(", "role2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_extra_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", "role2", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", "role2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_rolebounds_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", "role2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_rolebounds(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, rc, SEPOL_ERR);
}

void test_cil_gen_avrule(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertStrEquals(tc, ((struct cil_avrule*)test_ast_node->data)->src_str, test_current->next->data);
	CuAssertStrEquals(tc, ((struct cil_avrule*)test_ast_node->data)->tgt_str, test_current->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_avrule*)test_ast_node->data)->classpermset->class_str, test_current->next->next->next->cl_head->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_AVRULE);
	CuAssertPtrNotNull(tc, ((struct cil_avrule*)test_ast_node->data)->classpermset->permset->perms_list_str);

	struct cil_list_item *test_list = ((struct cil_avrule*)test_ast_node->data)->classpermset->permset->perms_list_str->head;
	test_current = test_current->next->next->next->cl_head->next->cl_head;

	while(test_list != NULL) {
	    CuAssertIntEquals(tc, test_list->flavor, CIL_AST_STR);
	    CuAssertStrEquals(tc, test_list->data, test_current->data );
	    test_list = test_list->next;
	    test_current = test_current->next;
	}
}

void test_cil_gen_avrule_permset(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "permset", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_avrule_permset_anon(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_avrule_extra_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "permset", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_sourceparens(CuTest *tc) {
	char *line[] = {"(", "allow", "(", "test", ")", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_sourceemptyparen_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "(", ")", "bar", "file", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_targetparens(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "(", "foo", ")", "bar", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_targetemptyparen_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "bar", "(", ")", "file", "(", "read", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "bar", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_sourcedomainnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_targetdomainnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_objectclassnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "foo", "bar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_permsnull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "foo", "bar", "(", "baz", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_avrule_twolists_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "bar", "(", "write", ")", "(", "read", ")",  NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	struct cil_tree_node *test_current;
	test_current = test_tree->root->cl_head->cl_head;

	int rc = cil_gen_avrule(test_current, test_ast_node, CIL_AVRULE_ALLOWED);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_transition(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->src_str, test_tree->root->cl_head->cl_head->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->tgt_str, test_tree->root->cl_head->cl_head->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->obj_str, test_tree->root->cl_head->cl_head->next->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->result_str, test_tree->root->cl_head->cl_head->next->next->next->next->data);
	CuAssertIntEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->rule_kind, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TYPE_RULE);
}

void test_cil_gen_type_rule_transition_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_type_rule(NULL, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc); 
}

void test_cil_gen_type_rule_transition_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}	

void test_cil_gen_type_rule_transition_srcnull_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next = NULL;
	
	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_transition_tgtnull_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_transition_objnull_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_transition_resultnull_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_transition_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_TRANSITION);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_change(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->src_str, test_tree->root->cl_head->cl_head->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->tgt_str, test_tree->root->cl_head->cl_head->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->obj_str, test_tree->root->cl_head->cl_head->next->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->result_str, test_tree->root->cl_head->cl_head->next->next->next->next->data);
	CuAssertIntEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->rule_kind, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TYPE_RULE);
}

void test_cil_gen_type_rule_change_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_type_rule(NULL, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc); 
}

void test_cil_gen_type_rule_change_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}	

void test_cil_gen_type_rule_change_srcnull_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next = NULL;
	
	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_change_tgtnull_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_change_objnull_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_change_resultnull_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_change_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_CHANGE);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_member(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->src_str, test_tree->root->cl_head->cl_head->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->tgt_str, test_tree->root->cl_head->cl_head->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->obj_str, test_tree->root->cl_head->cl_head->next->next->next->data);
	CuAssertStrEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->result_str, test_tree->root->cl_head->cl_head->next->next->next->next->data);
	CuAssertIntEquals(tc, ((struct cil_type_rule*)test_ast_node->data)->rule_kind, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_TYPE_RULE);
}

void test_cil_gen_type_rule_member_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_type_rule(NULL, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc); 
}

void test_cil_gen_type_rule_member_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}	

void test_cil_gen_type_rule_member_srcnull_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next = NULL;
	
	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_member_tgtnull_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_member_objnull_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_member_resultnull_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	test_tree->root->cl_head->cl_head->next->next->next->next = NULL;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_type_rule_member_extra_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_type_rule(test_tree->root->cl_head->cl_head, test_ast_node, CIL_TYPE_MEMBER);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_user(CuTest *tc) {
	char *line[] = {"(", "user", "sysadm", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_user(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, CIL_USER, test_ast_node->flavor);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertPtrEquals(tc, test_ast_node, ((struct cil_symtab_datum*)test_ast_node->data)->node);
	CuAssertStrEquals(tc, test_tree->root->cl_head->cl_head->next->data, ((struct cil_symtab_datum*)test_ast_node->data)->name);
}

void test_cil_gen_user_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "user", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_user(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_user_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_user(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_user_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "user", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_user(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_user_nouser_neg(CuTest *tc) {
	char *line[] = {"(", "user", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_user(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_user_xsinfo_neg(CuTest *tc) {
	char *line[] = {"(", "user", "sysadm", "xsinfo", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_user(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user_u", "lvl_l", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_userlevel_anon_level(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user_u", "(", "s0", "(", "c0", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_userlevel_anon_level_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user_u", "(", "s0", "(", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_usernull_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_userrange_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "(", "user", ")", "level", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_levelnull_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user_u", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_levelrangeempty_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user_u", "(", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_extra_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user_u", "level", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user", "level", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userlevel_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "user", "level", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_userlevel(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_named(CuTest *tc) {
	char *line[] = {"(", "userrange", "user_u", "range", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_userrange_anon(CuTest *tc) {
	char *line[] = {"(", "userrange", "user_u", "(", "low", "high", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_userrange_usernull_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_anonuser_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "(", "user_u", ")", "(", "low", "high", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_rangenamenull_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "user_u", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_anonrangeinvalid_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "user_u", "(", "low", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_anonrangeempty_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "user_u", "(", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_extra_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "user_u", "(", "low", "high", ")", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "user", "range", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrange_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "user", "range", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_userrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensitivity(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sensitivity(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_ast_node->data);
	CuAssertIntEquals(tc, test_ast_node->flavor, CIL_SENS);

}

void test_cil_gen_sensitivity_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db = NULL;

	int rc = cil_gen_sensitivity(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensitivity_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_sensitivity(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensitivity_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_sensitivity(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensitivity_sensnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensitivity(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensitivity_senslist_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "(", "s0", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensitivity(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensitivity_extra_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", "extra", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensitivity(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}	

void test_cil_gen_sensalias(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_sensalias_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_currnull_neg(CuTest *tc) {
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_sensalias(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init (&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_sensnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_senslist_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "(", "s0", "s1", ")", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_aliasnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_aliaslist_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "(", "alias", "alias2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_sensalias_extra_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_sensalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_category(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_category(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_category_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_category(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_category_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_category(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_category_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_category(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_category_catnull_neg(CuTest *tc){
	char *line[] = {"(", "category", "c0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_category(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_category_catlist_neg(CuTest *tc){
	char *line[] = {"(", "category", "(", "c0", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_category(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_category_extra_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_category(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_catset_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;
	
	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_namenull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_setnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_namelist_neg(CuTest *tc) { //This should fail before gen_node call - additional syntax checks are needed
	char *line[] = {"(", "categoryset", "(", "somecats", ")", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_extra_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", "extra",  ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_notset_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "blah", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

// TODO: This doesn't actually test failure of gen_node 
void test_cil_gen_catset_nodefail_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", "(", "c3", "c4", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catset_settolistfail_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catset(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catalias(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_catalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_catalias_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_catalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catalias_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catalias(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catalias_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_catalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catalias_catnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catalias_aliasnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root->cl_head->cl_head->next->next = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catalias_extra_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catalias(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", "c0", "c1", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_catrange_noname_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_norange_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_emptyrange_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_extrarange_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", "c0", "c1", "c2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;
	
	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", "c0", "c1", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catrange(test_db, NULL, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", "c0", "c1", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catrange_extra_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", "c0", "c1", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletype(CuTest *tc) {
	char *line[] = {"(", "roletype", "admin_r", "admin_t", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_roletype_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletype_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "roletype", "admin_r", "admin_t", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletype_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "roletype", "admin_r", "admin_t", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}


void test_cil_gen_roletype_empty_neg(CuTest *tc) {
	char *line[] = {"(", "roletype", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletype_rolelist_neg(CuTest *tc) {
	char *line[] = {"(", "roletype", "(", "admin_r", ")", "admin_t", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

// TODO
// Not sure this is actually testing roletype
// I think this will just test that type is null
void test_cil_gen_roletype_roletype_sublist_neg(CuTest *tc) {
	char *line[] = {"(", "(", "roletype", "admin_r", ")", "admin_t", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_roletype_typelist_neg(CuTest *tc) {
	char *line[] = {"(", "roletype", "admin_r", "(", "admin_t", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_roletype(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrole(CuTest *tc) {
	char *line[] = {"(", "userrole", "staff_u", "staff_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_userrole_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrole_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "userrole", "staff_u", "staff_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrole_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "userrole", "staff_u", "staff_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrole_empty_neg(CuTest *tc) {
	char *line[] = {"(", "userrole", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrole_userlist_neg(CuTest *tc) {
	char *line[] = {"(", "userrole", "(", "staff_u", ")", "staff_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}


//TODO: see above
void test_cil_gen_userrole_userrole_sublist_neg(CuTest *tc) {
	char *line[] = {"(", "(", "userrole", "staff_u", ")", "staff_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_userrole_rolelist_neg(CuTest *tc) {
	char *line[] = {"(", "userrole", "staff_u", "(", "staff_r", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);
	
	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_userrole(test_db, test_tree->root->cl_head->cl_head, test_ast_node);

	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classcommon(CuTest *tc) {
	char *line[] = {"(", "classcommon", "file", "file", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        char *test_key = test_tree->root->cl_head->cl_head->next->data;
        struct cil_class *test_cls;
		cil_class_init(&test_cls);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        cil_symtab_insert(&test_db->symtab[CIL_SYM_CLASSES], (hashtab_key_t)test_key, (struct cil_symtab_datum*)test_cls, test_ast_node);

        test_ast_node->data = test_cls;
        test_ast_node->flavor = CIL_CLASS;

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_classcommon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "classcommon", "file", "file", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classcommon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classcommon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "classcommon", "file", "file", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classcommon_missingclassname_neg(CuTest *tc) {
	char *line[] = {"(", "classcommon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classcommon_noperms_neg(CuTest *tc) {
	char *line[] = {"(", "classcommon", "file", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        char *test_key = test_tree->root->cl_head->cl_head->next->data;
        struct cil_class *test_cls;
		cil_class_init(&test_cls);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        cil_symtab_insert(&test_db->symtab[CIL_SYM_CLASSES], (hashtab_key_t)test_key, (struct cil_symtab_datum*)test_cls, test_ast_node);

        test_ast_node->data = test_cls;
        test_ast_node->flavor = CIL_CLASS;

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_classcommon_extraperms_neg(CuTest *tc) {
	char *line[] = {"(", "classcommon", "file", "file", "file", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        char *test_key = test_tree->root->cl_head->cl_head->next->data;
        struct cil_class *test_cls;
		cil_class_init(&test_cls);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        cil_symtab_insert(&test_db->symtab[CIL_SYM_CLASSES], (hashtab_key_t)test_key, (struct cil_symtab_datum*)test_cls, test_ast_node);

        test_ast_node->data = test_cls;
        test_ast_node->flavor = CIL_CLASS;

        int rc = cil_gen_classcommon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catorder(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_catorder_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db = NULL;

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catorder_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catorder_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node = NULL;

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catorder_missingcats_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catorder_nosublist_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "c0", "c255", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_catorder_nestedcat_neg(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "(", "c255", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	test_ast_node->parent = test_db->ast->root;
	test_ast_node->line = 1;

	int rc = cil_gen_catorder(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_dominance(CuTest *tc) {
        char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "sensitivity", "s2", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_dominance(test_db, test_tree->root->cl_head->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_dominance_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "sensitivity", "s1", ")",
			"(", "sensitivity", "s2", ")",
			"(", "dominance", "(", "s0", "s1", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_dominance(test_db, test_tree->root->cl_head->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_dominance_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "sensitivity", "s1", ")",
			"(", "sensitivity", "s2", ")",
			"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_dominance(test_db, test_tree->root->cl_head->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_dominance_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "sensitivity", "s1", ")",
			"(", "sensitivity", "s2", ")",
			"(", "dominance", "(", "s0", "s1", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_dominance(test_db, test_tree->root->cl_head->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_dominance_nosensitivities_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "sensitivity", "s1", ")",
			"(", "sensitivity", "s2", ")",
			"(", "dominance", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_dominance(test_db, test_tree->root->cl_head->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_dominance_nosublist_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "sensitivity", "s1", ")",
			"(", "sensitivity", "s2", ")",
			"(", "dominance", "s0", "s2", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_dominance(test_db, test_tree->root->cl_head->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_senscat(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", "sensitivitycategory", "s1", "(", "c0", "c255", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->next->next->next->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_senscat_nosublist(CuTest *tc) {
	char *line[] = {"(", "sensitivitycategory", "s1", "c0", "c255", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_senscat_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", "sensitivitycategory", "s1", "(", "c0", "c255", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->next->next->next->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_senscat_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->next->next->next->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_senscat_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", "sensitivitycategory", "s1", "(", "c0", "c255", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->next->next->next->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_senscat_nosensitivities_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", "sensitivitycategory", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->next->next->next->next->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_senscat_sublist_neg(CuTest *tc) {
      char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
                        "(", "category", "c0", ")",
                        "(", "category", "c255", ")",
                        "(", "categoryorder", "(", "c0", "c255", ")", ")",
                        "(", "sensitivitycategory", "s1", "(", "c0", "(", "c255", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->next->next->next->next->next->next->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_senscat_nocat_neg(CuTest *tc) {
      char *line[] = {"(", "sensitivitycategory", "s1", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_tree_node *test_ast_node;
	cil_tree_node_init(&test_ast_node);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_gen_senscat(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_level(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", "c1", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_level *test_level;
	cil_level_init(&test_level);

        int rc = cil_fill_level(test_tree->root->cl_head->next->next->cl_head->next->next->cl_head, test_level);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_level_sensnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_level *test_level;
	cil_level_init(&test_level);

        int rc = cil_fill_level(test_tree->root->cl_head->next->next->cl_head->next->next, test_level);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_level_levelnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", "c1", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_level *test_level = NULL;

        int rc = cil_fill_level(test_tree->root->cl_head->next->next->cl_head->next->next->cl_head, test_level);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_level_nocat(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_level *test_level;
	cil_level_init(&test_level);

        int rc = cil_fill_level(test_tree->root->cl_head->next->next->cl_head->next->next->cl_head, test_level);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_level_emptycat_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_level *test_level;
	cil_level_init(&test_level);

        int rc = cil_fill_level(test_tree->root->cl_head->next->next->cl_head->next->next->cl_head, test_level);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", "c1", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_level_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "level", "(", "low", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_emptysensparens_neg(CuTest *tc) {
	char *line[] = {"(", "level", "low", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_extra_neg(CuTest *tc) {
	char *line[] = {"(", "level", "low", "(", "s0", "(", "c0", ")", ")", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_emptycat_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_noname_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_nosens_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", "c1", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_currnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_level_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", "c1", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_level(test_db, test_tree->root->cl_head->next->next->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", "low", "high", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_levelrange_rangeinvalid_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", "low", "high", "extra", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_namenull_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_rangenull_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_rangeempty_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_extra_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", "low", "high", ")", "extra", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", "low", "high", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_levelrange_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", "low", "high", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_levelrange(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l2", "h2", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_constrain_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "dne", "l1", "l2", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_classset_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_classset_noclass_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_classset_noperm_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_permset_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_permset_noclass_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_permset_noperm_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "dir", ")", "(", "create", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_expression_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "12", "h2", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_constrain_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "12", "h2", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_constrain(test_db, test_tree->root->cl_head->cl_head, test_ast_node, CIL_MLSCONSTRAIN);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "node_lo_t", "(", "low", "high", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

        int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_context_unnamedlvl(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "node_lo_t", "(", "(", "s0", ")", "(", "s0", ")", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

        int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_context_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "node_lo_t", "(", "low", "high", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context = NULL;

        int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_nouser_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_norole_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_notype_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_nolowlvl_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "type_t", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_nohighlvl_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "type_t", "(", "low", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_unnamedlvl_nocontextlow_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "type_t", "(", "s0", "(", ")", ")", "high", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_context_unnamedlvl_nocontexthigh_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "type_t", "low", "(", "s0", "(", ")", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_context *test_context;
	cil_context_init(&test_context);

	int rc = cil_fill_context(test_tree->root->cl_head->cl_head->next->next->cl_head, test_context);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_context_notinparens_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_extralevel_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", "extra", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_emptycontext_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_extra_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", "(", "extra", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_doubleparen_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "(", "system_u", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_norole_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_roleinparens_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "(", "role_r", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_notype_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "role_r", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_typeinparens_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "role_r", "(", "type_t", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_nolevels_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "role_r", "type_t", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_nosecondlevel_neg(CuTest *tc) {
	char *line[] = {"(", "context", "packet_default", "(", "system_u", "role_r", "type_t", "(", "low", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_noname_neg(CuTest *tc) {
	char *line[] = {"(", "context", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_nouser_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "node_lo_t", "(", "s0", ")", "(", "s0", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_context_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "system_u", "object_r", "node_lo_t", "(", "s0", ")", "(", "s0", ")", ")", ")", NULL};
	
        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_context(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_dir(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "dir", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_file(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_char(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "char", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_block(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "block", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_socket(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "socket", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_pipe(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "pipe", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_symlink(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "symlink", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_any(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "any", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "dne", "context", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_anon_context(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_filecon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL; 

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_str1null_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_str1_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "(", "root", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_str2null_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_str2_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "(", "path", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_classnull_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_class_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "(", "file", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_contextnull_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_context_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "(", "system_u", "object_r", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_filecon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "context", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_filecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_udp(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "80", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_portcon_tcp(CuTest *tc) {
	char *line[] = {"(", "portcon", "tcp", "80", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_portcon_unknownprotocol_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "unknown", "80", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_anon_context(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "80", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_portcon_portrange(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "25", "75", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_portcon_portrange_one_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "0", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_portrange_morethanone_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "0", "1", "2", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_singleport_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "foo", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_lowport_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "foo", "90", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_highport_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "80", "foo", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "0", "1", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "(", "0", "1", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_str1null_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_str1parens_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "(", "80", ")", "port", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_portnull_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_contextnull_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "port", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_context_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "80", "(", "system_u", "object_r", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_portcon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "80", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_portcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_ipaddr(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "192.168.1.1", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_nodecon *nodecon;
	cil_nodecon_init(&nodecon);
	cil_ipaddr_init(&nodecon->addr);

        int rc = cil_fill_ipaddr(test_tree->root->cl_head->cl_head->next->cl_head, nodecon->addr);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_fill_ipaddr_addrnodenull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "192.168.1.1", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_nodecon *nodecon;
	cil_nodecon_init(&nodecon);
	cil_ipaddr_init(&nodecon->addr);

        int rc = cil_fill_ipaddr(NULL, nodecon->addr);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_ipaddr_addrnull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "192.168.1.1", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_nodecon *nodecon;
	cil_nodecon_init(&nodecon);
	nodecon->addr = NULL;

        int rc = cil_fill_ipaddr(test_tree->root->cl_head->cl_head->next->cl_head, nodecon->addr);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_ipaddr_addrinparens_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "(", "192.168.1.1", ")", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_nodecon *nodecon;
	cil_nodecon_init(&nodecon);
	cil_ipaddr_init(&nodecon->addr);

        int rc = cil_fill_ipaddr(test_tree->root->cl_head->cl_head->next->cl_head, nodecon->addr);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_fill_ipaddr_extra_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "192.168.1.1", "extra", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

	struct cil_nodecon *nodecon;
	cil_nodecon_init(&nodecon);
	cil_ipaddr_init(&nodecon->addr);

        int rc = cil_fill_ipaddr(test_tree->root->cl_head->cl_head->next->cl_head, nodecon->addr);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_nodecon_anon_context(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_nodecon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "con", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "con", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_ipnull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_ipanon(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "192.168.1.1", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_nodecon_ipanon_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "(", "192.1.1", ")", "ipaddr", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_netmasknull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_netmaskanon(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "(", "255.255.255.4", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_nodecon_netmaskanon_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "(", "str0", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_contextnull_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_context_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "(", "system_u", "object_r", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_nodecon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "(", "system_u", "object_r", "type_t", "(", "low", "high", ")", ")", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_nodecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_genfscon_anon_context(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_genfscon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_typenull_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_typeparens_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "(", "type", ")", "path", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_pathnull_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_pathparens_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "(", "path", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_contextnull_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_context_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "(", "system_u", "object_r", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_genfscon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_genfscon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth0", "if_default", "packet_default", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_netifcon_nested(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth1", 
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")",
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_netifcon_nested_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "(", "eth1", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_nested_emptysecondlist_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth1", 
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")",
			"(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_extra_nested_secondlist_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth0", "extra",  
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")",
			"(", "foo", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_nested_missingobjects_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth1", 
			"(", "system_u", ")",
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_nested_secondnested_missingobjects_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth1", 
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")",
			"(", "system_u", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth0", "if_default", "packet_default", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth0", "if_default", "packet_default", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_ethmissing_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_interfacemissing_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth0", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_netifcon_packetmissing_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth0", "if_default", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_netifcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_pirqcon_pirqnotint_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "notint", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_nopirq_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_pirqrange_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "(", "1", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_anoncontext_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", "(", "con", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pirqcon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_pirqcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_iomemcon_iomemrange(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "(", "1", "2", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_iomemcon_iomemrange_firstnotint_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "(", "foo", "2", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_iomemrange_secondnotint_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "(", "1", "foo", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_iomemrange_empty_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "(", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_iomemrange_singleiomem_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "(", "1", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_iomemrange_morethantwoiomem_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "(", "1", "2", "3", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_iomemnotint_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "notint", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_noiomem_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_anoncontext_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", "(", "con", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_iomemcon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_iomemcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_ioportcon_ioportrange(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "(", "1", "2", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_ioportcon_ioportrange_firstnotint_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "(", "foo", "2", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_ioportrange_secondnotint_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "(", "1", "foo", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_ioportrange_empty_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "(", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_ioportrange_singleioport_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "(", "1", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_ioportrange_morethantwoioport_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "(", "1", "2", "3", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_ioportnotint_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "notint", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_noioport_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_anoncontext_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", "(", "con", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ioportcon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_ioportcon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_pcidevicecon_pcidevicenotint_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "notint", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_nopcidevice_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_pcidevicerange_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "(", "1", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_anoncontext_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", "(", "con", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_extra_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_pcidevicecon_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_pcidevicecon(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_anoncontext(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "(", "system_u", "object_r", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_fsuse_anoncontext_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "(", "system_u", "etc_t", "(", "low", "high", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_xattr(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_fsuse_task(CuTest *tc) {
	char *line[] = {"(", "fsuse", "task", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_fsuse_transition(CuTest *tc) {
	char *line[] = {"(", "fsuse", "trans", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_fsuse_invalidtype_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "foo", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_notype_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_typeinparens_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "(", "xattr", ")", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_nofilesystem_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_filesysteminparens_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "(", "ext3", ")", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_nocontext_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_emptyconparens_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_extra_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "con", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_fsuse_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "con", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_fsuse(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_noparams(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", ")", "(", "type", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_type(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "type", "a", ")", ")", "(", "type", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_role(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "role", "a", ")", ")", "(", "role", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_user(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "user", "a", ")", ")", "(", "user", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_sensitivity(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "sensitivity", "a", ")", ")", "(", "sensitivity", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_category(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "category", "a", ")", ")", "(", "category", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_catset(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "categoryset", "a", ")", ")", "(", "categoryset", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_level(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "level", "a", ")", ")", "(", "level", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_class(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "class", "a", ")", ")", "(", "class", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_classmap(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "classmap", "a", ")", ")", "(", "classmap", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_permset(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "permissionset", "a", ")", ")", 
				"(", "allow", "foo", "bar", "baz", "a", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_duplicate(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "class", "a",")", "(", "class", "x", ")", ")", "(", "class", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_macro_duplicate_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "class", "a",")", "(", "class", "a", ")", ")", "(", "class", "b", "(", "read," ")", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_unknown_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "foo", "a", ")", ")", "(", "foo", "b", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "foo", "a", ")", ")", "(", "foo", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "foo", "a", ")", ")", "(", "foo", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node = NULL;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_unnamed_neg(CuTest *tc) {
	char *line[] = {"(", "macro", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_noparam_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_nosecondparam_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "foo", "a", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_noparam_name_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "type", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_emptyparam_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", ")", ")", "(", "foo", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_macro_paramcontainsperiod_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "type", "a.", ")", ")", "(", "type", "b", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_macro(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_call(CuTest *tc) {
	char *line[] = {"(", "call", "mm", "(", "foo", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_call_noargs(CuTest *tc) {
	char *line[] = {"(", "call", "mm", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_call_anon(CuTest *tc) {
	char *line[] = {"(", "call", "mm", "(", "(", "s0", "(", "c0", ")", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_call_empty_call_neg(CuTest *tc) {
	char *line[] = {"(", "call", "mm", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_call_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "call", "mm", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_call_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_call_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "call", "mm", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_call_name_inparens_neg(CuTest *tc) {
	char *line[] = {"(", "call", "(", "mm", ")", "(", "foo", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_call_noname_neg(CuTest *tc) {
	char *line[] = {"(", "call", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_call(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", "(", "allow", "foo", "bar", "(", "file", "(", "read", ")", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_optional_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", "(", "allow", "foo", "bar", "(", "file", "(", "read", ")", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", "(", "allow", "foo", "bar", "(", "file", "(", "read", ")", ")", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional_unnamed_neg(CuTest *tc) {
	char *line[] = {"(", "optional", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional_extra_neg(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", "(", "allow", "foo", "bar", "(", "file", "(", "read", ")", ")", ")", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "optional", "(", "opt", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_optional_emptyoptional(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_optional_norule_neg(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", "(", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_optional(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap(CuTest *tc) {
	char *line[] = {"(", "policycap", "open_perms", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_policycap_noname_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", "(", "pol", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap_extra_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", "pol", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", "pol", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", "pol", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_policycap_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", "pol", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_policycap(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_ipv4(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_ipaddr_ipv4_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", ".168.1.1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_ipv6(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "2001:0db8:85a3:0000:0000:8a2e:0370:7334", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_gen_ipaddr_ipv6_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "2001:0db8:85a3:0000:0000:8a2e:0370:::7334", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_noname_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_nameinparens_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "(", "ip", ")", "192.168.1.1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_noip_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_ipinparens_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "(", "192.168.1.1", ")", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_extra_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", "extra", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db = NULL;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_currnull_neg(CuTest *tc) {
	char *line[] = {"(", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node;
        cil_tree_node_init(&test_ast_node);

        struct cil_db *test_db;
        cil_db_init(&test_db);

        test_ast_node->parent = test_db->ast->root;
        test_ast_node->line = 1;

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_gen_ipaddr_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};

        struct cil_tree *test_tree;
        gen_test_tree(&test_tree, line);

        struct cil_tree_node *test_ast_node = NULL;

        struct cil_db *test_db;
        cil_db_init(&test_db);

        int rc = cil_gen_ipaddr(test_db, test_tree->root->cl_head->cl_head, test_ast_node);
        CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

/*
	cil_build_ast test cases
*/

void test_cil_build_ast(CuTest *tc) {
	char *line[] = {"(", "type", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_build_ast(test_db, test_tree->root, test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_dbnull_neg(CuTest *tc) {
	char *line[] = {"(", "test", "\"qstring\"", ")", ";comment", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *null_db = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_build_ast(null_db, test_tree->root, test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_astnull_neg(CuTest *tc) {
	char *line[] = {"(", "test", "\"qstring\"", ")", ";comment", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_db->ast->root = NULL;

	int rc = cil_build_ast(test_db, test_tree->root, test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_suberr_neg(CuTest *tc) {
	char *line[] = {"(", "block", "test", "(", "block", "(", "type", "log", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = cil_build_ast(test_db, test_tree->root, test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_treenull_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "bar", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	test_tree->root = NULL;

	struct cil_db *test_db;
	cil_db_init(&test_db);

	test_db->ast->root = NULL;

	int rc = cil_build_ast(test_db, test_tree->root, test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}
	
void test_cil_build_ast_node_helper_block(CuTest *tc) {
	char *line[] = {"(", "block", "test", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_block_neg(CuTest *tc) {
	char *line[] = {"(", "block", "(", "type", "log", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);

}

void test_cil_build_ast_node_helper_blockinherit(CuTest *tc) {
	char *line[] = {"(", "blockinherit", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_blockinherit_neg(CuTest *tc) {
	char *line[] = {"(", "blockinherit", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);

}

void test_cil_build_ast_node_helper_permset(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, CIL_TREE_SKIP_NEXT, finished);
}

void test_cil_build_ast_node_helper_permset_neg(CuTest *tc) {
	char *line[] = {"(", "permissionset", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);

}

void test_cil_build_ast_node_helper_in(CuTest *tc) {
	char *line[] = {"(", "in", "foo", "(", "allow", "test", "baz", "(", "char", "(", "read", ")", ")", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_in_neg(CuTest *tc) {
	char *line[] = {"(", "in", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_class(CuTest *tc) {
	char *line[] = {"(", "class", "file", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_class_neg(CuTest *tc) {
	char *line[] = {"(", "class", "(", "read", "write", "open", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_classpermset(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", "(", "read", "(", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, CIL_TREE_SKIP_NEXT, finished);
}

void test_cil_build_ast_node_helper_classpermset_neg(CuTest *tc) {
	char *line[] = {"(", "classpermissionset", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);

}

void test_cil_build_ast_node_helper_classmap(CuTest *tc) {
	char *line[] = {"(", "classmap", "files", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_classmap_neg(CuTest *tc) {
	char *line[] = {"(", "classmap", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_classmapping(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", "char_w", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_classmapping_neg(CuTest *tc) {
	char *line[] = {"(", "classmapping", "files", "read", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_common(CuTest *tc) {
	char *line[] = {"(", "common", "test", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_common_neg(CuTest *tc) {
	char *line[] = {"(", "common", "(", "read", "write", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_sid(CuTest *tc) {
	char *line[] = {"(", "sid", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_sid_neg(CuTest *tc) {
	char *line[] = {"(", "sid", "(", "blah", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_sidcontext(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "test", "(", "blah", "blah", "blah", "(", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_sidcontext_neg(CuTest *tc) {
	char *line[] = {"(", "sidcontext", "(", "blah", "blah", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_user(CuTest *tc) {
	char *line[] = {"(", "user", "jimmypage", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_user_neg(CuTest *tc) {
	char *line[] = {"(", "user", "foo", "bar", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_userlevel(CuTest *tc) {
	char *line[] = {"(", "userlevel", "johnpauljones", "level", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_userlevel_neg(CuTest *tc) {
	char *line[] = {"(", "userlevel", "johnpauljones", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_userrange(CuTest *tc) {
	char *line[] = {"(", "userrange", "johnpauljones", "range", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_userrange_neg(CuTest *tc) {
	char *line[] = {"(", "userrange", "johnpauljones", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type(CuTest *tc) {
	char *line[] = {"(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_neg(CuTest *tc) {
	char *line[] = {"(", "type", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_attribute(CuTest *tc) {
	char *line[] = {"(", "typeattribute", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_attribute_neg(CuTest *tc) {
	char *line[] = {"(", "typeattribute", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typebounds(CuTest *tc) {
	char *line[] = {"(", "typebounds", "foo", "bar", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typebounds_neg(CuTest *tc) {
	char *line[] = {"(", "typebounds", "bar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_typepermissive(CuTest *tc) {
	char *line[] = {"(", "typepermissive", "foo", ")", NULL};
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typepermissive_neg(CuTest *tc) {
	char *line[] = {"(", "typepermissive", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_nametypetransition(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_nametypetransition_neg(CuTest *tc) {
	char *line[] = {"(", "nametypetransition", "str", "foo", "bar", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_rangetransition(CuTest *tc) {
	char *line[] = {"(", "rangetransition", "type_a", "type_b", "class", "(", "low_l", "high_l", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_rangetransition_neg(CuTest *tc) {
	char *line[] = {"(", "rangetransition", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_boolif(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "and", "foo", "bar", ")",
			"(", "true",
			"(", "allow", "foo", "bar", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_boolif_neg(CuTest *tc) {
	char *line[] = {"(", "booleanif", "(", "*&", "foo", "bar", ")",
			"(", "true",
			"(", "allow", "foo", "bar", "read", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_condblock_true(CuTest *tc) {
	char *line[] = {"(", "true", "(", "allow", "foo", "bar", "baz", "(", "write", ")", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_condblock_true_neg(CuTest *tc) {
	char *line[] = {"(", "true", "(", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_condblock_false(CuTest *tc) {
	char *line[] = {"(", "false", "(", "allow", "foo", "bar", "baz", "(", "write", ")", ")", ")", NULL}; 

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_condblock_false_neg(CuTest *tc) {
	char *line[] = {"(", "false", "(", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_tunif(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "true",
			"(", "allow", "foo", "bar", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_tunif_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "*&", "foo", "bar", ")",
			"(", "allow", "foo", "bar", "read", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}	

void test_cil_build_ast_node_helper_typealias(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", "type_t", ")", "(", "type", "test", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typealias_notype_neg(CuTest *tc) {
	char *line[] = {"(", "typealias", ".test.type", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typeattribute(CuTest *tc)
{
	char *line[] = {"(", "typeattribute", "type", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typeattribute_neg(CuTest *tc)
{
	char *line[] = {"(", "typeattribute", ".fail.type", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_typeattributeset(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "filetypes", "(", "and", "test_t", "test2_t", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_typeattributeset_neg(CuTest *tc) {
	char *line[] = {"(", "typeattributeset", "files", "(", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_userbounds(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", "user2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_userbounds_neg(CuTest *tc) {
	char *line[] = {"(", "userbounds", "user1", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_role(CuTest *tc) {
	char *line[] = {"(", "role", "test_r", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_role_neg(CuTest *tc) {
	char *line[] = {"(", "role", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_roletransition(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r", "bar_t", "process", "foobar_r", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_roletransition_neg(CuTest *tc) {
	char *line[] = {"(", "roletransition", "foo_r", "bar_t", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_roleallow(CuTest *tc) {
        char *line[] = {"(", "roleallow", "staff_r", "sysadm_r", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_roleallow_neg(CuTest *tc) {
        char *line[] = {"(", "roleallow", "staff_r", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_rolebounds(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", "role2", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_rolebounds_neg(CuTest *tc) {
	char *line[] = {"(", "rolebounds", "role1", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_avrule_allow(CuTest *tc) {
	char *line[] = {"(", "allow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_avrule_allow_neg(CuTest *tc) {
	char *line[] = {"(", "allow", "foo", "bar", "(", "read", "write", ")", "blah", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_avrule_auditallow(CuTest *tc) {
	char *line[] = {"(", "auditallow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_avrule_auditallow_neg(CuTest *tc) {
	char *line[] = {"(", "auditallow", "foo", "bar", "(", "read", "write", ")", "blah", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_avrule_dontaudit(CuTest *tc) {
	char *line[] = {"(", "dontaudit", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_avrule_dontaudit_neg(CuTest *tc) {
	char *line[] = {"(", "dontaudit", "foo", "bar", "(", "read", "write", ")", "blah", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_avrule_neverallow(CuTest *tc) {
	char *line[] = {"(", "neverallow", "test", "foo", "(", "bar", "(", "read", "write", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_avrule_neverallow_neg(CuTest *tc) {
	char *line[] = {"(", "neverallow", "foo", "bar", "(", "read", "write", ")", "blah", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_rule_transition(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);	
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_rule_transition_neg(CuTest *tc) {
	char *line[] = {"(", "typetransition", "foo", "bar", "file", "foobar", "extra",  ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);	
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_rule_change(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);	
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_rule_change_neg(CuTest *tc) {
	char *line[] = {"(", "typechange", "foo", "bar", "file", "foobar", "extra",  ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);	
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_rule_member(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);	
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_type_rule_member_neg(CuTest *tc) {
	char *line[] = {"(", "typemember", "foo", "bar", "file", "foobar", "extra",  ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);	
	
	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_bool(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", "true", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_bool_neg(CuTest *tc) {
	char *line[] = {"(", "boolean", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_bool_tunable(CuTest *tc) {
	char *line[] = {"(", "tunable", "foo", "true", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_bool_tunable_neg(CuTest *tc) {
	char *line[] = {"(", "tunable", "foo", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_sensitivity(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_sensitivity_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_sensalias(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_sensalias_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivityalias", "s0", "alias", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_category(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_category_neg(CuTest *tc) {
	char *line[] = {"(", "category", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_catset(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished); 
}

void test_cil_build_ast_node_helper_catset_neg(CuTest *tc) {
	char *line[] = {"(", "categoryset", "somecats", "(", "c0", "c1", "c2", ")", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished); 
}

void test_cil_build_ast_node_helper_catorder(CuTest *tc) {
	char *line[] = {"(", "categoryorder", "(", "c0", "c1", "c2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished); 
}

void test_cil_build_ast_node_helper_catorder_neg(CuTest *tc) {
	char *line[] = {"(", "categoryorder", "c0", "c1", "c2", "extra", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished); 
}

void test_cil_build_ast_node_helper_catalias(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "c0", "red", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_catalias_neg(CuTest *tc) {
	char *line[] = {"(", "categoryalias", "range", "(", "c0", "c1", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_catrange(CuTest *tc) {
	char *line[] = {"(", "categoryrange", "range", "(", "c0", "c1", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertIntEquals(tc, 1, finished);
}

void test_cil_build_ast_node_helper_catrange_neg(CuTest *tc) {
	char *line[] = {"(", "categoryrange", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
	CuAssertIntEquals(tc, 0, finished);
}

void test_cil_build_ast_node_helper_roletype(CuTest *tc) {
	char *line[] = {"(", "roletype", "admin_r", "admin_t", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);


	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_roletype_neg(CuTest *tc) {
	char *line[] = {"(", "roletype", "(", "admin_r", ")", "admin_t", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;
	
	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_userrole(CuTest *tc) {
	char *line[] = {"(", "userrole", "staff_u", "staff_r", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, 0, finished);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_userrole_neg(CuTest *tc) {
	char *line[] = {"(", "userrole", "staff_u", "(", "staff_r", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_classcommon(CuTest *tc) {
	char *line[] = {"(", "classcommon", "foo", "foo", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_classcommon_neg(CuTest *tc) {
	char *line[] = {"(", "classcommon", "staff_u", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_dominance(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "sensitivity", "s2", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->next->next->next->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_dominance_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "sensitivity", "s2", ")",
                        "(", "dominance", "(", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->next->next->next->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_senscat(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", "sensitivitycategory", "s1", "(", "c0", "c255", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->next->next->next->next->next->next->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_senscat_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
                        "(", "sensitivity", "s1", ")",
                        "(", "dominance", "(", "s0", "s1", ")", ")",
			"(", "category", "c0", ")",
			"(", "category", "c255", ")",
			"(", "categoryorder", "(", "c0", "c255", ")", ")",
			"(", "sensitivitycategory", "s1", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->next->next->next->next->next->next->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_level(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", "c1", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->next->next->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_level_neg(CuTest *tc) {
	char *line[] = {"(", "sensitivity", "s0", ")",
			"(", "category", "c1", ")",
			"(", "level", "low", "(", "s0", "(", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->next->next->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_levelrange(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", "(", "low", "high", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_levelrange_neg(CuTest *tc) {
	char *line[] = {"(", "levelrange", "range", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_constrain(CuTest *tc) {
	char *line[] = {"(", "constrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "r1", "r2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_constrain_neg(CuTest *tc) {
	char *line[] = {"(", "constrain", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_mlsconstrain(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", "(", "file", "(", "create", "relabelto", ")", ")", "(", "eq", "l2", "h2", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_mlsconstrain_neg(CuTest *tc) {
	char *line[] = {"(", "mlsconstrain", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_context(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", "node_lo_t", "(", "low", "high", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_context_neg(CuTest *tc) {
	char *line[] = {"(", "context", "localhost_node_label", "(", "system_u", "object_r", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_filecon(CuTest *tc) {
	char *line[] = {"(", "filecon", "root", "path", "file", "context", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_filecon_neg(CuTest *tc) {
	char *line[] = {"(", "filecon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_portcon(CuTest *tc) {
	char *line[] = {"(", "portcon", "udp", "25", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_portcon_neg(CuTest *tc) {
	char *line[] = {"(", "portcon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_nodecon(CuTest *tc) {
	char *line[] = {"(", "nodecon", "ipaddr", "ipaddr", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_nodecon_neg(CuTest *tc) {
	char *line[] = {"(", "nodecon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_genfscon(CuTest *tc) {
	char *line[] = {"(", "genfscon", "type", "path", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_genfscon_neg(CuTest *tc) {
	char *line[] = {"(", "genfscon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_netifcon(CuTest *tc) {
	char *line[] = {"(", "netifcon", "eth1", 
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")",
			"(", "system_u", "object_r", "netif_t", "(", "low", "high", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_netifcon_neg(CuTest *tc) {
	char *line[] = {"(", "netifcon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_pirqcon(CuTest *tc) {
	char *line[] = {"(", "pirqcon", "1", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_pirqcon_neg(CuTest *tc) {
	char *line[] = {"(", "pirqcon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_iomemcon(CuTest *tc) {
	char *line[] = {"(", "iomemcon", "1", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_iomemcon_neg(CuTest *tc) {
	char *line[] = {"(", "iomemcon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_ioportcon(CuTest *tc) {
	char *line[] = {"(", "ioportcon", "1", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_ioportcon_neg(CuTest *tc) {
	char *line[] = {"(", "ioportcon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_pcidevicecon(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", "1", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_pcidevicecon_neg(CuTest *tc) {
	char *line[] = {"(", "pcidevicecon", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_fsuse(CuTest *tc) {
	char *line[] = {"(", "fsuse", "xattr", "ext3", "con", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_fsuse_neg(CuTest *tc) {
	char *line[] = {"(", "fsuse", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_macro(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "type", "a", ")", ")", "(", "type", "b", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_macro_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_macro_nested_macro_neg(CuTest *tc) {
	char *line[] = {"(", "macro", "mm", "(", "(", "type", "a", ")", ")", "(", "type", "b", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_macro *macro;
	cil_macro_init(&macro);

	struct cil_tree_node *macronode;
	cil_tree_node_init(&macronode);
	macronode->data = macro;
	macronode->flavor = CIL_MACRO;

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, macronode, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);

	cil_db_destroy(&test_db);
	cil_destroy_macro(macro);
}

void test_cil_build_ast_node_helper_gen_macro_nested_tunif_neg(CuTest *tc) {
	char *line[] = {"(", "tunableif", "(", "and", "foo", "bar", ")",
			"(", "allow", "foo", "bar", "(", "read", ")", ")", ")", NULL};

	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_macro *macro;
	cil_macro_init(&macro);
	
	struct cil_tree_node *macronode;
	cil_tree_node_init(&macronode);
	macronode->data = macro;
	macronode->flavor = CIL_MACRO;

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, macronode, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);

	cil_db_destroy(&test_db);
	cil_destroy_macro(macro);
}

void test_cil_build_ast_node_helper_gen_call(CuTest *tc) {
	char *line[] = {"(", "call", "mm", "(", "foo", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_call_neg(CuTest *tc) {
	char *line[] = {"(", "call", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_optional(CuTest *tc) {
	char *line[] = {"(", "optional", "opt", "(", "allow", "foo", "bar", "(", "file", "(", "read", ")", ")", ")", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_optional_neg(CuTest *tc) {
	char *line[] = {"(", "optional", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_policycap(CuTest *tc) {
	char *line[] = {"(", "policycap", "open_perms", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 1);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_policycap_neg(CuTest *tc) {
	char *line[] = {"(", "policycap", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_gen_ipaddr(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_node_helper_gen_ipaddr_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	uint32_t finished = 0;

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);

	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, finished, 0);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_node_helper_extraargsnull_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_args_build *extra_args = NULL;

	uint32_t finished = 0;
	
	int rc = __cil_build_ast_node_helper(test_tree->root->cl_head->cl_head, &finished, extra_args);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}

void test_cil_build_ast_last_child_helper(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	struct cil_args_build *extra_args = gen_build_args(test_db->ast->root, test_db, NULL, NULL);
	
	int rc = __cil_build_ast_last_child_helper(test_tree->root->cl_head->cl_head, extra_args);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_build_ast_last_child_helper_extraargsnull_neg(CuTest *tc) {
	char *line[] = {"(", "ipaddr", "ip", "192.168.1.1", ")", NULL};
	
	struct cil_tree *test_tree;
	gen_test_tree(&test_tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	int rc = __cil_build_ast_last_child_helper(test_tree->root->cl_head->cl_head, NULL);
	CuAssertIntEquals(tc, SEPOL_ERR, rc);
}


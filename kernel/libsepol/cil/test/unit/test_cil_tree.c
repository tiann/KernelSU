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
#include "test_cil_tree.h"

#include "../../src/cil_tree.h"

void test_cil_tree_node_init(CuTest *tc) {
   struct cil_tree_node *test_node;

   cil_tree_node_init(&test_node);

   CuAssertPtrNotNull(tc, test_node);
   CuAssertPtrEquals(tc, NULL, test_node->cl_head);
   CuAssertPtrEquals(tc, NULL, test_node->cl_tail);
   CuAssertPtrEquals(tc, NULL, test_node->parent);
   CuAssertPtrEquals(tc, NULL, test_node->data);
   CuAssertPtrEquals(tc, NULL, test_node->next);
   CuAssertIntEquals(tc, 0, test_node->flavor);
   CuAssertIntEquals(tc, 0, test_node->line);

   free(test_node);
}

void test_cil_tree_init(CuTest *tc) {
	struct cil_tree *test_tree;

	int rc = cil_tree_init(&test_tree);

	CuAssertIntEquals(tc, SEPOL_OK, rc);
	CuAssertPtrNotNull(tc, test_tree);
	CuAssertPtrEquals(tc, NULL, test_tree->root->cl_head);
	CuAssertPtrEquals(tc, NULL, test_tree->root->cl_tail);
	CuAssertPtrEquals(tc, NULL, test_tree->root->parent);
	CuAssertPtrEquals(tc, NULL, test_tree->root->data);
	CuAssertPtrEquals(tc, NULL, test_tree->root->next);
	CuAssertIntEquals(tc, 0, test_tree->root->flavor);
	CuAssertIntEquals(tc, 0, test_tree->root->line);

	free(test_tree);
}


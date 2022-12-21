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

#include "../../src/cil_fqn.h"
#include "../../src/cil_build_ast.h"

void test_cil_qualify_name(CuTest *tc) {
	char *line[] = {"(", "category", "c0", ")",
			"(", "categoryorder", "(", "c0", ")", ")",
			"(", "sensitivity", "s0", ")",
			"(", "sensitivitycategory", "s0", "(", "c0", ")", ")",
			"(", "type", "blah_t", ")",
			"(", "role", "blah_r", ")",
			"(", "user", "blah_u", ")",
			"(", "context", "con", "(", "blah_u", "blah_r", "blah_t", "(", "s0", "(", "c0", ")", ")", "(", "s0", "(", "c0", ")", ")", ")", ")",
			"(", "sid", "test", "con", NULL};

	struct cil_tree *tree;
	gen_test_tree(&tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	cil_build_ast(test_db, tree->root, test_db->ast->root);

	int rc = cil_fqn_qualify(test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

void test_cil_qualify_name_cil_flavor(CuTest *tc) {
	char *line[] = {"(", "class",  "file", "inherits", "file",
			"(", "open", ")", ")", NULL};

	struct cil_tree *tree;
	gen_test_tree(&tree, line);

	struct cil_db *test_db;
	cil_db_init(&test_db);

	cil_build_ast(test_db, tree->root, test_db->ast->root);

	int rc = cil_fqn_qualify(test_db->ast->root);
	CuAssertIntEquals(tc, SEPOL_OK, rc);
}

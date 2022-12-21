/*
 * Author: Joshua Brindle <jbrindle@tresys.com>
 *
 * Copyright (C) 2006 Tresys Technology, LLC
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

#include "test-linker-cond-map.h"
#include "parse_util.h"
#include "helpers.h"
#include "test-common.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/conditional.h>

#include <CUnit/Basic.h>
#include <stdlib.h>

/* Tests for conditionals
 * Test each cond/bool for these
 * - boolean copied correctly (state is correct)
 * - conditional expression is correct
 * Tests: 
 * - single boolean in base
 * - single boolean in module
 * - single boolean in base optional
 * - single boolean in module optional
 * - 2 booleans in base
 * - 2 booleans in module
 * - 2 booleans in base optional
 * - 2 booleans in module optional
 * - 2 booleans, base and module
 * - 2 booleans, base optional and module
 * - 2 booleans, base optional and module optional
 * - 3 booleans, base, base optional, module
 * - 4 boolean, base, base optional, module, module optional
 */

typedef struct test_cond_expr {
	const char *bool;
	uint32_t expr_type;
} test_cond_expr_t;

static void test_cond_expr_mapping(policydb_t * p, avrule_decl_t * d, test_cond_expr_t * bools, int len)
{
	int i;
	cond_expr_t *expr;

	CU_ASSERT_FATAL(d->cond_list != NULL);
	CU_ASSERT_FATAL(d->cond_list->expr != NULL);

	expr = d->cond_list->expr;

	for (i = 0; i < len; i++) {
		CU_ASSERT_FATAL(expr != NULL);

		CU_ASSERT(expr->expr_type == bools[i].expr_type);
		if (bools[i].bool) {
			CU_ASSERT(strcmp(p->sym_val_to_name[SYM_BOOLS][expr->bool - 1], bools[i].bool) == 0);
		}
		expr = expr->next;
	}
}

static void test_bool_state(policydb_t * p, const char *bool, int state)
{
	cond_bool_datum_t *b;

	b = hashtab_search(p->p_bools.table, bool);
	CU_ASSERT_FATAL(b != NULL);
	CU_ASSERT(b->state == state);
}

void base_cond_tests(policydb_t * base)
{
	avrule_decl_t *d;
	unsigned int decls[1];
	test_cond_expr_t bools[2];

	/* these tests look at booleans and conditionals in the base only
	 * to ensure that they aren't altered or removed during the link process */

	/* bool existence and state, global scope */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b");
	decls[0] = d->decl_id;
	test_sym_presence(base, "g_b_bool_1", SYM_BOOLS, SCOPE_DECL, decls, 1);
	test_bool_state(base, "g_b_bool_1", 0);
	/* conditional expression mapped correctly */
	bools[0].bool = "g_b_bool_1";
	bools[0].expr_type = COND_BOOL;
	test_cond_expr_mapping(base, d, bools, 1);

	/* bool existence and state, optional scope */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b");
	decls[0] = d->decl_id;
	test_sym_presence(base, "o1_b_bool_1", SYM_BOOLS, SCOPE_DECL, decls, 1);
	test_bool_state(base, "o1_b_bool_1", 1);
	/* conditional expression mapped correctly */
	bools[0].bool = "o1_b_bool_1";
	bools[0].expr_type = COND_BOOL;
	test_cond_expr_mapping(base, d, bools, 1);

}

void module_cond_tests(policydb_t * base)
{
	avrule_decl_t *d;
	unsigned int decls[1];
	test_cond_expr_t bools[3];

	/* bool existence and state, module 1 global scope */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1");
	decls[0] = d->decl_id;
	test_sym_presence(base, "g_m1_bool_1", SYM_BOOLS, SCOPE_DECL, decls, 1);
	test_bool_state(base, "g_m1_bool_1", 1);
	/* conditional expression mapped correctly */
	bools[0].bool = "g_m1_bool_1";
	bools[0].expr_type = COND_BOOL;
	test_cond_expr_mapping(base, d, bools, 1);

	/* bool existence and state, module 1 optional scope */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_m1");
	decls[0] = d->decl_id;
	test_sym_presence(base, "o1_m1_bool_1", SYM_BOOLS, SCOPE_DECL, decls, 1);
	test_bool_state(base, "o1_m1_bool_1", 0);
	/* conditional expression mapped correctly */
	bools[0].bool = "o1_m1_bool_1";
	bools[0].expr_type = COND_BOOL;
	test_cond_expr_mapping(base, d, bools, 1);

	/* bool existence and state, module 2 global scope */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m2");
	decls[0] = d->decl_id;
	test_sym_presence(base, "g_m2_bool_1", SYM_BOOLS, SCOPE_DECL, decls, 1);
	test_sym_presence(base, "g_m2_bool_2", SYM_BOOLS, SCOPE_DECL, decls, 1);
	test_bool_state(base, "g_m2_bool_1", 1);
	test_bool_state(base, "g_m2_bool_2", 0);
	/* conditional expression mapped correctly */
	bools[0].bool = "g_m2_bool_1";
	bools[0].expr_type = COND_BOOL;
	bools[1].bool = "g_m2_bool_2";
	bools[1].expr_type = COND_BOOL;
	bools[2].bool = NULL;
	bools[2].expr_type = COND_AND;
	test_cond_expr_mapping(base, d, bools, 3);
}

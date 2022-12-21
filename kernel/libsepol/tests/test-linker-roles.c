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

#include "test-linker-roles.h"
#include "parse_util.h"
#include "helpers.h"
#include "test-common.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>

#include <CUnit/Basic.h>
#include <stdlib.h>

/* Tests for roles:
 * Test for each of these for 
 * - role in appropriate symtab (global and decl)
 * - datum in the decl symtab has correct type_set
 * - scope datum has correct decl ids
 * - dominates bitmap is correct
 * Tests:
 * - role in base, no modules
 * - role in base optional, no modules
 * - role a in base, b in module
 * - role a in base and module (additive)
 * - role a in base and 2 module
 * - role a in base optional, b in module
 * - role a in base, b in module optional
 * - role a in base optional, b in module optional
 * - role a in base optional and module
 * - role a in base and module optional
 * - role a in base optional and module optional
 * - role a in base optional and 2 modules
 * - role a and b in base, b dom a, are types correct (TODO)
 */

/* this simply tests whether the passed in role only has its own 
 * value in its dominates ebitmap */
static void only_dominates_self(policydb_t * p, role_datum_t * role)
{
	ebitmap_node_t *tnode;
	unsigned int i;
	int found = 0;

	ebitmap_for_each_positive_bit(&role->dominates, tnode, i) {
		found++;
		CU_ASSERT(i == role->s.value - 1);
	}
	CU_ASSERT(found == 1);
}

void base_role_tests(policydb_t * base)
{
	avrule_decl_t *decl;
	role_datum_t *role;
	unsigned int decls[2];
	const char *types[2];

	/* These tests look at roles in the base only, the desire is to ensure that
	 * roles are not destroyed or otherwise removed during the link process */

	/**** test for g_b_role_1 in base and decl 1 (global) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_role_1", SYM_ROLES, SCOPE_DECL, decls, 1);
	/* make sure it has the correct type set (g_b_type_1, no negset, no flags) */
	types[0] = "g_b_type_1";
	role = test_role_type_set(base, "g_b_role_1", NULL, types, 1, 0);
	/* This role should only dominate itself */
	only_dominates_self(base, role);

	/**** test for o1_b_role_1 in optional (decl 2) ****/
	decl = test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b");
	decls[0] = decl->decl_id;
	test_sym_presence(base, "o1_b_role_1", SYM_ROLES, SCOPE_DECL, decls, 1);
	/* make sure it has the correct type set (o1_b_type_1, no negset, no flags) */
	types[0] = "o1_b_type_1";
	role = test_role_type_set(base, "o1_b_role_1", decl, types, 1, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);
}

void module_role_tests(policydb_t * base)
{
	role_datum_t *role;
	avrule_decl_t *decl;
	unsigned int decls[3];
	const char *types[3];

	/* These tests are run when the base is linked with 2 modules,
	 * They should test whether the roles get copied correctly from the 
	 * modules into the base */

	/**** test for role in module 1 (global) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "g_m1_role_1", SYM_ROLES, SCOPE_DECL, decls, 1);
	/* make sure it has the correct type set (g_m1_type_1, no negset, no flags) */
	types[0] = "g_m1_type_1";
	role = test_role_type_set(base, "g_m1_role_1", NULL, types, 1, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/**** test for role in module 1 (optional) ****/
	decl = test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_m1");
	decls[0] = decl->decl_id;
	test_sym_presence(base, "o1_m1_role_1", SYM_ROLES, SCOPE_DECL, decls, 1);
	/* make sure it has the correct type set (o1_m1_type_1, no negset, no flags) */
	types[0] = "o1_m1_type_1";
	role = test_role_type_set(base, "o1_m1_role_1", decl, types, 1, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/* These test whether the type sets are copied to the right place and
	 * correctly unioned when they should be */

	/**** test for type added to base role in module 1 (global) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_role_2", SYM_ROLES, SCOPE_DECL, decls, 1);
	/* make sure it has the correct type set (g_m1_type_1, no negset, no flags) */
	types[0] = "g_b_type_2";	/* added in base when declared */
	types[1] = "g_m1_type_1";	/* added in module */
	role = test_role_type_set(base, "g_b_role_2", NULL, types, 2, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/**** test for type added to base role in module 1 & 2 (global) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	decls[1] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	decls[2] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m2"))->decl_id;
	test_sym_presence(base, "g_b_role_3", SYM_ROLES, SCOPE_DECL, decls, 3);
	/* make sure it has the correct type set (g_b_type_2, g_m1_type_2, g_m2_type_2, no negset, no flags) */
	types[0] = "g_b_type_2";	/* added in base when declared */
	types[1] = "g_m1_type_2";	/* added in module 1 */
	types[2] = "g_m2_type_2";	/* added in module 2 */
	role = test_role_type_set(base, "g_b_role_3", NULL, types, 3, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/**** test for role in base optional and module 1 (additive) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b"))->decl_id;
	decls[1] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "o1_b_role_2", SYM_ROLES, SCOPE_DECL, decls, 2);
	/* this one will have 2 type sets, one in the global symtab and one in the base optional 1 */
	types[0] = "g_m1_type_1";
	role = test_role_type_set(base, "o1_b_role_2", NULL, types, 1, 0);
	types[0] = "o1_b_type_1";
	role = test_role_type_set(base, "o1_b_role_2", test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b"), types, 1, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/**** test for role in base and module 1 optional (additive) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	decls[1] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o2_m1"))->decl_id;
	test_sym_presence(base, "g_b_role_4", SYM_ROLES, SCOPE_DECL, decls, 2);
	/* this one will have 2 type sets, one in the global symtab and one in the base optional 1 */
	types[0] = "g_b_type_2";
	role = test_role_type_set(base, "g_b_role_4", NULL, types, 1, 0);
	types[0] = "g_m1_type_2";
	role = test_role_type_set(base, "g_b_role_4", test_find_decl_by_sym(base, SYM_TYPES, "tag_o2_m1"), types, 1, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/**** test for role in base and module 1 optional (additive) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_b"))->decl_id;
	decls[1] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_m1"))->decl_id;
	test_sym_presence(base, "o3_b_role_1", SYM_ROLES, SCOPE_DECL, decls, 2);
	/* this one will have 2 type sets, one in the 3rd base optional and one in the 3rd module optional */
	types[0] = "o3_b_type_1";
	role = test_role_type_set(base, "o3_b_role_1", test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_b"), types, 1, 0);
	types[0] = "o3_m1_type_1";
	role = test_role_type_set(base, "o3_b_role_1", test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_m1"), types, 1, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);

	/**** test for role in base and module 1 optional (additive) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o4_b"))->decl_id;
	decls[1] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	decls[2] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m2"))->decl_id;
	test_sym_presence(base, "o4_b_role_1", SYM_ROLES, SCOPE_DECL, decls, 3);
	/* this one will have 2 type sets, one in the global symtab (with both module types) and one in the 4th optional of base */
	types[0] = "g_m1_type_1";
	role = test_role_type_set(base, "o4_b_role_1", test_find_decl_by_sym(base, SYM_TYPES, "tag_o4_b"), types, 1, 0);
	types[0] = "g_m2_type_1";
	types[1] = "g_m1_type_2";
	role = test_role_type_set(base, "o4_b_role_1", NULL, types, 2, 0);
	/* and only dominates itself */
	only_dominates_self(base, role);
}

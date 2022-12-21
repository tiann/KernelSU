/*
 * Author: Joshua Brindle <jbrindle@tresys.com>
 *         Chad Sellers <csellers@tresys.com>
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

#include "test-linker-types.h"
#include "parse_util.h"
#include "helpers.h"
#include "test-common.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>

#include <CUnit/Basic.h>
#include <stdlib.h>

/* Tests for types:
 * Test for each of these for 
 * - type in appropriate symtab (global and decl)
 * - datum in the decl symtab has correct type bitmap (if attr)
 * - primary is set correctly
 * - scope datum has correct decl ids
 * Tests:
 * - type in base, no modules
 * - type in base optional, no modules
 * - type a in base, b in module
 * - type a in base optional, b in module
 * - type a in base, b in module optional
 * - type a in base optional, b in module optional
 * - attr in base, no modules
 * - attr in base optional, no modules
 * - attr a in base, b in module
 * - attr a in base optional, b in module
 * - attr a in base, b in module optional
 * - attr a in base optional, b in module optional
 * - attr a declared in base, added to in module
 * - attr a declared in base, added to in module optional
 * - attr a declared in base, added to in 2 modules 
 * - attr a declared in base, added to in 2 modules (optional and global)
 * - attr a declared in base optional, added to in module
 * - attr a declared in base optional, added to in module optional
 * - attr a added to in base optional, declared in module
 * - attr a added to in base optional, declared in module optional
 * - attr a added to in base optional, declared in module, added to in other module
 * - attr a added to in base optional, declared in module optional, added to in other module
 * - attr a added to in base optional, declared in module , added to in other module optional
 * - attr a added to in base optional, declared in module optional, added to in other module optional
 * - alias in base of primary type in base, no modules
 * - alias in base optional of primary type in base, no modules
 * - alias in base optional of primary type in base optional
 * - alias in module of primary type in base
 * - alias in module optional of primary type in base
 * - alias in module optional of primary type in base optional
 * - alias in module of primary type in module
 * - alias in module optional of primary type in module
 * - alias in module optional of primary type in module optional
 * - alias a in base, b in module, primary type in base
 * - alias a in base, b in module, primary type in module
 * - alias a in base optional, b in module, primary type in base
 * - alias a in base optional, b in module, primary type in module
 * - alias a in base, b in module optional, primary type in base
 * - alias a in base, b in module optional, primary type in module
 * - alias a in base optional, b in module optional, primary type in base
 * - alias a in base optional, b in module optional, primary type in module
 * - alias a in base, required in module, primary type in base
 * - alias a in base, required in base optional, primary type in base
 * - alias a in base, required in module optional, primary type in base
 * - alias a in module, required in base optional, primary type in base
 * - alias a in module, required in module optional, primary type in base
 * - alias a in base optional, required in module, primary type in base
 * - alias a in base optional, required in different base optional, primary type in base
 * - alias a in base optional, required in module optional, primary type in base
 * - alias a in module optional, required in base optional, primary type in base
 * - alias a in module optional, required in module optional, primary type in base
 * - alias a in module, required in base optional, primary type in module
 * - alias a in module, required in module optional, primary type in module
 * - alias a in base optional, required in module, primary type in module
 * - alias a in base optional, required in different base optional, primary type in module
 * - alias a in base optional, required in module optional, primary type in module
 * - alias a in module optional, required in base optional, primary type in module
 * - alias a in module optional, required in module optional, primary type in module
 */

/* Don't pass in decls from global blocks since symbols aren't stored in their symtab */
static void test_type_datum(policydb_t * p, const char *id, unsigned int *decls, int len, unsigned int primary)
{
	int i;
	unsigned int value;
	type_datum_t *type;

	/* just test the type datums for each decl to see if it is what we expect */
	type = hashtab_search(p->p_types.table, id);

	CU_ASSERT_FATAL(type != NULL);
	CU_ASSERT(type->primary == primary);
	CU_ASSERT(type->flavor == TYPE_TYPE);

	value = type->s.value;

	for (i = 0; i < len; i++) {
		type = hashtab_search(p->decl_val_to_struct[decls[i] - 1]->p_types.table, id);
		CU_ASSERT_FATAL(type != NULL);
		CU_ASSERT(type->primary == primary);
		CU_ASSERT(type->flavor == TYPE_TYPE);
		CU_ASSERT(type->s.value == value);
	}

}

void base_type_tests(policydb_t * base)
{
	unsigned int decls[2];
	const char *types[2];

	/* These tests look at types in the base only, the desire is to ensure that
	 * types are not destroyed or otherwise removed during the link process.
	 * if this happens these tests won't work anyway since we are using types to 
	 * mark blocks */

	/**** test for g_b_type_1 in base and decl 1 (global) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_type_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_type_datum(base, "g_b_type_1", NULL, 0, 1);
	/* this attr is in the same decl as the type */
	test_sym_presence(base, "g_b_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_b_type_1";
	test_attr_types(base, "g_b_attr_1", NULL, types, 1);

	/**** test for o1_b_type_1 in optional (decl 2) ****/
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b"))->decl_id;
	test_sym_presence(base, "o1_b_type_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_type_datum(base, "o1_b_type_1", NULL, 0, 1);
	/* this attr is in the same decl as the type */
	test_sym_presence(base, "o1_b_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "o1_b_type_1";
	test_attr_types(base, "o1_b_attr_1", base->decl_val_to_struct[decls[0] - 1], types, 1);

	/* tests for aliases */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_alias_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_alias_datum(base, "g_b_alias_1", "g_b_type_3", 1, 0);
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o6_b"))->decl_id;
	test_sym_presence(base, "g_b_alias_2", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_alias_datum(base, "g_b_alias_2", "g_b_type_3", 1, 0);

}

void module_type_tests(policydb_t * base)
{
	unsigned int decls[2];
	const char *types[2];
	avrule_decl_t *d;

	/* These tests look at types that were copied from modules or attributes
	 * that were modified and declared in modules and base. These apply to 
	 * declarations and modifications in and out of optionals. These tests
	 * should ensure that types and attributes are correctly copied from modules
	 * and that attribute type sets are correctly copied and mapped. */

	/* note: scope for attributes is currently smashed if the attribute is declared 
	 * somewhere so the scope test only looks at global, the type bitmap test looks
	 * at the appropriate decl symtab */

	/* test for type in module 1 (global) */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "g_m1_type_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_type_datum(base, "g_m1_type_1", NULL, 0, 1);
	/* attr has is in the same decl as the above type */
	test_sym_presence(base, "g_m1_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_m1_type_1";
	types[1] = "g_m1_type_2";
	test_attr_types(base, "g_m1_attr_1", NULL, types, 2);

	/* test for type in module 1 (optional) */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_m1"))->decl_id;
	test_sym_presence(base, "o1_m1_type_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_type_datum(base, "o1_m1_type_1", NULL, 0, 1);
	/* attr has is in the same decl as the above type */
	test_sym_presence(base, "o1_m1_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "o1_m1_type_2";
	test_attr_types(base, "o1_m1_attr_1", base->decl_val_to_struct[decls[0] - 1], types, 1);

	/* test for attr declared in base, added to in module (global). 
	 * Since these are both global it'll be merged in the main symtab */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_attr_3", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_m1_type_3";
	test_attr_types(base, "g_b_attr_3", NULL, types, 1);

	/* test for attr declared in base, added to in module (optional). */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_attr_4", SYM_TYPES, SCOPE_DECL, decls, 1);

	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_m1"))->decl_id;
	types[0] = "o1_m1_type_3";
	test_attr_types(base, "g_b_attr_4", base->decl_val_to_struct[decls[0] - 1], types, 1);

	/* test for attr declared in base, added to in 2 modules (global). (merged in main symtab) */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_attr_5", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_m1_type_4";
	types[1] = "g_m2_type_4";
	test_attr_types(base, "g_b_attr_5", NULL, types, 2);

	/* test for attr declared in base, added to in 2 modules (optional/global). */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_b"))->decl_id;
	test_sym_presence(base, "g_b_attr_6", SYM_TYPES, SCOPE_DECL, decls, 1);
	/* module 2 was global to its type is in main symtab */
	types[0] = "g_m2_type_5";
	test_attr_types(base, "g_b_attr_6", NULL, types, 1);
	d = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_m1"));
	types[0] = "o3_m1_type_2";
	test_attr_types(base, "g_b_attr_6", d, types, 1);

	/* test for attr declared in base optional, added to in module (global). */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o4_b"))->decl_id;
	test_sym_presence(base, "o4_b_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_m1_type_5";
	test_attr_types(base, "o4_b_attr_1", NULL, types, 1);

	/* test for attr declared in base optional, added to in module (optional). */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b"))->decl_id;
	test_sym_presence(base, "o1_b_attr_2", SYM_TYPES, SCOPE_DECL, decls, 1);
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_m1");
	types[0] = "o1_m1_type_5";
	test_attr_types(base, "o1_b_attr_2", d, types, 1);

	/* test for attr declared in module, added to in base optional */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "g_m1_attr_2", SYM_TYPES, SCOPE_DECL, decls, 1);
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o1_b");
	types[0] = "o1_b_type_2";
	test_attr_types(base, "g_m1_attr_2", d, types, 1);

	/* test for attr declared in module optional, added to in base optional */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_m1"))->decl_id;
	test_sym_presence(base, "o3_m1_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o4_b");
	types[0] = "o4_b_type_1";
	test_attr_types(base, "o3_m1_attr_1", d, types, 1);

	/* attr a added to in base optional, declared/added to in module, added to in other module */
	/* first the module declare/add and module 2 add (since its global it'll be in the main symtab */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "g_m1_attr_3", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_m1_type_6";
	types[1] = "g_m2_type_3";
	test_attr_types(base, "g_m1_attr_3", NULL, types, 2);
	/* base add */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o4_b");
	types[0] = "o4_b_type_2";
	test_attr_types(base, "g_m1_attr_3", d, types, 1);

	/* attr a added to in base optional, declared/added in module optional, added to in other module */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o3_m1");
	decls[0] = d->decl_id;
	test_sym_presence(base, "o3_m1_attr_2", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "o3_m1_type_3";
	test_attr_types(base, "o3_m1_attr_2", d, types, 1);
	/* module 2's type will be in the main symtab */
	types[0] = "g_m2_type_6";
	test_attr_types(base, "o3_m1_attr_2", NULL, types, 1);
	/* base add */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o2_b");
	types[0] = "o2_b_type_1";
	test_attr_types(base, "o3_m1_attr_2", d, types, 1);

	/* attr a added to in base optional, declared/added in module , added to in other module optional */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "g_m1_attr_4", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "g_m1_type_7";
	test_attr_types(base, "g_m1_attr_4", NULL, types, 1);
	/* module 2 */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o2_m2");
	types[0] = "o2_m2_type_1";
	test_attr_types(base, "g_m1_attr_4", d, types, 1);
	/* base add */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o5_b");
	types[0] = "o5_b_type_1";
	test_attr_types(base, "g_m1_attr_4", d, types, 1);

	/* attr a added to in base optional, declared/added in module optional, added to in other module optional */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o4_m1");
	decls[0] = d->decl_id;
	test_sym_presence(base, "o4_m1_attr_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	types[0] = "o4_m1_type_1";
	test_attr_types(base, "o4_m1_attr_1", d, types, 1);
	/* module 2 */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o2_m2");
	types[0] = "o2_m2_type_2";
	test_attr_types(base, "o4_m1_attr_1", d, types, 1);
	/* base add */
	d = test_find_decl_by_sym(base, SYM_TYPES, "tag_o5_b");
	types[0] = "o5_b_type_2";
	test_attr_types(base, "o4_m1_attr_1", d, types, 1);

	/* tests for aliases */
	decls[0] = (test_find_decl_by_sym(base, SYM_TYPES, "tag_g_m1"))->decl_id;
	test_sym_presence(base, "g_m_alias_1", SYM_TYPES, SCOPE_DECL, decls, 1);
	test_alias_datum(base, "g_m_alias_1", "g_b_type_3", 1, 0);

}

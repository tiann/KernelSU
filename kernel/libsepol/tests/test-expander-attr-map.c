/*
 * Authors: Chad Sellers <csellers@tresys.com>
 *          Joshua Brindle <jbrindle@tresys.com>
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

#include "test-expander-attr-map.h"
#include "test-common.h"
#include "helpers.h"

#include <sepol/policydb/policydb.h>
#include <CUnit/Basic.h>
#include <stdlib.h>

extern policydb_t base_expanded2;

void test_expander_attr_mapping(void)
{
	/* note that many cases are omitted because they don't make sense
	   (i.e. declaring in an optional and then using it in the base) or
	   because declare in optional then require in a different optional
	   logic still doesn't work */

	const char *typesb1[] = { "attr_check_base_1_1_t", "attr_check_base_1_2_t" };
	const char *typesb2[] = { "attr_check_base_2_1_t", "attr_check_base_2_2_t" };
	const char *typesb3[] = { "attr_check_base_3_1_t", "attr_check_base_3_2_t",
		"attr_check_base_3_3_t", "attr_check_base_3_4_t"
	};
	const char *typesb4[] = { "attr_check_base_4_1_t", "attr_check_base_4_2_t" };
	const char *typesb5[] = { "attr_check_base_5_1_t", "attr_check_base_5_2_t" };
	const char *typesb6[] = { "attr_check_base_6_1_t", "attr_check_base_6_2_t",
		"attr_check_base_6_3_t", "attr_check_base_6_4_t"
	};
	const char *typesbo2[] = { "attr_check_base_optional_2_1_t",
		"attr_check_base_optional_2_2_t"
	};
	const char *typesbo5[] = { "attr_check_base_optional_5_1_t",
		"attr_check_base_optional_5_2_t"
	};
	const char *typesm2[] = { "attr_check_mod_2_1_t", "attr_check_mod_2_2_t" };
	const char *typesm4[] = { "attr_check_mod_4_1_t", "attr_check_mod_4_2_t" };
	const char *typesm5[] = { "attr_check_mod_5_1_t", "attr_check_mod_5_2_t" };
	const char *typesm6[] = { "attr_check_mod_6_1_t", "attr_check_mod_6_2_t",
		"attr_check_mod_6_3_t", "attr_check_mod_6_4_t"
	};
	const char *typesmo2[] = { "attr_check_mod_optional_4_1_t",
		"attr_check_mod_optional_4_2_t"
	};
	const char *typesb10[] = { "attr_check_base_10_1_t", "attr_check_base_10_2_t" };
	const char *typesb11[] = { "attr_check_base_11_3_t", "attr_check_base_11_4_t" };
	const char *typesm10[] = { "attr_check_mod_10_1_t", "attr_check_mod_10_2_t" };
	const char *typesm11[] = { "attr_check_mod_11_3_t", "attr_check_mod_11_4_t" };

	test_attr_types(&base_expanded2, "attr_check_base_1", NULL, typesb1, 2);
	test_attr_types(&base_expanded2, "attr_check_base_2", NULL, typesb2, 2);
	test_attr_types(&base_expanded2, "attr_check_base_3", NULL, typesb3, 4);
	test_attr_types(&base_expanded2, "attr_check_base_4", NULL, typesb4, 2);
	test_attr_types(&base_expanded2, "attr_check_base_5", NULL, typesb5, 2);
	test_attr_types(&base_expanded2, "attr_check_base_6", NULL, typesb6, 4);
	test_attr_types(&base_expanded2, "attr_check_base_optional_2", NULL, typesbo2, 2);
	test_attr_types(&base_expanded2, "attr_check_base_optional_5", NULL, typesbo5, 2);
	test_attr_types(&base_expanded2, "attr_check_mod_2", NULL, typesm2, 2);
	test_attr_types(&base_expanded2, "attr_check_mod_4", NULL, typesm4, 2);
	test_attr_types(&base_expanded2, "attr_check_mod_5", NULL, typesm5, 2);
	test_attr_types(&base_expanded2, "attr_check_mod_6", NULL, typesm6, 4);
	test_attr_types(&base_expanded2, "attr_check_mod_optional_4", NULL, typesmo2, 2);
	test_attr_types(&base_expanded2, "attr_check_base_7", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_base_8", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_base_9", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_base_10", NULL, typesb10, 2);
	test_attr_types(&base_expanded2, "attr_check_base_11", NULL, typesb11, 2);
	test_attr_types(&base_expanded2, "attr_check_mod_7", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_mod_8", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_mod_9", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_mod_10", NULL, typesm10, 2);
	test_attr_types(&base_expanded2, "attr_check_mod_11", NULL, typesm11, 2);
	test_attr_types(&base_expanded2, "attr_check_base_optional_8", NULL, NULL, 0);
	test_attr_types(&base_expanded2, "attr_check_mod_optional_7", NULL, NULL, 0);
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_base_optional_disabled_5"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_base_optional_disabled_5_1_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_base_optional_disabled_5_2_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_base_optional_disabled_8"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_base_optional_disabled_8_1_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_base_optional_disabled_8_2_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_mod_optional_disabled_4"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_mod_optional_disabled_4_1_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_mod_optional_disabled_4_2_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_mod_optional_disabled_7"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_mod_optional_disabled_7_1_t"));
	CU_ASSERT(!hashtab_search((&base_expanded2)->p_types.table, "attr_check_mod_optional_disabled_7_2_t"));
}

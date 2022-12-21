/*
 * Author: Karl MacMillan <kmacmillan@tresys.com>
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

#include "test-cond.h"
#include "parse_util.h"
#include "helpers.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/conditional.h>

static policydb_t basemod;
static policydb_t base_expanded;

int cond_test_init(void)
{
	if (policydb_init(&base_expanded)) {
		fprintf(stderr, "out of memory!\n");
		ksu_policydb_destroy(&basemod);
		return -1;
	}

	if (test_load_policy(&basemod, POLICY_BASE, 1, "test-cond", "refpolicy-base.conf"))
		goto cleanup;

	if (link_modules(NULL, &basemod, NULL, 0, 0)) {
		fprintf(stderr, "link modules failed\n");
		goto cleanup;
	}

	if (expand_module(NULL, &basemod, &base_expanded, 0, 1)) {
		fprintf(stderr, "expand module failed\n");
		goto cleanup;
	}

	return 0;

      cleanup:
	ksu_policydb_destroy(&basemod);
	ksu_policydb_destroy(&base_expanded);
	return -1;
}

int cond_test_cleanup(void)
{
	ksu_policydb_destroy(&basemod);
	ksu_policydb_destroy(&base_expanded);

	return 0;
}

static void test_cond_expr_equal(void)
{
	cond_node_t *a, *b;

	a = base_expanded.cond_list;
	while (a) {
		b = base_expanded.cond_list;
		while (b) {
			if (a == b) {
				CU_ASSERT(cond_expr_equal(a, b));
			} else {
				CU_ASSERT(cond_expr_equal(a, b) == 0);
			}
			b = b->next;
		}
		a = a->next;
	}
}

int cond_add_tests(CU_pSuite suite)
{
	if (NULL == CU_add_test(suite, "cond_expr_equal", test_cond_expr_equal)) {
		return CU_get_error();
	}
	return 0;
}

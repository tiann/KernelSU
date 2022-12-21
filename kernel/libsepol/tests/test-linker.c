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

/* This is where the linker tests should go, including:
 * - check role, type, bool, user, attr mapping
 * - check for properly enabled optional
 * - check for properly disabled optional
 * - check for non-optional disabled blocks
 * - properly add symbols declared in optionals
 */

#include "test-linker.h"
#include "parse_util.h"
#include "helpers.h"
#include "test-common.h"
#include "test-linker-roles.h"
#include "test-linker-types.h"
#include "test-linker-cond-map.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/expand.h>
#include <limits.h>
#include <stdlib.h>

#define NUM_MODS 2
#define NUM_POLICIES NUM_MODS+1

#define BASEMOD NUM_MODS
const char *policies[NUM_POLICIES] = {
	"module1.conf",
	"module2.conf",
	"small-base.conf",
};

static policydb_t basenomods;
static policydb_t linkedbase;
static policydb_t *modules[NUM_MODS];
extern int mls;

int linker_test_init(void)
{
	int i;

	if (test_load_policy(&linkedbase, POLICY_BASE, mls, "test-linker", policies[BASEMOD]))
		return -1;

	if (test_load_policy(&basenomods, POLICY_BASE, mls, "test-linker", policies[BASEMOD]))
		return -1;

	for (i = 0; i < NUM_MODS; i++) {

		modules[i] = calloc(1, sizeof(*modules[i]));
		if (!modules[i]) {
			fprintf(stderr, "out of memory!\n");
			return -1;
		}

		if (test_load_policy(modules[i], POLICY_MOD, mls, "test-linker", policies[i]))
			return -1;

	}

	if (link_modules(NULL, &linkedbase, modules, NUM_MODS, 0)) {
		fprintf(stderr, "link modules failed\n");
		return -1;
	}

	if (link_modules(NULL, &basenomods, NULL, 0, 0)) {
		fprintf(stderr, "link modules failed\n");
		return -1;
	}

	return 0;
}

int linker_test_cleanup(void)
{
	int i;

	ksu_policydb_destroy(&basenomods);
	ksu_policydb_destroy(&linkedbase);

	for (i = 0; i < NUM_MODS; i++) {
		ksu_policydb_destroy(modules[i]);
		free(modules[i]);
	}
	return 0;
}

static void test_linker_indexes(void)
{
	test_policydb_indexes(&linkedbase);
}

static void test_linker_roles(void)
{
	base_role_tests(&basenomods);
	base_role_tests(&linkedbase);
	module_role_tests(&linkedbase);
}

static void test_linker_types(void)
{
	base_type_tests(&basenomods);
	base_type_tests(&linkedbase);
	module_type_tests(&linkedbase);
}

static void test_linker_cond(void)
{
	base_cond_tests(&basenomods);
	base_cond_tests(&linkedbase);
	module_cond_tests(&linkedbase);
}

int linker_add_tests(CU_pSuite suite)
{
	if (NULL == CU_add_test(suite, "linker_indexes", test_linker_indexes)) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	if (NULL == CU_add_test(suite, "linker_types", test_linker_types)) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	if (NULL == CU_add_test(suite, "linker_roles", test_linker_roles)) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	if (NULL == CU_add_test(suite, "linker_cond", test_linker_cond)) {
		CU_cleanup_registry();
		return CU_get_error();
	}
	return 0;
}

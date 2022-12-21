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

#include "test-deps.h"
#include "parse_util.h"
#include "helpers.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>

#include <stdlib.h>

/* Tests for dependency checking / handling, specifically:
 *
 * 1 type in module global.
 * 2 attribute in module global.
 * 3 object class / perm in module global.
 * 4 boolean in module global.
 * 5 role in module global.
 *
 * 6 type in module optional.
 * 7 attribute in module optional.
 * 8 object class / perm in module optional.
 * 9 boolean in module optional.
 * 10 role in module optional.
 *
 * 11 type in base optional.
 * 12 attribute in base optional.
 * 13 object class / perm in base optional.
 * 14 boolean in base optional.
 * 15 role in base optional.
 *
 * Each of these tests are done with the dependency met and not
 * met. Additionally, each of the required symbols is used in the
 * scope it is required.
 *
 * In addition to the simple tests, we have test with more complex
 * modules that test:
 *
 * 17 mutual dependencies between two modules.
 * 18 circular dependency between three modules.
 * 19 large number of dependencies in a module with a more complex base.
 * 20 nested optionals with requires.
 *
 * Again, each of these tests is done with the requirements met and not
 * met.
 */

#include <sepol/debug.h>
#include <sepol/handle.h>

#include "helpers.h"

#define BASE_MODREQ_TYPE_GLOBAL    0
#define BASE_MODREQ_ATTR_GLOBAL    1
#define BASE_MODREQ_OBJ_GLOBAL     2
#define BASE_MODREQ_BOOL_GLOBAL    3
#define BASE_MODREQ_ROLE_GLOBAL    4
#define BASE_MODREQ_PERM_GLOBAL    5
#define BASE_MODREQ_TYPE_OPT       6
#define BASE_MODREQ_ATTR_OPT       7
#define BASE_MODREQ_OBJ_OPT        8
#define BASE_MODREQ_BOOL_OPT       9
#define BASE_MODREQ_ROLE_OPT       10
#define BASE_MODREQ_PERM_OPT       11
#define NUM_BASES                  12

static policydb_t bases_met[NUM_BASES];
static policydb_t bases_notmet[NUM_BASES];

extern int mls;

int deps_test_init(void)
{
	int i;

	/* To test linking we need 1 base per link test and in
	 * order to load them in the init function we have
	 * to keep them all around. Not ideal, but it shouldn't
	 * matter too much.
	 */
	for (i = 0; i < NUM_BASES; i++) {
		if (test_load_policy(&bases_met[i], POLICY_BASE, mls, "test-deps", "base-metreq.conf"))
			return -1;
	}

	for (i = 0; i < NUM_BASES; i++) {
		if (test_load_policy(&bases_notmet[i], POLICY_BASE, mls, "test-deps", "base-notmetreq.conf"))
			return -1;
	}

	return 0;
}

int deps_test_cleanup(void)
{
	int i;

	for (i = 0; i < NUM_BASES; i++) {
		ksu_policydb_destroy(&bases_met[i]);
	}

	for (i = 0; i < NUM_BASES; i++) {
		ksu_policydb_destroy(&bases_notmet[i]);
	}

	return 0;
}

/* This function performs testing of the dependency handles for module global
 * symbols. It is capable of testing 2 scenarios - the dependencies are met
 * and the dependencies are not met.
 *
 * Parameters:
 *  req_met            boolean indicating whether the base policy meets the
 *                       requirements for the modules global block.
 *  b                  index of the base policy in the global bases_met array.
 *
 *  policy             name of the policy module to load for this test.
 *  decl_type          name of the unique type found in the module's global
 *                       section is to find that avrule_decl.
 */
static void do_deps_modreq_global(int req_met, int b, const char *policy, const char *decl_type)
{
	policydb_t *base;
	policydb_t mod;
	policydb_t *mods[] = { &mod };
	avrule_decl_t *decl;
	int ret, link_ret;
	sepol_handle_t *h;

	/* suppress error reporting - this is because we know that we
	 * are going to get errors and don't want libsepol complaining
	 * about it constantly. */
	h = sepol_handle_create();
	CU_ASSERT_FATAL(h != NULL);
	sepol_msg_set_callback(h, NULL, NULL);

	if (req_met) {
		base = &bases_met[b];
		link_ret = 0;
	} else {
		base = &bases_notmet[b];
		link_ret = -3;
	}

	CU_ASSERT_FATAL(test_load_policy(&mod, POLICY_MOD, mls, "test-deps", policy) == 0);

	/* link the modules and check for the correct return value.
	 */
	ret = link_modules(h, base, mods, 1, 0);
	CU_ASSERT_FATAL(ret == link_ret);
	ksu_policydb_destroy(&mod);
	sepol_handle_destroy(h);

	if (!req_met)
		return;

	decl = test_find_decl_by_sym(base, SYM_TYPES, decl_type);
	CU_ASSERT_FATAL(decl != NULL);

	CU_ASSERT(decl->enabled == 1);
}

/* Test that symbol require statements in the global scope of a module
 * work correctly. This will cover tests 1 - 5 (described above).
 *
 * Each of these policies will require as few symbols as possible to
 * use the required symbol in addition requiring (for example, the type
 * test also requires an object class for an allow rule).
 */
static void deps_modreq_global(void)
{
	/* object classes */
	do_deps_modreq_global(1, BASE_MODREQ_OBJ_GLOBAL, "modreq-obj-global.conf", "mod_global_t");
	do_deps_modreq_global(0, BASE_MODREQ_OBJ_GLOBAL, "modreq-obj-global.conf", "mod_global_t");
	/* types */
	do_deps_modreq_global(1, BASE_MODREQ_TYPE_GLOBAL, "modreq-type-global.conf", "mod_global_t");
	do_deps_modreq_global(0, BASE_MODREQ_TYPE_GLOBAL, "modreq-type-global.conf", "mod_global_t");
	/* attributes */
	do_deps_modreq_global(1, BASE_MODREQ_ATTR_GLOBAL, "modreq-attr-global.conf", "mod_global_t");
	do_deps_modreq_global(0, BASE_MODREQ_ATTR_GLOBAL, "modreq-attr-global.conf", "mod_global_t");
	/* booleans */
	do_deps_modreq_global(1, BASE_MODREQ_BOOL_GLOBAL, "modreq-bool-global.conf", "mod_global_t");
	do_deps_modreq_global(0, BASE_MODREQ_BOOL_GLOBAL, "modreq-bool-global.conf", "mod_global_t");
	/* roles */
	do_deps_modreq_global(1, BASE_MODREQ_ROLE_GLOBAL, "modreq-role-global.conf", "mod_global_t");
	do_deps_modreq_global(0, BASE_MODREQ_ROLE_GLOBAL, "modreq-role-global.conf", "mod_global_t");
	do_deps_modreq_global(1, BASE_MODREQ_PERM_GLOBAL, "modreq-perm-global.conf", "mod_global_t");
	do_deps_modreq_global(0, BASE_MODREQ_PERM_GLOBAL, "modreq-perm-global.conf", "mod_global_t");
}

/* This function performs testing of the dependency handles for module optional
 * symbols. It is capable of testing 2 scenarios - the dependencies are met
 * and the dependencies are not met.
 *
 * Parameters:
 *  req_met            boolean indicating whether the base policy meets the
 *                       requirements for the modules global block.
 *  b                  index of the base policy in the global bases_met array.
 *
 *  policy             name of the policy module to load for this test.
 *  decl_type          name of the unique type found in the module's global
 *                       section is to find that avrule_decl.
 */
static void do_deps_modreq_opt(int req_met, int ret_val, int b, const char *policy, const char *decl_type)
{
	policydb_t *base;
	policydb_t mod;
	policydb_t *mods[] = { &mod };
	avrule_decl_t *decl;
	int ret;
	sepol_handle_t *h;

	/* suppress error reporting - this is because we know that we
	 * are going to get errors and don't want libsepol complaining
	 * about it constantly. */
	h = sepol_handle_create();
	CU_ASSERT_FATAL(h != NULL);
	sepol_msg_set_callback(h, NULL, NULL);

	if (req_met) {
		base = &bases_met[b];
	} else {
		base = &bases_notmet[b];
	}

	CU_ASSERT_FATAL(test_load_policy(&mod, POLICY_MOD, mls, "test-deps", policy) == 0);

	/* link the modules and check for the correct return value.
	 */
	ret = link_modules(h, base, mods, 1, 0);
	CU_ASSERT_FATAL(ret == ret_val);
	ksu_policydb_destroy(&mod);
	sepol_handle_destroy(h);
	if (ret_val < 0)
		return;

	decl = test_find_decl_by_sym(base, SYM_TYPES, decl_type);
	CU_ASSERT_FATAL(decl != NULL);

	if (req_met) {
		CU_ASSERT(decl->enabled == 1);
	} else {
		CU_ASSERT(decl->enabled == 0);
	}
}

/* Test that symbol require statements in the global scope of a module
 * work correctly. This will cover tests 6 - 10 (described above).
 *
 * Each of these policies will require as few symbols as possible to
 * use the required symbol in addition requiring (for example, the type
 * test also requires an object class for an allow rule).
 */
static void deps_modreq_opt(void)
{
	/* object classes */
	do_deps_modreq_opt(1, 0, BASE_MODREQ_OBJ_OPT, "modreq-obj-opt.conf", "mod_opt_t");
	do_deps_modreq_opt(0, 0, BASE_MODREQ_OBJ_OPT, "modreq-obj-opt.conf", "mod_opt_t");
	/* types */
	do_deps_modreq_opt(1, 0, BASE_MODREQ_TYPE_OPT, "modreq-type-opt.conf", "mod_opt_t");
	do_deps_modreq_opt(0, 0, BASE_MODREQ_TYPE_OPT, "modreq-type-opt.conf", "mod_opt_t");
	/* attributes */
	do_deps_modreq_opt(1, 0, BASE_MODREQ_ATTR_OPT, "modreq-attr-opt.conf", "mod_opt_t");
	do_deps_modreq_opt(0, 0, BASE_MODREQ_ATTR_OPT, "modreq-attr-opt.conf", "mod_opt_t");
	/* booleans */
	do_deps_modreq_opt(1, 0, BASE_MODREQ_BOOL_OPT, "modreq-bool-opt.conf", "mod_opt_t");
	do_deps_modreq_opt(0, 0, BASE_MODREQ_BOOL_OPT, "modreq-bool-opt.conf", "mod_opt_t");
	/* roles */
	do_deps_modreq_opt(1, 0, BASE_MODREQ_ROLE_OPT, "modreq-role-opt.conf", "mod_opt_t");
	do_deps_modreq_opt(0, 0, BASE_MODREQ_ROLE_OPT, "modreq-role-opt.conf", "mod_opt_t");
	/* permissions */
	do_deps_modreq_opt(1, 0, BASE_MODREQ_PERM_OPT, "modreq-perm-opt.conf", "mod_opt_t");
	do_deps_modreq_opt(0, -3, BASE_MODREQ_PERM_OPT, "modreq-perm-opt.conf", "mod_opt_t");
}

int deps_add_tests(CU_pSuite suite)
{
	if (NULL == CU_add_test(suite, "deps_modreq_global", deps_modreq_global)) {
		return CU_get_error();
	}

	if (NULL == CU_add_test(suite, "deps_modreq_opt", deps_modreq_opt)) {
		return CU_get_error();
	}

	return 0;
}

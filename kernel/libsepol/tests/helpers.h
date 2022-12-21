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

#ifndef __COMMON_H__
#define __COMMON_H__

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include <CUnit/Basic.h>

/* helper functions */

/* Override CU_*_FATAL() in order to help static analyzers by really asserting that an assertion holds */
#ifdef __CHECKER__

#include <assert.h>

#undef CU_ASSERT_FATAL
#define CU_ASSERT_FATAL(value) do { \
		int _value = (value); \
		CU_ASSERT(_value); \
		assert(_value); \
	} while (0)

#undef CU_FAIL_FATAL
#define CU_FAIL_FATAL(msg) do { \
		CU_FAIL(msg); \
		assert(0); \
	} while (0)

#undef CU_ASSERT_PTR_NOT_NULL_FATAL
#define CU_ASSERT_PTR_NOT_NULL_FATAL(value) do { \
		const void *_value = (value); \
		CU_ASSERT_PTR_NOT_NULL(_value); \
		assert(_value != NULL); \
	} while (0)

#endif /* __CHECKER__ */


/* Load a source policy into p. policydb_init will called within this function.
 * 
 * Example: test_load_policy(p, POLICY_BASE, 1, "foo", "base.conf") will load the
 *  policy "policies/foo/mls/base.conf" into p.
 *
 * Arguments:
 *  p            policydb_t into which the policy will be read. This should be
 *                malloc'd but not passed to policydb_init.
 *  policy_type  Type of policy expected - POLICY_BASE or POLICY_MOD.
 *  mls          Boolean value indicating whether an mls policy is expected.
 *  test_name    Name of the test which will be the name of the directory in
 *                which the policies are stored.
 *  policy_name  Name of the policy in the directory.
 *
 * Returns:
 *  0            success
 * -1            error - the policydb will be destroyed but not freed.
 */
extern int test_load_policy(policydb_t * p, int policy_type, int mls, const char *test_name, const char *policy_name);

/* Find an avrule_decl_t by a unique symbol. If the symbol is declared in more
 * than one decl an error is returned.
 *
 * Returns:
 *  decl      success 
 *  NULL      error (including more than one declaration)
 */
extern avrule_decl_t *test_find_decl_by_sym(policydb_t * p, int symtab, const char *sym);

#endif

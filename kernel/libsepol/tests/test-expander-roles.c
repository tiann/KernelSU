/*
 * Authors: Chad Sellers <csellers@tresys.com>
 *          Joshua Brindle <jbrindle@tresys.com>
 *          Chris PeBenito <cpebenito@tresys.com>
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

#include "test-expander-roles.h"
#include "test-common.h"
#include "helpers.h"

#include <sepol/policydb/policydb.h>
#include <CUnit/Basic.h>
#include <stdlib.h>

extern policydb_t role_expanded;

void test_expander_role_mapping(void)
{
	const char *types1[] = { "role_check_1_1_t", "role_check_1_2_t" };

	test_role_type_set(&role_expanded, "role_check_1", NULL, types1, 2, 0);
}

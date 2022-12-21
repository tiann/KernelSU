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

#ifndef __TEST_COMMON_H__
#define __TEST_COMMON_H__

#include <sepol/policydb/policydb.h>

/* p		the policy being inspected
 * id		string symbol identifier
 * sym_type	symbol type (eg., SYM_ROLES, SYM_TYPES)
 * scope_type	what scope the role should have (eg., SCOPE_DECL or SCOPE_REQ)
 * decls	integer array of decl id's that we expect the role to have in the scope table
 * len		number of elements in decls
 * 
 * This is a utility function to test for the symbol's presence in the global symbol table, 
 * the scope table, and that the decl blocks we think this symbol is in are correct
 */
extern void test_sym_presence(policydb_t * p, const char *id, int sym_type, unsigned int scope_type, unsigned int *decls, unsigned int len);

/* Test the indexes in the policydb to ensure their correctness. These include
 * the sym_val_to_name[], class_val_to_struct, role_val_to_struct, type_val_to_struct,
 * user_val_to_struct, and bool_val_to_struct indexes.
 */
extern void test_policydb_indexes(policydb_t * p);

/* Test alias datum to ensure that it is as expected
 *
 * id = the key for the alias
 * primary_id = the key for its primary
 * mode: 0 = test the datum according to the flavor value in the call
         1 = automatically detect the flavor value and test the datum accordingly
 * flavor = flavor value if in mode 0
 */
extern void test_alias_datum(policydb_t * p, const char *id, const char *primary_id, char mode, unsigned int flavor);

/* p		the policy being inspected
 * id		string role identifier
 * decl		the decl block which we are looking in for the role datum
 * types	the array of string types which we expect the role has in its type ebitmap
 * len		number of elements in types
 * flags	the expected flags in the role typeset (eg., * or ~)
 *
 * This is a utility function to test whether the type set associated with a role in a specific
 * avrule decl block matches our expectations
 */
extern role_datum_t *test_role_type_set(policydb_t * p, const char *id, avrule_decl_t * decl, const char **types, unsigned int len, unsigned int flags);

/* p		the policy being inspected
 * id		string attribute identifier
 * decl		the decl block which we are looking in for the attribute datum
 * types	the array of string types which we expect the attribute has in its type ebitmap
 * len		number of elements in types
 *
 * This is a utility function to test whether the type set associated with an attribute in a specific
 * avrule decl block matches our expectations 
 */
extern void test_attr_types(policydb_t * p, const char *id, avrule_decl_t * decl, const char **types, int len);

#endif

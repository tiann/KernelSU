/* Authors: Jason Tang <jtang@tresys.com>
 *
 * Copyright (C) 2005 Tresys Technology, LLC
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

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/constraint.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/flask_types.h>

// #include <assert.h>
// #include <stdlib.h>
#include "kernel.h"

int constraint_expr_init(constraint_expr_t * expr)
{
	memset(expr, 0, sizeof(*expr));
	ebitmap_init(&expr->names);
	if ((expr->type_names = malloc(sizeof(*expr->type_names))) == NULL) {
		return -1;
	}
	type_set_init(expr->type_names);
	return 0;
}

void constraint_expr_destroy(constraint_expr_t * expr)
{
	constraint_expr_t *next;

	while (expr != NULL) {
		next = expr->next;
		ksu_ebitmap_destroy(&expr->names);
		type_set_destroy(expr->type_names);
		free(expr->type_names);
		free(expr);
		expr = next;
	}
}

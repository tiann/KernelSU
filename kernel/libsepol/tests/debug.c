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

/* This includes functions used to debug tests (display bitmaps, conditional expressions, etc */

#include "debug.h"

#include <stdlib.h>

void print_ebitmap(ebitmap_t * bitmap, FILE * fp)
{
	uint32_t i;
	for (i = 0; i < bitmap->highbit; i++) {
		fprintf(fp, "%d", ksu_ebitmap_get_bit(bitmap, i));
	}
	fprintf(fp, "\n");
}

/* stolen from dispol.c */
void display_expr(policydb_t * p, cond_expr_t * exp, FILE * fp)
{

	cond_expr_t *cur;
	for (cur = exp; cur != NULL; cur = cur->next) {
		switch (cur->expr_type) {
		case COND_BOOL:
			fprintf(fp, "%s ", p->p_bool_val_to_name[cur->bool - 1]);
			break;
		case COND_NOT:
			fprintf(fp, "! ");
			break;
		case COND_OR:
			fprintf(fp, "|| ");
			break;
		case COND_AND:
			fprintf(fp, "&& ");
			break;
		case COND_XOR:
			fprintf(fp, "^ ");
			break;
		case COND_EQ:
			fprintf(fp, "== ");
			break;
		case COND_NEQ:
			fprintf(fp, "!= ");
			break;
		default:
			fprintf(fp, "error! (%d)", cur->expr_type);
			break;
		}
	}
}

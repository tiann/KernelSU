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

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>

extern void print_ebitmap(ebitmap_t * bitmap, FILE * fp);
extern void display_expr(policydb_t * p, cond_expr_t * exp, FILE * fp);

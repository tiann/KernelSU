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

/* This has helper functions that are common between tests */

#include "helpers.h"
#include "parse_util.h"

#include <sepol/policydb/expand.h>
#include <sepol/policydb/avrule_block.h>

#include <CUnit/Basic.h>

#include <stdlib.h>
#include <limits.h>

int test_load_policy(policydb_t * p, int policy_type, int mls, const char *test_name, const char *policy_name)
{
	char filename[PATH_MAX];

	if (mls) {
		if (snprintf(filename, PATH_MAX, "policies/%s/%s.mls", test_name, policy_name) < 0) {
			return -1;
		}
	} else {
		if (snprintf(filename, PATH_MAX, "policies/%s/%s.std", test_name, policy_name) < 0) {
			return -1;
		}
	}

	if (policydb_init(p)) {
		fprintf(stderr, "Out of memory");
		return -1;
	}

	p->policy_type = policy_type;
	p->mls = mls;

	if (read_source_policy(p, filename, test_name)) {
		fprintf(stderr, "failed to read policy %s\n", filename);
		ksu_policydb_destroy(p);
		return -1;
	}

	return 0;
}

avrule_decl_t *test_find_decl_by_sym(policydb_t * p, int symtab, const char *sym)
{
	scope_datum_t *scope = (scope_datum_t *) hashtab_search(p->scope[symtab].table, sym);

	if (scope == NULL) {
		return NULL;
	}
	if (scope->scope != SCOPE_DECL) {
		return NULL;
	}
	if (scope->decl_ids_len != 1) {
		return NULL;
	}

	return p->decl_val_to_struct[scope->decl_ids[0] - 1];
}


/* Author : Stephen Smalley, <sds@tycho.nsa.gov> */

/*
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 *
 *	Support for enhanced MLS infrastructure.
 *
 * Updated: Frank Mayer <mayerf@tresys.com> and Karl MacMillan <kmacmillan@tresys.com>
 *
 * 	Added conditional policy language extensions
 * 
 * Updated: Red Hat, Inc.  James Morris <jmorris@redhat.com>
 *      Fine-grained netlink support
 *      IPv6 support
 *      Code cleanup
 *
 * Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
 * Copyright (C) 2003 - 2005 Tresys Technology, LLC
 * Copyright (C) 2003 - 2007 Red Hat, Inc.
 * Copyright (C) 2017 Mellanox Technologies Inc.
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

/* FLASK */

/*
 * Implementation of the policy database.
 */

// #include <assert.h>
// #include <stdlib.h>

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/avrule_block.h>
#include <sepol/policydb/util.h>

#include "kernel_to_common.h"
#include "private.h"
#include "debug.h"
#include "mls.h"
#include "policydb_validate.h"

#define POLICYDB_TARGET_SZ   ARRAY_SIZE(policydb_target_strings)
const char * const policydb_target_strings[] = { POLICYDB_STRING, POLICYDB_XEN_STRING };

/* These need to be updated if SYM_NUM or OCON_NUM changes */
static const struct policydb_compat_info policydb_compat[] = {
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_BOUNDARY,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_XEN_PCIDEVICE + 1,
	 .target_platform = SEPOL_TARGET_XEN,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_XEN_DEVICETREE,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_XEN_DEVICETREE + 1,
	 .target_platform = SEPOL_TARGET_XEN,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_BASE,
	 .sym_num = SYM_NUM - 3,
	 .ocon_num = OCON_FSUSE + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_BOOL,
	 .sym_num = SYM_NUM - 2,
	 .ocon_num = OCON_FSUSE + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_IPV6,
	 .sym_num = SYM_NUM - 2,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_NLCLASS,
	 .sym_num = SYM_NUM - 2,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_MLS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_AVTAB,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_RANGETRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_POLCAP,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_PERMISSIVE,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
        {
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_BOUNDARY,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_FILENAME_TRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_ROLETRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_NEW_OBJECT_DEFAULTS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_DEFAULT_TYPE,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_CONSTRAINT_NAMES,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_XPERMS_IOCTL,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_INFINIBAND,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_IBENDPORT + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_GLBLUB,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_IBENDPORT + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_KERN,
	 .version = POLICYDB_VERSION_COMP_FTRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_IBENDPORT + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_BASE,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_MLS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_MLS_USERS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_POLCAP,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_PERMISSIVE,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_BOUNDARY,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_BOUNDARY_ALIAS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_FILENAME_TRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_ROLETRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_ROLEATTRIB,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_TUNABLE_SEP,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_NEW_OBJECT_DEFAULTS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_DEFAULT_TYPE,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_CONSTRAINT_NAMES,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_XPERMS_IOCTL,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_NODE6 + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_INFINIBAND,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_IBENDPORT + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_GLBLUB,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_IBENDPORT + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_BASE,
	 .version = MOD_POLICYDB_VERSION_SELF_TYPETRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = OCON_IBENDPORT + 1,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_BASE,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_MLS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_MLS_USERS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_POLCAP,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_PERMISSIVE,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	 },
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_BOUNDARY,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_BOUNDARY_ALIAS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_FILENAME_TRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_ROLETRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_ROLEATTRIB,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_TUNABLE_SEP,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_NEW_OBJECT_DEFAULTS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_DEFAULT_TYPE,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_CONSTRAINT_NAMES,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_XPERMS_IOCTL,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_INFINIBAND,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_GLBLUB,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
	{
	 .type = POLICY_MOD,
	 .version = MOD_POLICYDB_VERSION_SELF_TYPETRANS,
	 .sym_num = SYM_NUM,
	 .ocon_num = 0,
	 .target_platform = SEPOL_TARGET_SELINUX,
	},
};

#if 0
static char *symtab_name[SYM_NUM] = {
	"common prefixes",
	"classes",
	"roles",
	"types",
	"users",
	"bools" mls_symtab_names cond_symtab_names
};
#endif

static const unsigned int symtab_sizes[SYM_NUM] = {
	2,
	32,
	16,
	512,
	128,
	16,
	16,
	16,
};

const struct policydb_compat_info *policydb_lookup_compat(unsigned int version,
						          unsigned int type,
						          unsigned int target_platform)
{
	unsigned int i;
	const struct policydb_compat_info *info = NULL;

	for (i = 0; i < sizeof(policydb_compat) / sizeof(*info); i++) {
		if (policydb_compat[i].version == version &&
		    policydb_compat[i].type == type &&
		    policydb_compat[i].target_platform == target_platform) {
			info = &policydb_compat[i];
			break;
		}
	}
	return info;
}

void type_set_init(type_set_t * x)
{
	memset(x, 0, sizeof(type_set_t));
	ebitmap_init(&x->types);
	ebitmap_init(&x->negset);
}

void type_set_destroy(type_set_t * x)
{
	if (x != NULL) {
		ksu_ebitmap_destroy(&x->types);
		ksu_ebitmap_destroy(&x->negset);
	}
}

void role_set_init(role_set_t * x)
{
	memset(x, 0, sizeof(role_set_t));
	ebitmap_init(&x->roles);
}

void role_set_destroy(role_set_t * x)
{
	ksu_ebitmap_destroy(&x->roles);
}

void role_datum_init(role_datum_t * x)
{
	memset(x, 0, sizeof(role_datum_t));
	ebitmap_init(&x->dominates);
	type_set_init(&x->types);
	ebitmap_init(&x->cache);
	ebitmap_init(&x->roles);
}

void role_datum_destroy(role_datum_t * x)
{
	if (x != NULL) {
		ksu_ebitmap_destroy(&x->dominates);
		type_set_destroy(&x->types);
		ksu_ebitmap_destroy(&x->cache);
		ksu_ebitmap_destroy(&x->roles);
	}
}

void type_datum_init(type_datum_t * x)
{
	memset(x, 0, sizeof(*x));
	ebitmap_init(&x->types);
}

void type_datum_destroy(type_datum_t * x)
{
	if (x != NULL) {
		ksu_ebitmap_destroy(&x->types);
	}
}

void user_datum_init(user_datum_t * x)
{
	memset(x, 0, sizeof(user_datum_t));
	role_set_init(&x->roles);
	mls_semantic_range_init(&x->range);
	mls_semantic_level_init(&x->dfltlevel);
	ebitmap_init(&x->cache);
	mls_range_init(&x->exp_range);
	mls_level_init(&x->exp_dfltlevel);
}

void user_datum_destroy(user_datum_t * x)
{
	if (x != NULL) {
		role_set_destroy(&x->roles);
		mls_semantic_range_destroy(&x->range);
		mls_semantic_level_destroy(&x->dfltlevel);
		ksu_ebitmap_destroy(&x->cache);
		mls_range_destroy(&x->exp_range);
		mls_level_destroy(&x->exp_dfltlevel);
	}
}

void level_datum_init(level_datum_t * x)
{
	memset(x, 0, sizeof(level_datum_t));
}

void level_datum_destroy(level_datum_t * x __attribute__ ((unused)))
{
	/* the mls_level_t referenced by the level_datum is managed
	 * separately for now, so there is nothing to destroy */
	return;
}

void cat_datum_init(cat_datum_t * x)
{
	memset(x, 0, sizeof(cat_datum_t));
}

void cat_datum_destroy(cat_datum_t * x __attribute__ ((unused)))
{
	/* it's currently a simple struct - really nothing to destroy */
	return;
}

void class_perm_node_init(class_perm_node_t * x)
{
	memset(x, 0, sizeof(class_perm_node_t));
}

void avrule_init(avrule_t * x)
{
	memset(x, 0, sizeof(avrule_t));
	type_set_init(&x->stypes);
	type_set_init(&x->ttypes);
}

void avrule_destroy(avrule_t * x)
{
	class_perm_node_t *cur, *next;

	if (x == NULL) {
		return;
	}
	type_set_destroy(&x->stypes);
	type_set_destroy(&x->ttypes);

	free(x->source_filename);

	next = x->perms;
	while (next) {
		cur = next;
		next = cur->next;
		free(cur);
	}

	free(x->xperms);
}

void role_trans_rule_init(role_trans_rule_t * x)
{
	memset(x, 0, sizeof(*x));
	role_set_init(&x->roles);
	type_set_init(&x->types);
	ebitmap_init(&x->classes);
}

static void role_trans_rule_destroy(role_trans_rule_t * x)
{
	if (x != NULL) {
		role_set_destroy(&x->roles);
		type_set_destroy(&x->types);
		ksu_ebitmap_destroy(&x->classes);
	}
}

void role_trans_rule_list_destroy(role_trans_rule_t * x)
{
	while (x != NULL) {
		role_trans_rule_t *next = x->next;
		role_trans_rule_destroy(x);
		free(x);
		x = next;
	}
}

void filename_trans_rule_init(filename_trans_rule_t * x)
{
	memset(x, 0, sizeof(*x));
	type_set_init(&x->stypes);
	type_set_init(&x->ttypes);
}

static void filename_trans_rule_destroy(filename_trans_rule_t * x)
{
	if (!x)
		return;
	type_set_destroy(&x->stypes);
	type_set_destroy(&x->ttypes);
	free(x->name);
}

void filename_trans_rule_list_destroy(filename_trans_rule_t * x)
{
	filename_trans_rule_t *next;
	while (x) {
		next = x->next;
		filename_trans_rule_destroy(x);
		free(x);
		x = next;
	}
}

void role_allow_rule_init(role_allow_rule_t * x)
{
	memset(x, 0, sizeof(role_allow_rule_t));
	role_set_init(&x->roles);
	role_set_init(&x->new_roles);
}

void role_allow_rule_destroy(role_allow_rule_t * x)
{
	role_set_destroy(&x->roles);
	role_set_destroy(&x->new_roles);
}

void role_allow_rule_list_destroy(role_allow_rule_t * x)
{
	while (x != NULL) {
		role_allow_rule_t *next = x->next;
		role_allow_rule_destroy(x);
		free(x);
		x = next;
	}
}

void range_trans_rule_init(range_trans_rule_t * x)
{
	type_set_init(&x->stypes);
	type_set_init(&x->ttypes);
	ebitmap_init(&x->tclasses);
	mls_semantic_range_init(&x->trange);
	x->next = NULL;
}

void range_trans_rule_destroy(range_trans_rule_t * x)
{
	type_set_destroy(&x->stypes);
	type_set_destroy(&x->ttypes);
	ksu_ebitmap_destroy(&x->tclasses);
	mls_semantic_range_destroy(&x->trange);
}

void range_trans_rule_list_destroy(range_trans_rule_t * x)
{
	while (x != NULL) {
		range_trans_rule_t *next = x->next;
		range_trans_rule_destroy(x);
		free(x);
		x = next;
	}
}

void avrule_list_destroy(avrule_t * x)
{
	avrule_t *next, *cur;

	if (!x)
		return;

	next = x;
	while (next) {
		cur = next;
		next = next->next;
		avrule_destroy(cur);
		free(cur);
	}
}

/* 
 * Initialize the role table by implicitly adding role 'object_r'.  If
 * the policy is a module, set object_r's scope to be SCOPE_REQ,
 * otherwise set it to SCOPE_DECL.
 */
static int roles_init(policydb_t * p)
{
	char *key = 0;
	int rc;
	role_datum_t *role;

	role = calloc(1, sizeof(role_datum_t));
	if (!role) {
		rc = -ENOMEM;
		goto out;
	}
	key = malloc(strlen(OBJECT_R) + 1);
	if (!key) {
		rc = -ENOMEM;
		goto out_free_role;
	}
	strcpy(key, OBJECT_R);
	rc = ksu_symtab_insert(p, SYM_ROLES, key, role,
			   (p->policy_type ==
			    POLICY_MOD ? SCOPE_REQ : SCOPE_DECL), 1,
			   &role->s.value);
	if (rc)
		goto out_free_key;
	if (role->s.value != OBJECT_R_VAL) {
		rc = -EINVAL;
		goto out_free_role;
	}
      out:
	return rc;

      out_free_key:
	free(key);
      out_free_role:
	free(role);
	goto out;
}

ignore_unsigned_overflow_
static inline unsigned long
partial_name_hash(unsigned long c, unsigned long prevhash)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}

static unsigned int filenametr_hash(hashtab_t h, const_hashtab_key_t k)
{
	const filename_trans_key_t *ft = (const filename_trans_key_t *)k;
	unsigned long hash;
	unsigned int byte_num;
	unsigned char focus;

	hash = ft->ttype ^ ft->tclass;

	byte_num = 0;
	while ((focus = ft->name[byte_num++]))
		hash = partial_name_hash(focus, hash);
	return hash & (h->size - 1);
}

static int filenametr_cmp(hashtab_t h __attribute__ ((unused)),
			  const_hashtab_key_t k1, const_hashtab_key_t k2)
{
	const filename_trans_key_t *ft1 = (const filename_trans_key_t *)k1;
	const filename_trans_key_t *ft2 = (const filename_trans_key_t *)k2;
	int v;

	v = spaceship_cmp(ft1->ttype, ft2->ttype);
	if (v)
		return v;

	v = spaceship_cmp(ft1->tclass, ft2->tclass);
	if (v)
		return v;

	return strcmp(ft1->name, ft2->name);

}

static unsigned int rangetr_hash(hashtab_t h, const_hashtab_key_t k)
{
	const struct range_trans *key = (const struct range_trans *)k;
	return (key->source_type + (key->target_type << 3) +
		(key->target_class << 5)) & (h->size - 1);
}

static int rangetr_cmp(hashtab_t h __attribute__ ((unused)),
		       const_hashtab_key_t k1, const_hashtab_key_t k2)
{
	const struct range_trans *key1 = (const struct range_trans *)k1;
	const struct range_trans *key2 = (const struct range_trans *)k2;
	int v;

	v = spaceship_cmp(key1->source_type, key2->source_type);
	if (v)
		return v;

	v = spaceship_cmp(key1->target_type, key2->target_type);
	if (v)
		return v;

	v = spaceship_cmp(key1->target_class, key2->target_class);

	return v;
}

/*
 * Initialize a policy database structure.
 */
int policydb_init(policydb_t * p)
{
	int i, rc;

	memset(p, 0, sizeof(policydb_t));

	for (i = 0; i < SYM_NUM; i++) {
		p->sym_val_to_name[i] = NULL;
		rc = ksu_symtab_init(&p->symtab[i], symtab_sizes[i]);
		if (rc)
			goto err;
	}

	/* initialize the module stuff */
	for (i = 0; i < SYM_NUM; i++) {
		if (ksu_symtab_init(&p->scope[i], symtab_sizes[i])) {
			goto err;
		}
	}
	if ((p->global = avrule_block_create()) == NULL ||
	    (p->global->branch_list = avrule_decl_create(1)) == NULL) {
		goto err;
	}
	p->decl_val_to_struct = NULL;

	rc = ksu_avtab_init(&p->te_avtab);
	if (rc)
		goto err;

	rc = roles_init(p);
	if (rc)
		goto err;

	rc = ksu_cond_policydb_init(p);
	if (rc)
		goto err;

	p->filename_trans = hashtab_create(filenametr_hash, filenametr_cmp, (1 << 10));
	if (!p->filename_trans) {
		rc = -ENOMEM;
		goto err;
	}

	p->range_tr = hashtab_create(rangetr_hash, rangetr_cmp, 256);
	if (!p->range_tr) {
		rc = -ENOMEM;
		goto err;
	}

	ebitmap_init(&p->policycaps);
	ebitmap_init(&p->permissive_map);

	return 0;
err:
	ksu_hashtab_destroy(p->filename_trans);
	ksu_hashtab_destroy(p->range_tr);
	for (i = 0; i < SYM_NUM; i++) {
		ksu_hashtab_destroy(p->symtab[i].table);
		ksu_hashtab_destroy(p->scope[i].table);
	}
	avrule_block_list_destroy(p->global);
	return rc;
}

int policydb_role_cache(hashtab_key_t key
			__attribute__ ((unused)), hashtab_datum_t datum,
			void *arg)
{
	policydb_t *p;
	role_datum_t *role;

	role = (role_datum_t *) datum;
	p = (policydb_t *) arg;

	ksu_ebitmap_destroy(&role->cache);
	if (type_set_expand(&role->types, &role->cache, p, 1)) {
		return -1;
	}

	return 0;
}

int policydb_user_cache(hashtab_key_t key
			__attribute__ ((unused)), hashtab_datum_t datum,
			void *arg)
{
	policydb_t *p;
	user_datum_t *user;

	user = (user_datum_t *) datum;
	p = (policydb_t *) arg;

	ksu_ebitmap_destroy(&user->cache);
	if (role_set_expand(&user->roles, &user->cache, p, NULL, NULL)) {
		return -1;
	}

	/* we do not expand user's MLS info in kernel policies because the
	 * semantic representation is not present and we do not expand user's
	 * MLS info in module policies because all of the necessary mls
	 * information is not present */
	if (p->policy_type != POLICY_KERN && p->policy_type != POLICY_MOD) {
		mls_range_destroy(&user->exp_range);
		if (mls_semantic_range_expand(&user->range,
					      &user->exp_range, p, NULL)) {
			return -1;
		}

		mls_level_destroy(&user->exp_dfltlevel);
		if (mls_semantic_level_expand(&user->dfltlevel,
					      &user->exp_dfltlevel, p, NULL)) {
			return -1;
		}
	}

	return 0;
}

/*
 * The following *_index functions are used to
 * define the val_to_name and val_to_struct arrays
 * in a policy database structure.  The val_to_name
 * arrays are used when converting security context
 * structures into string representations.  The
 * val_to_struct arrays are used when the attributes
 * of a class, role, or user are needed.
 */

static int common_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	common_datum_t *comdatum;

	comdatum = (common_datum_t *) datum;
	p = (policydb_t *) datap;
	if (!value_isvalid(comdatum->s.value, p->p_commons.nprim))
		return -EINVAL;
	if (p->p_common_val_to_name[comdatum->s.value - 1] != NULL)
		return -EINVAL;
	p->p_common_val_to_name[comdatum->s.value - 1] = (char *)key;

	return 0;
}

static int class_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	class_datum_t *cladatum;

	cladatum = (class_datum_t *) datum;
	p = (policydb_t *) datap;
	if (!value_isvalid(cladatum->s.value, p->p_classes.nprim))
		return -EINVAL;
	if (p->p_class_val_to_name[cladatum->s.value - 1] != NULL)
		return -EINVAL;
	p->p_class_val_to_name[cladatum->s.value - 1] = (char *)key;
	p->class_val_to_struct[cladatum->s.value - 1] = cladatum;

	return 0;
}

static int role_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	role_datum_t *role;

	role = (role_datum_t *) datum;
	p = (policydb_t *) datap;
	if (!value_isvalid(role->s.value, p->p_roles.nprim))
		return -EINVAL;
	if (p->p_role_val_to_name[role->s.value - 1] != NULL)
		return -EINVAL;
	p->p_role_val_to_name[role->s.value - 1] = (char *)key;
	p->role_val_to_struct[role->s.value - 1] = role;

	return 0;
}

static int type_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	type_datum_t *typdatum;

	typdatum = (type_datum_t *) datum;
	p = (policydb_t *) datap;

	if (typdatum->primary) {
		if (!value_isvalid(typdatum->s.value, p->p_types.nprim))
			return -EINVAL;
		if (p->p_type_val_to_name[typdatum->s.value - 1] != NULL)
			return -EINVAL;
		p->p_type_val_to_name[typdatum->s.value - 1] = (char *)key;
		p->type_val_to_struct[typdatum->s.value - 1] = typdatum;
	}

	return 0;
}

static int user_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	user_datum_t *usrdatum;

	usrdatum = (user_datum_t *) datum;
	p = (policydb_t *) datap;

	if (!value_isvalid(usrdatum->s.value, p->p_users.nprim))
		return -EINVAL;
	if (p->p_user_val_to_name[usrdatum->s.value - 1] != NULL)
		return -EINVAL;
	p->p_user_val_to_name[usrdatum->s.value - 1] = (char *)key;
	p->user_val_to_struct[usrdatum->s.value - 1] = usrdatum;

	return 0;
}

static int sens_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	level_datum_t *levdatum;

	levdatum = (level_datum_t *) datum;
	p = (policydb_t *) datap;

	if (!levdatum->isalias) {
		if (!value_isvalid(levdatum->level->sens, p->p_levels.nprim))
			return -EINVAL;
		if (p->p_sens_val_to_name[levdatum->level->sens - 1] != NULL)
			return -EINVAL;
		p->p_sens_val_to_name[levdatum->level->sens - 1] = (char *)key;
	}

	return 0;
}

static int cat_index(hashtab_key_t key, hashtab_datum_t datum, void *datap)
{
	policydb_t *p;
	cat_datum_t *catdatum;

	catdatum = (cat_datum_t *) datum;
	p = (policydb_t *) datap;

	if (!catdatum->isalias) {
		if (!value_isvalid(catdatum->s.value, p->p_cats.nprim))
			return -EINVAL;
		if (p->p_cat_val_to_name[catdatum->s.value - 1] != NULL)
			return -EINVAL;
		p->p_cat_val_to_name[catdatum->s.value - 1] = (char *)key;
	}

	return 0;
}

static int (*index_f[SYM_NUM]) (hashtab_key_t key, hashtab_datum_t datum,
				void *datap) = {
common_index, class_index, role_index, type_index, user_index,
	    ksu_cond_index_bool, sens_index, cat_index,};

/*
 * Define the common val_to_name array and the class
 * val_to_name and val_to_struct arrays in a policy
 * database structure.  
 */
int policydb_index_classes(policydb_t * p)
{
	free(p->p_common_val_to_name);
	p->p_common_val_to_name = (char **)
	    calloc(p->p_commons.nprim, sizeof(char *));
	if (!p->p_common_val_to_name)
		return -1;

	if (ksu_hashtab_map(p->p_commons.table, common_index, p))
		return -1;

	free(p->class_val_to_struct);
	p->class_val_to_struct = (class_datum_t **)
	    calloc(p->p_classes.nprim, sizeof(class_datum_t *));
	if (!p->class_val_to_struct)
		return -1;

	free(p->p_class_val_to_name);
	p->p_class_val_to_name = (char **)
	    calloc(p->p_classes.nprim, sizeof(char *));
	if (!p->p_class_val_to_name)
		return -1;

	if (ksu_hashtab_map(p->p_classes.table, class_index, p))
		return -1;

	return 0;
}

int policydb_index_bools(policydb_t * p)
{

	if (ksu_cond_init_bool_indexes(p) == -1)
		return -1;
	p->p_bool_val_to_name = (char **)
	    calloc(p->p_bools.nprim, sizeof(char *));
	if (!p->p_bool_val_to_name)
		return -1;
	if (ksu_hashtab_map(p->p_bools.table, ksu_cond_index_bool, p))
		return -1;
	return 0;
}

static int policydb_index_decls(sepol_handle_t * handle, policydb_t * p)
{
	avrule_block_t *curblock;
	avrule_decl_t *decl;
	unsigned int num_decls = 0;

	free(p->decl_val_to_struct);

	for (curblock = p->global; curblock != NULL; curblock = curblock->next) {
		for (decl = curblock->branch_list; decl != NULL;
		     decl = decl->next) {
			num_decls++;
		}
	}

	p->decl_val_to_struct =
	    calloc(num_decls, sizeof(*(p->decl_val_to_struct)));
	if (!p->decl_val_to_struct) {
		return -1;
	}

	for (curblock = p->global; curblock != NULL; curblock = curblock->next) {
		for (decl = curblock->branch_list; decl != NULL;
		     decl = decl->next) {
			if (!value_isvalid(decl->decl_id, num_decls)) {
				ERR(handle, "invalid decl ID %u", decl->decl_id);
				return -1;
			}
			if (p->decl_val_to_struct[decl->decl_id - 1] != NULL) {
				ERR(handle, "duplicated decl ID %u", decl->decl_id);
				return -1;
			}
			p->decl_val_to_struct[decl->decl_id - 1] = decl;
		}
	}

	return 0;
}

/*
 * Define the other val_to_name and val_to_struct arrays
 * in a policy database structure.  
 */
int policydb_index_others(sepol_handle_t * handle,
			  policydb_t * p, unsigned verbose)
{
	int i;

	if (verbose) {
		INFO(handle,
		     "security:  %d users, %d roles, %d types, %d bools",
		     p->p_users.nprim, p->p_roles.nprim, p->p_types.nprim,
		     p->p_bools.nprim);

		if (p->mls)
			INFO(handle, "security: %d sens, %d cats",
			     p->p_levels.nprim, p->p_cats.nprim);

		INFO(handle, "security:  %d classes, %d rules, %d cond rules",
		     p->p_classes.nprim, p->te_avtab.nel, p->te_cond_avtab.nel);
	}
#if 0
	ksu_avtab_hash_eval(&p->te_avtab, "rules");
	for (i = 0; i < SYM_NUM; i++)
		hashtab_hash_eval(p->symtab[i].table, symtab_name[i]);
#endif

	free(p->role_val_to_struct);
	p->role_val_to_struct = (role_datum_t **)
	    calloc(p->p_roles.nprim, sizeof(role_datum_t *));
	if (!p->role_val_to_struct)
		return -1;

	free(p->user_val_to_struct);
	p->user_val_to_struct = (user_datum_t **)
	    calloc(p->p_users.nprim, sizeof(user_datum_t *));
	if (!p->user_val_to_struct)
		return -1;

	free(p->type_val_to_struct);
	p->type_val_to_struct = (type_datum_t **)
	    calloc(p->p_types.nprim, sizeof(type_datum_t *));
	if (!p->type_val_to_struct)
		return -1;

	if (ksu_cond_init_bool_indexes(p))
		return -1;

	for (i = SYM_ROLES; i < SYM_NUM; i++) {
		free(p->sym_val_to_name[i]);
		p->sym_val_to_name[i] = NULL;
		if (p->symtab[i].nprim) {
			p->sym_val_to_name[i] = (char **)
			    calloc(p->symtab[i].nprim, sizeof(char *));
			if (!p->sym_val_to_name[i])
				return -1;
			if (ksu_hashtab_map(p->symtab[i].table, index_f[i], p))
				return -1;
		}
	}

	/* This pre-expands the roles and users for context validity checking */
	if (ksu_hashtab_map(p->p_roles.table, policydb_role_cache, p))
		return -1;

	if (ksu_hashtab_map(p->p_users.table, policydb_user_cache, p))
		return -1;

	return 0;
}

/*
 * The following *_destroy functions are used to
 * free any memory allocated for each kind of
 * symbol data in the policy database.
 */

static int perm_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			__attribute__ ((unused)))
{
	if (key)
		free(key);
	free(datum);
	return 0;
}

static int common_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			  __attribute__ ((unused)))
{
	common_datum_t *comdatum;

	if (key)
		free(key);
	comdatum = (common_datum_t *) datum;
	(void)ksu_hashtab_map(comdatum->permissions.table, perm_destroy, 0);
	ksu_hashtab_destroy(comdatum->permissions.table);
	free(datum);
	return 0;
}

static int class_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			 __attribute__ ((unused)))
{
	class_datum_t *cladatum;
	constraint_node_t *constraint, *ctemp;

	if (key)
		free(key);
	cladatum = (class_datum_t *) datum;
	if (cladatum == NULL) {
		return 0;
	}
	(void)ksu_hashtab_map(cladatum->permissions.table, perm_destroy, 0);
	ksu_hashtab_destroy(cladatum->permissions.table);
	constraint = cladatum->constraints;
	while (constraint) {
		constraint_expr_destroy(constraint->expr);
		ctemp = constraint;
		constraint = constraint->next;
		free(ctemp);
	}

	constraint = cladatum->validatetrans;
	while (constraint) {
		constraint_expr_destroy(constraint->expr);
		ctemp = constraint;
		constraint = constraint->next;
		free(ctemp);
	}

	if (cladatum->comkey)
		free(cladatum->comkey);
	free(datum);
	return 0;
}

static int role_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			__attribute__ ((unused)))
{
	free(key);
	role_datum_destroy((role_datum_t *) datum);
	free(datum);
	return 0;
}

static int type_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			__attribute__ ((unused)))
{
	free(key);
	type_datum_destroy((type_datum_t *) datum);
	free(datum);
	return 0;
}

static int user_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			__attribute__ ((unused)))
{
	free(key);
	user_datum_destroy((user_datum_t *) datum);
	free(datum);
	return 0;
}

static int sens_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
			__attribute__ ((unused)))
{
	level_datum_t *levdatum;

	if (key)
		free(key);
	levdatum = (level_datum_t *) datum;
	mls_level_destroy(levdatum->level);
	free(levdatum->level);
	level_datum_destroy(levdatum);
	free(levdatum);
	return 0;
}

static int cat_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
		       __attribute__ ((unused)))
{
	if (key)
		free(key);
	cat_datum_destroy((cat_datum_t *) datum);
	free(datum);
	return 0;
}

static int (*destroy_f[SYM_NUM]) (hashtab_key_t key, hashtab_datum_t datum,
				  void *datap) = {
common_destroy, class_destroy, role_destroy, type_destroy, user_destroy,
	    ksu_cond_destroy_bool, sens_destroy, cat_destroy,};

static int filenametr_destroy(hashtab_key_t key, hashtab_datum_t datum,
			      void *p __attribute__ ((unused)))
{
	filename_trans_key_t *ft = (filename_trans_key_t *)key;
	filename_trans_datum_t *fd = datum, *next;

	free(ft->name);
	free(key);
	do {
		next = fd->next;
		ksu_ebitmap_destroy(&fd->stypes);
		free(fd);
		fd = next;
	} while (fd);
	return 0;
}

static int range_tr_destroy(hashtab_key_t key, hashtab_datum_t datum,
			    void *p __attribute__ ((unused)))
{
	struct mls_range *rt = (struct mls_range *)datum;
	free(key);
	ksu_ebitmap_destroy(&rt->level[0].cat);
	ksu_ebitmap_destroy(&rt->level[1].cat);
	free(datum);
	return 0;
}

static void ocontext_selinux_free(ocontext_t **ocontexts)
{
	ocontext_t *c, *ctmp;
	int i;

	for (i = 0; i < OCON_NUM; i++) {
		c = ocontexts[i];
		while (c) {
			ctmp = c;
			c = c->next;
			context_destroy(&ctmp->context[0]);
			context_destroy(&ctmp->context[1]);
			if (i == OCON_ISID || i == OCON_FS || i == OCON_NETIF
				|| i == OCON_FSUSE)
				free(ctmp->u.name);
			else if (i == OCON_IBENDPORT)
				free(ctmp->u.ibendport.dev_name);
			free(ctmp);
		}
	}
}

static void ocontext_xen_free(ocontext_t **ocontexts)
{
	ocontext_t *c, *ctmp;
	int i;

	for (i = 0; i < OCON_NUM; i++) {
		c = ocontexts[i];
		while (c) {
			ctmp = c;
			c = c->next;
			context_destroy(&ctmp->context[0]);
			context_destroy(&ctmp->context[1]);
			if (i == OCON_ISID || i == OCON_XEN_DEVICETREE)
				free(ctmp->u.name);
			free(ctmp);
		}
	}
}

/*
 * Free any memory allocated by a policy database structure.
 */
void ksu_policydb_destroy(policydb_t * p)
{
	ocontext_t *c, *ctmp;
	genfs_t *g, *gtmp;
	unsigned int i;
	role_allow_t *ra, *lra = NULL;
	role_trans_t *tr, *ltr = NULL;

	if (!p)
		return;

	ksu_ebitmap_destroy(&p->policycaps);

	ksu_ebitmap_destroy(&p->permissive_map);

	symtabs_destroy(p->symtab);

	for (i = 0; i < SYM_NUM; i++) {
		if (p->sym_val_to_name[i])
			free(p->sym_val_to_name[i]);
	}

	if (p->class_val_to_struct)
		free(p->class_val_to_struct);
	if (p->role_val_to_struct)
		free(p->role_val_to_struct);
	if (p->user_val_to_struct)
		free(p->user_val_to_struct);
	if (p->type_val_to_struct)
		free(p->type_val_to_struct);
	free(p->decl_val_to_struct);

	for (i = 0; i < SYM_NUM; i++) {
		(void)ksu_hashtab_map(p->scope[i].table, scope_destroy, 0);
		ksu_hashtab_destroy(p->scope[i].table);
	}
	avrule_block_list_destroy(p->global);
	free(p->name);
	free(p->version);

	ksu_avtab_destroy(&p->te_avtab);

	if (p->target_platform == SEPOL_TARGET_SELINUX)
		ocontext_selinux_free(p->ocontexts);
	else if (p->target_platform == SEPOL_TARGET_XEN)
		ocontext_xen_free(p->ocontexts);

	g = p->genfs;
	while (g) {
		free(g->fstype);
		c = g->head;
		while (c) {
			ctmp = c;
			c = c->next;
			context_destroy(&ctmp->context[0]);
			free(ctmp->u.name);
			free(ctmp);
		}
		gtmp = g;
		g = g->next;
		free(gtmp);
	}
	ksu_cond_policydb_destroy(p);

	for (tr = p->role_tr; tr; tr = tr->next) {
		if (ltr)
			free(ltr);
		ltr = tr;
	}
	if (ltr)
		free(ltr);

	for (ra = p->role_allow; ra; ra = ra->next) {
		if (lra)
			free(lra);
		lra = ra;
	}
	if (lra)
		free(lra);

	ksu_hashtab_map(p->filename_trans, filenametr_destroy, NULL);
	ksu_hashtab_destroy(p->filename_trans);

	ksu_hashtab_map(p->range_tr, range_tr_destroy, NULL);
	ksu_hashtab_destroy(p->range_tr);

	if (p->type_attr_map) {
		for (i = 0; i < p->p_types.nprim; i++) {
			ksu_ebitmap_destroy(&p->type_attr_map[i]);
		}
		free(p->type_attr_map);
	}

	if (p->attr_type_map) {
		for (i = 0; i < p->p_types.nprim; i++) {
			ksu_ebitmap_destroy(&p->attr_type_map[i]);
		}
		free(p->attr_type_map);
	}

	return;
}

void symtabs_destroy(symtab_t * symtab)
{
	int i;
	for (i = 0; i < SYM_NUM; i++) {
		(void)ksu_hashtab_map(symtab[i].table, destroy_f[i], 0);
		ksu_hashtab_destroy(symtab[i].table);
	}
}

int scope_destroy(hashtab_key_t key, hashtab_datum_t datum, void *p
		  __attribute__ ((unused)))
{
	scope_datum_t *cur = (scope_datum_t *) datum;
	free(key);
	if (cur != NULL) {
		free(cur->decl_ids);
	}
	free(cur);
	return 0;
}

/*
 * Load the initial SIDs specified in a policy database
 * structure into a SID table.
 */
int ksu_policydb_load_isids(policydb_t * p, sidtab_t * s)
{
	ocontext_t *head, *c;

	if (sepol_sidtab_init(s)) {
		ERR(NULL, "out of memory on SID table init");
		return -1;
	}

	head = p->ocontexts[OCON_ISID];
	for (c = head; c; c = c->next) {
		if (sepol_sidtab_insert(s, c->sid[0], &c->context[0])) {
			ERR(NULL, "unable to load initial SID %s", c->u.name);
			return -1;
		}
	}

	return 0;
}

/* Declare a symbol for a certain avrule_block context.  Insert it
 * into a symbol table for a policy.  This function will handle
 * inserting the appropriate scope information in addition to
 * inserting the symbol into the hash table.
 *
 * arguments:
 *   policydb_t *pol       module policy to modify
 *   uint32_t sym          the symbole table for insertion (SYM_*)
 *   hashtab_key_t key     the key for the symbol - not cloned
 *   hashtab_datum_t data  the data for the symbol - not cloned
 *   scope                 scope of this symbol, either SCOPE_REQ or SCOPE_DECL
 *   avrule_decl_id        identifier for this symbol's encapsulating declaration
 *   value (out)           assigned value to the symbol (if value is not NULL)
 *
 * returns:
 *   0                     success
 *   1                     success, but symbol already existed as a requirement
 *                         (datum was not inserted and needs to be free()d)
 *   -1                    general error
 *   -2                    scope conflicted
 *   -ENOMEM               memory error
 *   error codes from hashtab_insert
 */
int ksu_symtab_insert(policydb_t * pol, uint32_t sym,
		  hashtab_key_t key, hashtab_datum_t datum,
		  uint32_t scope, uint32_t avrule_decl_id, uint32_t * value)
{
	int rc, retval = 0;
	unsigned int i;
	scope_datum_t *scope_datum;

	/* check if the symbol is already there.  multiple
	 * declarations of non-roles/non-users are illegal, but
	 * multiple requires are allowed. */

	/* FIX ME - the failures after the hashtab_insert will leave
	 * the policy in a inconsistent state. */
	rc = hashtab_insert(pol->symtab[sym].table, key, datum);
	if (rc == SEPOL_OK) {
		/* if no value is passed in the symbol is not primary
		 * (i.e. aliases) */
		if (value)
			*value = ++pol->symtab[sym].nprim;
	} else if (rc == SEPOL_EEXIST) {
		retval = 1;	/* symbol not added -- need to free() later */
	} else {
		return rc;
	}

	/* get existing scope information; if there is not one then
	 * create it */
	scope_datum =
	    (scope_datum_t *) hashtab_search(pol->scope[sym].table, key);
	if (scope_datum == NULL) {
		hashtab_key_t key2 = strdup((char *)key);
		if (!key2)
			return -ENOMEM;
		if ((scope_datum = malloc(sizeof(*scope_datum))) == NULL) {
			free(key2);
			return -ENOMEM;
		}
		scope_datum->scope = scope;
		scope_datum->decl_ids = NULL;
		scope_datum->decl_ids_len = 0;
		if ((rc =
		     hashtab_insert(pol->scope[sym].table, key2,
				    scope_datum)) != 0) {
			free(key2);
			free(scope_datum);
			return rc;
		}
	} else if (scope_datum->scope == SCOPE_DECL && scope == SCOPE_DECL) {
		/* disallow multiple declarations for non-roles/users */
		if (sym != SYM_ROLES && sym != SYM_USERS) {
			return -2;
		}
		/* Further confine that a role attribute can't have the same
		 * name as another regular role, and a role attribute can't
		 * be declared more than once. */
		if (sym == SYM_ROLES) {
			role_datum_t *base_role;
			role_datum_t *cur_role = (role_datum_t *)datum;
		
			base_role = (role_datum_t *)
					hashtab_search(pol->symtab[sym].table,
						       key);
			assert(base_role != NULL);

			if (!((base_role->flavor == ROLE_ROLE) &&
			    (cur_role->flavor == ROLE_ROLE))) {
				/* Only regular roles are allowed to have
				 * multiple declarations. */
				return -2;
			}
		}
	} else if (scope_datum->scope == SCOPE_REQ && scope == SCOPE_DECL) {
		scope_datum->scope = SCOPE_DECL;
	}

	/* search through the pre-existing list to avoid adding duplicates */
	for (i = 0; i < scope_datum->decl_ids_len; i++) {
		if (scope_datum->decl_ids[i] == avrule_decl_id) {
			/* already there, so don't modify its scope */
			return retval;
		}
	}

	if (add_i_to_a(avrule_decl_id,
		       &scope_datum->decl_ids_len,
		       &scope_datum->decl_ids) == -1) {
		return -ENOMEM;
	}

	if (scope_datum->scope == SCOPE_DECL && scope == SCOPE_REQ) {
		/* Need to keep the decl at the end of the list */
		uint32_t len, tmp;
		len = scope_datum->decl_ids_len;
		if (len < 2) {
			/* This should be impossible here */
			return -1;
		}
		tmp = scope_datum->decl_ids[len-2];
		scope_datum->decl_ids[len-2] = scope_datum->decl_ids[len-1];
		scope_datum->decl_ids[len-1] = tmp;
	}

	return retval;
}

static int type_set_or(type_set_t * dst, const type_set_t * a, const type_set_t * b)
{
	type_set_init(dst);

	if (ebitmap_or(&dst->types, &a->types, &b->types)) {
		return -1;
	}
	if (ebitmap_or(&dst->negset, &a->negset, &b->negset)) {
		return -1;
	}

	dst->flags |= a->flags;
	dst->flags |= b->flags;

	return 0;
}

int type_set_cpy(type_set_t * dst, const type_set_t * src)
{
	type_set_init(dst);

	dst->flags = src->flags;
	if (ksu_ebitmap_cpy(&dst->types, &src->types))
		return -1;
	if (ksu_ebitmap_cpy(&dst->negset, &src->negset))
		return -1;

	return 0;
}

int type_set_or_eq(type_set_t * dst, const type_set_t * other)
{
	int ret;
	type_set_t tmp;

	if (type_set_or(&tmp, dst, other))
		return -1;
	type_set_destroy(dst);
	ret = type_set_cpy(dst, &tmp);
	type_set_destroy(&tmp);

	return ret;
}

/***********************************************************************/
/* everything below is for policy reads */

/* The following are read functions for module structures */

static int role_set_read(role_set_t * r, struct policy_file *fp)
{
	uint32_t buf[1];
	int rc;

	if (ksu_ebitmap_read(&r->roles, fp))
		return -1;
	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	r->flags = le32_to_cpu(buf[0]);

	return 0;
}

static int type_set_read(type_set_t * t, struct policy_file *fp)
{
	uint32_t buf[1];
	int rc;

	if (ksu_ebitmap_read(&t->types, fp))
		return -1;
	if (ksu_ebitmap_read(&t->negset, fp))
		return -1;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	t->flags = le32_to_cpu(buf[0]);

	return 0;
}

/*
 * Read a MLS range structure from a policydb binary 
 * representation file.
 */
static int mls_read_range_helper(mls_range_t * r, struct policy_file *fp)
{
	uint32_t buf[2], items;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		goto out;

	items = le32_to_cpu(buf[0]);
	if (items > ARRAY_SIZE(buf)) {
		ERR(fp->handle, "range overflow");
		rc = -EINVAL;
		goto out;
	}
	rc = next_entry(buf, fp, sizeof(uint32_t) * items);
	if (rc < 0) {
		ERR(fp->handle, "truncated range");
		goto out;
	}
	r->level[0].sens = le32_to_cpu(buf[0]);
	if (items > 1)
		r->level[1].sens = le32_to_cpu(buf[1]);
	else
		r->level[1].sens = r->level[0].sens;

	rc = ksu_ebitmap_read(&r->level[0].cat, fp);
	if (rc) {
		ERR(fp->handle, "error reading low categories");
		goto out;
	}
	if (items > 1) {
		rc = ksu_ebitmap_read(&r->level[1].cat, fp);
		if (rc) {
			ERR(fp->handle, "error reading high categories");
			goto bad_high;
		}
	} else {
		rc = ksu_ebitmap_cpy(&r->level[1].cat, &r->level[0].cat);
		if (rc) {
			ERR(fp->handle, "out of memory");
			goto bad_high;
		}
	}

	rc = 0;
      out:
	return rc;
      bad_high:
	ksu_ebitmap_destroy(&r->level[0].cat);
	goto out;
}

/*
 * Read a semantic MLS level structure from a policydb binary 
 * representation file.
 */
static int mls_read_semantic_level_helper(mls_semantic_level_t * l,
					  struct policy_file *fp)
{
	uint32_t buf[2], ncat;
	unsigned int i;
	mls_semantic_cat_t *cat;
	int rc;

	mls_semantic_level_init(l);

	rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
	if (rc < 0) {
		ERR(fp->handle, "truncated level");
		goto bad;
	}
	l->sens = le32_to_cpu(buf[0]);

	ncat = le32_to_cpu(buf[1]);
	for (i = 0; i < ncat; i++) {
		cat = (mls_semantic_cat_t *) malloc(sizeof(mls_semantic_cat_t));
		if (!cat) {
			ERR(fp->handle, "out of memory");
			goto bad;
		}

		mls_semantic_cat_init(cat);
		cat->next = l->cat;
		l->cat = cat;

		rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
		if (rc < 0) {
			ERR(fp->handle, "error reading level categories");
			goto bad;
		}
		cat->low = le32_to_cpu(buf[0]);
		cat->high = le32_to_cpu(buf[1]);
	}

	return 0;

      bad:
	return -EINVAL;
}

/*
 * Read a semantic MLS range structure from a policydb binary 
 * representation file.
 */
static int mls_read_semantic_range_helper(mls_semantic_range_t * r,
					  struct policy_file *fp)
{
	int rc;

	rc = mls_read_semantic_level_helper(&r->level[0], fp);
	if (rc)
		return rc;

	rc = mls_read_semantic_level_helper(&r->level[1], fp);

	return rc;
}

static int mls_level_to_semantic(mls_level_t * l, mls_semantic_level_t * sl)
{
	unsigned int i;
	ebitmap_node_t *cnode;
	mls_semantic_cat_t *open_cat = NULL;

	mls_semantic_level_init(sl);
	sl->sens = l->sens;
	ebitmap_for_each_bit(&l->cat, cnode, i) {
		if (ebitmap_node_get_bit(cnode, i)) {
			if (open_cat)
				continue;
			open_cat = (mls_semantic_cat_t *)
			    malloc(sizeof(mls_semantic_cat_t));
			if (!open_cat)
				return -1;

			mls_semantic_cat_init(open_cat);
			open_cat->low = i + 1;
			open_cat->next = sl->cat;
			sl->cat = open_cat;
		} else {
			if (!open_cat)
				continue;
			open_cat->high = i;
			open_cat = NULL;
		}
	}
	if (open_cat)
		open_cat->high = i;

	return 0;
}

static int mls_range_to_semantic(mls_range_t * r, mls_semantic_range_t * sr)
{
	if (mls_level_to_semantic(&r->level[0], &sr->level[0]))
		return -1;

	if (mls_level_to_semantic(&r->level[1], &sr->level[1]))
		return -1;

	return 0;
}

/*
 * Read and validate a security context structure
 * from a policydb binary representation file.
 */
static int context_read_and_validate(context_struct_t * c,
				     policydb_t * p, struct policy_file *fp)
{
	uint32_t buf[3];
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
	if (rc < 0) {
		ERR(fp->handle, "context truncated");
		return -1;
	}
	c->user = le32_to_cpu(buf[0]);
	c->role = le32_to_cpu(buf[1]);
	c->type = le32_to_cpu(buf[2]);
	if ((p->policy_type == POLICY_KERN
	     && p->policyvers >= POLICYDB_VERSION_MLS)
	    || (p->policy_type == POLICY_BASE
		&& p->policyvers >= MOD_POLICYDB_VERSION_MLS)) {
		if (mls_read_range_helper(&c->range, fp)) {
			ERR(fp->handle, "error reading MLS range "
			    "of context");
			return -1;
		}
	}

	if (!ksu_policydb_context_isvalid(p, c)) {
		ERR(fp->handle, "invalid security context");
		context_destroy(c);
		return -1;
	}
	return 0;
}

/*
 * The following *_read functions are used to
 * read the symbol data from a policy database
 * binary representation file.
 */

static int perm_read(policydb_t * p
		     __attribute__ ((unused)), hashtab_t h,
		     struct policy_file *fp, uint32_t nprim)
{
	char *key = 0;
	perm_datum_t *perdatum;
	uint32_t buf[2];
	size_t len;
	int rc;

	perdatum = calloc(1, sizeof(perm_datum_t));
	if (!perdatum)
		return -1;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if(str_read(&key, fp, len))
		goto bad;

	perdatum->s.value = le32_to_cpu(buf[1]);
	if (!value_isvalid(perdatum->s.value, nprim))
		goto bad;

	if (hashtab_insert(h, key, perdatum))
		goto bad;

	return 0;

      bad:
	perm_destroy(key, perdatum, NULL);
	return -1;
}

static int common_read(policydb_t * p, hashtab_t h, struct policy_file *fp)
{
	char *key = 0;
	common_datum_t *comdatum;
	uint32_t buf[4];
	size_t len, nel;
	unsigned int i;
	int rc;

	comdatum = calloc(1, sizeof(common_datum_t));
	if (!comdatum)
		return -1;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 4);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		goto bad;

	comdatum->s.value = le32_to_cpu(buf[1]);

	if (ksu_symtab_init(&comdatum->permissions, PERM_SYMTAB_SIZE))
		goto bad;
	comdatum->permissions.nprim = le32_to_cpu(buf[2]);
	if (comdatum->permissions.nprim > PERM_SYMTAB_SIZE)
		goto bad;
	nel = le32_to_cpu(buf[3]);

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	for (i = 0; i < nel; i++) {
		if (perm_read(p, comdatum->permissions.table, fp, comdatum->permissions.nprim))
			goto bad;
	}

	if (hashtab_insert(h, key, comdatum))
		goto bad;

	return 0;

      bad:
	common_destroy(key, comdatum, NULL);
	return -1;
}

static int read_cons_helper(policydb_t * p, constraint_node_t ** nodep,
			    unsigned int ncons,
			    int allowxtarget, struct policy_file *fp)
{
	constraint_node_t *c, *lc;
	constraint_expr_t *e, *le;
	uint32_t buf[3];
	size_t nexpr;
	unsigned int i, j;
	int rc, depth;

	lc = NULL;
	for (i = 0; i < ncons; i++) {
		c = calloc(1, sizeof(constraint_node_t));
		if (!c)
			return -1;

		if (lc)
			lc->next = c;
		else
			*nodep = c;

		rc = next_entry(buf, fp, (sizeof(uint32_t) * 2));
		if (rc < 0)
			return -1;
		c->permissions = le32_to_cpu(buf[0]);
		nexpr = le32_to_cpu(buf[1]);
		le = NULL;
		depth = -1;
		for (j = 0; j < nexpr; j++) {
			e = malloc(sizeof(constraint_expr_t));
			if (!e)
				return -1;
			if (constraint_expr_init(e) == -1) {
				free(e);
				return -1;
			}
			if (le) {
				le->next = e;
			} else {
				c->expr = e;
			}

			rc = next_entry(buf, fp, (sizeof(uint32_t) * 3));
			if (rc < 0)
				return -1;
			e->expr_type = le32_to_cpu(buf[0]);
			e->attr = le32_to_cpu(buf[1]);
			e->op = le32_to_cpu(buf[2]);

			switch (e->expr_type) {
			case CEXPR_NOT:
				if (depth < 0)
					return -1;
				break;
			case CEXPR_AND:
			case CEXPR_OR:
				if (depth < 1)
					return -1;
				depth--;
				break;
			case CEXPR_ATTR:
				if (depth == (CEXPR_MAXDEPTH - 1))
					return -1;
				depth++;
				break;
			case CEXPR_NAMES:
				if (!allowxtarget && (e->attr & CEXPR_XTARGET))
					return -1;
				if (depth == (CEXPR_MAXDEPTH - 1))
					return -1;
				depth++;
				if (ksu_ebitmap_read(&e->names, fp))
					return -1;
				if (p->policy_type != POLICY_KERN &&
				    type_set_read(e->type_names, fp))
					return -1;
				else if (p->policy_type == POLICY_KERN &&
					 p->policyvers >= POLICYDB_VERSION_CONSTRAINT_NAMES &&
					 type_set_read(e->type_names, fp))
					return -1;
				break;
			default:
				return -1;
			}
			le = e;
		}
		if (depth != 0)
			return -1;
		lc = c;
	}

	return 0;
}

static int class_read(policydb_t * p, hashtab_t h, struct policy_file *fp)
{
	char *key = 0;
	class_datum_t *cladatum;
	uint32_t buf[6];
	size_t len, len2, ncons, nel;
	unsigned int i;
	int rc;

	cladatum = (class_datum_t *) calloc(1, sizeof(class_datum_t));
	if (!cladatum)
		return -1;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 6);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		goto bad;
	len2 = le32_to_cpu(buf[1]);
	if (is_saturated(len2))
		goto bad;
	cladatum->s.value = le32_to_cpu(buf[2]);

	if (ksu_symtab_init(&cladatum->permissions, PERM_SYMTAB_SIZE))
		goto bad;
	cladatum->permissions.nprim = le32_to_cpu(buf[3]);
	if (cladatum->permissions.nprim > PERM_SYMTAB_SIZE)
		goto bad;
	nel = le32_to_cpu(buf[4]);

	ncons = le32_to_cpu(buf[5]);

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	if (len2) {
		cladatum->comkey = malloc(len2 + 1);
		if (!cladatum->comkey)
			goto bad;
		rc = next_entry(cladatum->comkey, fp, len2);
		if (rc < 0)
			goto bad;
		cladatum->comkey[len2] = 0;

		cladatum->comdatum = hashtab_search(p->p_commons.table,
						    cladatum->comkey);
		if (!cladatum->comdatum) {
			ERR(fp->handle, "unknown common %s", cladatum->comkey);
			goto bad;
		}
	}
	for (i = 0; i < nel; i++) {
		if (perm_read(p, cladatum->permissions.table, fp, cladatum->permissions.nprim))
			goto bad;
	}

	if (read_cons_helper(p, &cladatum->constraints, ncons, 0, fp))
		goto bad;

	if ((p->policy_type == POLICY_KERN
	     && p->policyvers >= POLICYDB_VERSION_VALIDATETRANS)
	    || (p->policy_type == POLICY_BASE
		&& p->policyvers >= MOD_POLICYDB_VERSION_VALIDATETRANS)) {
		/* grab the validatetrans rules */
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto bad;
		ncons = le32_to_cpu(buf[0]);
		if (read_cons_helper(p, &cladatum->validatetrans, ncons, 1, fp))
			goto bad;
	}

	if ((p->policy_type == POLICY_KERN &&
	     p->policyvers >= POLICYDB_VERSION_NEW_OBJECT_DEFAULTS) ||
	    (p->policy_type == POLICY_BASE &&
	     p->policyvers >= MOD_POLICYDB_VERSION_NEW_OBJECT_DEFAULTS)) {
		rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
		if (rc < 0)
			goto bad;
		cladatum->default_user = le32_to_cpu(buf[0]);
		cladatum->default_role = le32_to_cpu(buf[1]);
		cladatum->default_range = le32_to_cpu(buf[2]);
	}

	if ((p->policy_type == POLICY_KERN &&
	     p->policyvers >= POLICYDB_VERSION_DEFAULT_TYPE) ||
	    (p->policy_type == POLICY_BASE &&
	     p->policyvers >= MOD_POLICYDB_VERSION_DEFAULT_TYPE)) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto bad;
		cladatum->default_type = le32_to_cpu(buf[0]);
	}

	if (hashtab_insert(h, key, cladatum))
		goto bad;

	return 0;

      bad:
	class_destroy(key, cladatum, NULL);
	return -1;
}

static int role_read(policydb_t * p, hashtab_t h, struct policy_file *fp)
{
	char *key = 0;
	role_datum_t *role;
	uint32_t buf[3];
	size_t len;
	int rc, to_read = 2;

	role = calloc(1, sizeof(role_datum_t));
	if (!role)
		return -1;

	if (policydb_has_boundary_feature(p))
		to_read = 3;

	rc = next_entry(buf, fp, sizeof(uint32_t) * to_read);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		goto bad;

	role->s.value = le32_to_cpu(buf[1]);
	if (policydb_has_boundary_feature(p))
		role->bounds = le32_to_cpu(buf[2]);

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	if (ksu_ebitmap_read(&role->dominates, fp))
		goto bad;

	if (p->policy_type == POLICY_KERN) {
		if (ksu_ebitmap_read(&role->types.types, fp))
			goto bad;
	} else {
		if (type_set_read(&role->types, fp))
			goto bad;
	}
	
	if (p->policy_type != POLICY_KERN &&
	    p->policyvers >= MOD_POLICYDB_VERSION_ROLEATTRIB) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto bad;

		role->flavor = le32_to_cpu(buf[0]);

		if (ksu_ebitmap_read(&role->roles, fp))
			goto bad;
	}

	if (strcmp(key, OBJECT_R) == 0) {
		if (role->s.value != OBJECT_R_VAL) {
			ERR(fp->handle, "role %s has wrong value %d",
			    OBJECT_R, role->s.value);
			role_destroy(key, role, NULL);
			return -1;
		}
		role_destroy(key, role, NULL);
		return 0;
	}

	if (hashtab_insert(h, key, role))
		goto bad;

	return 0;

      bad:
	role_destroy(key, role, NULL);
	return -1;
}

static int type_read(policydb_t * p, hashtab_t h, struct policy_file *fp)
{
	char *key = 0;
	type_datum_t *typdatum;
	uint32_t buf[5];
	size_t len;
	int rc, to_read;
	int pos = 0;

	typdatum = calloc(1, sizeof(type_datum_t));
	if (!typdatum)
		return -1;

	if (policydb_has_boundary_feature(p)) {
		if (p->policy_type != POLICY_KERN
		    && p->policyvers >= MOD_POLICYDB_VERSION_BOUNDARY_ALIAS)
			to_read = 5;
		else
			to_read = 4;
	}
	else if (p->policy_type == POLICY_KERN)
		to_read = 3;
	else if (p->policyvers >= MOD_POLICYDB_VERSION_PERMISSIVE)
		to_read = 5;
	else
		to_read = 4;

	rc = next_entry(buf, fp, sizeof(uint32_t) * to_read);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[pos]);
	if (zero_or_saturated(len))
		goto bad;

	typdatum->s.value = le32_to_cpu(buf[++pos]);
	if (policydb_has_boundary_feature(p)) {
		uint32_t properties;

		if (p->policy_type != POLICY_KERN
		    && p->policyvers >= MOD_POLICYDB_VERSION_BOUNDARY_ALIAS) {
			typdatum->primary = le32_to_cpu(buf[++pos]);
			properties = le32_to_cpu(buf[++pos]);
		}
		else {
			properties = le32_to_cpu(buf[++pos]);

			if (properties & TYPEDATUM_PROPERTY_PRIMARY)
				typdatum->primary = 1;
		}

		if (properties & TYPEDATUM_PROPERTY_ATTRIBUTE)
			typdatum->flavor = TYPE_ATTRIB;
		if (properties & TYPEDATUM_PROPERTY_ALIAS
		    && p->policy_type != POLICY_KERN)
			typdatum->flavor = TYPE_ALIAS;
		if (properties & TYPEDATUM_PROPERTY_PERMISSIVE
		    && p->policy_type != POLICY_KERN)
			typdatum->flags |= TYPE_FLAGS_PERMISSIVE;

		typdatum->bounds = le32_to_cpu(buf[++pos]);
	} else {
		typdatum->primary = le32_to_cpu(buf[++pos]);
		if (p->policy_type != POLICY_KERN) {
			typdatum->flavor = le32_to_cpu(buf[++pos]);
			if (p->policyvers >= MOD_POLICYDB_VERSION_PERMISSIVE)
				typdatum->flags = le32_to_cpu(buf[++pos]);
		}
	}

	if (p->policy_type != POLICY_KERN) {
		if (ksu_ebitmap_read(&typdatum->types, fp))
			goto bad;
	}

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	if (hashtab_insert(h, key, typdatum))
		goto bad;

	return 0;

      bad:
	type_destroy(key, typdatum, NULL);
	return -1;
}

static int role_trans_read(policydb_t *p, struct policy_file *fp)
{
	role_trans_t **t = &p->role_tr;
	unsigned int i;
	uint32_t buf[3], nel;
	role_trans_t *tr, *ltr;
	int rc;
	int new_roletr = (p->policy_type == POLICY_KERN &&
			  p->policyvers >= POLICYDB_VERSION_ROLETRANS);

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	ltr = NULL;
	for (i = 0; i < nel; i++) {
		tr = calloc(1, sizeof(struct role_trans));
		if (!tr) {
			return -1;
		}
		if (ltr) {
			ltr->next = tr;
		} else {
			*t = tr;
		}
		rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
		if (rc < 0)
			return -1;
		tr->role = le32_to_cpu(buf[0]);
		tr->type = le32_to_cpu(buf[1]);
		tr->new_role = le32_to_cpu(buf[2]);
		if (new_roletr) {
			rc = next_entry(buf, fp, sizeof(uint32_t));
			if (rc < 0)
				return -1;
			tr->tclass = le32_to_cpu(buf[0]);
		} else
			tr->tclass = p->process_class;
		ltr = tr;
	}
	return 0;
}

static int role_allow_read(role_allow_t ** r, struct policy_file *fp)
{
	unsigned int i;
	uint32_t buf[2], nel;
	role_allow_t *ra, *lra;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	lra = NULL;
	for (i = 0; i < nel; i++) {
		ra = calloc(1, sizeof(struct role_allow));
		if (!ra) {
			return -1;
		}
		if (lra) {
			lra->next = ra;
		} else {
			*r = ra;
		}
		rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
		if (rc < 0)
			return -1;
		ra->role = le32_to_cpu(buf[0]);
		ra->new_role = le32_to_cpu(buf[1]);
		lra = ra;
	}
	return 0;
}

int policydb_filetrans_insert(policydb_t *p, uint32_t stype, uint32_t ttype,
			      uint32_t tclass, const char *name,
			      char **name_alloc, uint32_t otype,
			      uint32_t *present_otype)
{
	filename_trans_key_t *ft, key;
	filename_trans_datum_t *datum, *last;

	key.ttype = ttype;
	key.tclass = tclass;
	key.name = (char *)name;

	last = NULL;
	datum = hashtab_search(p->filename_trans, (hashtab_key_t)&key);
	while (datum) {
		if (ksu_ebitmap_get_bit(&datum->stypes, stype - 1)) {
			if (present_otype)
				*present_otype = datum->otype;
			return SEPOL_EEXIST;
		}
		if (datum->otype == otype)
			break;
		last = datum;
		datum = datum->next;
	}
	if (!datum) {
		datum = malloc(sizeof(*datum));
		if (!datum)
			return SEPOL_ENOMEM;

		ebitmap_init(&datum->stypes);
		datum->otype = otype;
		datum->next = NULL;

		if (last) {
			last->next = datum;
		} else {
			char *name_dup;

			if (name_alloc) {
				name_dup = *name_alloc;
				*name_alloc = NULL;
			} else {
				name_dup = strdup(name);
				if (!name_dup) {
					free(datum);
					return SEPOL_ENOMEM;
				}
			}

			ft = malloc(sizeof(*ft));
			if (!ft) {
				free(name_dup);
				free(datum);
				return SEPOL_ENOMEM;
			}

			ft->ttype = ttype;
			ft->tclass = tclass;
			ft->name = name_dup;

			if (hashtab_insert(p->filename_trans, (hashtab_key_t)ft,
					   (hashtab_datum_t)datum)) {
				free(name_dup);
				free(datum);
				free(ft);
				return SEPOL_ENOMEM;
			}
		}
	}

	p->filename_trans_count++;
	return ksu_ebitmap_set_bit(&datum->stypes, stype - 1, 1);
}

static int filename_trans_read_one_compat(policydb_t *p, struct policy_file *fp)
{
	uint32_t buf[4], len, stype, ttype, tclass, otype;
	char *name = NULL;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		return -1;

	name = calloc(len + 1, sizeof(*name));
	if (!name)
		return -1;

	rc = next_entry(name, fp, len);
	if (rc < 0)
		goto err;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 4);
	if (rc < 0)
		goto err;

	stype = le32_to_cpu(buf[0]);
	if (stype == 0)
		goto err;

	ttype  = le32_to_cpu(buf[1]);
	tclass = le32_to_cpu(buf[2]);
	otype  = le32_to_cpu(buf[3]);

	rc = policydb_filetrans_insert(p, stype, ttype, tclass, name, &name,
				       otype, NULL);
	if (rc) {
		if (rc != SEPOL_EEXIST)
			goto err;
		/*
		 * Some old policies were wrongly generated with
		 * duplicate filename transition rules.  For backward
		 * compatibility, do not reject such policies, just
		 * ignore the duplicate.
		 */
	}
	free(name);
	return 0;
err:
	free(name);
	return -1;
}

static int filename_trans_check_datum(filename_trans_datum_t *datum)
{
	ebitmap_t stypes, otypes;
	int rc = -1;

	ebitmap_init(&stypes);
	ebitmap_init(&otypes);

	while (datum) {
		if (ksu_ebitmap_get_bit(&otypes, datum->otype))
			goto out;

		if (ksu_ebitmap_set_bit(&otypes, datum->otype, 1))
			goto out;

		if (ebitmap_match_any(&stypes, &datum->stypes))
			goto out;

		if (ebitmap_union(&stypes, &datum->stypes))
			goto out;

		datum = datum->next;
	}
	rc = 0;
out:
	ksu_ebitmap_destroy(&stypes);
	ksu_ebitmap_destroy(&otypes);
	return rc;
}

static int filename_trans_read_one(policydb_t *p, struct policy_file *fp)
{
	filename_trans_key_t *ft = NULL;
	filename_trans_datum_t **dst, *datum, *first = NULL;
	unsigned int i;
	uint32_t buf[3], len, ttype, tclass, ndatum;
	char *name = NULL;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		return -1;

	name = calloc(len + 1, sizeof(*name));
	if (!name)
		return -1;

	rc = next_entry(name, fp, len);
	if (rc < 0)
		goto err;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
	if (rc < 0)
		goto err;

	ttype = le32_to_cpu(buf[0]);
	tclass = le32_to_cpu(buf[1]);
	ndatum = le32_to_cpu(buf[2]);
	if (ndatum == 0)
		goto err;

	dst = &first;
	for (i = 0; i < ndatum; i++) {
		datum = malloc(sizeof(*datum));
		if (!datum)
			goto err;

		datum->next = NULL;
		*dst = datum;

		/* ksu_ebitmap_read() will at least init the bitmap */
		rc = ksu_ebitmap_read(&datum->stypes, fp);
		if (rc < 0)
			goto err;

		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto err;

		datum->otype = le32_to_cpu(buf[0]);

		p->filename_trans_count += ebitmap_cardinality(&datum->stypes);

		dst = &datum->next;
	}

	if (ndatum > 1 && filename_trans_check_datum(first))
		goto err;

	ft = malloc(sizeof(*ft));
	if (!ft)
		goto err;

	ft->ttype = ttype;
	ft->tclass = tclass;
	ft->name = name;

	rc = hashtab_insert(p->filename_trans, (hashtab_key_t)ft,
			    (hashtab_datum_t)first);
	if (rc)
		goto err;

	return 0;
err:
	free(ft);
	free(name);
	while (first) {
		datum = first;
		first = first->next;

		ksu_ebitmap_destroy(&datum->stypes);
		free(datum);
	}
	return -1;
}

static int filename_trans_read(policydb_t *p, struct policy_file *fp)
{
	unsigned int i;
	uint32_t buf[1], nel;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);

	if (p->policyvers < POLICYDB_VERSION_COMP_FTRANS) {
		for (i = 0; i < nel; i++) {
			rc = filename_trans_read_one_compat(p, fp);
			if (rc < 0)
				return -1;
		}
	} else {
		for (i = 0; i < nel; i++) {
			rc = filename_trans_read_one(p, fp);
			if (rc < 0)
				return -1;
		}
	}
	return 0;
}

static int ocontext_read_xen(const struct policydb_compat_info *info,
	policydb_t *p, struct policy_file *fp)
{
	unsigned int i, j;
	size_t nel, len;
	ocontext_t *l, *c;
	uint32_t buf[8];
	int rc;

	for (i = 0; i < info->ocon_num; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			return -1;
		nel = le32_to_cpu(buf[0]);
		l = NULL;
		for (j = 0; j < nel; j++) {
			c = calloc(1, sizeof(ocontext_t));
			if (!c)
				return -1;
			if (l)
				l->next = c;
			else
				p->ocontexts[i] = c;
			l = c;
			switch (i) {
			case OCON_XEN_ISID:
				rc = next_entry(buf, fp, sizeof(uint32_t));
				if (rc < 0)
					return -1;
				c->sid[0] = le32_to_cpu(buf[0]);
				if (is_saturated(c->sid[0]))
					return -1;
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_XEN_PIRQ:
				rc = next_entry(buf, fp, sizeof(uint32_t));
				if (rc < 0)
					return -1;
				c->u.pirq = le32_to_cpu(buf[0]);
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_XEN_IOPORT:
				rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
				if (rc < 0)
					return -1;
				c->u.ioport.low_ioport = le32_to_cpu(buf[0]);
				c->u.ioport.high_ioport = le32_to_cpu(buf[1]);
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_XEN_IOMEM:
				if (p->policyvers >= POLICYDB_VERSION_XEN_DEVICETREE) {
					uint64_t b64[2];
					rc = next_entry(b64, fp, sizeof(uint64_t) * 2);
					if (rc < 0)
						return -1;
					c->u.iomem.low_iomem = le64_to_cpu(b64[0]);
					c->u.iomem.high_iomem = le64_to_cpu(b64[1]);
				} else {
					rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
					if (rc < 0)
						return -1;
					c->u.iomem.low_iomem = le32_to_cpu(buf[0]);
					c->u.iomem.high_iomem = le32_to_cpu(buf[1]);
				}
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_XEN_PCIDEVICE:
				rc = next_entry(buf, fp, sizeof(uint32_t));
				if (rc < 0)
					return -1;
				c->u.device = le32_to_cpu(buf[0]);
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_XEN_DEVICETREE:
				rc = next_entry(buf, fp, sizeof(uint32_t));
				if (rc < 0)
					return -1;
				len = le32_to_cpu(buf[0]);
				if (zero_or_saturated(len))
					return -1;

				c->u.name = malloc(len + 1);
				if (!c->u.name)
					return -1;
				rc = next_entry(c->u.name, fp, len);
				if (rc < 0)
					return -1;
				c->u.name[len] = 0;
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			default:
				/* should never get here */
				ERR(fp->handle, "Unknown Xen ocontext");
				return -1;
			}
		}
	}
	return 0;
}
static int ocontext_read_selinux(const struct policydb_compat_info *info,
			 policydb_t * p, struct policy_file *fp)
{
	unsigned int i, j;
	size_t nel, len;
	ocontext_t *l, *c;
	uint32_t buf[8];
	int rc;

	for (i = 0; i < info->ocon_num; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			return -1;
		nel = le32_to_cpu(buf[0]);
		l = NULL;
		for (j = 0; j < nel; j++) {
			c = calloc(1, sizeof(ocontext_t));
			if (!c) {
				return -1;
			}
			if (l) {
				l->next = c;
			} else {
				p->ocontexts[i] = c;
			}
			l = c;
			switch (i) {
			case OCON_ISID:
				rc = next_entry(buf, fp, sizeof(uint32_t));
				if (rc < 0)
					return -1;
				c->sid[0] = le32_to_cpu(buf[0]);
				if (is_saturated(c->sid[0]))
					return -1;
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_FS:
			case OCON_NETIF:
				rc = next_entry(buf, fp, sizeof(uint32_t));
				if (rc < 0)
					return -1;
				len = le32_to_cpu(buf[0]);
				if (zero_or_saturated(len) || len > 63)
					return -1;
				c->u.name = malloc(len + 1);
				if (!c->u.name)
					return -1;
				rc = next_entry(c->u.name, fp, len);
				if (rc < 0)
					return -1;
				c->u.name[len] = 0;
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				if (context_read_and_validate
				    (&c->context[1], p, fp))
					return -1;
				break;
			case OCON_IBPKEY: {
				uint32_t pkey_lo, pkey_hi;

				rc = next_entry(buf, fp, sizeof(uint32_t) * 4);
				if (rc < 0)
					return -1;

				pkey_lo = le32_to_cpu(buf[2]);
				pkey_hi = le32_to_cpu(buf[3]);

				if (pkey_lo > UINT16_MAX || pkey_hi > UINT16_MAX)
					return -1;

				c->u.ibpkey.low_pkey  = pkey_lo;
				c->u.ibpkey.high_pkey = pkey_hi;

				/* we want c->u.ibpkey.subnet_prefix in network
				 * (big-endian) order, just memcpy it */
				memcpy(&c->u.ibpkey.subnet_prefix, buf,
				       sizeof(c->u.ibpkey.subnet_prefix));

				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			}
			case OCON_IBENDPORT: {
				uint32_t port;

				rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
				if (rc < 0)
					return -1;
				len = le32_to_cpu(buf[0]);
				if (len == 0 || len > IB_DEVICE_NAME_MAX - 1)
					return -1;

				port = le32_to_cpu(buf[1]);
				if (port > UINT8_MAX || port == 0)
					return -1;

				c->u.ibendport.dev_name = malloc(len + 1);
				if (!c->u.ibendport.dev_name)
					return -1;
				rc = next_entry(c->u.ibendport.dev_name, fp, len);
				if (rc < 0)
					return -1;
				c->u.ibendport.dev_name[len] = 0;
				c->u.ibendport.port = port;
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			}
			case OCON_PORT:
				rc = next_entry(buf, fp, sizeof(uint32_t) * 3);
				if (rc < 0)
					return -1;
				c->u.port.protocol = le32_to_cpu(buf[0]);
				c->u.port.low_port = le32_to_cpu(buf[1]);
				c->u.port.high_port = le32_to_cpu(buf[2]);
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_NODE:
				rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
				if (rc < 0)
					return -1;
				c->u.node.addr = buf[0]; /* network order */
				c->u.node.mask = buf[1]; /* network order */
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_FSUSE:
				rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
				if (rc < 0)
					return -1;
				c->v.behavior = le32_to_cpu(buf[0]);
				len = le32_to_cpu(buf[1]);
				if (zero_or_saturated(len))
					return -1;
				c->u.name = malloc(len + 1);
				if (!c->u.name)
					return -1;
				rc = next_entry(c->u.name, fp, len);
				if (rc < 0)
					return -1;
				c->u.name[len] = 0;
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
			case OCON_NODE6:{
				int k;

				rc = next_entry(buf, fp, sizeof(uint32_t) * 8);
				if (rc < 0)
					return -1;
				for (k = 0; k < 4; k++)
					 /* network order */
					c->u.node6.addr[k] = buf[k];
				for (k = 0; k < 4; k++)
					/* network order */
					c->u.node6.mask[k] = buf[k + 4];
				if (context_read_and_validate
				    (&c->context[0], p, fp))
					return -1;
				break;
				}
			default:{
				ERR(fp->handle, "Unknown SELinux ocontext");
				return -1;
				}
			}
		}
	}
	return 0;
}

static int ocontext_read(const struct policydb_compat_info *info,
	policydb_t *p, struct policy_file *fp)
{
	int rc = -1;
	switch (p->target_platform) {
	case SEPOL_TARGET_SELINUX:
		rc = ocontext_read_selinux(info, p, fp);
		break;
	case SEPOL_TARGET_XEN:
		rc = ocontext_read_xen(info, p, fp);
		break;
	default:
		ERR(fp->handle, "Unknown target");
	}
	return rc;
}

static int genfs_read(policydb_t * p, struct policy_file *fp)
{
	uint32_t buf[1];
	size_t nel, nel2, len, len2;
	genfs_t *genfs_p, *newgenfs, *genfs;
	size_t i, j;
	ocontext_t *l, *c, *newc = NULL;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		goto bad;
	nel = le32_to_cpu(buf[0]);
	genfs_p = NULL;
	for (i = 0; i < nel; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto bad;
		len = le32_to_cpu(buf[0]);
		if (zero_or_saturated(len))
			goto bad;
		newgenfs = calloc(1, sizeof(genfs_t));
		if (!newgenfs)
			goto bad;
		newgenfs->fstype = malloc(len + 1);
		if (!newgenfs->fstype) {
			free(newgenfs);
			goto bad;
		}
		rc = next_entry(newgenfs->fstype, fp, len);
		if (rc < 0) {
			free(newgenfs->fstype);
			free(newgenfs);
			goto bad;
		}
		newgenfs->fstype[len] = 0;
		for (genfs_p = NULL, genfs = p->genfs; genfs;
		     genfs_p = genfs, genfs = genfs->next) {
			if (strcmp(newgenfs->fstype, genfs->fstype) == 0) {
				ERR(fp->handle, "dup genfs fstype %s",
				    newgenfs->fstype);
				free(newgenfs->fstype);
				free(newgenfs);
				goto bad;
			}
			if (strcmp(newgenfs->fstype, genfs->fstype) < 0)
				break;
		}
		newgenfs->next = genfs;
		if (genfs_p)
			genfs_p->next = newgenfs;
		else
			p->genfs = newgenfs;
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			goto bad;
		nel2 = le32_to_cpu(buf[0]);
		for (j = 0; j < nel2; j++) {
			newc = calloc(1, sizeof(ocontext_t));
			if (!newc) {
				goto bad;
			}
			rc = next_entry(buf, fp, sizeof(uint32_t));
			if (rc < 0)
				goto bad;
			len = le32_to_cpu(buf[0]);
			if (zero_or_saturated(len))
				goto bad;
			newc->u.name = malloc(len + 1);
			if (!newc->u.name) {
				goto bad;
			}
			rc = next_entry(newc->u.name, fp, len);
			if (rc < 0)
				goto bad;
			newc->u.name[len] = 0;
			rc = next_entry(buf, fp, sizeof(uint32_t));
			if (rc < 0)
				goto bad;
			newc->v.sclass = le32_to_cpu(buf[0]);
			if (context_read_and_validate(&newc->context[0], p, fp))
				goto bad;
			for (l = NULL, c = newgenfs->head; c;
			     l = c, c = c->next) {
				if (!strcmp(newc->u.name, c->u.name) &&
				    (!c->v.sclass || !newc->v.sclass ||
				     newc->v.sclass == c->v.sclass)) {
					ERR(fp->handle, "dup genfs entry "
					    "(%s,%s)", newgenfs->fstype,
					    c->u.name);
					goto bad;
				}
				len = strlen(newc->u.name);
				len2 = strlen(c->u.name);
				if (len > len2)
					break;
			}
			newc->next = c;
			if (l)
				l->next = newc;
			else
				newgenfs->head = newc;
			/* clear newc after a new owner has the pointer */
			newc = NULL;
		}
	}

	return 0;

      bad:
	if (newc) {
		context_destroy(&newc->context[0]);
		context_destroy(&newc->context[1]);
		free(newc->u.name);
		free(newc);
	}
	return -1;
}

/*
 * Read a MLS level structure from a policydb binary 
 * representation file.
 */
static int mls_read_level(mls_level_t * lp, struct policy_file *fp)
{
	uint32_t buf[1];
	int rc;

	mls_level_init(lp);

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0) {
		ERR(fp->handle, "truncated level");
		goto bad;
	}
	lp->sens = le32_to_cpu(buf[0]);

	if (ksu_ebitmap_read(&lp->cat, fp)) {
		ERR(fp->handle, "error reading level categories");
		goto bad;
	}
	return 0;

      bad:
	return -EINVAL;
}

static int user_read(policydb_t * p, hashtab_t h, struct policy_file *fp)
{
	char *key = 0;
	user_datum_t *usrdatum;
	uint32_t buf[3];
	size_t len;
	int rc, to_read = 2;

	usrdatum = calloc(1, sizeof(user_datum_t));
	if (!usrdatum)
		return -1;

	if (policydb_has_boundary_feature(p))
		to_read = 3;

	rc = next_entry(buf, fp, sizeof(uint32_t) * to_read);
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		goto bad;

	usrdatum->s.value = le32_to_cpu(buf[1]);
	if (policydb_has_boundary_feature(p))
		usrdatum->bounds = le32_to_cpu(buf[2]);

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	if (p->policy_type == POLICY_KERN) {
		if (ksu_ebitmap_read(&usrdatum->roles.roles, fp))
			goto bad;
	} else {
		if (role_set_read(&usrdatum->roles, fp))
			goto bad;
	}

	/* users were not allowed in mls modules before version
	 * MOD_POLICYDB_VERSION_MLS_USERS, but they could have been
	 * required - the mls fields will be empty.  user declarations in
	 * non-mls modules will also have empty mls fields */
	if ((p->policy_type == POLICY_KERN
	     && p->policyvers >= POLICYDB_VERSION_MLS)
	    || (p->policy_type == POLICY_MOD
		&& p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policyvers < MOD_POLICYDB_VERSION_MLS_USERS)
	    || (p->policy_type == POLICY_BASE
		&& p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policyvers < MOD_POLICYDB_VERSION_MLS_USERS)) {
		if (mls_read_range_helper(&usrdatum->exp_range, fp))
			goto bad;
		if (mls_read_level(&usrdatum->exp_dfltlevel, fp))
			goto bad;
		if (p->policy_type != POLICY_KERN) {
			if (mls_range_to_semantic(&usrdatum->exp_range,
						  &usrdatum->range))
				goto bad;
			if (mls_level_to_semantic(&usrdatum->exp_dfltlevel,
						  &usrdatum->dfltlevel))
				goto bad;
		}
	} else if ((p->policy_type == POLICY_MOD
		    && p->policyvers >= MOD_POLICYDB_VERSION_MLS_USERS)
		   || (p->policy_type == POLICY_BASE
		       && p->policyvers >= MOD_POLICYDB_VERSION_MLS_USERS)) {
		if (mls_read_semantic_range_helper(&usrdatum->range, fp))
			goto bad;
		if (mls_read_semantic_level_helper(&usrdatum->dfltlevel, fp))
			goto bad;
	}

	if (hashtab_insert(h, key, usrdatum))
		goto bad;

	return 0;

      bad:
	user_destroy(key, usrdatum, NULL);
	return -1;
}

static int sens_read(policydb_t * p
		     __attribute__ ((unused)), hashtab_t h,
		     struct policy_file *fp)
{
	char *key = 0;
	level_datum_t *levdatum;
	uint32_t buf[2], len;
	int rc;

	levdatum = malloc(sizeof(level_datum_t));
	if (!levdatum)
		return -1;
	level_datum_init(levdatum);

	rc = next_entry(buf, fp, (sizeof(uint32_t) * 2));
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(len))
		goto bad;

	levdatum->isalias = le32_to_cpu(buf[1]);

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	levdatum->level = malloc(sizeof(mls_level_t));
	if (!levdatum->level || mls_read_level(levdatum->level, fp))
		goto bad;

	if (hashtab_insert(h, key, levdatum))
		goto bad;

	return 0;

      bad:
	sens_destroy(key, levdatum, NULL);
	return -1;
}

static int cat_read(policydb_t * p
		    __attribute__ ((unused)), hashtab_t h,
		    struct policy_file *fp)
{
	char *key = 0;
	cat_datum_t *catdatum;
	uint32_t buf[3], len;
	int rc;

	catdatum = malloc(sizeof(cat_datum_t));
	if (!catdatum)
		return -1;
	cat_datum_init(catdatum);

	rc = next_entry(buf, fp, (sizeof(uint32_t) * 3));
	if (rc < 0)
		goto bad;

	len = le32_to_cpu(buf[0]);
	if(zero_or_saturated(len))
		goto bad;

	catdatum->s.value = le32_to_cpu(buf[1]);
	catdatum->isalias = le32_to_cpu(buf[2]);

	key = malloc(len + 1);
	if (!key)
		goto bad;
	rc = next_entry(key, fp, len);
	if (rc < 0)
		goto bad;
	key[len] = 0;

	if (hashtab_insert(h, key, catdatum))
		goto bad;

	return 0;

      bad:
	cat_destroy(key, catdatum, NULL);
	return -1;
}

static int (*read_f[SYM_NUM]) (policydb_t * p, hashtab_t h,
			       struct policy_file * fp) = {
common_read, class_read, role_read, type_read, user_read,
	    ksu_cond_read_bool, sens_read, cat_read,};

/************** module reading functions below **************/

static avrule_t *avrule_read(policydb_t * p, struct policy_file *fp)
{
	unsigned int i;
	uint32_t buf[2], len;
	class_perm_node_t *cur, *tail = NULL;
	avrule_t *avrule;
	int rc;

	avrule = (avrule_t *) malloc(sizeof(avrule_t));
	if (!avrule)
		return NULL;

	avrule_init(avrule);

	rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
	if (rc < 0)
		goto bad;

	avrule->specified = le32_to_cpu(buf[0]);
	avrule->flags = le32_to_cpu(buf[1]);

	if (type_set_read(&avrule->stypes, fp))
		goto bad;

	if (type_set_read(&avrule->ttypes, fp))
		goto bad;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		goto bad;
	len = le32_to_cpu(buf[0]);

	for (i = 0; i < len; i++) {
		cur = (class_perm_node_t *) malloc(sizeof(class_perm_node_t));
		if (!cur)
			goto bad;
		class_perm_node_init(cur);

		rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
		if (rc < 0) {
			free(cur);
			goto bad;
		}

		cur->tclass = le32_to_cpu(buf[0]);
		cur->data = le32_to_cpu(buf[1]);

		if (!tail) {
			avrule->perms = cur;
		} else {
			tail->next = cur;
		}
		tail = cur;
	}

	if (avrule->specified & AVRULE_XPERMS) {
		uint8_t buf8;
		size_t nel = ARRAY_SIZE(avrule->xperms->perms);
		uint32_t buf32[nel];

		if (p->policyvers < MOD_POLICYDB_VERSION_XPERMS_IOCTL) {
			ERR(fp->handle,
			    "module policy version %u does not support ioctl"
			    " extended permissions rules and one was specified",
			    p->policyvers);
			goto bad;
		}

		if (p->target_platform != SEPOL_TARGET_SELINUX) {
			ERR(fp->handle,
			    "Target platform %s does not support ioctl"
			    " extended permissions rules and one was specified",
			    policydb_target_strings[p->target_platform]);
			goto bad;
		}

		avrule->xperms = calloc(1, sizeof(*avrule->xperms));
		if (!avrule->xperms)
			goto bad;

		rc = next_entry(&buf8, fp, sizeof(uint8_t));
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			goto bad;
		}
		avrule->xperms->specified = buf8;
		rc = next_entry(&buf8, fp, sizeof(uint8_t));
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			goto bad;
		}
		avrule->xperms->driver = buf8;
		rc = next_entry(buf32, fp, sizeof(uint32_t)*nel);
		if (rc < 0) {
			ERR(fp->handle, "truncated entry");
			goto bad;
		}
		for (i = 0; i < nel; i++)
			avrule->xperms->perms[i] = le32_to_cpu(buf32[i]);
	}

	return avrule;
      bad:
	if (avrule) {
		avrule_destroy(avrule);
		free(avrule);
	}
	return NULL;
}

static int range_read(policydb_t * p, struct policy_file *fp)
{
	uint32_t buf[2], nel;
	range_trans_t *rt = NULL;
	struct mls_range *r = NULL;
	range_trans_rule_t *rtr = NULL, *lrtr = NULL;
	unsigned int i;
	int new_rangetr = (p->policy_type == POLICY_KERN &&
			   p->policyvers >= POLICYDB_VERSION_RANGETRANS);
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	for (i = 0; i < nel; i++) {
		rt = calloc(1, sizeof(range_trans_t));
		if (!rt)
			return -1;
		rc = next_entry(buf, fp, (sizeof(uint32_t) * 2));
		if (rc < 0)
			goto err;
		rt->source_type = le32_to_cpu(buf[0]);
		if (!value_isvalid(rt->source_type, p->p_types.nprim))
			goto err;
		rt->target_type = le32_to_cpu(buf[1]);
		if (!value_isvalid(rt->target_type, p->p_types.nprim))
			goto err;
		if (new_rangetr) {
			rc = next_entry(buf, fp, (sizeof(uint32_t)));
			if (rc < 0)
				goto err;
			rt->target_class = le32_to_cpu(buf[0]);
			if (!value_isvalid(rt->target_class, p->p_classes.nprim))
				goto err;
		} else
			rt->target_class = p->process_class;
		r = calloc(1, sizeof(*r));
		if (!r)
			goto err;
		if (mls_read_range_helper(r, fp))
			goto err;

		if (p->policy_type == POLICY_KERN) {
			rc = hashtab_insert(p->range_tr, (hashtab_key_t)rt, r);
			if (rc)
				goto err;
			rt = NULL;
			r = NULL;
			continue;
		}

		/* Module policy: convert to range_trans_rule and discard. */
		rtr = malloc(sizeof(range_trans_rule_t));
		if (!rtr)
			goto err;
		range_trans_rule_init(rtr);

		if (ksu_ebitmap_set_bit(&rtr->stypes.types, rt->source_type - 1, 1))
			goto err;

		if (ksu_ebitmap_set_bit(&rtr->ttypes.types, rt->target_type - 1, 1))
			goto err;

		if (ksu_ebitmap_set_bit(&rtr->tclasses, rt->target_class - 1, 1))
			goto err;

		if (mls_range_to_semantic(r, &rtr->trange))
			goto err;

		if (lrtr)
			lrtr->next = rtr;
		else
			p->global->enabled->range_tr_rules = rtr;

		free(rt);
		rt = NULL;
		free(r);
		r = NULL;
		lrtr = rtr;
	}

	return 0;
err:
	free(rt);
	if (r) {
		mls_range_destroy(r);
		free(r);
	}
	if (rtr) {
		range_trans_rule_destroy(rtr);
		free(rtr);
	}
	return -1;
}

int avrule_read_list(policydb_t * p, avrule_t ** avrules,
		     struct policy_file *fp)
{
	unsigned int i;
	avrule_t *cur, *tail;
	uint32_t buf[1], len;
	int rc;

	*avrules = tail = NULL;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0) {
		return -1;
	}
	len = le32_to_cpu(buf[0]);

	for (i = 0; i < len; i++) {
		cur = avrule_read(p, fp);
		if (!cur) {
			return -1;
		}

		if (!tail) {
			*avrules = cur;
		} else {
			tail->next = cur;
		}
		tail = cur;
	}

	return 0;
}

static int role_trans_rule_read(policydb_t *p, role_trans_rule_t ** r,
				struct policy_file *fp)
{
	uint32_t buf[1], nel;
	unsigned int i;
	role_trans_rule_t *tr, *ltr;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	ltr = NULL;
	for (i = 0; i < nel; i++) {
		tr = malloc(sizeof(role_trans_rule_t));
		if (!tr) {
			return -1;
		}
		role_trans_rule_init(tr);

		if (ltr) {
			ltr->next = tr;
		} else {
			*r = tr;
		}

		if (role_set_read(&tr->roles, fp))
			return -1;

		if (type_set_read(&tr->types, fp))
			return -1;

		if (p->policyvers >= MOD_POLICYDB_VERSION_ROLETRANS) {
			if (ksu_ebitmap_read(&tr->classes, fp))
				return -1;
		} else {
			if (!p->process_class)
				return -1;
			if (ksu_ebitmap_set_bit(&tr->classes, p->process_class - 1, 1))
				return -1;
		}

		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			return -1;
		tr->new_role = le32_to_cpu(buf[0]);
		ltr = tr;
	}

	return 0;
}

static int role_allow_rule_read(role_allow_rule_t ** r, struct policy_file *fp)
{
	unsigned int i;
	uint32_t buf[1], nel;
	role_allow_rule_t *ra, *lra;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	lra = NULL;
	for (i = 0; i < nel; i++) {
		ra = malloc(sizeof(role_allow_rule_t));
		if (!ra) {
			return -1;
		}
		role_allow_rule_init(ra);

		if (lra) {
			lra->next = ra;
		} else {
			*r = ra;
		}

		if (role_set_read(&ra->roles, fp))
			return -1;

		if (role_set_read(&ra->new_roles, fp))
			return -1;

		lra = ra;
	}
	return 0;
}

static int filename_trans_rule_read(policydb_t *p, filename_trans_rule_t **r,
				    struct policy_file *fp)
{
	uint32_t buf[3], nel, i, len;
	unsigned int entries;
	filename_trans_rule_t *ftr, *lftr;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	lftr = NULL;
	for (i = 0; i < nel; i++) {
		ftr = malloc(sizeof(*ftr));
		if (!ftr)
			return -1;

		filename_trans_rule_init(ftr);

		if (lftr)
			lftr->next = ftr;
		else
			*r = ftr;
		lftr = ftr;

		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0)
			return -1;

		len = le32_to_cpu(buf[0]);
		if (zero_or_saturated(len))
			return -1;

		ftr->name = malloc(len + 1);
		if (!ftr->name)
			return -1;

		rc = next_entry(ftr->name, fp, len);
		if (rc)
			return -1;
		ftr->name[len] = 0;

		if (type_set_read(&ftr->stypes, fp))
			return -1;

		if (type_set_read(&ftr->ttypes, fp))
			return -1;

		if (p->policyvers >= MOD_POLICYDB_VERSION_SELF_TYPETRANS)
			entries = 3;
		else
			entries = 2;

		rc = next_entry(buf, fp, sizeof(uint32_t) * entries);
		if (rc < 0)
			return -1;
		ftr->tclass = le32_to_cpu(buf[0]);
		ftr->otype = le32_to_cpu(buf[1]);
		if (p->policyvers >= MOD_POLICYDB_VERSION_SELF_TYPETRANS)
			ftr->flags = le32_to_cpu(buf[2]);
	}

	return 0;
}

static int range_trans_rule_read(range_trans_rule_t ** r,
				 struct policy_file *fp)
{
	uint32_t buf[1], nel;
	unsigned int i;
	range_trans_rule_t *rt, *lrt = NULL;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	nel = le32_to_cpu(buf[0]);
	for (i = 0; i < nel; i++) {
		rt = malloc(sizeof(range_trans_rule_t));
		if (!rt) {
			return -1;
		}
		range_trans_rule_init(rt);

		if (lrt)
			lrt->next = rt;
		else
			*r = rt;

		if (type_set_read(&rt->stypes, fp))
			return -1;

		if (type_set_read(&rt->ttypes, fp))
			return -1;

		if (ksu_ebitmap_read(&rt->tclasses, fp))
			return -1;

		if (mls_read_semantic_range_helper(&rt->trange, fp))
			return -1;

		lrt = rt;
	}

	return 0;
}

static int scope_index_read(scope_index_t * scope_index,
			    unsigned int num_scope_syms, struct policy_file *fp)
{
	unsigned int i;
	uint32_t buf[1];
	int rc;

	for (i = 0; i < num_scope_syms; i++) {
		if (ksu_ebitmap_read(scope_index->scope + i, fp) < 0) {
			return -1;
		}
	}
	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	scope_index->class_perms_len = le32_to_cpu(buf[0]);
	if (is_saturated(scope_index->class_perms_len))
		return -1;
	if (scope_index->class_perms_len == 0) {
		scope_index->class_perms_map = NULL;
		return 0;
	}
	if ((scope_index->class_perms_map =
	     calloc(scope_index->class_perms_len,
		    sizeof(*scope_index->class_perms_map))) == NULL) {
		return -1;
	}
	for (i = 0; i < scope_index->class_perms_len; i++) {
		if (ksu_ebitmap_read(scope_index->class_perms_map + i, fp) < 0) {
			return -1;
		}
	}
	return 0;
}

static int avrule_decl_read(policydb_t * p, avrule_decl_t * decl,
			    unsigned int num_scope_syms, struct policy_file *fp)
{
	uint32_t buf[2], nprim, nel;
	unsigned int i, j;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
	if (rc < 0)
		return -1;
	decl->decl_id = le32_to_cpu(buf[0]);
	decl->enabled = le32_to_cpu(buf[1]);
	if (ksu_cond_read_list(p, &decl->cond_list, fp) == -1 ||
	    avrule_read_list(p, &decl->avrules, fp) == -1 ||
	    role_trans_rule_read(p, &decl->role_tr_rules, fp) == -1 ||
	    role_allow_rule_read(&decl->role_allow_rules, fp) == -1) {
		return -1;
	}

	if (p->policyvers >= MOD_POLICYDB_VERSION_FILENAME_TRANS &&
	    filename_trans_rule_read(p, &decl->filename_trans_rules, fp))
		return -1;

	if (p->policyvers >= MOD_POLICYDB_VERSION_RANGETRANS &&
	    range_trans_rule_read(&decl->range_tr_rules, fp) == -1) {
		return -1;
	}
	if (scope_index_read(&decl->required, num_scope_syms, fp) == -1 ||
	    scope_index_read(&decl->declared, num_scope_syms, fp) == -1) {
		return -1;
	}

	for (i = 0; i < num_scope_syms; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
		if (rc < 0) 
			return -1;
		nprim = le32_to_cpu(buf[0]);
		if (is_saturated(nprim))
			return -1;
		nel = le32_to_cpu(buf[1]);
		for (j = 0; j < nel; j++) {
			if (read_f[i] (p, decl->symtab[i].table, fp)) {
				return -1;
			}
		}
		decl->symtab[i].nprim = nprim;
	}
	return 0;
}

static int avrule_block_read(policydb_t * p,
			     avrule_block_t ** block,
			     unsigned int num_scope_syms,
			     struct policy_file *fp)
{
	avrule_block_t *last_block = NULL, *curblock;
	uint32_t buf[1], num_blocks, nel;
	int rc;

	assert(*block == NULL);

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		return -1;
	num_blocks = le32_to_cpu(buf[0]);
	nel = num_blocks;
	while (num_blocks > 0) {
		avrule_decl_t *last_decl = NULL, *curdecl;
		uint32_t num_decls;
		if ((curblock = calloc(1, sizeof(*curblock))) == NULL) {
			return -1;
		}
		rc = next_entry(buf, fp, sizeof(uint32_t));
		if (rc < 0) {
			free(curblock);
			return -1;
		}
		/* if this is the first block its non-optional, else its optional */
		if (num_blocks != nel)
			curblock->flags |= AVRULE_OPTIONAL;

		num_decls = le32_to_cpu(buf[0]);
		while (num_decls > 0) {
			if ((curdecl = avrule_decl_create(0)) == NULL) {
				avrule_block_destroy(curblock);
				return -1;
			}
			if (avrule_decl_read(p, curdecl, num_scope_syms, fp) ==
			    -1) {
				avrule_decl_destroy(curdecl);
				avrule_block_destroy(curblock);
				return -1;
			}
			if (curdecl->enabled) {
				if (curblock->enabled != NULL) {
					/* probably a corrupt file */
					avrule_decl_destroy(curdecl);
					avrule_block_destroy(curblock);
					return -1;
				}
				curblock->enabled = curdecl;
			}
			/* one must be careful to reconstruct the
			 * decl chain in its correct order */
			if (curblock->branch_list == NULL) {
				curblock->branch_list = curdecl;
			} else {
				assert(last_decl);
				last_decl->next = curdecl;
			}
			last_decl = curdecl;
			num_decls--;
		}

		if (*block == NULL) {
			*block = curblock;
		} else {
			assert(last_block);
			last_block->next = curblock;
		}
		last_block = curblock;

		num_blocks--;
	}

	return 0;
}

static int scope_read(policydb_t * p, int symnum, struct policy_file *fp)
{
	scope_datum_t *scope = NULL;
	uint32_t buf[2];
	char *key = NULL;
	size_t key_len;
	unsigned int i;
	hashtab_t h = p->scope[symnum].table;
	int rc;

	rc = next_entry(buf, fp, sizeof(uint32_t));
	if (rc < 0)
		goto cleanup;
	key_len = le32_to_cpu(buf[0]);
	if (zero_or_saturated(key_len))
		goto cleanup;
	key = malloc(key_len + 1);
	if (!key)
		goto cleanup;
	rc = next_entry(key, fp, key_len);
	if (rc < 0)
		goto cleanup;
	key[key_len] = '\0';

	/* ensure that there already exists a symbol with this key */
	if (hashtab_search(p->symtab[symnum].table, key) == NULL) {
		goto cleanup;
	}

	if ((scope = calloc(1, sizeof(*scope))) == NULL) {
		goto cleanup;
	}
	rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
	if (rc < 0)
		goto cleanup;
	scope->scope = le32_to_cpu(buf[0]);
	scope->decl_ids_len = le32_to_cpu(buf[1]);
	if (zero_or_saturated(scope->decl_ids_len)) {
		ERR(fp->handle, "invalid scope with no declaration");
		goto cleanup;
	}
	if ((scope->decl_ids =
	     calloc(scope->decl_ids_len, sizeof(uint32_t))) == NULL) {
		goto cleanup;
	}
	rc = next_entry(scope->decl_ids, fp, sizeof(uint32_t) * scope->decl_ids_len);
	if (rc < 0)
		goto cleanup;
	for (i = 0; i < scope->decl_ids_len; i++) {
		scope->decl_ids[i] = le32_to_cpu(scope->decl_ids[i]);
	}

	if (strcmp(key, "object_r") == 0 && h == p->p_roles_scope.table) {
		/* object_r was already added to this table in roles_init() */
		scope_destroy(key, scope, NULL);
	} else {
		if (hashtab_insert(h, key, scope)) {
			goto cleanup;
		}
	}

	return 0;

      cleanup:
	scope_destroy(key, scope, NULL);
	return -1;
}

static sepol_security_class_t policydb_string_to_security_class(
	struct policydb *policydb,
	const char *class_name)
{
	class_datum_t *tclass_datum;

	tclass_datum = hashtab_search(policydb->p_classes.table,
				      class_name);
	if (!tclass_datum)
		return 0;
	return tclass_datum->s.value;
}

static sepol_access_vector_t policydb_string_to_av_perm(
	struct policydb *policydb,
	sepol_security_class_t tclass,
	const char *perm_name)
{
	class_datum_t *tclass_datum;
	perm_datum_t *perm_datum;

	if (!tclass || tclass > policydb->p_classes.nprim)
		return 0;
	tclass_datum = policydb->class_val_to_struct[tclass - 1];

	perm_datum = (perm_datum_t *)
			hashtab_search(tclass_datum->permissions.table,
			perm_name);
	if (perm_datum != NULL)
		return UINT32_C(1) << (perm_datum->s.value - 1);

	if (tclass_datum->comdatum == NULL)
		return 0;

	perm_datum = (perm_datum_t *)
			hashtab_search(tclass_datum->comdatum->permissions.table,
			perm_name);

	if (perm_datum != NULL)
		return UINT32_C(1) << (perm_datum->s.value - 1);

	return 0;
}


/*
 * Read the configuration data from a policy database binary
 * representation file into a policy database structure.
 */
int ksu_policydb_read(policydb_t * p, struct policy_file *fp, unsigned verbose)
{

	unsigned int i, j, r_policyvers;
	uint32_t buf[5];
	size_t len, nprim, nel;
	char *policydb_str;
	const struct policydb_compat_info *info;
	unsigned int policy_type, bufindex;
	ebitmap_node_t *tnode;
	int rc;

	/* Read the magic number and string length. */
	rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
	if (rc < 0)
		return POLICYDB_ERROR;
	for (i = 0; i < 2; i++)
		buf[i] = le32_to_cpu(buf[i]);

	if (buf[0] == POLICYDB_MAGIC) {
		policy_type = POLICY_KERN;
	} else if (buf[0] == POLICYDB_MOD_MAGIC) {
		policy_type = POLICY_MOD;
	} else {
		ERR(fp->handle, "policydb magic number %#08x does not "
		    "match expected magic number %#08x or %#08x",
		    buf[0], POLICYDB_MAGIC, POLICYDB_MOD_MAGIC);
		return POLICYDB_ERROR;
	}

	len = buf[1];
	if (len == 0 || len > POLICYDB_STRING_MAX_LENGTH) {
		ERR(fp->handle, "policydb string length %s ", len ? "too long" : "zero");
		return POLICYDB_ERROR;
	}

	policydb_str = malloc(len + 1);
	if (!policydb_str) {
		ERR(fp->handle, "unable to allocate memory for policydb "
		    "string of length %zu", len);
		return POLICYDB_ERROR;
	}
	rc = next_entry(policydb_str, fp, len);
	if (rc < 0) {
		ERR(fp->handle, "truncated policydb string identifier");
		free(policydb_str);
		return POLICYDB_ERROR;
	}
	policydb_str[len] = 0;

	if (policy_type == POLICY_KERN) {
		for (i = 0; i < POLICYDB_TARGET_SZ; i++) {
			if ((strcmp(policydb_str, policydb_target_strings[i])
				== 0)) {
				policydb_set_target_platform(p, i);
				break;
			}
		}

		if (i == POLICYDB_TARGET_SZ) {
			ERR(fp->handle, "cannot find a valid target for policy "
				"string %s", policydb_str);
			free(policydb_str);
			return POLICYDB_ERROR;
		}
	} else {
		if (strcmp(policydb_str, POLICYDB_MOD_STRING)) {
			ERR(fp->handle, "invalid string identifier %s",
				policydb_str);
			free(policydb_str);
			return POLICYDB_ERROR;
		}
	}

	/* Done with policydb_str. */
	free(policydb_str);
	policydb_str = NULL;

	/* Read the version, config, and table sizes (and policy type if it's a module). */
	if (policy_type == POLICY_KERN)
		nel = 4;
	else
		nel = 5;

	rc = next_entry(buf, fp, sizeof(uint32_t) * nel);
	if (rc < 0)
		return POLICYDB_ERROR;
	for (i = 0; i < nel; i++)
		buf[i] = le32_to_cpu(buf[i]);

	bufindex = 0;

	if (policy_type == POLICY_MOD) {
		/* We know it's a module but not whether it's a base
		   module or regular binary policy module.  buf[0]
		   tells us which. */
		policy_type = buf[bufindex];
		if (policy_type != POLICY_MOD && policy_type != POLICY_BASE) {
			ERR(fp->handle, "unknown module type: %#08x",
			    policy_type);
			return POLICYDB_ERROR;
		}
		bufindex++;
	}

	r_policyvers = buf[bufindex];
	if (policy_type == POLICY_KERN) {
		if (r_policyvers < POLICYDB_VERSION_MIN ||
		    r_policyvers > POLICYDB_VERSION_MAX) {
			ERR(fp->handle, "policydb version %d does not match "
			    "my version range %d-%d", buf[bufindex],
			    POLICYDB_VERSION_MIN, POLICYDB_VERSION_MAX);
			return POLICYDB_ERROR;
		}
	} else if (policy_type == POLICY_BASE || policy_type == POLICY_MOD) {
		if (r_policyvers < MOD_POLICYDB_VERSION_MIN ||
		    r_policyvers > MOD_POLICYDB_VERSION_MAX) {
			ERR(fp->handle, "policydb module version %d does "
			    "not match my version range %d-%d",
			    buf[bufindex], MOD_POLICYDB_VERSION_MIN,
			    MOD_POLICYDB_VERSION_MAX);
			return POLICYDB_ERROR;
		}
	} else {
		assert(0);
	}
	bufindex++;

	/* Set the policy type and version from the read values. */
	p->policy_type = policy_type;
	p->policyvers = r_policyvers;

	if (buf[bufindex] & POLICYDB_CONFIG_MLS) {
		p->mls = 1;
	} else {
		p->mls = 0;
	}

	p->handle_unknown = buf[bufindex] & POLICYDB_CONFIG_UNKNOWN_MASK;

	bufindex++;

	info = policydb_lookup_compat(r_policyvers, policy_type,
					p->target_platform);
	if (!info) {
		ERR(fp->handle, "unable to find policy compat info "
		    "for version %d", r_policyvers);
		goto bad;
	}

	if (buf[bufindex] != info->sym_num
	    || buf[bufindex + 1] != info->ocon_num) {
		ERR(fp->handle,
		    "policydb table sizes (%d,%d) do not " "match mine (%d,%d)",
		    buf[bufindex], buf[bufindex + 1], info->sym_num,
		    info->ocon_num);
		goto bad;
	}

	if (p->policy_type == POLICY_MOD) {
		/* Get the module name and version */
		if ((rc = next_entry(buf, fp, sizeof(uint32_t))) < 0) {
			goto bad;
		}
		len = le32_to_cpu(buf[0]);
		if (zero_or_saturated(len))
			goto bad;
		if ((p->name = malloc(len + 1)) == NULL) {
			goto bad;
		}
		if ((rc = next_entry(p->name, fp, len)) < 0) {
			goto bad;
		}
		p->name[len] = '\0';
		if ((rc = next_entry(buf, fp, sizeof(uint32_t))) < 0) {
			goto bad;
		}
		len = le32_to_cpu(buf[0]);
		if (zero_or_saturated(len))
			goto bad;
		if ((p->version = malloc(len + 1)) == NULL) {
			goto bad;
		}
		if ((rc = next_entry(p->version, fp, len)) < 0) {
			goto bad;
		}
		p->version[len] = '\0';
	}

	if ((p->policyvers >= POLICYDB_VERSION_POLCAP &&
	     p->policy_type == POLICY_KERN) ||
	    (p->policyvers >= MOD_POLICYDB_VERSION_POLCAP &&
	     p->policy_type == POLICY_BASE) ||
	    (p->policyvers >= MOD_POLICYDB_VERSION_POLCAP &&
	     p->policy_type == POLICY_MOD)) {
		if (ksu_ebitmap_read(&p->policycaps, fp))
			goto bad;
	}

	if (p->policyvers >= POLICYDB_VERSION_PERMISSIVE &&
	    p->policy_type == POLICY_KERN) {
		if (ksu_ebitmap_read(&p->permissive_map, fp))
			goto bad;
	}

	for (i = 0; i < info->sym_num; i++) {
		rc = next_entry(buf, fp, sizeof(uint32_t) * 2);
		if (rc < 0)
			goto bad;
		nprim = le32_to_cpu(buf[0]);
		if (is_saturated(nprim))
			goto bad;
		nel = le32_to_cpu(buf[1]);
		if (nel && !nprim) {
			ERR(fp->handle, "unexpected items in symbol table with no symbol");
			goto bad;
		}
		for (j = 0; j < nel; j++) {
			if (read_f[i] (p, p->symtab[i].table, fp))
				goto bad;
		}

		p->symtab[i].nprim = nprim;
	}

	switch (p->target_platform) {
	case SEPOL_TARGET_SELINUX:
		p->process_class = policydb_string_to_security_class(p, "process");
		p->dir_class = policydb_string_to_security_class(p, "dir");
		break;
	case SEPOL_TARGET_XEN:
		p->process_class = policydb_string_to_security_class(p, "domain");
		break;
	default:
		break;
	}

	if (policy_type == POLICY_KERN) {
		if (ksu_avtab_read(&p->te_avtab, fp, r_policyvers))
			goto bad;
		if (r_policyvers >= POLICYDB_VERSION_BOOL)
			if (ksu_cond_read_list(p, &p->cond_list, fp))
				goto bad;
		if (role_trans_read(p, fp))
			goto bad;
		if (role_allow_read(&p->role_allow, fp))
			goto bad;
		if (r_policyvers >= POLICYDB_VERSION_FILENAME_TRANS &&
		    filename_trans_read(p, fp))
			goto bad;
	} else {
		/* first read the AV rule blocks, then the scope tables */
		avrule_block_destroy(p->global);
		p->global = NULL;
		if (avrule_block_read(p, &p->global, info->sym_num, fp) == -1) {
			goto bad;
		}
		if (p->global == NULL) {
			ERR(fp->handle, "no avrule block in policy");
			goto bad;
		}
		for (i = 0; i < info->sym_num; i++) {
			if ((rc = next_entry(buf, fp, sizeof(uint32_t))) < 0) {
				goto bad;
			}
			nel = le32_to_cpu(buf[0]);
			for (j = 0; j < nel; j++) {
				if (scope_read(p, i, fp))
					goto bad;
			}
		}

	}

	if (policydb_index_decls(fp->handle, p))
		goto bad;

	if (policydb_index_classes(p))
		goto bad;

	switch (p->target_platform) {
	case SEPOL_TARGET_SELINUX:
		/* fall through */
	case SEPOL_TARGET_XEN:
		p->process_trans = policydb_string_to_av_perm(p, p->process_class,
							      "transition");
		p->process_trans_dyntrans = p->process_trans |
			policydb_string_to_av_perm(p, p->process_class,
						   "dyntransition");
		break;
	default:
		break;
	}

	if (policydb_index_others(fp->handle, p, verbose))
		goto bad;

	if (ocontext_read(info, p, fp) == -1) {
		goto bad;
	}

	if (genfs_read(p, fp) == -1) {
		goto bad;
	}

	if ((p->policy_type == POLICY_KERN
	     && p->policyvers >= POLICYDB_VERSION_MLS)
	    || (p->policy_type == POLICY_BASE
		&& p->policyvers >= MOD_POLICYDB_VERSION_MLS
		&& p->policyvers < MOD_POLICYDB_VERSION_RANGETRANS)) {
		if (range_read(p, fp)) {
			goto bad;
		}
	}

	if (policy_type == POLICY_KERN) {
		p->type_attr_map = calloc(p->p_types.nprim, sizeof(ebitmap_t));
		p->attr_type_map = calloc(p->p_types.nprim, sizeof(ebitmap_t));
		if (!p->type_attr_map || !p->attr_type_map)
			goto bad;
		for (i = 0; i < p->p_types.nprim; i++) {
			if (r_policyvers >= POLICYDB_VERSION_AVTAB) {
				if (ksu_ebitmap_read(&p->type_attr_map[i], fp))
					goto bad;
				ebitmap_for_each_positive_bit(&p->type_attr_map[i],
							 tnode, j) {
					if (i == j)
						continue;

					if (j >= p->p_types.nprim)
						goto bad;

					if (ksu_ebitmap_set_bit
					    (&p->attr_type_map[j], i, 1))
						goto bad;
				}
			}
			/* add the type itself as the degenerate case */
			if (ksu_ebitmap_set_bit(&p->type_attr_map[i], i, 1))
				goto bad;
			if (p->type_val_to_struct[i] && p->type_val_to_struct[i]->flavor != TYPE_ATTRIB) {
				if (ksu_ebitmap_set_bit(&p->attr_type_map[i], i, 1))
					goto bad;
			}
		}
	}

	if (validate_policydb(fp->handle, p))
		goto bad;

	return POLICYDB_SUCCESS;
      bad:
	return POLICYDB_ERROR;
}

int policydb_reindex_users(policydb_t * p)
{
	unsigned int i = SYM_USERS;

	if (p->user_val_to_struct)
		free(p->user_val_to_struct);
	if (p->sym_val_to_name[i])
		free(p->sym_val_to_name[i]);

	p->user_val_to_struct = (user_datum_t **)
	    calloc(p->p_users.nprim, sizeof(user_datum_t *));
	if (!p->user_val_to_struct)
		return -1;

	p->sym_val_to_name[i] = (char **)
	    calloc(p->symtab[i].nprim, sizeof(char *));
	if (!p->sym_val_to_name[i])
		return -1;

	if (ksu_hashtab_map(p->symtab[i].table, index_f[i], p))
		return -1;

	/* Expand user roles for context validity checking */
	if (ksu_hashtab_map(p->p_users.table, policydb_user_cache, p))
		return -1;

	return 0;
}

void policy_file_init(policy_file_t *pf)
{
	memset(pf, 0, sizeof(policy_file_t));
}

int policydb_set_target_platform(policydb_t *p, int platform)
{
	if (platform == SEPOL_TARGET_SELINUX)
		p->target_platform = SEPOL_TARGET_SELINUX;
	else if (platform == SEPOL_TARGET_XEN)
		p->target_platform = SEPOL_TARGET_XEN;
	else
		return -1;

	return 0;
}

// int policydb_sort_ocontexts(policydb_t *p)
// {
// 	return sort_ocontexts(p);
// }

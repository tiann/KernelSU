/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sepol/policydb/conditional.h>
#include <sepol/errcodes.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_log.h"
#include "cil_mem.h"
#include "cil_tree.h"
#include "cil_list.h"
#include "cil_post.h"
#include "cil_policy.h"
#include "cil_verify.h"
#include "cil_symtab.h"

#define GEN_REQUIRE_ATTR "cil_gen_require" /* Also in libsepol/src/module_to_cil.c */
#define TYPEATTR_INFIX "_typeattr_"        /* Also in libsepol/src/module_to_cil.c */

struct fc_data {
	unsigned int meta;
	size_t stem_len;
	size_t str_len;
};

static int __cil_expr_to_bitmap(struct cil_list *expr, ebitmap_t *out, int max, struct cil_db *db);
static int __cil_expr_list_to_bitmap(struct cil_list *expr_list, ebitmap_t *out, int max, struct cil_db *db);

static int cats_compare(struct cil_cats *a, struct cil_cats *b)
{
	struct cil_list_item *i, *j;
	int rc;

	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;

	/* Expects cat expression to have been evaluated */
	cil_list_for_each(i, a->datum_expr) {
		cil_list_for_each(j, b->datum_expr) {
			rc = strcmp(DATUM(i->data)->fqn, DATUM(j->data)->fqn);
			if (!rc) return rc;
		}
	}
	return 0;
}

static int level_compare(struct cil_level *a, struct cil_level *b)
{
	int rc;

	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;

	if (a->sens != b->sens) {
		rc = strcmp(DATUM(a->sens)->fqn, DATUM(b->sens)->fqn);
		if (rc != 0) return rc;
	}
	if (a->cats != b->cats) {
		return cats_compare(a->cats, b->cats);
	}
	return 0;
}

static int range_compare(struct cil_levelrange *a, struct cil_levelrange *b)
{
	int rc;

	if (a == b) return 0;
	if (!a) return -1;
	if (!b) return 1;

	if (a->low != b->low) {
		rc = level_compare(a->low, b->low);
		if (rc != 0) return rc;
	}
	if (a->high != b->high) {
		return level_compare(a->high, b->high);
	}
	return 0;
}

static int context_compare(struct cil_context *a, struct cil_context *b)
{
	int rc;

	if (a->user != b->user) {
		rc = strcmp(DATUM(a->user)->fqn, DATUM(b->user)->fqn);
		if (rc != 0) return rc;
	}
	if (a->role != b->role) {
		rc = strcmp(DATUM(a->role)->fqn, DATUM(b->role)->fqn);
		if (rc != 0) return rc;
	}
	if (a->type != b->type) {
		rc = strcmp(DATUM(a->type)->fqn, DATUM(b->type)->fqn);
		if (rc != 0) return rc;
	}
	if (a->range != b->range) {
		return range_compare(a->range, b->range);
	}
	return 0;
}

static int cil_verify_is_list(struct cil_list *list, enum cil_flavor flavor)
{
	struct cil_list_item *curr;

	cil_list_for_each(curr, list) {
		switch (curr->flavor) {
		case CIL_LIST:
			return CIL_FALSE;
			break;
		case CIL_OP:
			return CIL_FALSE;
			break;
		default:
			if (flavor == CIL_CAT) {
				struct cil_symtab_datum *d = curr->data;
				struct cil_tree_node *n = d->nodes->head->data;
				if (n->flavor == CIL_CATSET) {
					return CIL_FALSE;
				}
			}
			break;
		}	
	}
	return CIL_TRUE;
}

static void cil_post_fc_fill_data(struct fc_data *fc, const char *path)
{
	size_t c = 0;
	fc->meta = 0;
	fc->stem_len = 0;
	fc->str_len = 0;
	
	while (path[c] != '\0') {
		switch (path[c]) {
		case '.':
		case '^':
		case '$':
		case '?':
		case '*':
		case '+':
		case '|':
		case '[':
		case '(':
		case '{':
			fc->meta = 1;
			break;
		case '\\':
			c++;
			if (path[c] == '\0') {
				if (!fc->meta) {
					fc->stem_len++;
				}
				fc->str_len++;
				return;
			}
			/* FALLTHRU */
		default:
			if (!fc->meta) {
				fc->stem_len++;
			}
			break;
		}
		fc->str_len++;
		c++;
	}
}

int cil_post_filecon_compare(const void *a, const void *b)
{
	int rc = 0;
	struct cil_filecon *a_filecon = *(struct cil_filecon**)a;
	struct cil_filecon *b_filecon = *(struct cil_filecon**)b;
	struct fc_data *a_data = cil_malloc(sizeof(*a_data));
	struct fc_data *b_data = cil_malloc(sizeof(*b_data));
	char *a_path = cil_malloc(strlen(a_filecon->path_str) + 1);
	char *b_path = cil_malloc(strlen(b_filecon->path_str) + 1);
	a_path[0] = '\0';
	b_path[0] = '\0';
	strcat(a_path, a_filecon->path_str);
	strcat(b_path, b_filecon->path_str);
	cil_post_fc_fill_data(a_data, a_path);
	cil_post_fc_fill_data(b_data, b_path);
	if (a_data->meta && !b_data->meta) {
		rc = -1;
	} else if (b_data->meta && !a_data->meta) {
		rc = 1;
	} else if (a_data->stem_len < b_data->stem_len) {
		rc = -1;
	} else if (b_data->stem_len < a_data->stem_len) {
		rc = 1;
	} else if (a_data->str_len < b_data->str_len) {
		rc = -1;
	} else if (b_data->str_len < a_data->str_len) {
		rc = 1;
	} else if (a_filecon->type < b_filecon->type) {
		rc = -1;
	} else if (b_filecon->type < a_filecon->type) {
		rc = 1;
	} else {
		rc = strcmp(a_filecon->path_str, b_filecon->path_str);
	}

	free(a_path);
	free(b_path);
	free(a_data);
	free(b_data);

	return rc;
}

int cil_post_ibpkeycon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_ibpkeycon *aibpkeycon = *(struct cil_ibpkeycon **)a;
	struct cil_ibpkeycon *bibpkeycon = *(struct cil_ibpkeycon **)b;

	rc = strcmp(aibpkeycon->subnet_prefix_str, bibpkeycon->subnet_prefix_str);
	if (rc)
		return rc;

	rc = (aibpkeycon->pkey_high - aibpkeycon->pkey_low)
		- (bibpkeycon->pkey_high - bibpkeycon->pkey_low);
	if (rc == 0) {
		if (aibpkeycon->pkey_low < bibpkeycon->pkey_low)
			rc = -1;
		else if (bibpkeycon->pkey_low < aibpkeycon->pkey_low)
			rc = 1;
	}

	return rc;
}

int cil_post_portcon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_portcon *aportcon = *(struct cil_portcon**)a;
	struct cil_portcon *bportcon = *(struct cil_portcon**)b;

	rc = (aportcon->port_high - aportcon->port_low) 
		- (bportcon->port_high - bportcon->port_low);
	if (rc == 0) {
		if (aportcon->port_low < bportcon->port_low) {
			rc = -1;
		} else if (bportcon->port_low < aportcon->port_low) {
			rc = 1;
		} else if (aportcon->proto < bportcon->proto) {
			rc = -1;
		} else if (aportcon->proto > bportcon->proto) {
			rc = 1;
		}
	}

	return rc;
}

int cil_post_genfscon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_genfscon *agenfscon = *(struct cil_genfscon**)a;
	struct cil_genfscon *bgenfscon = *(struct cil_genfscon**)b;

	rc = strcmp(agenfscon->fs_str, bgenfscon->fs_str);
	if (rc == 0) {
		rc = strcmp(agenfscon->path_str, bgenfscon->path_str);
	}

	return rc;
}

int cil_post_netifcon_compare(const void *a, const void *b)
{
	struct cil_netifcon *anetifcon = *(struct cil_netifcon**)a;
	struct cil_netifcon *bnetifcon = *(struct cil_netifcon**)b;

	return  strcmp(anetifcon->interface_str, bnetifcon->interface_str);
}

int cil_post_ibendportcon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;

	struct cil_ibendportcon *aibendportcon = *(struct cil_ibendportcon **)a;
	struct cil_ibendportcon *bibendportcon = *(struct cil_ibendportcon **)b;

	rc = strcmp(aibendportcon->dev_name_str, bibendportcon->dev_name_str);
	if (rc)
		return rc;

	if (aibendportcon->port < bibendportcon->port)
		return -1;
	else if (bibendportcon->port < aibendportcon->port)
		return 1;

	return rc;
}

int cil_post_nodecon_compare(const void *a, const void *b)
{
	struct cil_nodecon *anodecon;
	struct cil_nodecon *bnodecon;
	anodecon = *(struct cil_nodecon**)a;
	bnodecon = *(struct cil_nodecon**)b;

	/* sort ipv4 before ipv6 */
	if (anodecon->addr->family != bnodecon->addr->family) {
		if (anodecon->addr->family == AF_INET) {
			return -1;
		} else {
			return 1;
		}
	}

	/* most specific netmask goes first, then order by ip addr */
	if (anodecon->addr->family == AF_INET) {
		int rc = memcmp(&anodecon->mask->ip.v4, &bnodecon->mask->ip.v4, sizeof(anodecon->mask->ip.v4));
		if (rc != 0) {
			return -1 * rc;
		}
		return memcmp(&anodecon->addr->ip.v4, &bnodecon->addr->ip.v4, sizeof(anodecon->addr->ip.v4));
	} else {
		int rc = memcmp(&anodecon->mask->ip.v6, &bnodecon->mask->ip.v6, sizeof(anodecon->mask->ip.v6));
		if (rc != 0) {
			return -1 * rc;
		}
		return memcmp(&anodecon->addr->ip.v6, &bnodecon->addr->ip.v6, sizeof(anodecon->addr->ip.v6));
	}
}

static int cil_post_pirqcon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_pirqcon *apirqcon = *(struct cil_pirqcon**)a;
	struct cil_pirqcon *bpirqcon = *(struct cil_pirqcon**)b;

	if (apirqcon->pirq < bpirqcon->pirq) {
		rc = -1;
	} else if (bpirqcon->pirq < apirqcon->pirq) {
		rc = 1;
	} else {
		rc = 0;
	}

	return rc;
}

static int cil_post_iomemcon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_iomemcon *aiomemcon = *(struct cil_iomemcon**)a;
	struct cil_iomemcon *biomemcon = *(struct cil_iomemcon**)b;

	rc = (aiomemcon->iomem_high - aiomemcon->iomem_low) 
		- (biomemcon->iomem_high - biomemcon->iomem_low);
	if (rc == 0) {
		if (aiomemcon->iomem_low < biomemcon->iomem_low) {
			rc = -1;
		} else if (biomemcon->iomem_low < aiomemcon->iomem_low) {
			rc = 1;
		}
	}

	return rc;
}

static int cil_post_ioportcon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_ioportcon *aioportcon = *(struct cil_ioportcon**)a;
	struct cil_ioportcon *bioportcon = *(struct cil_ioportcon**)b;

	rc = (aioportcon->ioport_high - aioportcon->ioport_low) 
		- (bioportcon->ioport_high - bioportcon->ioport_low);
	if (rc == 0) {
		if (aioportcon->ioport_low < bioportcon->ioport_low) {
			rc = -1;
		} else if (bioportcon->ioport_low < aioportcon->ioport_low) {
			rc = 1;
		}
	}

	return rc;
}

static int cil_post_pcidevicecon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_pcidevicecon *apcidevicecon = *(struct cil_pcidevicecon**)a;
	struct cil_pcidevicecon *bpcidevicecon = *(struct cil_pcidevicecon**)b;

	if (apcidevicecon->dev < bpcidevicecon->dev) {
		rc = -1;
	} else if (bpcidevicecon->dev < apcidevicecon->dev) {
		rc = 1;
	} else {
		rc = 0;
	}

	return rc;
}

static int cil_post_devicetreecon_compare(const void *a, const void *b)
{
	int rc = SEPOL_ERR;
	struct cil_devicetreecon *adevicetreecon = *(struct cil_devicetreecon**)a;
	struct cil_devicetreecon *bdevicetreecon = *(struct cil_devicetreecon**)b;

	rc = strcmp(adevicetreecon->path, bdevicetreecon->path);

	return rc;
}

int cil_post_fsuse_compare(const void *a, const void *b)
{
	int rc;
	struct cil_fsuse *afsuse;
	struct cil_fsuse *bfsuse;
	afsuse = *(struct cil_fsuse**)a;
	bfsuse = *(struct cil_fsuse**)b;
	if (afsuse->type < bfsuse->type) {
		rc = -1;
	} else if (bfsuse->type < afsuse->type) {
		rc = 1;
	} else {
		rc = strcmp(afsuse->fs_str, bfsuse->fs_str);
	}
	return rc;
}

static int cil_post_filecon_context_compare(const void *a, const void *b)
{
	struct cil_filecon *a_filecon = *(struct cil_filecon**)a;
	struct cil_filecon *b_filecon = *(struct cil_filecon**)b;
	return context_compare(a_filecon->context, b_filecon->context);
}

static int cil_post_ibpkeycon_context_compare(const void *a, const void *b)
{
	struct cil_ibpkeycon *a_ibpkeycon = *(struct cil_ibpkeycon **)a;
	struct cil_ibpkeycon *b_ibpkeycon = *(struct cil_ibpkeycon **)b;
	return context_compare(a_ibpkeycon->context, b_ibpkeycon->context);
}

static int cil_post_portcon_context_compare(const void *a, const void *b)
{
	struct cil_portcon *a_portcon = *(struct cil_portcon**)a;
	struct cil_portcon *b_portcon = *(struct cil_portcon**)b;
	return context_compare(a_portcon->context, b_portcon->context);
}

static int cil_post_genfscon_context_compare(const void *a, const void *b)
{
	struct cil_genfscon *a_genfscon = *(struct cil_genfscon**)a;
	struct cil_genfscon *b_genfscon = *(struct cil_genfscon**)b;
	return context_compare(a_genfscon->context, b_genfscon->context);
}

static int cil_post_netifcon_context_compare(const void *a, const void *b)
{
	int rc;
	struct cil_netifcon *a_netifcon = *(struct cil_netifcon**)a;
	struct cil_netifcon *b_netifcon = *(struct cil_netifcon**)b;
	rc = context_compare(a_netifcon->if_context, b_netifcon->if_context);
	if (rc != 0) {
		return rc;
	}
	return context_compare(a_netifcon->packet_context, b_netifcon->packet_context);
}

static int cil_post_ibendportcon_context_compare(const void *a, const void *b)
{
	struct cil_ibendportcon *a_ibendportcon = *(struct cil_ibendportcon **)a;
	struct cil_ibendportcon *b_ibendportcon = *(struct cil_ibendportcon **)b;
	return context_compare(a_ibendportcon->context, b_ibendportcon->context);
}

static int cil_post_nodecon_context_compare(const void *a, const void *b)
{
	struct cil_nodecon *a_nodecon = *(struct cil_nodecon **)a;
	struct cil_nodecon *b_nodecon = *(struct cil_nodecon **)b;
	return context_compare(a_nodecon->context, b_nodecon->context);
}

static int cil_post_pirqcon_context_compare(const void *a, const void *b)
{
	struct cil_pirqcon *a_pirqcon = *(struct cil_pirqcon**)a;
	struct cil_pirqcon *b_pirqcon = *(struct cil_pirqcon**)b;
	return context_compare(a_pirqcon->context, b_pirqcon->context);
}

static int cil_post_iomemcon_context_compare(const void *a, const void *b)
{
	struct cil_iomemcon *a_iomemcon = *(struct cil_iomemcon**)a;
	struct cil_iomemcon *b_iomemcon = *(struct cil_iomemcon**)b;
	return context_compare(a_iomemcon->context, b_iomemcon->context);
}

static int cil_post_ioportcon_context_compare(const void *a, const void *b)
{
	struct cil_ioportcon *a_ioportcon = *(struct cil_ioportcon**)a;
	struct cil_ioportcon *b_ioportcon = *(struct cil_ioportcon**)b;
	return context_compare(a_ioportcon->context, b_ioportcon->context);
}

static int cil_post_pcidevicecon_context_compare(const void *a, const void *b)
{
	struct cil_pcidevicecon *a_pcidevicecon = *(struct cil_pcidevicecon**)a;
	struct cil_pcidevicecon *b_pcidevicecon = *(struct cil_pcidevicecon**)b;
	return context_compare(a_pcidevicecon->context, b_pcidevicecon->context);
}

static int cil_post_devicetreecon_context_compare(const void *a, const void *b)
{
	struct cil_devicetreecon *a_devicetreecon = *(struct cil_devicetreecon**)a;
	struct cil_devicetreecon *b_devicetreecon = *(struct cil_devicetreecon**)b;
	return context_compare(a_devicetreecon->context, b_devicetreecon->context);
}

static int cil_post_fsuse_context_compare(const void *a, const void *b)
{
	struct cil_fsuse *a_fsuse = *(struct cil_fsuse**)a;
	struct cil_fsuse *b_fsuse = *(struct cil_fsuse**)b;
	return context_compare(a_fsuse->context, b_fsuse->context);
}

static int __cil_post_db_count_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct cil_db *db = extra_args;

	switch(node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_CLASS: {
		struct cil_class *class = node->data;
		if (class->datum.nodes->head->data == node) {
			// Multiple nodes can point to the same datum. Only count once.
			db->num_classes++;
		}
		break;
	}
	case CIL_TYPE: {
		struct cil_type *type = node->data;
		if (type->datum.nodes->head->data == node) {
			// Multiple nodes can point to the same datum. Only count once.
			type->value = db->num_types;
			db->num_types++;
			db->num_types_and_attrs++;
		}
		break;
	}
	case CIL_TYPEATTRIBUTE: {
		struct cil_typeattribute *attr = node->data;
		if (attr->datum.nodes->head->data == node) {
			// Multiple nodes can point to the same datum. Only count once.
			db->num_types_and_attrs++;
		}
		break;
	}

	case CIL_ROLE: {
		struct cil_role *role = node->data;
		if (role->datum.nodes->head->data == node) {
			// Multiple nodes can point to the same datum. Only count once.
			role->value = db->num_roles;
			db->num_roles++;
		}
		break;
	}
	case CIL_USER: {
		struct cil_user *user = node->data;
		if (user->datum.nodes->head->data == node) {
			// multiple AST nodes can point to the same cil_user data (like if
			// copied from a macro). This check ensures we only count the
			// duplicates once
			user->value = db->num_users;
			db->num_users++;
		}
		break;
	}
	case CIL_NETIFCON:
		db->netifcon->count++;
		break;
	case CIL_GENFSCON:
		db->genfscon->count++;
		break;
	case CIL_FILECON:
		db->filecon->count++;
		break;
	case CIL_NODECON:
		db->nodecon->count++;
		break;
	case CIL_IBPKEYCON:
		db->ibpkeycon->count++;
		break;
	case CIL_IBENDPORTCON:
		db->ibendportcon->count++;
		break;
	case CIL_PORTCON:
		db->portcon->count++;
		break;
	case CIL_PIRQCON:
		db->pirqcon->count++;
		break;
	case CIL_IOMEMCON:
		db->iomemcon->count++;
		break;
	case CIL_IOPORTCON:
		db->ioportcon->count++;
		break;
	case CIL_PCIDEVICECON:
		db->pcidevicecon->count++;
		break;	
	case CIL_DEVICETREECON:
		db->devicetreecon->count++;
		break;
	case CIL_FSUSE:
		db->fsuse->count++;
		break;
	default:
		break;
	}

	return SEPOL_OK;
}

static int __cil_post_db_array_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct cil_db *db = extra_args;

	switch(node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_TYPE: {
		struct cil_type *type = node->data;
		if (db->val_to_type == NULL) {
			db->val_to_type = cil_malloc(sizeof(*db->val_to_type) * db->num_types);
		}
		db->val_to_type[type->value] = type;
		break;
	}
	case CIL_ROLE: {
		struct cil_role *role = node->data;
		if (db->val_to_role == NULL) {
			db->val_to_role = cil_malloc(sizeof(*db->val_to_role) * db->num_roles);
		}
		db->val_to_role[role->value] = role;
		break;
	}
	case CIL_USER: {
		struct cil_user *user= node->data;
		if (db->val_to_user == NULL) {
			db->val_to_user = cil_malloc(sizeof(*db->val_to_user) * db->num_users);
		}
		db->val_to_user[user->value] = user;
		break;
	}
	case CIL_USERPREFIX: {
		cil_list_append(db->userprefixes, CIL_USERPREFIX, node->data);
		break;
	}
	case CIL_SELINUXUSER: {
		cil_list_prepend(db->selinuxusers, CIL_SELINUXUSER, node->data);
		break;
	}
	case CIL_SELINUXUSERDEFAULT: {
		cil_list_append(db->selinuxusers, CIL_SELINUXUSERDEFAULT, node->data);
		break;
	}
	case CIL_NETIFCON: {
		struct cil_sort *sort = db->netifcon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_IBENDPORTCON: {
		struct cil_sort *sort = db->ibendportcon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;

		if (!sort->array)
			sort->array = cil_malloc(sizeof(*sort->array) * count);
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_FSUSE: {
		struct cil_sort *sort = db->fsuse;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_GENFSCON: {
		struct cil_sort *sort = db->genfscon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_FILECON: {
		struct cil_sort *sort = db->filecon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
		sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_NODECON: {
		struct cil_sort *sort = db->nodecon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_IBPKEYCON: {
		struct cil_sort *sort = db->ibpkeycon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;

		if (!sort->array)
			sort->array = cil_malloc(sizeof(*sort->array) * count);
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_PORTCON: {
		struct cil_sort *sort = db->portcon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_PIRQCON: {
		struct cil_sort *sort = db->pirqcon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_IOMEMCON: {
		struct cil_sort *sort = db->iomemcon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_IOPORTCON: {
		struct cil_sort *sort = db->ioportcon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_PCIDEVICECON: {
		struct cil_sort *sort = db->pcidevicecon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	case CIL_DEVICETREECON: {
		struct cil_sort *sort = db->devicetreecon;
		uint32_t count = sort->count;
		uint32_t i = sort->index;
		if (sort->array == NULL) {
			sort->array = cil_malloc(sizeof(*sort->array)*count);
		}
		sort->array[i] = node->data;
		sort->index++;
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;
}

static int __evaluate_type_expression(struct cil_typeattribute *attr, struct cil_db *db)
{
	int rc;

	attr->types = cil_malloc(sizeof(*attr->types));
	rc = __cil_expr_list_to_bitmap(attr->expr_list, attr->types, db->num_types, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to expand type attribute to bitmap\n");
		ksu_ebitmap_destroy(attr->types);
		free(attr->types);
		attr->types = NULL;
	}
	return rc;
}

static int __cil_type_to_bitmap(struct cil_symtab_datum *datum, ebitmap_t *bitmap, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_tree_node *node = datum->nodes->head->data;

	ebitmap_init(bitmap);

	if (node->flavor == CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *attr = (struct cil_typeattribute *)datum;
		if (attr->types == NULL) {
			rc = __evaluate_type_expression(attr, db);
			if (rc != SEPOL_OK) goto exit;
		}
		ebitmap_union(bitmap, attr->types);
	} else if (node->flavor == CIL_TYPEALIAS) {
		struct cil_alias *alias = (struct cil_alias *)datum;
		struct cil_type *type = alias->actual;
		if (ksu_ebitmap_set_bit(bitmap, type->value, 1)) {
			cil_log(CIL_ERR, "Failed to set type bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	} else {
		struct cil_type *type = (struct cil_type *)datum;
		if (ksu_ebitmap_set_bit(bitmap, type->value, 1)) {
			cil_log(CIL_ERR, "Failed to set type bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __evaluate_user_expression(struct cil_userattribute *attr, struct cil_db *db)
{
	int rc;

	attr->users = cil_malloc(sizeof(*attr->users));
	rc = __cil_expr_list_to_bitmap(attr->expr_list, attr->users, db->num_users, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to expand user attribute to bitmap\n");
		ksu_ebitmap_destroy(attr->users);
		free(attr->users);
		attr->users = NULL;
	}
	return rc;
}

static int __cil_user_to_bitmap(struct cil_symtab_datum *datum, ebitmap_t *bitmap, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_tree_node *node = datum->nodes->head->data;
	struct cil_userattribute *attr = NULL;
	struct cil_user *user = NULL;

	ebitmap_init(bitmap);

	if (node->flavor == CIL_USERATTRIBUTE) {
		attr = (struct cil_userattribute *)datum;
		if (attr->users == NULL) {
			rc = __evaluate_user_expression(attr, db);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
		ebitmap_union(bitmap, attr->users);
	} else {
		user = (struct cil_user *)datum;
		if (ksu_ebitmap_set_bit(bitmap, user->value, 1)) {
			cil_log(CIL_ERR, "Failed to set user bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __evaluate_role_expression(struct cil_roleattribute *attr, struct cil_db *db)
{
	int rc;

	attr->roles = cil_malloc(sizeof(*attr->roles));
	rc = __cil_expr_list_to_bitmap(attr->expr_list, attr->roles, db->num_roles, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to expand role attribute to bitmap\n");
		ksu_ebitmap_destroy(attr->roles);
		free(attr->roles);
		attr->roles = NULL;
	}
	return rc;
}

static int __cil_role_to_bitmap(struct cil_symtab_datum *datum, ebitmap_t *bitmap, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_tree_node *node = datum->nodes->head->data;

	ebitmap_init(bitmap);

	if (node->flavor == CIL_ROLEATTRIBUTE) {
		struct cil_roleattribute *attr = (struct cil_roleattribute *)datum;
		if (attr->roles == NULL) {
			rc = __evaluate_role_expression(attr, db);
			if (rc != SEPOL_OK) goto exit;
		}
		ebitmap_union(bitmap, attr->roles);
	} else {
		struct cil_role *role = (struct cil_role *)datum;
		if (ksu_ebitmap_set_bit(bitmap, role->value, 1)) {
			cil_log(CIL_ERR, "Failed to set role bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __evaluate_permissionx_expression(struct cil_permissionx *permx, struct cil_db *db)
{
	int rc;

	permx->perms = cil_malloc(sizeof(*permx->perms));
	ebitmap_init(permx->perms);

	rc = __cil_expr_to_bitmap(permx->expr_str, permx->perms, 0x10000, db); // max is one more than 0xFFFF
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to expand permissionx expression\n");
		ksu_ebitmap_destroy(permx->perms);
		free(permx->perms);
		permx->perms = NULL;
	}

	return rc;
}

static int __cil_permx_str_to_int(char *permx_str, uint16_t *val)
{
	char *endptr = NULL;
	long lval = strtol(permx_str, &endptr, 0);

	if (*endptr != '\0') {
		cil_log(CIL_ERR, "permissionx value %s not valid number\n", permx_str);
		goto exit;
	}
	if (lval < 0x0000 || lval > 0xFFFF) {
		cil_log(CIL_ERR, "permissionx value %s must be between 0x0000 and 0xFFFF\n", permx_str);
		goto exit;
	}

	*val = (uint16_t)lval;

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

static int __cil_permx_to_bitmap(struct cil_symtab_datum *datum, ebitmap_t *bitmap, __attribute__((unused)) struct cil_db *db)
{
	int rc = SEPOL_ERR;
	uint16_t val;

	rc = __cil_permx_str_to_int((char*)datum, &val);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	ebitmap_init(bitmap);
	if (ksu_ebitmap_set_bit(bitmap, (unsigned int)val, 1)) {
		cil_log(CIL_ERR, "Failed to set permissionx bit\n");
		ksu_ebitmap_destroy(bitmap);
		goto exit;
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_perm_to_bitmap(struct cil_symtab_datum *datum, ebitmap_t *bitmap, __attribute__((unused)) struct cil_db *db)
{
	struct cil_perm *perm = (struct cil_perm *)datum;
	unsigned int value = perm->value;

	ebitmap_init(bitmap);
	if (ksu_ebitmap_set_bit(bitmap, value, 1)) {
		cil_log(CIL_INFO, "Failed to set perm bit\n");
		ksu_ebitmap_destroy(bitmap);
		return SEPOL_ERR;
	}

	return SEPOL_OK;
}

static int __evaluate_cat_expression(struct cil_cats *cats, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	ebitmap_t bitmap;
	struct cil_list *new;
	struct cil_list_item *curr;

	if (cats->evaluated == CIL_TRUE) {
		return SEPOL_OK;
	}

	if (cil_verify_is_list(cats->datum_expr, CIL_CAT)) {
		return SEPOL_OK;
	}

	ebitmap_init(&bitmap);
	rc = __cil_expr_to_bitmap(cats->datum_expr, &bitmap, db->num_cats, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to expand category expression to bitmap\n");
		ksu_ebitmap_destroy(&bitmap);
		goto exit;
	}

	cil_list_init(&new, CIL_CAT);

	cil_list_for_each(curr, db->catorder) {
		struct cil_cat *cat = curr->data;
		if (ksu_ebitmap_get_bit(&bitmap, cat->value)) {
			cil_list_append(new, CIL_DATUM, cat);
		}
	}

	ksu_ebitmap_destroy(&bitmap);
	cil_list_destroy(&cats->datum_expr, CIL_FALSE);
	cats->datum_expr = new;

	cats->evaluated = CIL_TRUE;

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_cat_to_bitmap(struct cil_symtab_datum *datum, ebitmap_t *bitmap, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_tree_node *node = datum->nodes->head->data;

	ebitmap_init(bitmap);

	if (node->flavor == CIL_CATSET) {
		struct cil_catset *catset = (struct cil_catset *)datum;
		struct cil_list_item *curr;
		if (catset->cats->evaluated == CIL_FALSE) {
			rc = __evaluate_cat_expression(catset->cats, db);
			if (rc != SEPOL_OK) goto exit;
		}
		for (curr = catset->cats->datum_expr->head; curr; curr = curr->next) {
			struct cil_cat *cat = (struct cil_cat *)curr->data;
			if (ksu_ebitmap_set_bit(bitmap, cat->value, 1)) {
				cil_log(CIL_ERR, "Failed to set cat bit\n");
				ksu_ebitmap_destroy(bitmap);
				goto exit;
			}
		}
	} else if (node->flavor == CIL_CATALIAS) {
		struct cil_alias *alias = (struct cil_alias *)datum;
		struct cil_cat *cat = alias->actual;
		if (ksu_ebitmap_set_bit(bitmap, cat->value, 1)) {
			cil_log(CIL_ERR, "Failed to set cat bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	} else {
		struct cil_cat *cat = (struct cil_cat *)datum;
		if (ksu_ebitmap_set_bit(bitmap, cat->value, 1)) {
			cil_log(CIL_ERR, "Failed to set cat bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_cat_expr_range_to_bitmap_helper(struct cil_list_item *i1, struct cil_list_item *i2, ebitmap_t *bitmap)
{
	int rc = SEPOL_ERR;
	struct cil_symtab_datum *d1 = i1->data;
	struct cil_symtab_datum *d2 = i2->data;
	struct cil_tree_node *n1 = d1->nodes->head->data;
	struct cil_tree_node *n2 = d2->nodes->head->data;
	struct cil_cat *c1 = (struct cil_cat *)d1;
	struct cil_cat *c2 = (struct cil_cat *)d2;
	int i;

	if (n1->flavor == CIL_CATSET || n2->flavor == CIL_CATSET) {
		cil_log(CIL_ERR, "Category sets cannont be used in a category range\n");
		goto exit;
	}

	if (n1->flavor == CIL_CATALIAS) {
		struct cil_alias *alias = (struct cil_alias *)d1;
		c1 = alias->actual;
	}

	if (n2->flavor == CIL_CATALIAS) {
		struct cil_alias *alias = (struct cil_alias *)d2;
		c2 = alias->actual;
	}

	if (c1->value > c2->value) {
		cil_log(CIL_ERR, "Invalid category range\n");
		goto exit;
	}

	for (i = c1->value; i <= c2->value; i++) {
		if (ksu_ebitmap_set_bit(bitmap, i, 1)) {
			cil_log(CIL_ERR, "Failed to set cat bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_permissionx_expr_range_to_bitmap_helper(struct cil_list_item *i1, struct cil_list_item *i2, ebitmap_t *bitmap)
{
	int rc = SEPOL_ERR;
	char *p1 = i1->data;
	char *p2 = i2->data;
	uint16_t v1;
	uint16_t v2;
	uint32_t i;

	rc = __cil_permx_str_to_int(p1, &v1);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	rc = __cil_permx_str_to_int(p2, &v2);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	for (i = v1; i <= v2; i++) {
		if (ksu_ebitmap_set_bit(bitmap, i, 1)) {
			cil_log(CIL_ERR, "Failed to set permissionx bit\n");
			ksu_ebitmap_destroy(bitmap);
			goto exit;
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_expr_to_bitmap_helper(struct cil_list_item *curr, enum cil_flavor flavor, ebitmap_t *bitmap, int max, struct cil_db *db)
{
	int rc = SEPOL_ERR;

	if (curr->flavor == CIL_DATUM) {
		switch (flavor) {
		case CIL_TYPE:
			rc = __cil_type_to_bitmap(curr->data, bitmap, db);
			break;
		case CIL_ROLE:
			rc = __cil_role_to_bitmap(curr->data, bitmap, db);
			break;
		case CIL_USER:
			rc = __cil_user_to_bitmap(curr->data, bitmap, db);
			break;
		case CIL_PERM:
			rc = __cil_perm_to_bitmap(curr->data, bitmap, db);
			break;
		case CIL_CAT:
			rc = __cil_cat_to_bitmap(curr->data, bitmap, db);
			break;
		default:
			rc = SEPOL_ERR;
		}
	} else if (curr->flavor == CIL_LIST) {
		struct cil_list *l = curr->data;
		ebitmap_init(bitmap);
		rc = __cil_expr_to_bitmap(l, bitmap, max, db);
		if (rc != SEPOL_OK) {
			ksu_ebitmap_destroy(bitmap);
		}	
	} else if (flavor == CIL_PERMISSIONX) {
		// permissionx expressions aren't resolved into anything, so curr->flavor
		// is just a CIL_STRING, not a CIL_DATUM, so just check on flavor for those
		rc = __cil_permx_to_bitmap(curr->data, bitmap, db);
	}

	return rc;
}

static int __cil_expr_to_bitmap(struct cil_list *expr, ebitmap_t *out, int max, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *curr;
	enum cil_flavor flavor;
	ebitmap_t tmp, b1, b2;

	if (expr == NULL || expr->head == NULL) {
		return SEPOL_OK;
	}

	curr = expr->head;
	flavor = expr->flavor;

	if (curr->flavor == CIL_OP) {
		enum cil_flavor op = (enum cil_flavor)(uintptr_t)curr->data;

		if (op == CIL_ALL) {
			ebitmap_init(&b1); /* all zeros */
			rc = ebitmap_not(&tmp, &b1, max);
			ksu_ebitmap_destroy(&b1);
			if (rc != SEPOL_OK) {
				cil_log(CIL_INFO, "Failed to expand 'all' operator\n");
				ksu_ebitmap_destroy(&tmp);
				goto exit;
			}
		} else if (op == CIL_RANGE) {
			if (flavor == CIL_CAT) {
				ebitmap_init(&tmp);
				rc = __cil_cat_expr_range_to_bitmap_helper(curr->next, curr->next->next, &tmp);
				if (rc != SEPOL_OK) {
					cil_log(CIL_INFO, "Failed to expand category range\n");
					ksu_ebitmap_destroy(&tmp);
					goto exit;
				}
			} else if (flavor == CIL_PERMISSIONX) {
				ebitmap_init(&tmp);
				rc = __cil_permissionx_expr_range_to_bitmap_helper(curr->next, curr->next->next, &tmp);
				if (rc != SEPOL_OK) {
					cil_log(CIL_INFO, "Failed to expand category range\n");
					ksu_ebitmap_destroy(&tmp);
					goto exit;
				}
			} else {
				cil_log(CIL_INFO, "Range operation only supported for categories permissionx\n");
				rc = SEPOL_ERR;
				goto exit;
			}
		} else {
			rc = __cil_expr_to_bitmap_helper(curr->next, flavor, &b1, max, db);
			if (rc != SEPOL_OK) {
				cil_log(CIL_INFO, "Failed to get first operand bitmap\n");
				goto exit;
			}

			if (op == CIL_NOT) {
				rc = ebitmap_not(&tmp, &b1, max);
				ksu_ebitmap_destroy(&b1);
				if (rc != SEPOL_OK) {
					cil_log(CIL_INFO, "Failed to NOT bitmap\n");
					ksu_ebitmap_destroy(&tmp);
					goto exit;
				}
			} else {
				rc = __cil_expr_to_bitmap_helper(curr->next->next, flavor, &b2, max, db);
				if (rc != SEPOL_OK) {
					cil_log(CIL_INFO, "Failed to get second operand bitmap\n");
					ksu_ebitmap_destroy(&b1);
					goto exit;
				}

				if (op == CIL_OR) {
					rc = ebitmap_or(&tmp, &b1, &b2);
				} else if (op == CIL_AND) {
					rc = ksu_ebitmap_and(&tmp, &b1, &b2);
				} else if (op == CIL_XOR) {
					rc = ebitmap_xor(&tmp, &b1, &b2);
				} else {
					rc = SEPOL_ERR;
				}
				ksu_ebitmap_destroy(&b1);
				ksu_ebitmap_destroy(&b2);
				if (rc != SEPOL_OK) {
					cil_log(CIL_INFO, "Failed to apply operator to bitmaps\n");
					ksu_ebitmap_destroy(&tmp);
					goto exit;
				}
			}
		}
	} else {
		ebitmap_init(&tmp);
		for (;curr; curr = curr->next) {
			rc = __cil_expr_to_bitmap_helper(curr, flavor, &b2, max, db);
			if (rc != SEPOL_OK) {
				cil_log(CIL_INFO, "Failed to get operand in list\n");
				ksu_ebitmap_destroy(&tmp);
				goto exit;
			}
			b1 = tmp;
			rc = ebitmap_or(&tmp, &b1, &b2);
			ksu_ebitmap_destroy(&b1);
			ksu_ebitmap_destroy(&b2);
			if (rc != SEPOL_OK) {
				cil_log(CIL_INFO, "Failed to OR operands in list\n");
				ksu_ebitmap_destroy(&tmp);
				goto exit;
			}

		}
	}

	ebitmap_union(out, &tmp);
	ksu_ebitmap_destroy(&tmp);

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_expr_list_to_bitmap(struct cil_list *expr_list, ebitmap_t *out, int max, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *expr;

	ebitmap_init(out);

	if (expr_list == NULL) {
		return SEPOL_OK;
	}

	cil_list_for_each(expr, expr_list) {
		ebitmap_t bitmap;
		struct cil_list *l = (struct cil_list *)expr->data;
		ebitmap_init(&bitmap);
		rc = __cil_expr_to_bitmap(l, &bitmap, max, db);
		if (rc != SEPOL_OK) {
			cil_log(CIL_INFO, "Failed to expand expression list to bitmap\n");
			ksu_ebitmap_destroy(&bitmap);
			goto exit;
		}
		ebitmap_union(out, &bitmap);
		ksu_ebitmap_destroy(&bitmap);
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

static int cil_typeattribute_used(struct cil_typeattribute *attr, struct cil_db *db)
{
	if (!attr->used) {
		return CIL_FALSE;
	}

	if (attr->used & CIL_ATTR_EXPAND_FALSE) {
		return CIL_TRUE;
	}

	if (attr->used & CIL_ATTR_EXPAND_TRUE) {
		return CIL_FALSE;
	}

	if (attr->used & CIL_ATTR_CONSTRAINT) {
		return CIL_TRUE;
	}

	if (db->attrs_expand_generated || attr->used == CIL_ATTR_NEVERALLOW) {
		if (strcmp(DATUM(attr)->name, GEN_REQUIRE_ATTR) == 0) {
			return CIL_FALSE;
		} else if (strstr(DATUM(attr)->name, TYPEATTR_INFIX) != NULL) {
			return CIL_FALSE;
		}

		if (attr->used == CIL_ATTR_NEVERALLOW) {
			return CIL_TRUE;
		}
	}

	if (attr->used == CIL_ATTR_AVRULE) {
		if (ebitmap_cardinality(attr->types) < db->attrs_expand_size) {
			return CIL_FALSE;
		}
	}

	return CIL_TRUE;
}

static void __mark_neverallow_attrs(struct cil_list *expr_list)
{
	struct cil_list_item *curr;

	if (!expr_list) {
		return;
	}

	cil_list_for_each(curr, expr_list) {
		if (curr->flavor == CIL_DATUM) {
			if (FLAVOR(curr->data) == CIL_TYPEATTRIBUTE) {
				struct cil_typeattribute *attr = curr->data;
				if (strstr(DATUM(attr)->name, TYPEATTR_INFIX)) {
					__mark_neverallow_attrs(attr->expr_list);
				} else {
					attr->used |= CIL_ATTR_NEVERALLOW;
				}
			}
		} else if (curr->flavor == CIL_LIST) {
			 __mark_neverallow_attrs(curr->data);
		}
	}
}

static int __cil_post_db_neverallow_attr_helper(struct cil_tree_node *node, uint32_t *finished, __attribute__((unused)) void *extra_args)
{
	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO: {
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	case CIL_TYPEATTRIBUTE: {
		struct cil_typeattribute *attr = node->data;
		if ((attr->used & CIL_ATTR_NEVERALLOW) &&
		    strstr(DATUM(attr)->name, TYPEATTR_INFIX)) {
			__mark_neverallow_attrs(attr->expr_list);
		}
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;
}

static int __cil_post_db_attr_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_db *db = extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO: {
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	case CIL_TYPEATTRIBUTE: {
		struct cil_typeattribute *attr = node->data;
		if (attr->types == NULL) {
			rc = __evaluate_type_expression(attr, db);
			if (rc != SEPOL_OK) goto exit;
		}
		attr->keep = cil_typeattribute_used(attr, db);
		break;
	}
	case CIL_ROLEATTRIBUTE: {
		struct cil_roleattribute *attr = node->data;
		if (attr->roles == NULL) {
			rc = __evaluate_role_expression(attr, db);
			if (rc != SEPOL_OK) goto exit;
		}
		break;
	}
	case CIL_AVRULEX: {
		struct cil_avrule *rule = node->data;
		if (rule->perms.x.permx_str == NULL) {
			rc = __evaluate_permissionx_expression(rule->perms.x.permx, db);
			if (rc != SEPOL_OK) goto exit;
		}
		break;
	}
	case CIL_PERMISSIONX: {
		struct cil_permissionx *permx = node->data;
		rc = __evaluate_permissionx_expression(permx, db);
		if (rc != SEPOL_OK) goto exit;
		break;
	}
	case CIL_USERATTRIBUTE: {
		struct cil_userattribute *attr = node->data;
		if (attr->users == NULL) {
			rc = __evaluate_user_expression(attr, db);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_role_assign_types(struct cil_role *role, struct cil_symtab_datum *datum)
{
	struct cil_tree_node *node = datum->nodes->head->data;

	if (role->types == NULL) {
		role->types = cil_malloc(sizeof(*role->types));
		ebitmap_init(role->types);
	}

	if (node->flavor == CIL_TYPE) {
		struct cil_type *type = (struct cil_type *)datum;
		if (ksu_ebitmap_set_bit(role->types, type->value, 1)) {
			cil_log(CIL_INFO, "Failed to set bit in role types bitmap\n");
			goto exit;
		}
	} else if (node->flavor == CIL_TYPEALIAS) {
		struct cil_alias *alias = (struct cil_alias *)datum;
		struct cil_type *type = alias->actual;
		if (ksu_ebitmap_set_bit(role->types, type->value, 1)) {
			cil_log(CIL_INFO, "Failed to set bit in role types bitmap\n");
			goto exit;
		}
	} else if (node->flavor == CIL_TYPEATTRIBUTE) {
		struct cil_typeattribute *attr = (struct cil_typeattribute *)datum;
		ebitmap_union(role->types, attr->types);
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

static int __cil_post_db_roletype_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_db *db = extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO: {
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	case CIL_ROLETYPE: {
		struct cil_roletype *roletype = node->data;
		struct cil_symtab_datum *role_datum = roletype->role;
		struct cil_symtab_datum *type_datum = roletype->type;
		struct cil_tree_node *role_node = role_datum->nodes->head->data;

		if (role_node->flavor == CIL_ROLEATTRIBUTE) {
			struct cil_roleattribute *attr = roletype->role;
			ebitmap_node_t *rnode;
			unsigned int i;
	
			ebitmap_for_each_positive_bit(attr->roles, rnode, i) {
				struct cil_role *role = NULL;

				role = db->val_to_role[i];

				rc = __cil_role_assign_types(role, type_datum);
				if (rc != SEPOL_OK) {
					goto exit;
				}
			}
		} else {
			struct cil_role *role = roletype->role;

			rc = __cil_role_assign_types(role, type_datum);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;
exit:
	cil_log(CIL_INFO, "cil_post_db_roletype_helper failed\n");
	return rc;
}

static int __cil_user_assign_roles(struct cil_user *user, struct cil_symtab_datum *datum)
{
	struct cil_tree_node *node = datum->nodes->head->data;
	struct cil_role *role = NULL;
	struct cil_roleattribute *attr = NULL;

	if (user->roles == NULL) {
		user->roles = cil_malloc(sizeof(*user->roles));
		ebitmap_init(user->roles);
	}

	if (node->flavor == CIL_ROLE) {
		role = (struct cil_role *)datum;
		if (ksu_ebitmap_set_bit(user->roles, role->value, 1)) {
			cil_log(CIL_INFO, "Failed to set bit in user roles bitmap\n");
			goto exit;
		}
	} else if (node->flavor == CIL_ROLEATTRIBUTE) {
		attr = (struct cil_roleattribute *)datum;
		ebitmap_union(user->roles, attr->roles);
	}

	return SEPOL_OK;

exit:
	return SEPOL_ERR;
}

static int __cil_post_db_userrole_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_db *db = extra_args;
	struct cil_block *blk = NULL;
	struct cil_userrole *userrole = NULL;
	struct cil_symtab_datum *user_datum = NULL;
	struct cil_symtab_datum *role_datum = NULL;
	struct cil_tree_node *user_node = NULL;
	struct cil_userattribute *u_attr = NULL;
	unsigned int i;
	struct cil_user *user = NULL;
	ebitmap_node_t *unode = NULL;

	switch (node->flavor) {
	case CIL_BLOCK: {
		blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO: {
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	case CIL_USERROLE: {
		userrole = node->data;
		user_datum = userrole->user;
		role_datum = userrole->role;
		user_node = user_datum->nodes->head->data;

		if (user_node->flavor == CIL_USERATTRIBUTE) {
			u_attr = userrole->user;

			ebitmap_for_each_positive_bit(u_attr->users, unode, i) {
				user = db->val_to_user[i];

				rc = __cil_user_assign_roles(user, role_datum);
				if (rc != SEPOL_OK) {
					goto exit;
				}
			}
		} else {
			user = userrole->user;

			rc = __cil_user_assign_roles(user, role_datum);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}

		break;
	}
	default:
		break;
	}

	return SEPOL_OK;
exit:
	cil_log(CIL_INFO, "cil_post_db_userrole_helper failed\n");
	return rc;
}

static int __evaluate_level_expression(struct cil_level *level, struct cil_db *db)
{
	if (level->cats != NULL) {
		return __evaluate_cat_expression(level->cats, db);
	}

	return SEPOL_OK;
}

static int __evaluate_levelrange_expression(struct cil_levelrange *levelrange, struct cil_db *db)
{
	int rc = SEPOL_OK;

	if (levelrange->low != NULL && levelrange->low->cats != NULL) {
		rc =  __evaluate_cat_expression(levelrange->low->cats, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}
	if (levelrange->high != NULL && levelrange->high->cats != NULL) {
		rc = __evaluate_cat_expression(levelrange->high->cats, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

exit:
	return rc;
}

static int __cil_post_db_cat_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_db *db = extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO: {
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	}
	case CIL_CATSET: {
		struct cil_catset *catset = node->data;
		rc = __evaluate_cat_expression(catset->cats, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_SENSCAT: {
		struct cil_senscat *senscat = node->data;
		rc = __evaluate_cat_expression(senscat->cats, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_LEVEL: {
		rc = __evaluate_level_expression(node->data, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_LEVELRANGE: {
		rc = __evaluate_levelrange_expression(node->data, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_USER: {
		struct cil_user *user = node->data;
		rc = __evaluate_level_expression(user->dftlevel, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		rc = __evaluate_levelrange_expression(user->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_SELINUXUSERDEFAULT:
	case CIL_SELINUXUSER: {
		struct cil_selinuxuser *selinuxuser = node->data;
		rc = __evaluate_levelrange_expression(selinuxuser->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_RANGETRANSITION: {
		struct cil_rangetransition *rangetrans = node->data;
		rc = __evaluate_levelrange_expression(rangetrans->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_CONTEXT: {
		struct cil_context *context = node->data;
		rc = __evaluate_levelrange_expression(context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_SIDCONTEXT: {
		struct cil_sidcontext *sidcontext = node->data;
		rc = __evaluate_levelrange_expression(sidcontext->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_FILECON: {
		struct cil_filecon *filecon = node->data;
		if (filecon->context) {
			rc = __evaluate_levelrange_expression(filecon->context->range, db);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
		break;
	}
	case CIL_IBPKEYCON: {
		struct cil_ibpkeycon *ibpkeycon = node->data;

		rc = __evaluate_levelrange_expression(ibpkeycon->context->range, db);
		if (rc != SEPOL_OK)
			goto exit;
		break;
	}
	case CIL_IBENDPORTCON: {
		struct cil_ibendportcon *ibendportcon = node->data;

		rc = __evaluate_levelrange_expression(ibendportcon->context->range, db);
		if (rc != SEPOL_OK)
			goto exit;
		break;
	}
	case CIL_PORTCON: {
		struct cil_portcon *portcon = node->data;
		rc = __evaluate_levelrange_expression(portcon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_NODECON: {
		struct cil_nodecon *nodecon = node->data;
		rc = __evaluate_levelrange_expression(nodecon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_GENFSCON: {
		struct cil_genfscon *genfscon = node->data;
		rc = __evaluate_levelrange_expression(genfscon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_NETIFCON: {
		struct cil_netifcon *netifcon = node->data;
		rc = __evaluate_levelrange_expression(netifcon->if_context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		rc = __evaluate_levelrange_expression(netifcon->packet_context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_PIRQCON: {
		struct cil_pirqcon *pirqcon = node->data;
		rc = __evaluate_levelrange_expression(pirqcon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_IOMEMCON: {
		struct cil_iomemcon *iomemcon = node->data;
		rc = __evaluate_levelrange_expression(iomemcon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_IOPORTCON: {
		struct cil_ioportcon *ioportcon = node->data;
		rc = __evaluate_levelrange_expression(ioportcon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_PCIDEVICECON: {
		struct cil_pcidevicecon *pcidevicecon = node->data;
		rc = __evaluate_levelrange_expression(pcidevicecon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_DEVICETREECON: {
		struct cil_devicetreecon *devicetreecon = node->data;
		rc = __evaluate_levelrange_expression(devicetreecon->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_FSUSE: {
		struct cil_fsuse *fsuse = node->data;
		rc = __evaluate_levelrange_expression(fsuse->context->range, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;

exit:
	return rc;
}

struct perm_to_list {
	enum cil_flavor flavor;
	ebitmap_t *perms;
	struct cil_list *new_list;
};

static int __perm_bits_to_list(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct perm_to_list *perm_args = (struct perm_to_list *)args;
	ebitmap_t *perms = perm_args->perms;
	struct cil_list *new_list = perm_args->new_list;
	struct cil_perm *perm = (struct cil_perm *)d;
	unsigned int value = perm->value;

	if (!ksu_ebitmap_get_bit(perms, value)) {
		return SEPOL_OK;
	}

	cil_list_append(new_list, CIL_DATUM, d);

	return SEPOL_OK;
}

static int __evaluate_perm_expression(struct cil_list *perms, enum cil_flavor flavor, symtab_t *class_symtab, symtab_t *common_symtab, unsigned int num_perms, struct cil_list **new_list, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct perm_to_list args;
	ebitmap_t bitmap;

	if (cil_verify_is_list(perms, CIL_PERM)) {
		return SEPOL_OK;
	}

	ebitmap_init(&bitmap);
	rc = __cil_expr_to_bitmap(perms, &bitmap, num_perms, db);
	if (rc != SEPOL_OK) {
		ksu_ebitmap_destroy(&bitmap);
		goto exit;
	}

	cil_list_init(new_list, flavor);

	args.flavor = flavor;
	args.perms = &bitmap;
	args.new_list = *new_list;

	cil_symtab_map(class_symtab, __perm_bits_to_list, &args);

	if (common_symtab != NULL) {
		cil_symtab_map(common_symtab, __perm_bits_to_list, &args);
	}

	ksu_ebitmap_destroy(&bitmap);
	return SEPOL_OK;

exit:
	return rc;
}

static int __evaluate_classperms(struct cil_classperms *cp, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_class *class = cp->class;
	struct cil_class *common = class->common;
	symtab_t *common_symtab = NULL;
	struct cil_list *new_list = NULL;

	if (common) {
		common_symtab = &common->perms;
	}

	rc = __evaluate_perm_expression(cp->perms, CIL_PERM, &class->perms, common_symtab, class->num_perms, &new_list, db);
	if (rc != SEPOL_OK) {
		goto exit;
	}

	if (new_list == NULL) {
		return SEPOL_OK;
	}

	cil_list_destroy(&cp->perms, CIL_FALSE);

	cp->perms = new_list;

	return SEPOL_OK;

exit:
	return rc;
}

static int __evaluate_classperms_list(struct cil_list *classperms, struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_list_item *curr;

	cil_list_for_each(curr, classperms) {
		if (curr->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp = curr->data;
			if (FLAVOR(cp->class) == CIL_CLASS) {
				rc = __evaluate_classperms(cp, db);
				if (rc != SEPOL_OK) {
					goto exit;
				}
			} else { /* MAP */
				struct cil_list_item *i = NULL;
				rc = __evaluate_classperms(cp, db);
				if (rc != SEPOL_OK) {
					goto exit;
				}
				cil_list_for_each(i, cp->perms) {
					struct cil_perm *cmp = i->data;
					rc = __evaluate_classperms_list(cmp->classperms, db);
					if (rc != SEPOL_OK) {
						goto exit;
					}
				}
			}	
		} else { /* SET */
			struct cil_classperms_set *cp_set = curr->data;
			struct cil_classpermission *cp = cp_set->set;
			rc = __evaluate_classperms_list(cp->classperms, db);
			if (rc != SEPOL_OK) {
				goto exit;
			}
		}
	}

	return SEPOL_OK;

exit:
	return rc;
}

struct class_map_args {
	struct cil_db *db;
	int rc;
};

static int __evaluate_map_perm_classperms(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct class_map_args *map_args = args;
	struct cil_perm *cmp = (struct cil_perm *)d;

	int rc = __evaluate_classperms_list(cmp->classperms, map_args->db);

	if (rc != SEPOL_OK) {
		map_args->rc = rc;
	}

	return SEPOL_OK;
}

static int __evaluate_map_class(struct cil_class *mc, struct cil_db *db)
{
	struct class_map_args map_args;

	map_args.db = db;
	map_args.rc = SEPOL_OK;
	cil_symtab_map(&mc->perms, __evaluate_map_perm_classperms, &map_args);

	return map_args.rc;
}

static int __cil_post_db_classperms_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_db *db = extra_args;

	switch (node->flavor) {
	case CIL_BLOCK: {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
		break;
	}
	case CIL_MACRO:
		*finished = CIL_TREE_SKIP_HEAD;
		break;
	case CIL_MAP_CLASS: {
		rc = __evaluate_map_class(node->data, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_CLASSPERMISSION: {
		struct cil_classpermission *cp = node->data;
		rc = __evaluate_classperms_list(cp->classperms, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_AVRULE: {
		struct cil_avrule *avrule = node->data;
		rc = __evaluate_classperms_list(avrule->perms.classperms, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	case CIL_CONSTRAIN:
	case CIL_MLSCONSTRAIN: {
		struct cil_constrain *constrain = node->data;
		rc = __evaluate_classperms_list(constrain->classperms, db);
		if (rc != SEPOL_OK) {
			goto exit;
		}
		break;
	}
	default:
		break;
	}

	return SEPOL_OK;

exit:
	return rc;
}

static int __cil_post_report_conflict(struct cil_tree_node *node, uint32_t *finished, void *extra_args)
{
	struct cil_list_item *li = extra_args;

	if (node->flavor == CIL_BLOCK) {
		struct cil_block *blk = node->data;
		if (blk->is_abstract == CIL_TRUE) {
			*finished = CIL_TREE_SKIP_HEAD;
		}
	} else if (node->flavor == CIL_MACRO) {
		*finished = CIL_TREE_SKIP_HEAD;
	} else if (node->flavor == li->flavor) {
		if (node->data == li->data) {
			char *path = cil_tree_get_cil_path(node);
			cil_log(CIL_WARN, "  at %s:%d\n", path, node->line);
		}
	}
	return SEPOL_OK;
}

static int __cil_post_process_context_rules(struct cil_sort *sort, int (*compar)(const void *, const void *), int (*concompar)(const void *, const void *), struct cil_db *db, enum cil_flavor flavor, const char *flavor_str)
{
	uint32_t count = sort->count;
	uint32_t i = 0, j, removed = 0;
	int conflicting = 0;
	int rc = SEPOL_OK;
	enum cil_log_level log_level = cil_get_log_level();

	if (count < 2) {
		return SEPOL_OK;
	}

	qsort(sort->array, sort->count, sizeof(sort->array), compar);

	for (j=1; j<count; j++) {
		if (compar(&sort->array[i], &sort->array[j]) != 0) {
			i++;
			if (conflicting >= 4) {
				/* 2 rules were written when conflicting == 1 */
				cil_log(CIL_WARN, "  Only first 4 of %d conflicting rules shown\n", conflicting);
			}
			conflicting = 0;
		} else {
			removed++;
			if (!db->multiple_decls || concompar(&sort->array[i], &sort->array[j]) != 0) {
				conflicting++;
				if (log_level >= CIL_WARN) {
					struct cil_list_item li;
					int rc2;
					li.flavor = flavor;
					if (conflicting == 1) {
						cil_log(CIL_WARN, "Found conflicting %s rules\n", flavor_str);
						rc = SEPOL_ERR;
						li.data = sort->array[i];
						rc2 = cil_tree_walk(db->ast->root, __cil_post_report_conflict,
											NULL, NULL, &li);
						if (rc2 != SEPOL_OK) goto exit;
					}
					if (conflicting < 4 || log_level > CIL_WARN) {
						li.data = sort->array[j];
						rc2 = cil_tree_walk(db->ast->root, __cil_post_report_conflict,
											NULL, NULL, &li);
						if (rc2 != SEPOL_OK) goto exit;
					}
				}
			}
		}
		if (i != j && !conflicting) {
			sort->array[i] = sort->array[j];
		}
	}
	sort->count = count - removed;

exit:
	return rc;
}

static int cil_post_db(struct cil_db *db)
{
	int rc = SEPOL_ERR;

	rc = cil_tree_walk(db->ast->root, __cil_post_db_count_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failure during cil database count helper\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_array_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failure during cil database array helper\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_neverallow_attr_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed to mark attributes used by generated attributes used in neverallow rules\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_attr_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed to create attribute bitmaps\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_roletype_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed during roletype association\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_userrole_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed during userrole association\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_classperms_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed to evaluate class mapping permissions expressions\n");
		goto exit;
	}

	rc = cil_tree_walk(db->ast->root, __cil_post_db_cat_helper, NULL, NULL, db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_INFO, "Failed to evaluate category expressions\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->netifcon, cil_post_netifcon_compare, cil_post_netifcon_context_compare, db, CIL_NETIFCON, CIL_KEY_NETIFCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing netifcon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->genfscon, cil_post_genfscon_compare, cil_post_genfscon_context_compare, db, CIL_GENFSCON, CIL_KEY_GENFSCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing genfscon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->ibpkeycon, cil_post_ibpkeycon_compare, cil_post_ibpkeycon_context_compare, db, CIL_IBPKEYCON, CIL_KEY_IBPKEYCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing ibpkeycon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->ibendportcon, cil_post_ibendportcon_compare, cil_post_ibendportcon_context_compare, db, CIL_IBENDPORTCON, CIL_KEY_IBENDPORTCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing ibendportcon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->portcon, cil_post_portcon_compare, cil_post_portcon_context_compare, db, CIL_PORTCON, CIL_KEY_PORTCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing portcon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->nodecon, cil_post_nodecon_compare, cil_post_nodecon_context_compare, db, CIL_NODECON, CIL_KEY_NODECON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing nodecon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->fsuse, cil_post_fsuse_compare, cil_post_fsuse_context_compare, db, CIL_FSUSE, CIL_KEY_FSUSE);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing fsuse rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->filecon, cil_post_filecon_compare, cil_post_filecon_context_compare, db, CIL_FILECON, CIL_KEY_FILECON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing filecon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->pirqcon, cil_post_pirqcon_compare, cil_post_pirqcon_context_compare, db, CIL_PIRQCON, CIL_KEY_IOMEMCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing pirqcon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->iomemcon, cil_post_iomemcon_compare, cil_post_iomemcon_context_compare, db, CIL_IOMEMCON, CIL_KEY_IOMEMCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing iomemcon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->ioportcon, cil_post_ioportcon_compare, cil_post_ioportcon_context_compare, db, CIL_IOPORTCON, CIL_KEY_IOPORTCON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing ioportcon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->pcidevicecon, cil_post_pcidevicecon_compare, cil_post_pcidevicecon_context_compare, db, CIL_PCIDEVICECON, CIL_KEY_PCIDEVICECON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing pcidevicecon rules\n");
		goto exit;
	}

	rc = __cil_post_process_context_rules(db->devicetreecon, cil_post_devicetreecon_compare, cil_post_devicetreecon_context_compare, db, CIL_DEVICETREECON, CIL_KEY_DEVICETREECON);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Problems processing devicetreecon rules\n");
		goto exit;
	}

exit:
	return rc;
}

static int cil_post_verify(struct cil_db *db)
{
	int rc = SEPOL_ERR;
	int avrule_cnt = 0;
	int handleunknown = -1;
	int mls = -1;
	int nseuserdflt = 0;
	int pass = 0;
	struct cil_args_verify extra_args;
	struct cil_complex_symtab csymtab;

	cil_complex_symtab_init(&csymtab, CIL_CLASS_SYM_SIZE);

	extra_args.db = db;
	extra_args.csymtab = &csymtab;
	extra_args.avrule_cnt = &avrule_cnt;
	extra_args.handleunknown = &handleunknown;
	extra_args.mls = &mls;
	extra_args.nseuserdflt = &nseuserdflt;
	extra_args.pass = &pass;

	for (pass = 0; pass < 2; pass++) {
		rc = cil_tree_walk(db->ast->root, __cil_verify_helper, NULL, NULL, &extra_args);
		if (rc != SEPOL_OK) {
			cil_log(CIL_ERR, "Failed to verify cil database\n");
			goto exit;
		}
	}

	if (db->handle_unknown == -1) {
		if (handleunknown == -1) {
			db->handle_unknown = SEPOL_DENY_UNKNOWN;
		} else {
			db->handle_unknown = handleunknown;
		}
	}

	if (db->mls == -1) {
		if (mls == -1) {
			db->mls = CIL_FALSE;
		} else {
			db->mls = mls;
		}
	}

	if (avrule_cnt == 0) {
		cil_log(CIL_ERR, "Policy must include at least one avrule\n");
		rc = SEPOL_ERR;
		goto exit;
	}

	if (nseuserdflt > 1) {
		cil_log(CIL_ERR, "Policy cannot contain more than one selinuxuserdefault, found: %d\n", nseuserdflt);
		rc = SEPOL_ERR;
		goto exit;
	}

exit:
	cil_complex_symtab_destroy(&csymtab);
	return rc;
}

static int cil_pre_verify(struct cil_db *db)
{
	int rc = SEPOL_ERR;
	struct cil_args_verify extra_args;

	extra_args.db = db;

	rc = cil_tree_walk(db->ast->root, __cil_pre_verify_helper, NULL, NULL, &extra_args);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to verify cil database\n");
		goto exit;
	}

exit:
	return rc;
}

int cil_post_process(struct cil_db *db)
{
	int rc = SEPOL_ERR;

	rc = cil_pre_verify(db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to verify cil database\n");
		goto exit;
	}

	rc = cil_post_db(db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed post db handling\n");
		goto exit;
	}

	rc = cil_post_verify(db);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to verify cil database\n");
		goto exit;
	}

exit:
	return rc;
		
}

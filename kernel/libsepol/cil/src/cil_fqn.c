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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "cil_fqn.h"
#include "cil_internal.h"
#include "cil_log.h"
#include "cil_strpool.h"
#include "cil_symtab.h"

struct cil_fqn_args {
	char prefix[CIL_MAX_NAME_LENGTH];
	int len;
	struct cil_tree_node *node;
};

static int __cil_fqn_qualify_decls(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct cil_fqn_args *fqn_args = args;
	struct cil_symtab_datum *datum = (struct cil_symtab_datum *)d;
	int newlen;
	char prefix[CIL_MAX_NAME_LENGTH];
	int rc = SEPOL_OK;

	if (fqn_args->len == 0) {
		goto exit;
	}

	newlen = fqn_args->len + strlen(datum->name);
	if (newlen >= CIL_MAX_NAME_LENGTH) {
		cil_log(CIL_INFO, "Fully qualified name for %s is too long\n", datum->name);
		rc = SEPOL_ERR;
		goto exit;
	}
	strcpy(prefix, fqn_args->prefix);
	strcat(prefix, datum->name);
	datum->fqn = cil_strpool_add(prefix);

exit:
	return rc;
}

static int __cil_fqn_qualify_blocks(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct cil_fqn_args *fqn_args = args;
	struct cil_fqn_args child_args;
	struct cil_block *block = (struct cil_block *)d;
	struct cil_symtab_datum *datum = (struct cil_symtab_datum *)block;
	struct cil_tree_node *node = NODE(datum);
	int i;
	int rc = SEPOL_OK;
	int newlen;

	if (node->flavor != CIL_BLOCK) {
		goto exit;
	}

	newlen = fqn_args->len + strlen(datum->name) + 1;
	if (newlen >= CIL_MAX_NAME_LENGTH) {
		cil_log(CIL_INFO, "Fully qualified name for block %s is too long\n", datum->name);
		rc = SEPOL_ERR;
		goto exit;
	}

	child_args.node = node;
	child_args.len = newlen;
	strcpy(child_args.prefix, fqn_args->prefix);
	strcat(child_args.prefix, datum->name);
	strcat(child_args.prefix, ".");

	for (i=1; i<CIL_SYM_NUM; i++) {
		switch (i) {
		case CIL_SYM_CLASSPERMSETS:
		case CIL_SYM_CONTEXTS:
		case CIL_SYM_LEVELRANGES:
		case CIL_SYM_IPADDRS:
		case CIL_SYM_NAMES:
		case CIL_SYM_PERMX:
			/* These do not show up in the kernel policy */
			break;
		case CIL_SYM_POLICYCAPS:
			/* Valid policy capability names are defined in libsepol */
			break;
		default:
			rc = cil_symtab_map(&(block->symtab[i]), __cil_fqn_qualify_decls, &child_args);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			break;
		}
	}

	rc = cil_symtab_map(&(block->symtab[CIL_SYM_BLOCKS]), __cil_fqn_qualify_blocks, &child_args);

exit:
	if (rc != SEPOL_OK) {
		cil_tree_log(node, CIL_ERR,"Problem qualifying names in block");
	}

	return rc;
}

int cil_fqn_qualify(struct cil_tree_node *root_node)
{
	struct cil_root *root = root_node->data;
	struct cil_fqn_args fqn_args;

	fqn_args.prefix[0] = '\0';
	fqn_args.len = 0;
	fqn_args.node = root_node;

	return cil_symtab_map(&(root->symtab[CIL_SYM_BLOCKS]), __cil_fqn_qualify_blocks, &fqn_args);
}


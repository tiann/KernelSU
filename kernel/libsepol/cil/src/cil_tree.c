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

#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>

#include <sepol/policydb/conditional.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_log.h"
#include "cil_tree.h"
#include "cil_list.h"
#include "cil_parser.h"
#include "cil_strpool.h"

struct cil_tree_node *cil_tree_get_next_path(struct cil_tree_node *node, char **info_kind, uint32_t *hll_line, char **path)
{
	int rc;

	if (!node) {
		goto exit;
	}

	node = node->parent;

	while (node) {
		if (node->flavor == CIL_NODE && node->data == NULL) {
			if (node->cl_head && node->cl_head->data == CIL_KEY_SRC_INFO) {
				if (!node->cl_head->next || !node->cl_head->next->next || !node->cl_head->next->next->next) {
					goto exit;
				}
				/* Parse Tree */
				*info_kind = node->cl_head->next->data;
				rc = cil_string_to_uint32(node->cl_head->next->next->data, hll_line, 10);
				if (rc != SEPOL_OK) {
					goto exit;
				}
				*path = node->cl_head->next->next->next->data;
				return node;
			}
			node = node->parent;
		} else if (node->flavor == CIL_SRC_INFO) {
				/* AST */
				struct cil_src_info *info = node->data;
				*info_kind = info->kind;
				*hll_line = info->hll_line;
				*path = info->path;
				return node;
		} else {
			if (node->flavor == CIL_CALL) {
				struct cil_call *call = node->data;
				node = NODE(call->macro);
			} else if (node->flavor == CIL_BLOCKINHERIT) {
				struct cil_blockinherit *inherit = node->data;
				node = NODE(inherit->block);
			} else {
				node = node->parent;
			}
		}
	}

exit:
	*info_kind = NULL;
	*hll_line = 0;
	*path = NULL;
	return NULL;
}

char *cil_tree_get_cil_path(struct cil_tree_node *node)
{
	char *info_kind;
	uint32_t hll_line;
	char *path;

	while (node) {
		node = cil_tree_get_next_path(node, &info_kind, &hll_line, &path);
		if (node && info_kind == CIL_KEY_SRC_CIL) {
			return path;
		}
	}

	return NULL;
}

__attribute__((format (printf, 3, 4))) void cil_tree_log(struct cil_tree_node *node, enum cil_log_level lvl, const char* msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	cil_vlog(lvl, msg, ap);
	va_end(ap);

	if (node) {
		char *path = NULL;
		uint32_t hll_offset = node->hll_offset;

		path = cil_tree_get_cil_path(node);

		if (path != NULL) {
			cil_log(lvl, " at %s:%u", path, node->line);
		}

		while (node) {
			do {
				char *info_kind;
				uint32_t hll_line;

				node = cil_tree_get_next_path(node, &info_kind, &hll_line, &path);
				if (!node || info_kind == CIL_KEY_SRC_CIL) {
					break;
				}
				if (info_kind == CIL_KEY_SRC_HLL_LMS) {
					hll_line += hll_offset - node->hll_offset - 1;
				}

				cil_log(lvl," from %s:%u", path, hll_line);
			} while (1);
		}
	}

	cil_log(lvl,"\n");
}

int cil_tree_subtree_has_decl(struct cil_tree_node *node)
{
	while (node) {
		if (node->flavor >= CIL_MIN_DECLARATIVE) {
			return CIL_TRUE;
		}
		if (node->cl_head != NULL) {
			if (cil_tree_subtree_has_decl(node->cl_head))
				return CIL_TRUE;
		}
		node = node->next;
	}

	return CIL_FALSE;
}

int cil_tree_init(struct cil_tree **tree)
{
	struct cil_tree *new_tree = cil_malloc(sizeof(*new_tree));

	cil_tree_node_init(&new_tree->root);
	
	*tree = new_tree;
	
	return SEPOL_OK;
}

void cil_tree_destroy(struct cil_tree **tree)
{
	if (tree == NULL || *tree == NULL) {
		return;
	}

	cil_tree_subtree_destroy((*tree)->root);
	free(*tree);
	*tree = NULL;
}

void cil_tree_subtree_destroy(struct cil_tree_node *node)
{
	cil_tree_children_destroy(node);
	cil_tree_node_destroy(&node);
}

void cil_tree_children_destroy(struct cil_tree_node *node)
{
	struct cil_tree_node *curr, *next;

	if (!node) {
		return;
	}

	curr = node->cl_head;
	while (curr) {
		next = curr->next;
		cil_tree_children_destroy(curr);
		cil_tree_node_destroy(&curr);
		curr = next;
	}
	node->cl_head = NULL;
	node->cl_tail = NULL;
}

void cil_tree_node_init(struct cil_tree_node **node)
{
	struct cil_tree_node *new_node = cil_malloc(sizeof(*new_node));
	new_node->cl_head = NULL;
	new_node->cl_tail = NULL;
	new_node->parent = NULL;
	new_node->data = NULL;
	new_node->next = NULL;
	new_node->flavor = CIL_ROOT;
	new_node->line = 0;
	new_node->hll_offset = 0;

	*node = new_node;
}

void cil_tree_node_destroy(struct cil_tree_node **node)
{
	struct cil_symtab_datum *datum;

	if (node == NULL || *node == NULL) {
		return;
	}

	if ((*node)->flavor >= CIL_MIN_DECLARATIVE) {
		datum = (*node)->data;
		cil_symtab_datum_remove_node(datum, *node);
		if (datum->nodes == NULL) {
			cil_destroy_data(&(*node)->data, (*node)->flavor);
		}
	} else {
		cil_destroy_data(&(*node)->data, (*node)->flavor);
	}
	free(*node);
	*node = NULL;
}

/* Perform depth-first walk of the tree
   Parameters:
   start_node:          root node to start walking from
   process_node:        function to call when visiting a node
                        Takes parameters:
                            node:     node being visited
                            finished: boolean indicating to the tree walker that it should move on from this branch
                            extra_args:    additional data
   first_child:		Function to call before entering list of children
                        Takes parameters:
                            node:     node of first child
                            extra args:     additional data
   last_child:		Function to call when finished with the last child of a node's children
   extra_args:               any additional data to be passed to the helper functions
*/

static int cil_tree_walk_core(struct cil_tree_node *node,
					   int (*process_node)(struct cil_tree_node *node, uint32_t *finished, void *extra_args),
					   int (*first_child)(struct cil_tree_node *node, void *extra_args), 
					   int (*last_child)(struct cil_tree_node *node, void *extra_args), 
					   void *extra_args)
{
	int rc = SEPOL_ERR;

	while (node) {
		uint32_t finished = CIL_TREE_SKIP_NOTHING;

		if (process_node != NULL) {
			rc = (*process_node)(node, &finished, extra_args);
			if (rc != SEPOL_OK) {
				cil_tree_log(node, CIL_INFO, "Problem");
				return rc;
			}
		}

		if (finished & CIL_TREE_SKIP_NEXT) {
			return SEPOL_OK;
		}

		if (node->cl_head != NULL && !(finished & CIL_TREE_SKIP_HEAD)) {
			rc = cil_tree_walk(node, process_node, first_child, last_child, extra_args);
			if (rc != SEPOL_OK) {
				return rc;
			}
		}

		node = node->next;
	}

	return SEPOL_OK;
}

int cil_tree_walk(struct cil_tree_node *node, 
				  int (*process_node)(struct cil_tree_node *node, uint32_t *finished, void *extra_args), 
				  int (*first_child)(struct cil_tree_node *node, void *extra_args), 
				  int (*last_child)(struct cil_tree_node *node, void *extra_args), 
				  void *extra_args)
{
	int rc = SEPOL_ERR;

	if (!node || !node->cl_head) {
		return SEPOL_OK;
	}

	if (first_child != NULL) {
		rc = (*first_child)(node->cl_head, extra_args);
		if (rc != SEPOL_OK) {
			cil_tree_log(node, CIL_INFO, "Problem");
			return rc;
		}
	}

	rc = cil_tree_walk_core(node->cl_head, process_node, first_child, last_child, extra_args);
	if (rc != SEPOL_OK) {
		return rc;
	}

	if (last_child != NULL) {
		rc = (*last_child)(node->cl_tail, extra_args);
		if (rc != SEPOL_OK) {
			cil_tree_log(node, CIL_INFO, "Problem");
			return rc;
		}
	}

	return SEPOL_OK;
}

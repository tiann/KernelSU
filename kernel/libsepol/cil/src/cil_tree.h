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

#ifndef CIL_TREE_H_
#define CIL_TREE_H_

#include <stdint.h>

#include "cil_flavor.h"
#include "cil_list.h"

struct cil_tree {
	struct cil_tree_node *root;
};

struct cil_tree_node {
	struct cil_tree_node *parent;
	struct cil_tree_node *cl_head;		//Head of child_list
	struct cil_tree_node *cl_tail;		//Tail of child_list
	struct cil_tree_node *next;		//Each element in the list points to the next element
	enum cil_flavor flavor;
	uint32_t line;
	uint32_t hll_offset;
	void *data;
};

struct cil_tree_node *cil_tree_get_next_path(struct cil_tree_node *node, char **info_kind, uint32_t *hll_line, char **path);
char *cil_tree_get_cil_path(struct cil_tree_node *node);
__attribute__((format (printf, 3, 4))) void cil_tree_log(struct cil_tree_node *node, enum cil_log_level lvl, const char* msg, ...);

int cil_tree_subtree_has_decl(struct cil_tree_node *node);

int cil_tree_init(struct cil_tree **tree);
void cil_tree_destroy(struct cil_tree **tree);
void cil_tree_subtree_destroy(struct cil_tree_node *node);
void cil_tree_children_destroy(struct cil_tree_node *node);

void cil_tree_node_init(struct cil_tree_node **node);
void cil_tree_node_destroy(struct cil_tree_node **node);

//finished values
#define CIL_TREE_SKIP_NOTHING	0
#define CIL_TREE_SKIP_NEXT	1
#define CIL_TREE_SKIP_HEAD	2
#define CIL_TREE_SKIP_ALL	(CIL_TREE_SKIP_NOTHING | CIL_TREE_SKIP_NEXT | CIL_TREE_SKIP_HEAD)
int cil_tree_walk(struct cil_tree_node *start_node, int (*process_node)(struct cil_tree_node *node, uint32_t *finished, void *extra_args), int (*first_child)(struct cil_tree_node *node, void *extra_args), int (*last_child)(struct cil_tree_node *node, void *extra_args), void *extra_args);

#endif /* CIL_TREE_H_ */


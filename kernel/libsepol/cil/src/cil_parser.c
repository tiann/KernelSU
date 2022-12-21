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
#include <stdint.h>
#include <sepol/errcodes.h>

#include "cil_internal.h"
#include "cil_log.h"
#include "cil_mem.h"
#include "cil_tree.h"
#include "cil_lexer.h"
#include "cil_parser.h"
#include "cil_strpool.h"
#include "cil_stack.h"

#define CIL_PARSER_MAX_EXPR_DEPTH (0x1 << 12)

struct hll_info {
	uint32_t hll_offset;
	uint32_t hll_expand;
};

static void push_hll_info(struct cil_stack *stack, uint32_t hll_offset, uint32_t hll_expand)
{
	struct hll_info *new = cil_malloc(sizeof(*new));

	new->hll_offset = hll_offset;
	new->hll_expand = hll_expand;

	cil_stack_push(stack, CIL_NONE, new);
}

static void pop_hll_info(struct cil_stack *stack, uint32_t *hll_offset, uint32_t *hll_expand)
{
	struct cil_stack_item *curr = cil_stack_pop(stack);
	struct hll_info *info;

	if (!curr) {
		return;
	}
	info = curr->data;
	*hll_expand = info->hll_expand;
	*hll_offset = info->hll_offset;
	free(curr->data);
}

static void create_node(struct cil_tree_node **node, struct cil_tree_node *current, uint32_t line, uint32_t hll_offset, void *value)
{
	cil_tree_node_init(node);
	(*node)->parent = current;
	(*node)->flavor = CIL_NODE;
	(*node)->line = line;
	(*node)->hll_offset = hll_offset;
	(*node)->data = value;
}

static void insert_node(struct cil_tree_node *node, struct cil_tree_node *current)
{
	if (current->cl_head == NULL) {
		current->cl_head = node;
	} else {
		current->cl_tail->next = node;
	}
	current->cl_tail = node;
}

static int add_hll_linemark(struct cil_tree_node **current, uint32_t *hll_offset, uint32_t *hll_expand, struct cil_stack *stack, char *path)
{
	char *hll_type;
	struct cil_tree_node *node;
	struct token tok;
	uint32_t prev_hll_expand, prev_hll_offset;

	cil_lexer_next(&tok);
	if (tok.type != SYMBOL) {
		cil_log(CIL_ERR, "Invalid line mark syntax\n");
		goto exit;
	}
	hll_type = cil_strpool_add(tok.value);
	if (hll_type != CIL_KEY_SRC_HLL_LME && hll_type != CIL_KEY_SRC_HLL_LMS && hll_type != CIL_KEY_SRC_HLL_LMX) {
		cil_log(CIL_ERR, "Invalid line mark syntax\n");
		goto exit;
	}
	if (hll_type == CIL_KEY_SRC_HLL_LME) {
		if (cil_stack_is_empty(stack)) {
			cil_log(CIL_ERR, "Line mark end without start\n");
			goto exit;
		}
		prev_hll_expand = *hll_expand;
		prev_hll_offset = *hll_offset;
		pop_hll_info(stack, hll_offset, hll_expand);
		if (!*hll_expand) {
			/* This is needed if not going back to an lmx section. */
			*hll_offset = prev_hll_offset;
		}
		if (prev_hll_expand && !*hll_expand) {
			/* This is needed to count the lme at the end of an lmx section
			 * within an lms section (or within no hll section).
			 */
			(*hll_offset)++;
		}
		*current = (*current)->parent;
	} else {
		push_hll_info(stack, *hll_offset, *hll_expand);
		if (cil_stack_number_of_items(stack) > CIL_PARSER_MAX_EXPR_DEPTH) {
			cil_log(CIL_ERR, "Number of active line marks exceeds limit of %d\n", CIL_PARSER_MAX_EXPR_DEPTH);
			goto exit;
		}

		create_node(&node, *current, tok.line, *hll_offset, NULL);
		insert_node(node, *current);
		*current = node;

		create_node(&node, *current, tok.line, *hll_offset, CIL_KEY_SRC_INFO);
		insert_node(node, *current);

		create_node(&node, *current, tok.line, *hll_offset, hll_type);
		insert_node(node, *current);

		cil_lexer_next(&tok);
		if (tok.type != SYMBOL) {
			cil_log(CIL_ERR, "Invalid line mark syntax\n");
			goto exit;
		}

		create_node(&node, *current, tok.line, *hll_offset, cil_strpool_add(tok.value));
		insert_node(node, *current);

		cil_lexer_next(&tok);
		if (tok.type != SYMBOL && tok.type != QSTRING) {
			cil_log(CIL_ERR, "Invalid line mark syntax\n");
			goto exit;
		}

		if (tok.type == QSTRING) {
			tok.value[strlen(tok.value) - 1] = '\0';
			tok.value = tok.value+1;
		}

		create_node(&node, *current, tok.line, *hll_offset, cil_strpool_add(tok.value));
		insert_node(node, *current);

		*hll_expand = (hll_type == CIL_KEY_SRC_HLL_LMX) ? 1 : 0;
	}

	cil_lexer_next(&tok);
	if (tok.type != NEWLINE) {
		cil_log(CIL_ERR, "Invalid line mark syntax\n");
		goto exit;
	}

	if (!*hll_expand) {
		/* Need to increment because of the NEWLINE */
		(*hll_offset)++;
	}

	return SEPOL_OK;

exit:
	cil_log(CIL_ERR, "Problem with high-level line mark at line %u of %s\n", tok.line, path);
	return SEPOL_ERR;
}

static void add_cil_path(struct cil_tree_node **current, char *path)
{
	struct cil_tree_node *node;

	create_node(&node, *current, 0, 0, NULL);
	insert_node(node, *current);
	*current = node;

	create_node(&node, *current, 0, 0, CIL_KEY_SRC_INFO);
	insert_node(node, *current);

	create_node(&node, *current, 0, 0, CIL_KEY_SRC_CIL);
	insert_node(node, *current);

	create_node(&node, *current, 0, 0, cil_strpool_add("1"));
	insert_node(node, *current);

	create_node(&node, *current, 0, 0, path);
	insert_node(node, *current);
}

int cil_parser(const char *_path, char *buffer, uint32_t size, struct cil_tree **parse_tree)
{

	int paren_count = 0;

	struct cil_tree *tree = NULL;
	struct cil_tree_node *node = NULL;
	struct cil_tree_node *current = NULL;
	char *path = cil_strpool_add(_path);
	struct cil_stack *stack;
	uint32_t hll_offset = 1;
	uint32_t hll_expand = 0;
	struct token tok;
	int rc = SEPOL_OK;

	cil_stack_init(&stack);

	cil_lexer_setup(buffer, size);

	tree = *parse_tree;
	current = tree->root;

	add_cil_path(&current, path);

	do {
		cil_lexer_next(&tok);
		switch (tok.type) {
		case HLL_LINEMARK:
			rc = add_hll_linemark(&current, &hll_offset, &hll_expand, stack, path);
			if (rc != SEPOL_OK) {
				goto exit;
			}
			break;
		case OPAREN:
			paren_count++;
			if (paren_count > CIL_PARSER_MAX_EXPR_DEPTH) {
				cil_log(CIL_ERR, "Number of open parenthesis exceeds limit of %d at line %d of %s\n", CIL_PARSER_MAX_EXPR_DEPTH, tok.line, path);
				goto exit;
			}
			create_node(&node, current, tok.line, hll_offset, NULL);
			insert_node(node, current);
			current = node;
			break;
		case CPAREN:
			paren_count--;
			if (paren_count < 0) {
				cil_log(CIL_ERR, "Close parenthesis without matching open at line %d of %s\n", tok.line, path);
				goto exit;
			}
			current = current->parent;
			break;
		case QSTRING:
			tok.value[strlen(tok.value) - 1] = '\0';
			tok.value = tok.value+1;
			/* FALLTHRU */
		case SYMBOL:
			if (paren_count == 0) {
				cil_log(CIL_ERR, "Symbol not inside parenthesis at line %d of %s\n", tok.line, path);
				goto exit;
			}

			create_node(&node, current, tok.line, hll_offset, cil_strpool_add(tok.value));
			insert_node(node, current);
			break;
		case NEWLINE :
			if (!hll_expand) {
				hll_offset++;
			}
			break;
		case COMMENT:
			while (tok.type != NEWLINE && tok.type != END_OF_FILE) {
				cil_lexer_next(&tok);
			}
			if (!hll_expand) {
				hll_offset++;
			}
			if (tok.type != END_OF_FILE) {
				break;
			}
			/* FALLTHRU */
			// Fall through if EOF
		case END_OF_FILE:
			if (paren_count > 0) {
				cil_log(CIL_ERR, "Open parenthesis without matching close at line %d of %s\n", tok.line, path);
				goto exit;
			}
			if (!cil_stack_is_empty(stack)) {
				cil_log(CIL_ERR, "High-level language line marker start without close at line %d of %s\n", tok.line, path);
				goto exit;
			}
			break;
		case UNKNOWN:
			cil_log(CIL_ERR, "Invalid token '%s' at line %d of %s\n", tok.value, tok.line, path);
			goto exit;
		default:
			cil_log(CIL_ERR, "Unknown token type '%d' at line %d of %s\n", tok.type, tok.line, path);
			goto exit;
		}
	}
	while (tok.type != END_OF_FILE);

	cil_lexer_destroy();

	cil_stack_destroy(&stack);

	*parse_tree = tree;

	return SEPOL_OK;

exit:
	while (!cil_stack_is_empty(stack)) {
		pop_hll_info(stack, &hll_offset, &hll_expand);
	}
	cil_lexer_destroy();
	cil_stack_destroy(&stack);

	return SEPOL_ERR;
}

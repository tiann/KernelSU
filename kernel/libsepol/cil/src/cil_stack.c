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

#include "cil_internal.h"
#include "cil_log.h"
#include "cil_mem.h"
#include "cil_stack.h"


#define CIL_STACK_INIT_SIZE 16

void cil_stack_init(struct cil_stack **stack)
{
	struct cil_stack *new_stack = cil_malloc(sizeof(*new_stack));
	new_stack->stack = cil_malloc(sizeof(*(new_stack->stack)) * CIL_STACK_INIT_SIZE);
	new_stack->size = CIL_STACK_INIT_SIZE;
	new_stack->pos = -1;
	*stack = new_stack;
}

void cil_stack_destroy(struct cil_stack **stack)
{
	if (stack == NULL || *stack == NULL) {
		return;
	}

	free((*stack)->stack);
	free(*stack);
	*stack = NULL;
}

void cil_stack_empty(struct cil_stack *stack)
{
	stack->pos = -1;
}

int cil_stack_is_empty(struct cil_stack *stack)
{
	return (stack->pos == -1);
}

int cil_stack_number_of_items(struct cil_stack *stack)
{
	return stack->pos + 1;
}

void cil_stack_push(struct cil_stack *stack, enum cil_flavor flavor, void *data)
{
	stack->pos++;

	if (stack->pos == stack->size) {
		stack->size *= 2;
		stack->stack = cil_realloc(stack->stack, sizeof(*stack->stack) * stack->size);
	}

	stack->stack[stack->pos].flavor = flavor;
	stack->stack[stack->pos].data = data;
}

struct cil_stack_item *cil_stack_pop(struct cil_stack *stack)
{
	if (stack->pos == -1) {
		return NULL;
	}

	stack->pos--;
	return &stack->stack[stack->pos + 1];
}

struct cil_stack_item *cil_stack_peek(struct cil_stack *stack)
{
	if (stack->pos < 0) {
		return NULL;
	}

	return &stack->stack[stack->pos];
}

struct cil_stack_item *cil_stack_peek_at(struct cil_stack *stack, int pos)
{
	int peekpos = stack->pos - pos;

	if (peekpos < 0 || peekpos > stack->pos) {
		return NULL;
	}

	return &stack->stack[peekpos];
}

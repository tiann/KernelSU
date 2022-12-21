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

#ifndef CIL_STACK_H_
#define CIL_STACK_H_

struct cil_stack {
	struct cil_stack_item *stack;
	int size;
	int pos;
};

struct cil_stack_item {
	enum cil_flavor flavor;
	void *data;
};

#define cil_stack_for_each_starting_at(stack, start, pos, item) \
	for (pos = start, item = cil_stack_peek_at(stack, pos); item != NULL; pos++, item = cil_stack_peek_at(stack, pos))

#define cil_stack_for_each(stack, pos, item) cil_stack_for_each_starting_at(stack, 0, pos, item)


void cil_stack_init(struct cil_stack **stack);
void cil_stack_destroy(struct cil_stack **stack);

void cil_stack_empty(struct cil_stack *stack);
int cil_stack_is_empty(struct cil_stack *stack);
int cil_stack_number_of_items(struct cil_stack *stack);

void cil_stack_push(struct cil_stack *stack, enum cil_flavor flavor, void *data);
struct cil_stack_item *cil_stack_pop(struct cil_stack *stack);
struct cil_stack_item *cil_stack_peek(struct cil_stack *stack);
struct cil_stack_item *cil_stack_peek_at(struct cil_stack *stack, int pos);


#endif

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

#ifndef CIL_LIST_H_
#define CIL_LIST_H_

#include "cil_flavor.h"

struct cil_list {
	struct cil_list_item *head;
	struct cil_list_item *tail;
	enum cil_flavor flavor;
};

struct cil_list_item {
	struct cil_list_item *next;
	enum cil_flavor flavor;
	void *data;
};

#define cil_list_for_each(item, list) \
	for (item = (list)->head; item != NULL; item = item->next)


void cil_list_init(struct cil_list **list, enum cil_flavor flavor);
void cil_list_destroy (struct cil_list **list, unsigned destroy_data);
void cil_list_item_init(struct cil_list_item **item);
void cil_list_item_destroy(struct cil_list_item **item, unsigned destroy_data);
void cil_list_append(struct cil_list *list, enum cil_flavor flavor, void *data);
void cil_list_prepend(struct cil_list *list, enum cil_flavor flavor, void *data);
void cil_list_remove(struct cil_list *list, enum cil_flavor flavor, void *data, unsigned destroy_data);
struct cil_list_item *cil_list_insert(struct cil_list *list, struct cil_list_item *curr, enum cil_flavor flavor, void *data);
void cil_list_append_item(struct cil_list *list, struct cil_list_item *item);
void cil_list_prepend_item(struct cil_list *list, struct cil_list_item *item);
int cil_list_contains(struct cil_list *list, void *data);
int cil_list_match_any(struct cil_list *l1, struct cil_list *l2);

#endif

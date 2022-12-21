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
#include <stdarg.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_log.h"
#include "cil_mem.h"

__attribute__((noreturn)) __attribute__((format (printf, 1, 2))) static void cil_list_error(const char* msg, ...)
{
	va_list ap;
	va_start(ap, msg);
	cil_vlog(CIL_ERR, msg, ap);
	va_end(ap);
	exit(1);
}

void cil_list_init(struct cil_list **list, enum cil_flavor flavor)
{
	struct cil_list *new_list = cil_malloc(sizeof(*new_list));
	new_list->head = NULL;
	new_list->tail = NULL;
	new_list->flavor = flavor;
	*list = new_list;
}

void cil_list_destroy(struct cil_list **list, unsigned destroy_data)
{
	struct cil_list_item *item;

	if (*list == NULL) {
		return;
	}

	item = (*list)->head;
	while (item != NULL)
	{
		struct cil_list_item *next = item->next;
		if (item->flavor == CIL_LIST) {
			cil_list_destroy((struct cil_list**)&(item->data), destroy_data);
			free(item);
		} else {
			cil_list_item_destroy(&item, destroy_data);
		}
		item = next;
	}
	free(*list);
	*list = NULL;
}

void cil_list_item_init(struct cil_list_item **item)
{
	struct cil_list_item *new_item = cil_malloc(sizeof(*new_item));
	new_item->next = NULL;
	new_item->flavor = CIL_NONE;
	new_item->data = NULL;

	*item = new_item;
}

void cil_list_item_destroy(struct cil_list_item **item, unsigned destroy_data)
{
	if (destroy_data) {
		cil_destroy_data(&(*item)->data, (*item)->flavor);
	}
	free(*item);
	*item = NULL;
}

void cil_list_append(struct cil_list *list, enum cil_flavor flavor, void *data)
{
	struct cil_list_item *item;

	if (list == NULL) {
		cil_list_error("Attempt to append data to a NULL list");
	}

	cil_list_item_init(&item);
	item->flavor = flavor;
	item->data = data;

	if (list->tail == NULL) {
		list->head = item;
		list->tail = item;
		return;
	}

	list->tail->next = item;
	list->tail = item;
}

void cil_list_prepend(struct cil_list *list, enum cil_flavor flavor, void *data)
{
	struct cil_list_item *item;

	if (list == NULL) {
		cil_list_error("Attempt to prepend data to a NULL list");
	}

	cil_list_item_init(&item);
	item->flavor = flavor;
	item->data = data;

	if (list->tail == NULL) {
		list->head = item;
		list->tail = item;
		return;
	}

	item->next = list->head;
	list->head = item;
}

struct cil_list_item *cil_list_insert(struct cil_list *list, struct cil_list_item *curr, enum cil_flavor flavor, void *data)
{
	struct cil_list_item *item;

	if (list == NULL) {
		cil_list_error("Attempt to append data to a NULL list");
	}

	if (curr == NULL) {
		/* Insert at the front of the list */
		cil_list_prepend(list, flavor, data);
		return list->head;
	}

	if (curr == list->tail) {
		cil_list_append(list, flavor, data);
		return list->tail;
	}

	cil_list_item_init(&item);
	item->flavor = flavor;
	item->data = data;
	item->next = curr->next;

	curr->next = item;

	return item;
}

void cil_list_append_item(struct cil_list *list, struct cil_list_item *item)
{
	struct cil_list_item *last = item;

	if (list == NULL) {
		cil_list_error("Attempt to append an item to a NULL list");
	}

	if (item == NULL) {
		cil_list_error("Attempt to append a NULL item to a list");
	}

	while (last->next != NULL) {
		last = last->next;
	}

	if (list->tail == NULL) {
		list->head = item;
		list->tail = last;
		return;
	}

	list->tail->next = item;
	list->tail = last;

}

void cil_list_prepend_item(struct cil_list *list, struct cil_list_item *item)
{
	struct cil_list_item *last = item;

	if (list == NULL) {
		cil_list_error("Attempt to prepend an item to a NULL list");
	}

	if (item == NULL) {
		cil_list_error("Attempt to prepend a NULL item to a list");
	}

	while (last->next != NULL) {
		last = last->next;
	}

	if (list->tail == NULL) {
		list->head = item;
		list->tail = last;
		return;
	}

	last->next = list->head;
	list->head = item;
}

void cil_list_remove(struct cil_list *list, enum cil_flavor flavor, void *data, unsigned destroy_data)
{
	struct cil_list_item *item;
	struct cil_list_item *previous = NULL;

	if (list == NULL) {
		cil_list_error("Attempt to remove data from a NULL list");
	}

	cil_list_for_each(item, list) {
		if (item->data == data && item->flavor == flavor) {
			if (previous == NULL) {
				list->head = item->next;
			} else {
				previous->next = item->next;
			}
			if (item->next == NULL) {
				list->tail = previous;
			}
			cil_list_item_destroy(&item, destroy_data);
			break;
		}
		previous = item;
	}
}

int cil_list_contains(struct cil_list *list, void *data)
{
	struct cil_list_item *curr = NULL;

	cil_list_for_each(curr, list) {
		if (curr->data == data) {
			return CIL_TRUE;
		}
	}

	return CIL_FALSE;
}

int cil_list_match_any(struct cil_list *l1, struct cil_list *l2)
{
	struct cil_list_item *i1;
	struct cil_list_item *i2;

	cil_list_for_each(i1, l1) {
		cil_list_for_each(i2, l2) {
			if (i1->data == i2->data && i1->flavor == i2->flavor) {
				return CIL_TRUE;
			}
		}
	}

	return CIL_FALSE;
}

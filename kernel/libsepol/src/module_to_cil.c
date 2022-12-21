/* Authors: Steve Lawrence <slawrence@tresys.com>
 *
 * Functions to convert policy module to CIL
 *
 * Copyright (C) 2015 Tresys Technology, LLC
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

// #include <arpa/inet.h>
#include <linux/inet.h>
#include <linux/ctype.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <getopt.h>
// #include <libgen.h>
// #include <netinet/in.h>
#include <linux/in.h>
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif
// #include <signal.h>
// #include <stdarg.h>
// #include <stdio.h>
// #include <stdlib.h>
#include <linux/string.h>
// #include <inttypes.h>
#include <linux/types.h>
// #include <sys/stat.h>
// #include <unistd.h>

#include <sepol/module.h>
#include <sepol/module_to_cil.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/polcaps.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/services.h>
#include <sepol/policydb/util.h>

#include "kernel_to_common.h"
#include "private.h"
#include "module_internal.h"

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

static FILE *out_file;

#define STACK_SIZE 16
#define DEFAULT_LEVEL "systemlow"
#define DEFAULT_OBJECT "object_r"
#define GEN_REQUIRE_ATTR "cil_gen_require" /* Also in libsepol/cil/src/cil_post.c */
#define TYPEATTR_INFIX "_typeattr_"        /* Also in libsepol/cil/src/cil_post.c */
#define ROLEATTR_INFIX "_roleattr_"

__attribute__ ((format(printf, 1, 2)))
static void log_err(const char *fmt, ...)
{
	va_list argptr;
	va_start(argptr, fmt);
	if (vfprintf(stderr, fmt, argptr) < 0) {
		_exit(EXIT_FAILURE);
	}
	va_end(argptr);
	if (fprintf(stderr, "\n") < 0) {
		_exit(EXIT_FAILURE);
	}
}

static void cil_indent(int indent)
{
	if (fprintf(out_file, "%*s", indent * 4, "") < 0) {
		log_err("Failed to write to output");
		_exit(EXIT_FAILURE);
	}
}

__attribute__ ((format(printf, 1, 2)))
static void cil_printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	if (vfprintf(out_file, fmt, argptr) < 0) {
		log_err("Failed to write to output");
		_exit(EXIT_FAILURE);
	}
	va_end(argptr);
}

__attribute__ ((format(printf, 2, 3)))
static void cil_println(int indent, const char *fmt, ...)
{
	va_list argptr;
	cil_indent(indent);
	va_start(argptr, fmt);
	if (vfprintf(out_file, fmt, argptr) < 0) {
		log_err("Failed to write to output");
		_exit(EXIT_FAILURE);
	}
	va_end(argptr);
	if (fprintf(out_file, "\n") < 0) {
		log_err("Failed to write to output");
		_exit(EXIT_FAILURE);
	}
}

static int get_line(char **start, char *end, char **line)
{
	int rc = 1;
	char *p = NULL;
	size_t len = 0;

	*line = NULL;

	for (p = *start; p < end && isspace(*p); p++);

	*start = p;

	for (len = 0; p < end && *p != '\n' && *p != '\0'; p++, len++);

	if (zero_or_saturated(len)) {
		rc = 0;
		goto exit;
	}

	*line = malloc(len+1);
	if (*line == NULL) {
		log_err("Out of memory");
		rc = -1;
		goto exit;
	}

	memcpy(*line, *start, len);
	(*line)[len] = '\0';

	*start = p;

	return rc;

exit:
	*start = NULL;
	return rc;
}

struct map_args {
	struct policydb *pdb;
	struct avrule_block *block;
	struct stack *decl_stack;
	int scope;
	int indent;
	int sym_index;
};

struct stack {
	 void **stack;
	 int pos;
	 int size;
};

struct role_list_node {
	char *role_name;
	role_datum_t *role;
};

struct attr_list_node {
	char *attr_name;
	int is_type;
	void *set;
};

struct list_node {
	void *data;
	struct list_node *next;
};

struct list {
	struct list_node *head;
};

/* A linked list of all roles stored in the pdb
 * which is iterated to determine types associated
 * with each role when printing role_type statements
 */
static struct list *role_list;

static void list_destroy(struct list **list)
{
	struct list_node *curr = (*list)->head;
	struct list_node *tmp;

	while (curr != NULL) {
		tmp = curr->next;
		free(curr);
		curr = tmp;
	}

	free(*list);
	*list = NULL;
}

static void role_list_destroy(void)
{
	struct list_node *curr;

	if (role_list == NULL) {
		return;
	}
	curr = role_list->head;

	while (curr != NULL) {
		free(curr->data);
		curr->data = NULL;
		curr = curr->next;
	}

	list_destroy(&role_list);
}

static void attr_list_destroy(struct list **attr_list)
{
	struct list_node *curr;
	struct attr_list_node *attr;

	if (attr_list == NULL || *attr_list == NULL) {
		return;
	}

	curr = (*attr_list)->head;

	while (curr != NULL) {
		attr = curr->data;
		if (attr != NULL) {
			free(attr->attr_name);
		}

		free(curr->data);
		curr->data = NULL;
		curr = curr->next;
	}

	list_destroy(attr_list);
}

static int list_init(struct list **list)
{
	struct list *l = calloc(1, sizeof(*l));
	if (l == NULL) {
		return -1;
	}

	*list = l;
	return 0;
}

static int list_prepend(struct list *list, void *data)
{
	int rc = -1;
	struct list_node *node = calloc(1, sizeof(*node));
	if (node == NULL) {
		goto exit;
	}

	node->data = data;
	node->next = list->head;
	list->head = node;

	rc = 0;

exit:
	return rc;
}

static int roles_gather_map(char *key, void *data, void *args)
{
	struct role_list_node *role_node;
	role_datum_t *role = data;
	int rc = -1;

	role_node = calloc(1, sizeof(*role_node));
	if (role_node == NULL) {
		return rc;
	}

	role_node->role_name = key;
	role_node->role = role;

	rc = list_prepend((struct list *)args, role_node);
	if (rc != 0)
		free(role_node);
	return rc;
}

static int role_list_create(hashtab_t roles_tab)
{
	int rc = -1;

	rc = list_init(&role_list);
	if (rc != 0) {
		goto exit;
	}

	rc = ksu_hashtab_map(roles_tab, roles_gather_map, role_list);

exit:
	return rc;
}

// array of lists, where each list contains all the aliases defined in the scope at index i
static struct list **typealias_lists;
static uint32_t typealias_lists_len;

static int typealiases_gather_map(char *key, void *data, void *arg)
{
	int rc = -1;
	struct type_datum *type = data;
	struct policydb *pdb = arg;
	struct scope_datum *scope;
	uint32_t len;
	uint32_t scope_id;

	if (type->primary != 1) {
		scope = hashtab_search(pdb->scope[SYM_TYPES].table, key);
		if (scope == NULL) {
			return -1;
		}

		len = scope->decl_ids_len;
		if (len > 0) {
			scope_id = scope->decl_ids[len-1];
			if (typealias_lists[scope_id] == NULL) {
				rc = list_init(&typealias_lists[scope_id]);
				if (rc != 0) {
					goto exit;
				}
			}
			/* As typealias_lists[scope_id] does not hold the
			 * ownership of its items (typealias_list_destroy does
			 * not free the list items), "key" does not need to be
			 * strdup'ed before it is inserted in the list.
			 */
			list_prepend(typealias_lists[scope_id], key);
		}
	}

	return 0;

exit:
	return rc;
}

static void typealias_list_destroy(void)
{
	uint32_t i;
	for (i = 0; i < typealias_lists_len; i++) {
		if (typealias_lists[i] != NULL) {
			list_destroy(&typealias_lists[i]);
		}
	}
	typealias_lists_len = 0;
	free(typealias_lists);
	typealias_lists = NULL;
}

static int typealias_list_create(struct policydb *pdb)
{
	uint32_t max_decl_id = 0;
	struct avrule_decl *decl;
	struct avrule_block *block;
	uint32_t rc = -1;

	for (block = pdb->global; block != NULL; block = block->next) {
		decl = block->branch_list;
		if (decl != NULL && decl->decl_id > max_decl_id) {
			max_decl_id = decl->decl_id;
		}
	}

	typealias_lists = calloc(max_decl_id + 1, sizeof(*typealias_lists));
	if (!typealias_lists)
		goto exit;
	typealias_lists_len = max_decl_id + 1;

	rc = ksu_hashtab_map(pdb->p_types.table, typealiases_gather_map, pdb);
	if (rc != 0) {
		goto exit;
	}

	return 0;

exit:
	typealias_list_destroy();

	return rc;
}


static int stack_destroy(struct stack **stack)
{
	if (stack == NULL || *stack == NULL) {
		return 0;
	}

	free((*stack)->stack);
	free(*stack);
	*stack = NULL;

	return 0;
}

static int stack_init(struct stack **stack)
{
	int rc = -1;
	struct stack *s = calloc(1, sizeof(*s));
	if (s == NULL) {
		goto exit;
	}

	s->stack = calloc(STACK_SIZE, sizeof(*s->stack));
	if (s->stack == NULL) {
		goto exit;
	}

	s->pos = -1;
	s->size = STACK_SIZE;

	*stack = s;

	return 0;

exit:
	stack_destroy(&s);
	return rc;
}

static int stack_push(struct stack *stack, void *ptr)
{
	int rc = -1;
	void *new_stack;

	if (stack->pos + 1 == stack->size) {
		new_stack = reallocarray(stack->stack, stack->size * 2, sizeof(*stack->stack));
		if (new_stack == NULL) {
			goto exit;
		}
		stack->stack = new_stack;
		stack->size *= 2;
	}

	stack->pos++;
	stack->stack[stack->pos] = ptr;

	rc = 0;
exit:
	return rc;
}

static void *stack_pop(struct stack *stack)
{
	if (stack->pos == -1) {
		return NULL;
	}

	stack->pos--;
	return stack->stack[stack->pos + 1];
}

static void *stack_peek(struct stack *stack)
{
	if (stack->pos == -1) {
		return NULL;
	}

	return stack->stack[stack->pos];
}

static int is_id_in_scope_with_start(struct policydb *pdb, struct stack *decl_stack, int start, uint32_t symbol_type, char *id)
{
	int i;
	uint32_t j;
	struct avrule_decl *decl;
	struct scope_datum *scope;

	scope = hashtab_search(pdb->scope[symbol_type].table, id);
	if (scope == NULL) {
		return 0;
	}

	for (i = start; i >= 0; i--) {
		decl = decl_stack->stack[i];

		for (j = 0; j < scope->decl_ids_len; j++) {
			if (scope->decl_ids[j] == decl->decl_id) {
				return 1;
			}
		}
	}

	return 0;
}

static int is_id_in_ancestor_scope(struct policydb *pdb, struct stack *decl_stack, char *type, uint32_t symbol_type)
{
	int start = decl_stack->pos - 1;

	return is_id_in_scope_with_start(pdb, decl_stack, start, symbol_type, type);
}

static int is_id_in_scope(struct policydb *pdb, struct stack *decl_stack, char *type, uint32_t symbol_type)
{
	int start = decl_stack->pos;

	return is_id_in_scope_with_start(pdb, decl_stack, start, symbol_type, type);
}

static int semantic_level_to_cil(struct policydb *pdb, int sens_offset, struct mls_semantic_level *level)
{
	struct mls_semantic_cat *cat;

	cil_printf("(%s ", pdb->p_sens_val_to_name[level->sens - sens_offset]);

	if (level->cat != NULL) {
		cil_printf("(");
	}

	for (cat = level->cat; cat != NULL; cat = cat->next) {
		if (cat->low == cat->high) {
			cil_printf("%s", pdb->p_cat_val_to_name[cat->low - 1]);
		} else {
			cil_printf("range %s %s", pdb->p_cat_val_to_name[cat->low - 1], pdb->p_cat_val_to_name[cat->high - 1]);
		}

		if (cat->next != NULL) {
			cil_printf(" ");
		}
	}

	if (level->cat != NULL) {
		cil_printf(")");
	}

	cil_printf(")");

	return 0;
}

static int avrule_to_cil(int indent, struct policydb *pdb, uint32_t type, const char *src, const char *tgt, const struct class_perm_node *classperms)
{
	int rc = -1;
	const char *rule;
	const struct class_perm_node *classperm;
	char *perms;

	switch (type) {
	case AVRULE_ALLOWED:
		rule = "allow";
		break;
	case AVRULE_AUDITALLOW:
		rule = "auditallow";
		break;
	case AVRULE_AUDITDENY:
		rule = "auditdeny";
		break;
	case AVRULE_DONTAUDIT:
		rule = "dontaudit";
		break;
	case AVRULE_NEVERALLOW:
		rule = "neverallow";
		break;
	case AVRULE_TRANSITION:
		rule = "typetransition";
		break;
	case AVRULE_MEMBER:
		rule = "typemember";
		break;
	case AVRULE_CHANGE:
		rule = "typechange";
		break;
	default:
		log_err("Unknown avrule type: %i", type);
		rc = -1;
		goto exit;
	}

	for (classperm = classperms; classperm != NULL; classperm = classperm->next) {
		if (type & AVRULE_AV) {
			perms = sepol_av_to_string(pdb, classperm->tclass, classperm->data);
			if (perms == NULL) {
				log_err("Failed to generate permission string");
				rc = -1;
				goto exit;
			}
			cil_println(indent, "(%s %s %s (%s (%s)))",
					rule, src, tgt,
					pdb->p_class_val_to_name[classperm->tclass - 1],
					perms + 1);
		} else {
			cil_println(indent, "(%s %s %s %s %s)",
					rule, src, tgt,
					pdb->p_class_val_to_name[classperm->tclass - 1],
					pdb->p_type_val_to_name[classperm->data - 1]);
		}
	}

	return 0;

exit:
	return rc;
}

#define next_bit_in_range(i, p) ((i + 1 < sizeof(p)*8) && xperm_test((i + 1), p))

static int xperms_to_cil(const av_extended_perms_t *xperms)
{
	uint16_t value;
	uint16_t low_bit;
	uint16_t low_value;
	unsigned int bit;
	unsigned int in_range = 0;
	int first = 1;

	if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
		&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER))
		return -1;

	for (bit = 0; bit < sizeof(xperms->perms)*8; bit++) {
		if (!xperm_test(bit, xperms->perms))
			continue;

		if (in_range && next_bit_in_range(bit, xperms->perms)) {
			/* continue until high value found */
			continue;
		} else if (next_bit_in_range(bit, xperms->perms)) {
			/* low value */
			low_bit = bit;
			in_range = 1;
			continue;
		}

		if (!first)
			cil_printf(" ");
		else
			first = 0;

		if (xperms->specified & AVTAB_XPERMS_IOCTLFUNCTION) {
			value = xperms->driver<<8 | bit;
			if (in_range) {
				low_value = xperms->driver<<8 | low_bit;
				cil_printf("(range 0x%hx 0x%hx)", low_value, value);
				in_range = 0;
			} else {
				cil_printf("0x%hx", value);
			}
		} else if (xperms->specified & AVTAB_XPERMS_IOCTLDRIVER) {
			value = bit << 8;
			if (in_range) {
				low_value = low_bit << 8;
				cil_printf("(range 0x%hx 0x%hx)", low_value, (uint16_t) (value|0xff));
				in_range = 0;
			} else {
				cil_printf("(range 0x%hx 0x%hx)", value, (uint16_t) (value|0xff));
			}
		}
	}

	return 0;
}

static int avrulex_to_cil(int indent, struct policydb *pdb, uint32_t type, const char *src, const char *tgt, const class_perm_node_t *classperms, const av_extended_perms_t *xperms)
{
	int rc = -1;
	const char *rule;
	const struct class_perm_node *classperm;

	switch (type) {
	case AVRULE_XPERMS_ALLOWED:
		rule = "allowx";
		break;
	case AVRULE_XPERMS_AUDITALLOW:
		rule = "auditallowx";
		break;
	case AVRULE_XPERMS_DONTAUDIT:
		rule = "dontauditx";
		break;
	case AVRULE_XPERMS_NEVERALLOW:
		rule = "neverallowx";
		break;
	default:
		log_err("Unknown avrule xperm type: %i", type);
		rc = -1;
		goto exit;
	}

	for (classperm = classperms; classperm != NULL; classperm = classperm->next) {
		cil_indent(indent);
		cil_printf("(%s %s %s (%s %s (", rule, src, tgt,
			   "ioctl", pdb->p_class_val_to_name[classperm->tclass - 1]);
		xperms_to_cil(xperms);
		cil_printf(")))\n");
	}

	return 0;

exit:
	return rc;
}

static unsigned int num_digits(unsigned int n)
{
	unsigned int num = 1;
	while (n >= 10) {
		n /= 10;
		num++;
	}
	return num;
}

static int ebitmap_to_cil(struct policydb *pdb, struct ebitmap *map, int type)
{
	struct ebitmap_node *node;
	uint32_t i;
	char **val_to_name = pdb->sym_val_to_name[type];

	ebitmap_for_each_positive_bit(map, node, i) {
		cil_printf("%s ", val_to_name[i]);
	}

	return 0;
}

static char *get_new_attr_name(struct policydb *pdb, int is_type)
{
	static unsigned int num_attrs = 0;
	int len, rlen;
	const char *infix;
	char *attr_name = NULL;

	num_attrs++;

	if (is_type) {
		infix = TYPEATTR_INFIX;
	} else {
		infix = ROLEATTR_INFIX;
	}

	len = strlen(pdb->name) + strlen(infix) + num_digits(num_attrs) + 1;
	attr_name = malloc(len);
	if (!attr_name) {
		log_err("Out of memory");
		goto exit;
	}

	rlen = snprintf(attr_name, len, "%s%s%i", pdb->name, infix, num_attrs);
	if (rlen < 0 || rlen >= len) {
		log_err("Failed to generate attribute name");
		free(attr_name);
		attr_name = NULL;
		goto exit;
	}

exit:
	return attr_name;
}

static int cil_add_attr_to_list(struct list *attr_list, char *attr_name, int is_type, void *set)
{
	struct attr_list_node *attr_list_node = NULL;
	int rc = 0;

	attr_list_node = calloc(1, sizeof(*attr_list_node));
	if (attr_list_node == NULL) {
		log_err("Out of memory");
		rc = -1;
		goto exit;
	}

	rc = list_prepend(attr_list, attr_list_node);
	if (rc != 0) {
		goto exit;
	}

	attr_list_node->attr_name = attr_name;
	attr_list_node->is_type = is_type;
	attr_list_node->set = set;

	return rc;

exit:
	free(attr_list_node);
	return rc;
}

static int cil_print_attr_strs(int indent, struct policydb *pdb, int is_type, void *set, char *attr_name)
{
	// CIL doesn't support anonymous positive/negative/complemented sets.  So
	// instead we create a CIL type/roleattributeset that matches the set. If
	// the set has a negative set, then convert it to is (P & !N), where P is
	// the list of members in the positive set and N is the list of members
	// in the negative set. Additionally, if the set is complemented, then wrap
	// the whole thing with a negation.

	struct ebitmap_node *node;
	struct ebitmap *pos, *neg;
	uint32_t flags;
	unsigned i;
	struct type_set *ts;
	struct role_set *rs;
	int has_positive, has_negative;
	const char *kind;
	char **val_to_name;
	int rc = 0;

	if (is_type) {
		kind = "type";
		val_to_name = pdb->p_type_val_to_name;
		ts = (struct type_set *)set;
		pos = &ts->types;
		neg = &ts->negset;
		flags = ts->flags;
		has_positive = pos && !ebitmap_is_empty(pos);
		has_negative = neg && !ebitmap_is_empty(neg);
	} else {
		kind = "role";
		val_to_name = pdb->p_role_val_to_name;
		rs = (struct role_set *)set;
		pos = &rs->roles;
		neg = NULL;
		flags = rs->flags;
		has_positive = pos && !ebitmap_is_empty(pos);
		has_negative = 0;
	}

	cil_println(indent, "(%sattribute %s)", kind, attr_name);
	cil_indent(indent);
	cil_printf("(%sattributeset %s ", kind, attr_name);

	if (flags & TYPE_STAR) {
		cil_printf("(all)");
	}

	if (flags & TYPE_COMP) {
		cil_printf("(not ");
	}

	if (has_positive && has_negative) {
		cil_printf("(and ");
	}

	if (has_positive) {
		cil_printf("(");
		ebitmap_for_each_positive_bit(pos, node, i) {
			cil_printf("%s ", val_to_name[i]);
		}
		cil_printf(") ");
	}

	if (has_negative) {
		cil_printf("(not (");

		ebitmap_for_each_positive_bit(neg, node, i) {
			cil_printf("%s ", val_to_name[i]);
		}

		cil_printf("))");
	}

	if (has_positive && has_negative) {
		cil_printf(")");
	}

	if (flags & TYPE_COMP) {
		cil_printf(")");
	}

	cil_printf(")\n");

	return rc;
}

static int cil_print_attr_list(int indent, struct policydb *pdb, struct list *attr_list)
{
	struct list_node *curr;
	struct attr_list_node *node;
	int rc = 0;

	for (curr = attr_list->head; curr != NULL; curr = curr->next) {
		node = curr->data;
		rc = cil_print_attr_strs(indent, pdb, node->is_type, node->set, node->attr_name);
		if (rc != 0) {
			return rc;
		}
	}

	return rc;
}

static char *search_attr_list(struct list *attr_list, int is_type, void *set)
{
	struct list_node *curr;
	struct attr_list_node *node;
	struct role_set *rs1 = NULL, *rs2;
	struct type_set *ts1 = NULL, *ts2;

	if (is_type) {
		ts1 = (struct type_set *)set;
	} else {
		rs1 = (struct role_set *)set;
	}

	for (curr = attr_list->head; curr != NULL; curr = curr->next) {
		node = curr->data;
		if (node->is_type != is_type)
			continue;
		if (ts1) {
			ts2 = (struct type_set *)node->set;
			if (ts1->flags != ts2->flags)
				continue;
			if (ksu_ebitmap_cmp(&ts1->negset, &ts2->negset) == 0)
				continue;
			if (ksu_ebitmap_cmp(&ts1->types, &ts2->types) == 0)
				continue;
			return node->attr_name;
		} else {
			rs2 = (struct role_set *)node->set;
			if (rs1->flags != rs2->flags)
				continue;
			if (ksu_ebitmap_cmp(&rs1->roles, &rs2->roles) == 0)
				continue;
			return node->attr_name;
		}
	}

	return NULL;
}

static int set_to_names(struct policydb *pdb, int is_type, void *set, struct list *attr_list, char ***names, unsigned int *num_names)
{
	char *attr_name = NULL;
	int rc = 0;

	*names = NULL;
	*num_names = 0;

	attr_name = search_attr_list(attr_list, is_type, set);

	if (!attr_name) {
		attr_name = get_new_attr_name(pdb, is_type);
		if (!attr_name) {
			rc = -1;
			goto exit;
		}

		rc = cil_add_attr_to_list(attr_list, attr_name, is_type, set);
		if (rc != 0) {
			free(attr_name);
			goto exit;
		}
	}

	*names = malloc(sizeof(char *));
	if (!*names) {
		log_err("Out of memory");
		rc = -1;
		goto exit;
	}
	*names[0] = attr_name;
	*num_names = 1;

exit:
	return rc;
}

static int ebitmap_to_names(struct ebitmap *map, char **vals_to_names, char ***names, unsigned int *num_names)
{
	int rc = 0;
	struct ebitmap_node *node;
	uint32_t i;
	unsigned int num;
	char **name_arr;

	num = 0;
	ebitmap_for_each_positive_bit(map, node, i) {
		if (num >= UINT32_MAX / sizeof(*name_arr)) {
			log_err("Overflow");
			rc = -1;
			goto exit;
		}
		num++;
	}

	if (!num) {
		*names = NULL;
		*num_names = 0;
		goto exit;
	}

	name_arr = calloc(num, sizeof(*name_arr));
	if (name_arr == NULL) {
		log_err("Out of memory");
		rc = -1;
		goto exit;
	}

	num = 0;
	ebitmap_for_each_positive_bit(map, node, i) {
		name_arr[num] = vals_to_names[i];
		num++;
	}

	*names = name_arr;
	*num_names = num;

exit:
	return rc;
}

static int process_roleset(struct policydb *pdb, struct role_set *rs, struct list *attr_list, char ***names, unsigned int *num_names)
{
	int rc = 0;

	*names = NULL;
	*num_names = 0;

	if (rs->flags) {
		rc = set_to_names(pdb, 0, &rs->roles, attr_list, names, num_names);
		if (rc != 0) {
			goto exit;
		}
	} else {
		rc = ebitmap_to_names(&rs->roles, pdb->p_role_val_to_name, names, num_names);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	return rc;
}

static int process_typeset(struct policydb *pdb, struct type_set *ts, struct list *attr_list, char ***names, unsigned int *num_names)
{
	int rc = 0;

	*names = NULL;
	*num_names = 0;

	if (!ebitmap_is_empty(&ts->negset) || ts->flags != 0) {
		rc = set_to_names(pdb, 1, ts, attr_list, names, num_names);
		if (rc != 0) {
			goto exit;
		}
	} else {
		rc = ebitmap_to_names(&ts->types, pdb->p_type_val_to_name, names, num_names);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	return rc;
}

static void names_destroy(char ***names, unsigned int *num_names)
{
	free(*names);
	*names = NULL;
	*num_names = 0;
}

static int roletype_role_in_ancestor_to_cil(struct policydb *pdb, struct stack *decl_stack, char *type_name, int indent)
{
	struct list_node *curr;
	char **tnames = NULL;
	unsigned int num_tnames, i;
	struct role_list_node *role_node = NULL;
	int rc;
	struct type_set *ts;
	struct list *attr_list = NULL;

	rc = list_init(&attr_list);
	if (rc != 0) {
		goto exit;
	}

	for (curr = role_list->head; curr != NULL; curr = curr->next) {
		role_node = curr->data;
		if (!is_id_in_ancestor_scope(pdb, decl_stack, role_node->role_name, SYM_ROLES)) {
			continue;
		}

		ts = &role_node->role->types;
		rc = process_typeset(pdb, ts, attr_list, &tnames, &num_tnames);
		if (rc != 0) {
			goto exit;
		}
		for (i = 0; i < num_tnames; i++) {
			if (!strcmp(type_name, tnames[i])) {
				cil_println(indent, "(roletype %s %s)", role_node->role_name, type_name);
			}
		}
		names_destroy(&tnames, &num_tnames);
	}

	rc = cil_print_attr_list(indent, pdb, attr_list);
	if (rc != 0) {
		goto exit;
	}

exit:
	attr_list_destroy(&attr_list);
	return rc;
}


static int name_list_to_string(char **names, unsigned int num_names, char **string)
{
	// create a space separated string of the names
	int rc = -1;
	size_t len = 0;
	unsigned int i;
	char *str;
	char *strpos;

	for (i = 0; i < num_names; i++) {
		if (__builtin_add_overflow(len, strlen(names[i]), &len)) {
			log_err("Overflow");
			return -1;
		}
	}

	// add spaces + null terminator
	if (__builtin_add_overflow(len, (size_t)num_names, &len)) {
		log_err("Overflow");
		return -1;
	}

	if (!len) {
		log_err("Empty list");
		return -1;
	}

	str = malloc(len);
	if (str == NULL) {
		log_err("Out of memory");
		rc = -1;
		goto exit;
	}
	str[0] = 0;

	strpos = str;

	for (i = 0; i < num_names; i++) {
		strpos = stpcpy(strpos, names[i]);
		if (i < num_names - 1) {
			*strpos++ = ' ';
		}
	}

	*string = str;

	return 0;
exit:
	free(str);
	return rc;
}

static int avrule_list_to_cil(int indent, struct policydb *pdb, struct avrule *avrule_list, struct list *attr_list)
{
	int rc = -1;
	struct avrule *avrule;
	char **snames = NULL;
	char **tnames = NULL;
	unsigned int s, t, num_snames, num_tnames;
	struct type_set *ts;

	for (avrule = avrule_list; avrule != NULL; avrule = avrule->next) {
		if ((avrule->specified & (AVRULE_NEVERALLOW|AVRULE_XPERMS_NEVERALLOW)) &&
		    avrule->source_filename) {
			cil_println(0, ";;* lmx %lu %s\n",avrule->source_line, avrule->source_filename);
		}

		ts = &avrule->stypes;
		rc = process_typeset(pdb, ts, attr_list, &snames, &num_snames);
		if (rc != 0) {
			goto exit;
		}

		ts = &avrule->ttypes;
		rc = process_typeset(pdb, ts, attr_list, &tnames, &num_tnames);
		if (rc != 0) {
			goto exit;
		}

		for (s = 0; s < num_snames; s++) {
			for (t = 0; t < num_tnames; t++) {
				if (avrule->specified & AVRULE_XPERMS) {
					rc = avrulex_to_cil(indent, pdb, avrule->specified, snames[s], tnames[t], avrule->perms, avrule->xperms);
				} else {
					rc = avrule_to_cil(indent, pdb, avrule->specified, snames[s], tnames[t], avrule->perms);
				}
				if (rc != 0) {
					goto exit;
				}
			}

			if (avrule->flags & RULE_SELF) {
				if (avrule->specified & AVRULE_XPERMS) {
					rc = avrulex_to_cil(indent, pdb, avrule->specified, snames[s], "self", avrule->perms, avrule->xperms);
				} else {
					rc = avrule_to_cil(indent, pdb, avrule->specified, snames[s], "self", avrule->perms);
				}
				if (rc != 0) {
					goto exit;
				}
			}
		}

		names_destroy(&snames, &num_snames);
		names_destroy(&tnames, &num_tnames);

		if ((avrule->specified & (AVRULE_NEVERALLOW|AVRULE_XPERMS_NEVERALLOW)) &&
		    avrule->source_filename) {
			cil_println(0, ";;* lme\n");
		}
	}

	return 0;

exit:
	names_destroy(&snames, &num_snames);
	names_destroy(&tnames, &num_tnames);

	return rc;
}

static int cond_expr_to_cil(int indent, struct policydb *pdb, struct cond_expr *cond_expr, uint32_t flags)
{
	int rc = 0;
	struct cond_expr *curr;
	struct stack *stack = NULL;
	int len = 0;
	int rlen;
	char *new_val = NULL;
	char *val1 = NULL;
	char *val2 = NULL;
	unsigned int num_params;
	const char *op;
	const char *sep;
	const char *type;

	rc = stack_init(&stack);
	if (rc != 0) {
		log_err("Out of memory");
		goto exit;
	}

	for (curr = cond_expr; curr != NULL; curr = curr->next) {
		if (curr->expr_type == COND_BOOL) {
			val1 = pdb->p_bool_val_to_name[curr->bool - 1];
			// length of boolean + 2 parens + null terminator
			len = strlen(val1) + 2 + 1;
			new_val = malloc(len);
			if (new_val == NULL) {
				log_err("Out of memory");
				rc = -1;
				goto exit;
			}
			rlen = snprintf(new_val, len, "(%s)", val1);
			if (rlen < 0 || rlen >= len) {
				log_err("Failed to generate conditional expression");
				rc = -1;
				goto exit;
			}
		} else {
			switch(curr->expr_type) {
			case COND_NOT:	op = "not";	break;
			case COND_OR:	op = "or";	break;
			case COND_AND:	op = "and";	break;
			case COND_XOR:	op = "xor";	break;
			case COND_EQ:	op = "eq";	break;
			case COND_NEQ:	op = "neq";	break;
			default:
				rc = -1;
				goto exit;
			}

			num_params = curr->expr_type == COND_NOT ? 1 : 2;

			if (num_params == 1) {
				val1 = stack_pop(stack);
				val2 = strdup("");
				if (val2 == NULL) {
					log_err("Out of memory");
					rc = -1;
					goto exit;
				}
				sep = "";
			} else {
				val2 = stack_pop(stack);
				val1 = stack_pop(stack);
				sep = " ";
			}

			if (val1 == NULL || val2 == NULL) {
				log_err("Invalid conditional expression");
				rc = -1;
				goto exit;
			}

			// length = length of parameters +
			//          length of operator +
			//          1 space preceding each parameter +
			//          2 parens around the whole expression
			//          + null terminator
			len = strlen(val1) + strlen(val2) + strlen(op) + (num_params * 1) + 2 + 1;
			new_val = malloc(len);
			if (new_val == NULL) {
				log_err("Out of memory");
				rc = -1;
				goto exit;
			}

			rlen = snprintf(new_val, len, "(%s %s%s%s)", op, val1, sep, val2);
			if (rlen < 0 || rlen >= len) {
				log_err("Failed to generate conditional expression");
				rc = -1;
				goto exit;
			}

			free(val1);
			free(val2);
			val1 = NULL;
			val2 = NULL;
		}

		rc = stack_push(stack, new_val);
		if (rc != 0) {
			log_err("Out of memory");
			goto exit;
		}
		new_val = NULL;
	}

	if (flags & COND_NODE_FLAGS_TUNABLE) {
		type = "tunableif";
	} else {
		type = "booleanif";
	}

	val1 = stack_pop(stack);
	if (val1 == NULL || stack_peek(stack) != NULL) {
		log_err("Invalid conditional expression");
		rc = -1;
		goto exit;
	}

	cil_println(indent, "(%s %s", type, val1);
	free(val1);
	val1 = NULL;

	rc = 0;

exit:
	free(new_val);
	free(val1);
	free(val2);
	if (stack != NULL) {
		while ((val1 = stack_pop(stack)) != NULL) {
			free(val1);
		}
		stack_destroy(&stack);
	}
	return rc;
}

static int cond_list_to_cil(int indent, struct policydb *pdb, struct cond_node *cond_list, struct list *attr_list)
{
	int rc = 0;
	struct cond_node *cond;

	for (cond = cond_list; cond != NULL; cond = cond->next) {

		rc = cond_expr_to_cil(indent, pdb, cond->expr, cond->flags);
		if (rc != 0) {
			goto exit;
		}

		if (cond->avtrue_list != NULL) {
			cil_println(indent + 1, "(true");
			rc = avrule_list_to_cil(indent + 2, pdb, cond->avtrue_list, attr_list);
			if (rc != 0) {
				goto exit;
			}
			cil_println(indent + 1, ")");
		}

		if (cond->avfalse_list != NULL) {
			cil_println(indent + 1, "(false");
			rc = avrule_list_to_cil(indent + 2, pdb, cond->avfalse_list, attr_list);
			if (rc != 0) {
				goto exit;
			}
			cil_println(indent + 1, ")");
		}

		cil_println(indent, ")");
	}

exit:
	return rc;
}

static int role_trans_to_cil(int indent, struct policydb *pdb, struct role_trans_rule *rules, struct list *role_attr_list, struct list *type_attr_list)
{
	int rc = 0;
	struct role_trans_rule *rule;
	char **role_names = NULL;
	unsigned int num_role_names = 0;
	unsigned int role;
	char **type_names = NULL;
	unsigned int num_type_names = 0;
	unsigned int type;
	uint32_t i;
	struct ebitmap_node *node;
	struct type_set *ts;
	struct role_set *rs;

	for (rule = rules; rule != NULL; rule = rule->next) {
		rs = &rule->roles;
		rc = process_roleset(pdb, rs, role_attr_list, &role_names, &num_role_names);
		if (rc != 0) {
			goto exit;
		}

		ts = &rule->types;
		rc = process_typeset(pdb, ts, type_attr_list, &type_names, &num_type_names);
		if (rc != 0) {
			goto exit;
		}

		for (role = 0; role < num_role_names; role++) {
			for (type = 0; type < num_type_names; type++) {
				ebitmap_for_each_positive_bit(&rule->classes, node, i) {
					cil_println(indent, "(roletransition %s %s %s %s)",
						    role_names[role], type_names[type],
						    pdb->p_class_val_to_name[i],
						    pdb->p_role_val_to_name[rule->new_role - 1]);
				}
			}
		}

		names_destroy(&role_names, &num_role_names);
		names_destroy(&type_names, &num_type_names);
	}

exit:
	names_destroy(&role_names, &num_role_names);
	names_destroy(&type_names, &num_type_names);

	return rc;
}

static int role_allows_to_cil(int indent, struct policydb *pdb, struct role_allow_rule *rules, struct list *attr_list)
{
	int rc = -1;
	struct role_allow_rule *rule;
	char **roles = NULL;
	unsigned int num_roles = 0;
	char **new_roles = NULL;
	unsigned int num_new_roles = 0;
	unsigned int i, j;
	struct role_set *rs;

	for (rule = rules; rule != NULL; rule = rule->next) {
		rs = &rule->roles;
		rc = process_roleset(pdb, rs, attr_list, &roles, &num_roles);
		if (rc != 0) {
			goto exit;
		}

		rs = &rule->new_roles;
		rc = process_roleset(pdb, rs, attr_list, &new_roles, &num_new_roles);
		if (rc != 0) {
			goto exit;
		}

		for (i = 0; i < num_roles; i++) {
			for (j = 0; j < num_new_roles; j++) {
				cil_println(indent, "(roleallow %s %s)", roles[i], new_roles[j]);
			}
		}

		names_destroy(&roles, &num_roles);
		names_destroy(&new_roles, &num_new_roles);
	}

	rc = 0;

exit:
	names_destroy(&roles, &num_roles);
	names_destroy(&new_roles, &num_new_roles);

	return rc;
}

static int range_trans_to_cil(int indent, struct policydb *pdb, struct range_trans_rule *rules, struct list *attr_list)
{
	int rc = -1;
	struct range_trans_rule *rule;
	char **stypes = NULL;
	unsigned int num_stypes = 0;
	unsigned int stype;
	char **ttypes = NULL;
	unsigned int num_ttypes = 0;
	unsigned int ttype;
	struct ebitmap_node *node;
	uint32_t i;
	struct type_set *ts;

	if (!pdb->mls) {
		return 0;
	}

	for (rule = rules; rule != NULL; rule = rule->next) {
		ts = &rule->stypes;
		rc = process_typeset(pdb, ts, attr_list, &stypes, &num_stypes);
		if (rc != 0) {
			goto exit;
		}

		ts = &rule->ttypes;
		rc = process_typeset(pdb, ts, attr_list, &ttypes, &num_ttypes);
		if (rc != 0) {
			goto exit;
		}

		for (stype = 0; stype < num_stypes; stype++) {
			for (ttype = 0; ttype < num_ttypes; ttype++) {
				ebitmap_for_each_positive_bit(&rule->tclasses, node, i) {
					cil_indent(indent);
					cil_printf("(rangetransition %s %s %s ", stypes[stype], ttypes[ttype], pdb->p_class_val_to_name[i]);

					cil_printf("(");

					rc = semantic_level_to_cil(pdb, 1, &rule->trange.level[0]);
					if (rc != 0) {
						goto exit;
					}

					cil_printf(" ");

					rc = semantic_level_to_cil(pdb, 1, &rule->trange.level[1]);
					if (rc != 0) {
						goto exit;
					}

					cil_printf("))\n");
				}

			}
		}

		names_destroy(&stypes, &num_stypes);
		names_destroy(&ttypes, &num_ttypes);
	}

	rc = 0;

exit:
	names_destroy(&stypes, &num_stypes);
	names_destroy(&ttypes, &num_ttypes);

	return rc;
}

static int filename_trans_to_cil(int indent, struct policydb *pdb, struct filename_trans_rule *rules, struct list *attr_list)
{
	int rc = -1;
	char **stypes = NULL;
	unsigned int num_stypes = 0;
	unsigned int stype;
	char **ttypes = NULL;
	unsigned int num_ttypes = 0;
	unsigned int ttype;
	struct type_set *ts;
	struct filename_trans_rule *rule;

	for (rule = rules; rule != NULL; rule = rule->next) {
		ts = &rule->stypes;
		rc = process_typeset(pdb, ts, attr_list, &stypes, &num_stypes);
		if (rc != 0) {
			goto exit;
		}

		ts = &rule->ttypes;
		rc = process_typeset(pdb, ts, attr_list, &ttypes, &num_ttypes);
		if (rc != 0) {
			goto exit;
		}

		for (stype = 0; stype < num_stypes; stype++) {
			for (ttype = 0; ttype < num_ttypes; ttype++) {
				cil_println(indent, "(typetransition %s %s %s \"%s\" %s)",
					    stypes[stype], ttypes[ttype],
					    pdb->p_class_val_to_name[rule->tclass - 1],
					    rule->name,
					    pdb->p_type_val_to_name[rule->otype - 1]);
			}
			if (rule->flags & RULE_SELF) {
				cil_println(indent, "(typetransition %s self %s \"%s\" %s)",
					    stypes[stype],
					    pdb->p_class_val_to_name[rule->tclass - 1],
					    rule->name,
					    pdb->p_type_val_to_name[rule->otype - 1]);
			}
		}

		names_destroy(&stypes, &num_stypes);
		names_destroy(&ttypes, &num_ttypes);
	}

	rc = 0;
exit:
	names_destroy(&stypes, &num_stypes);
	names_destroy(&ttypes, &num_ttypes);

	return rc;
}

struct class_perm_datum {
	char *name;
	uint32_t val;
};

struct class_perm_array {
	struct class_perm_datum *perms;
	uint32_t count;
};

static int class_perm_to_array(char *key, void *data, void *args)
{
	struct class_perm_array *arr = args;
	struct perm_datum *datum = data;
	arr->perms[arr->count].name = key;
	arr->perms[arr->count].val = datum->s.value;
	arr->count++;

	return 0;
}

static int class_perm_cmp(const void *a, const void *b)
{
	const struct class_perm_datum *aa = a;
	const struct class_perm_datum *bb = b;

	return aa->val - bb->val;
}

static int common_to_cil(char *key, void *data, void *UNUSED(arg))
{
	int rc = -1;
	struct common_datum *common = data;
	struct class_perm_array arr;
	uint32_t i;

	arr.count = 0;
	arr.perms = calloc(common->permissions.nprim, sizeof(*arr.perms));
	if (arr.perms == NULL) {
		goto exit;
	}
	rc = ksu_hashtab_map(common->permissions.table, class_perm_to_array, &arr);
	if (rc != 0) {
		goto exit;
	}

	qsort(arr.perms, arr.count, sizeof(*arr.perms), class_perm_cmp);

	cil_printf("(common %s (", key);
	for (i = 0; i < arr.count; i++) {
		cil_printf("%s ", arr.perms[i].name);
	}
	cil_printf("))\n");

	rc = 0;

exit:
	free(arr.perms);
	return rc;
}


static int constraint_expr_to_string(struct policydb *pdb, struct constraint_expr *exprs, char **expr_string)
{
	int rc = -1;
	struct constraint_expr *expr;
	struct stack *stack = NULL;
	int len = 0;
	int rlen;
	char *new_val = NULL;
	char *val1 = NULL;
	char *val2 = NULL;
	uint32_t num_params;
	const char *op;
	const char *sep;
	const char *attr1;
	const char *attr2;
	char *names = NULL;
	char **name_list = NULL;
	unsigned int num_names = 0;
	struct type_set *ts;

	rc = stack_init(&stack);
	if (rc != 0) {
		goto exit;
	}

	for (expr = exprs; expr != NULL; expr = expr->next) {
		if (expr->expr_type == CEXPR_ATTR || expr->expr_type == CEXPR_NAMES) {
			switch (expr->op) {
			case CEXPR_EQ:      op = "eq";     break;
			case CEXPR_NEQ:     op = "neq";    break;
			case CEXPR_DOM:     op = "dom";    break;
			case CEXPR_DOMBY:   op = "domby";  break;
			case CEXPR_INCOMP:  op = "incomp"; break;
			default:
				log_err("Unknown constraint operator type: %i", expr->op);
				rc = -1;
				goto exit;
			}

			switch (expr->attr) {
			case CEXPR_USER:                 attr1 = "u1"; attr2 = "u2"; break;
			case CEXPR_USER | CEXPR_TARGET:  attr1 = "u2"; attr2 = "";   break;
			case CEXPR_USER | CEXPR_XTARGET: attr1 = "u3"; attr2 = "";   break;
			case CEXPR_ROLE:                 attr1 = "r1"; attr2 = "r2"; break;
			case CEXPR_ROLE | CEXPR_TARGET:  attr1 = "r2"; attr2 = "";   break;
			case CEXPR_ROLE | CEXPR_XTARGET: attr1 = "r3"; attr2 = "";   break;
			case CEXPR_TYPE:                 attr1 = "t1"; attr2 = "t2"; break;
			case CEXPR_TYPE | CEXPR_TARGET:  attr1 = "t2"; attr2 = "";   break;
			case CEXPR_TYPE | CEXPR_XTARGET: attr1 = "t3"; attr2 = "";   break;
			case CEXPR_L1L2:                 attr1 = "l1"; attr2 = "l2"; break;
			case CEXPR_L1H2:                 attr1 = "l1"; attr2 = "h2"; break;
			case CEXPR_H1L2:                 attr1 = "h1"; attr2 = "l2"; break;
			case CEXPR_H1H2:                 attr1 = "h1"; attr2 = "h2"; break;
			case CEXPR_L1H1:                 attr1 = "l1"; attr2 = "h1"; break;
			case CEXPR_L2H2:                 attr1 = "l2"; attr2 = "h2"; break;
			default:
				log_err("Unknown expression attribute type: %i", expr->attr);
				rc = -1;
				goto exit;
			}

			if (expr->expr_type == CEXPR_ATTR) {
				// length of values/attrs + 2 separating spaces + 2 parens + null terminator
				len = strlen(op) + strlen(attr1) + strlen(attr2) + 2 + 2 + 1;
				new_val = malloc(len);
				if (new_val == NULL) {
					log_err("Out of memory");
					rc = -1;
					goto exit;
				}
				rlen = snprintf(new_val, len, "(%s %s %s)", op, attr1, attr2);
				if (rlen < 0 || rlen >= len) {
					log_err("Failed to generate constraint expression");
					rc = -1;
					goto exit;
				}
			} else {
				if (expr->attr & CEXPR_TYPE) {
					ts = expr->type_names;
					rc = ebitmap_to_names(&ts->types, pdb->p_type_val_to_name, &name_list, &num_names);
					if (rc != 0) {
						goto exit;
					}
				} else if (expr->attr & CEXPR_USER) {
					rc = ebitmap_to_names(&expr->names, pdb->p_user_val_to_name, &name_list, &num_names);
					if (rc != 0) {
						goto exit;
					}
				} else if (expr->attr & CEXPR_ROLE) {
					rc = ebitmap_to_names(&expr->names, pdb->p_role_val_to_name, &name_list, &num_names);
					if (rc != 0) {
						goto exit;
					}
				}
				if (num_names == 0) {
					names = strdup("NO_IDENTIFIER");
					if (!names) {
						rc = -1;
						goto exit;
					}
				} else {
					rc = name_list_to_string(name_list, num_names, &names);
					if (rc != 0) {
						goto exit;
					}
				}

				// length of values/oper + 2 spaces + 2 parens + null terminator
				len = strlen(op) + strlen(attr1) +  strlen(names) + 2 + 2 + 1;
				if (num_names > 1) {
					len += 2; // 2 more parens
				}
				new_val = malloc(len);
				if (new_val == NULL) {
					log_err("Out of memory");
					rc = -1;
					goto exit;
				}
				if (num_names > 1) {
					rlen = snprintf(new_val, len, "(%s %s (%s))", op, attr1, names);
				} else {
					rlen = snprintf(new_val, len, "(%s %s %s)", op, attr1, names);
				}
				if (rlen < 0 || rlen >= len) {
					log_err("Failed to generate constraint expression");
					rc = -1;
					goto exit;
				}

				names_destroy(&name_list, &num_names);
				free(names);
				names = NULL;
			}
		} else {
			switch (expr->expr_type) {
			case CEXPR_NOT: op = "not"; break;
			case CEXPR_AND: op = "and"; break;
			case CEXPR_OR:  op = "or"; break;
			default:
				log_err("Unknown constraint expression type: %i", expr->expr_type);
				rc = -1;
				goto exit;
			}

			num_params = expr->expr_type == CEXPR_NOT ? 1 : 2;

			if (num_params == 1) {
				val1 = stack_pop(stack);
				val2 = strdup("");
				if (val2 == NULL) {
					log_err("Out of memory");
					rc = -1;
					goto exit;
				}
				sep = "";
			} else {
				val2 = stack_pop(stack);
				val1 = stack_pop(stack);
				sep = " ";
			}

			if (val1 == NULL || val2 == NULL) {
				log_err("Invalid constraint expression");
				rc = -1;
				goto exit;
			}

			// length = length of parameters +
			//          length of operator +
			//          1 space preceding each parameter +
			//          2 parens around the whole expression
			//          + null terminator
			len = strlen(val1) + strlen(val2) + strlen(op) + (num_params * 1) + 2 + 1;
			new_val = malloc(len);
			if (new_val == NULL) {
				log_err("Out of memory");
				rc = -1;
				goto exit;
			}

			rlen = snprintf(new_val, len, "(%s %s%s%s)", op, val1, sep, val2);
			if (rlen < 0 || rlen >= len) {
				log_err("Failed to generate constraint expression");
				rc = -1;
				goto exit;
			}

			free(val1);
			free(val2);
			val1 = NULL;
			val2 = NULL;
		}

		rc = stack_push(stack, new_val);
		if (rc != 0) {
			log_err("Out of memory");
			goto exit;
		}

		new_val = NULL;
	}

	new_val = stack_pop(stack);
	if (new_val == NULL || stack_peek(stack) != NULL) {
		log_err("Invalid constraint expression");
		rc = -1;
		goto exit;
	}

	*expr_string = new_val;
	new_val = NULL;

	rc = 0;

exit:
	names_destroy(&name_list, &num_names);
	free(names);

	free(new_val);
	free(val1);
	free(val2);
	if (stack != NULL) {
		while ((val1 = stack_pop(stack)) != NULL) {
			free(val1);
		}
		stack_destroy(&stack);
	}

	return rc;
}


static int constraints_to_cil(int indent, struct policydb *pdb, char *classkey, struct class_datum *class, struct constraint_node *constraints, int is_constraint)
{
	int rc = -1;
	struct constraint_node *node;
	char *expr = NULL;
	const char *mls;
	char *perms;

	mls = pdb->mls ? "mls" : "";

	for (node = constraints; node != NULL; node = node->next) {

		rc = constraint_expr_to_string(pdb, node->expr, &expr);
		if (rc != 0) {
			goto exit;
		}

		if (is_constraint) {
			perms = sepol_av_to_string(pdb, class->s.value, node->permissions);
			cil_println(indent, "(%sconstrain (%s (%s)) %s)", mls, classkey, perms + 1, expr);
		} else {
			cil_println(indent, "(%svalidatetrans %s %s)", mls, classkey, expr);
		}

		free(expr);
		expr = NULL;
	}

	rc = 0;

exit:
	free(expr);
	return rc;
}

static int class_to_cil(int indent, struct policydb *pdb, struct avrule_block *UNUSED(block), struct stack *UNUSED(decl_stack), char *key, void *datum, int scope)
{
	int rc = -1;
	struct class_datum *class = datum;
	const char *dflt;
	struct class_perm_array arr;
	uint32_t i;

	if (scope == SCOPE_REQ) {
		return 0;
	}

	arr.count = 0;
	arr.perms = calloc(class->permissions.nprim, sizeof(*arr.perms));
	if (arr.perms == NULL) {
		goto exit;
	}
	rc = ksu_hashtab_map(class->permissions.table, class_perm_to_array, &arr);
	if (rc != 0) {
		goto exit;
	}

	qsort(arr.perms, arr.count, sizeof(*arr.perms), class_perm_cmp);

	cil_indent(indent);
	cil_printf("(class %s (", key);
	for (i = 0; i < arr.count; i++) {
		cil_printf("%s ", arr.perms[i].name);
	}
	cil_printf("))\n");

	if (class->comkey != NULL) {
		cil_println(indent, "(classcommon %s %s)", key, class->comkey);
	}

	if (class->default_user != 0) {
		switch (class->default_user) {
		case DEFAULT_SOURCE:	dflt = "source";	break;
		case DEFAULT_TARGET:	dflt = "target";	break;
		default:
			log_err("Unknown default user value: %i", class->default_user);
			rc = -1;
			goto exit;
		}
		cil_println(indent, "(defaultuser %s %s)", key, dflt);
	}

	if (class->default_role != 0) {
		switch (class->default_role) {
		case DEFAULT_SOURCE:	dflt = "source";	break;
		case DEFAULT_TARGET:	dflt = "target";	break;
		default:
			log_err("Unknown default role value: %i", class->default_role);
			rc = -1;
			goto exit;
		}
		cil_println(indent, "(defaultrole %s %s)", key, dflt);
	}

	if (class->default_type != 0) {
		switch (class->default_type) {
		case DEFAULT_SOURCE:	dflt = "source";	break;
		case DEFAULT_TARGET:	dflt = "target";	break;
		default:
			log_err("Unknown default type value: %i", class->default_type);
			rc = -1;
			goto exit;
		}
		cil_println(indent, "(defaulttype %s %s)", key, dflt);
	}

	if (class->default_range != 0) {
		switch (class->default_range) {
		case DEFAULT_SOURCE_LOW:		dflt = "source low";	break;
		case DEFAULT_SOURCE_HIGH:		dflt = "source high";	break;
		case DEFAULT_SOURCE_LOW_HIGH:	dflt = "source low-high";	break;
		case DEFAULT_TARGET_LOW:		dflt = "target low";	break;
		case DEFAULT_TARGET_HIGH:		dflt = "target high";	break;
		case DEFAULT_TARGET_LOW_HIGH:	dflt = "target low-high";	break;
		case DEFAULT_GLBLUB:		dflt = "glblub";		break;
		default:
			log_err("Unknown default range value: %i", class->default_range);
			rc = -1;
			goto exit;
		}
		cil_println(indent, "(defaultrange %s %s)", key, dflt);

	}

	if (class->constraints != NULL) {
		rc = constraints_to_cil(indent, pdb, key, class, class->constraints, 1);
		if (rc != 0) {
			goto exit;
		}
	}

	if (class->validatetrans != NULL) {
		rc = constraints_to_cil(indent, pdb, key, class, class->validatetrans, 0);
		if (rc != 0) {
			goto exit;
		}
	}

	rc = 0;

exit:
	free(arr.perms);
	return rc;
}

static int class_order_to_cil(int indent, struct policydb *pdb, struct ebitmap order)
{
	struct ebitmap_node *node;
	uint32_t i;

	if (ebitmap_is_empty(&order)) {
		return 0;
	}

	cil_indent(indent);
	cil_printf("(classorder (");

	ebitmap_for_each_positive_bit(&order, node, i) {
		cil_printf("%s ", pdb->sym_val_to_name[SYM_CLASSES][i]);
	}

	cil_printf("))\n");

	return 0;
}

static int role_to_cil(int indent, struct policydb *pdb, struct avrule_block *UNUSED(block), struct stack *decl_stack, char *key, void *datum, int scope)
{
	int rc = -1;
	struct ebitmap_node *node;
	uint32_t i;
	unsigned int j;
	char **types = NULL;
	unsigned int num_types = 0;
	struct role_datum *role = datum;
	struct type_set *ts;
	struct list *attr_list = NULL;

	rc = list_init(&attr_list);
	if (rc != 0) {
		goto exit;
	}

	if (scope == SCOPE_REQ) {
		// if a role/roleattr is in the REQ scope, then it could cause an
		// optional block to fail, even if it is never used. However in CIL,
		// symbols must be used in order to cause an optional block to fail. So
		// for symbols in the REQ scope, add them to a roleattribute as a way
		// to 'use' them in the optional without affecting the resulting policy.
		cil_println(indent, "(roleattributeset " GEN_REQUIRE_ATTR " %s)", key);
	}

	switch (role->flavor) {
	case ROLE_ROLE:
		if (scope == SCOPE_DECL) {
			// Only declare certain roles if we are reading a base module.
			// These roles are defined in the base module and sometimes in
			// other non-base modules. If we generated the roles regardless of
			// the policy type, it would result in duplicate declarations,
			// which isn't allowed in CIL. Patches have been made to refpolicy
			// to remove these duplicate role declarations, but we need to be
			// backwards compatible and support older policies. Since we know
			// these roles are always declared in base, only print them when we
			// see them in the base module. If the declarations appear in a
			// non-base module, ignore their declarations.
			//
			// Note that this is a hack, and if a policy author does not define
			// one of these roles in base, the declaration will not appear in
			// the resulting policy, likely resulting in a compilation error in
			// CIL.
			//
			// To make things more complicated, the auditadm_r and secadm_r
			// roles could actually be in either the base module or a non-base
			// module, or both. So we can't rely on this same behavior. So for
			// these roles, don't declare them here, even if they are in a base
			// or non-base module. Instead we will just declare them in the
			// base module elsewhere.
			int is_base_role = (!strcmp(key, "user_r") ||
			                    !strcmp(key, "staff_r") ||
			                    !strcmp(key, "sysadm_r") ||
			                    !strcmp(key, "system_r") ||
			                    !strcmp(key, "unconfined_r"));
			int is_builtin_role = (!strcmp(key, "auditadm_r") ||
			                       !strcmp(key, "secadm_r"));
			if ((is_base_role && pdb->policy_type == SEPOL_POLICY_BASE) ||
			    (!is_base_role && !is_builtin_role)) {
				cil_println(indent, "(role %s)", key);
			}
		}

		if (ebitmap_cardinality(&role->dominates) > 1) {
			log_err("Warning: role 'dominance' statement unsupported in CIL. Dropping from output.");
		}

		ts = &role->types;
		rc = process_typeset(pdb, ts, attr_list, &types, &num_types);
		if (rc != 0) {
			goto exit;
		}

		for (j = 0; j < num_types; j++) {
			if (is_id_in_scope(pdb, decl_stack, types[j], SYM_TYPES)) {
				cil_println(indent, "(roletype %s %s)", key, types[j]);
			}
		}

		if (role->bounds > 0) {
			cil_println(indent, "(rolebounds %s %s)", key, pdb->p_role_val_to_name[role->bounds - 1]);
		}
		break;

	case ROLE_ATTRIB:
		if (scope == SCOPE_DECL) {
			cil_println(indent, "(roleattribute %s)", key);
		}

		if (!ebitmap_is_empty(&role->roles)) {
			cil_indent(indent);
			cil_printf("(roleattributeset %s (", key);
			ebitmap_for_each_positive_bit(&role->roles, node, i) {
				cil_printf("%s ", pdb->p_role_val_to_name[i]);
			}
			cil_printf("))\n");
		}

		ts = &role->types;
		rc = process_typeset(pdb, ts, attr_list, &types, &num_types);
		if (rc != 0) {
			goto exit;
		}


		for (j = 0; j < num_types; j++) {
			if (is_id_in_scope(pdb, decl_stack, types[j], SYM_TYPES)) {
				cil_println(indent, "(roletype %s %s)", key, types[j]);
			}
		}

		break;

	default:
		log_err("Unknown role type: %i", role->flavor);
		rc = -1;
		goto exit;
	}

	rc = cil_print_attr_list(indent, pdb, attr_list);
	if (rc != 0) {
		goto exit;
	}

exit:
	attr_list_destroy(&attr_list);
	names_destroy(&types, &num_types);

	return rc;
}

static int type_to_cil(int indent, struct policydb *pdb, struct avrule_block *UNUSED(block), struct stack *decl_stack, char *key, void *datum, int scope)
{
	int rc = -1;
	struct type_datum *type = datum;

	if (scope == SCOPE_REQ) {
		// if a type/typeattr is in the REQ scope, then it could cause an
		// optional block to fail, even if it is never used. However in CIL,
		// symbols must be used in order to cause an optional block to fail. So
		// for symbols in the REQ scope, add them to a typeattribute as a way
		// to 'use' them in the optional without affecting the resulting policy.
		cil_println(indent, "(typeattributeset " GEN_REQUIRE_ATTR " %s)", key);
	}

	rc = roletype_role_in_ancestor_to_cil(pdb, decl_stack, key, indent);
	if (rc != 0) {
		goto exit;
	}

	switch(type->flavor) {
	case TYPE_TYPE:
		if (scope == SCOPE_DECL) {
			cil_println(indent, "(type %s)", key);
			// object_r is implicit in checkmodule, but not with CIL,
			// create it as part of base
			cil_println(indent, "(roletype " DEFAULT_OBJECT " %s)", key);
		}

		if (type->flags & TYPE_FLAGS_PERMISSIVE) {
			cil_println(indent, "(typepermissive %s)", key);
		}

		if (type->bounds > 0) {
			cil_println(indent, "(typebounds %s %s)", pdb->p_type_val_to_name[type->bounds - 1], key);
		}
		break;
	case TYPE_ATTRIB:
		if (scope == SCOPE_DECL) {
			cil_println(indent, "(typeattribute %s)", key);
		}

		if (type->flags & TYPE_FLAGS_EXPAND_ATTR) {
			cil_indent(indent);
			cil_printf("(expandtypeattribute (%s) ", key);
			if (type->flags & TYPE_FLAGS_EXPAND_ATTR_TRUE) {
				cil_printf("true");
			} else if (type->flags & TYPE_FLAGS_EXPAND_ATTR_FALSE) {
				cil_printf("false");
			}
			cil_printf(")\n");
		}

		if (!ebitmap_is_empty(&type->types)) {
			cil_indent(indent);
			cil_printf("(typeattributeset %s (", key);
			ebitmap_to_cil(pdb, &type->types, SYM_TYPES);
			cil_printf("))\n");
		}
		break;
	case TYPE_ALIAS:
		break;
	default:
		log_err("Unknown flavor (%i) of type %s", type->flavor, key);
		rc = -1;
		goto exit;
	}

	rc = 0;

exit:
	return rc;
}

static int user_to_cil(int indent, struct policydb *pdb, struct avrule_block *block, struct stack *UNUSED(decl_stack), char *key, void *datum,  int scope)
{
	struct user_datum *user = datum;
	struct ebitmap roles = user->roles.roles;
	struct mls_semantic_level level = user->dfltlevel;
	struct mls_semantic_range range = user->range;
	struct ebitmap_node *node;
	uint32_t i;
	int sens_offset = 1;

	if (scope == SCOPE_DECL) {
		cil_println(indent, "(user %s)", key);
		// object_r is implicit in checkmodule, but not with CIL, create it
		// as part of base
		cil_println(indent, "(userrole %s " DEFAULT_OBJECT ")", key);
	}

	ebitmap_for_each_positive_bit(&roles, node, i) {
		cil_println(indent, "(userrole %s %s)", key, pdb->p_role_val_to_name[i]);
	}

	if (block->flags & AVRULE_OPTIONAL) {
		// sensitivites in user statements in optionals do not have the
		// standard -1 offset
		sens_offset = 0;
	}

	cil_indent(indent);
	cil_printf("(userlevel %s ", key);
	if (pdb->mls) {
		semantic_level_to_cil(pdb, sens_offset, &level);
	} else {
		cil_printf(DEFAULT_LEVEL);
	}
	cil_printf(")\n");

	cil_indent(indent);
	cil_printf("(userrange %s (", key);
	if (pdb->mls) {
		semantic_level_to_cil(pdb, sens_offset, &range.level[0]);
		cil_printf(" ");
		semantic_level_to_cil(pdb, sens_offset, &range.level[1]);
	} else {
		cil_printf(DEFAULT_LEVEL " " DEFAULT_LEVEL);
	}
	cil_printf("))\n");


	return 0;
}

static int boolean_to_cil(int indent, struct policydb *UNUSED(pdb), struct avrule_block *UNUSED(block), struct stack *UNUSED(decl_stack), char *key, void *datum,  int scope)
{
	struct cond_bool_datum *boolean = datum;
	const char *type;

	if (scope == SCOPE_DECL) {
		if (boolean->flags & COND_BOOL_FLAGS_TUNABLE) {
			type = "tunable";
		} else {
			type = "boolean";
		}

		cil_println(indent, "(%s %s %s)", type, key, boolean->state ? "true" : "false");
	}

	return 0;
}

static int sens_to_cil(int indent, struct policydb *pdb, struct avrule_block *UNUSED(block), struct stack *UNUSED(decl_stack), char *key, void *datum, int scope)
{
	struct level_datum *level = datum;

	if (scope == SCOPE_DECL) {
		if (!level->isalias) {
			cil_println(indent, "(sensitivity %s)", key);
		} else {
			cil_println(indent, "(sensitivityalias %s)", key);
			cil_println(indent, "(sensitivityaliasactual %s %s)", key, pdb->p_sens_val_to_name[level->level->sens - 1]);
		}
	}

	if (!ebitmap_is_empty(&level->level->cat)) {
		cil_indent(indent);
		cil_printf("(sensitivitycategory %s (", key);
		ebitmap_to_cil(pdb, &level->level->cat, SYM_CATS);
		cil_printf("))\n");
	}

	return 0;
}

static int sens_order_to_cil(int indent, struct policydb *pdb, struct ebitmap order)
{
	struct ebitmap_node *node;
	uint32_t i;

	if (ebitmap_is_empty(&order)) {
		return 0;
	}

	cil_indent(indent);
	cil_printf("(sensitivityorder (");

	ebitmap_for_each_positive_bit(&order, node, i) {
		cil_printf("%s ", pdb->p_sens_val_to_name[i]);
	}

	cil_printf("))\n");

	return 0;
}

static int cat_to_cil(int indent, struct policydb *pdb, struct avrule_block *UNUSED(block), struct stack *UNUSED(decl_stack), char *key, void *datum,  int scope)
{
	struct cat_datum *cat = datum;

	if (scope == SCOPE_REQ) {
		return 0;
	}

	if (!cat->isalias) {
		cil_println(indent, "(category %s)", key);
	} else {
		cil_println(indent, "(categoryalias %s)", key);
		cil_println(indent, "(categoryaliasactual %s %s)", key, pdb->p_cat_val_to_name[cat->s.value - 1]);
	}

	return 0;
}

static int cat_order_to_cil(int indent, struct policydb *pdb, struct ebitmap order)
{
	int rc = -1;
	struct ebitmap_node *node;
	uint32_t i;

	if (ebitmap_is_empty(&order)) {
		rc = 0;
		goto exit;
	}

	cil_indent(indent);
	cil_printf("(categoryorder (");

	ebitmap_for_each_positive_bit(&order, node, i) {
		cil_printf("%s ", pdb->p_cat_val_to_name[i]);
	}

	cil_printf("))\n");

	return 0;
exit:
	return rc;
}

static int polcaps_to_cil(struct policydb *pdb)
{
	int rc = -1;
	struct ebitmap *map;
	struct ebitmap_node *node;
	uint32_t i;
	const char *name;

	map = &pdb->policycaps;

	ebitmap_for_each_positive_bit(map, node, i) {
		name = sepol_polcap_getname(i);
		if (name == NULL) {
			log_err("Unknown policy capability id: %i", i);
			rc = -1;
			goto exit;
		}

		cil_println(0, "(policycap %s)", name);
	}

	return 0;
exit:
	return rc;
}

static int level_to_cil(struct policydb *pdb, struct mls_level *level)
{
	struct ebitmap *map = &level->cat;

	cil_printf("(%s", pdb->p_sens_val_to_name[level->sens - 1]);

	if (!ebitmap_is_empty(map)) {
		cil_printf("(");
		ebitmap_to_cil(pdb, map, SYM_CATS);
		cil_printf(")");
	}

	cil_printf(")");

	return 0;
}

static int context_to_cil(struct policydb *pdb, struct context_struct *con)
{
	cil_printf("(%s %s %s (",
		pdb->p_user_val_to_name[con->user - 1],
		pdb->p_role_val_to_name[con->role - 1],
		pdb->p_type_val_to_name[con->type - 1]);

	if (pdb->mls) {
		level_to_cil(pdb, &con->range.level[0]);
		cil_printf(" ");
		level_to_cil(pdb, &con->range.level[1]);
	} else {
		cil_printf(DEFAULT_LEVEL);
		cil_printf(" ");
		cil_printf(DEFAULT_LEVEL);
	}

	cil_printf("))");

	return 0;
}

static int ocontext_isid_to_cil(struct policydb *pdb, const char *const *sid_to_string,
				unsigned num_sids, struct ocontext *isids)
{
	int rc = -1;

	struct ocontext *isid;

	struct sid_item {
		char *sid_key;
		struct sid_item *next;
	};

	struct sid_item *head = NULL;
	struct sid_item *item = NULL;
	char *sid;
	char unknown[18];
	unsigned i;

	for (isid = isids; isid != NULL; isid = isid->next) {
		i = isid->sid[0];
		if (i < num_sids) {
			sid = (char*)sid_to_string[i];
		} else {
			snprintf(unknown, 18, "%s%u", "UNKNOWN", i);
			sid = unknown;
		}
		cil_println(0, "(sid %s)", sid);
		cil_printf("(sidcontext %s ", sid);
		context_to_cil(pdb, &isid->context[0]);
		cil_printf(")\n");

		// get the sid names in the correct order (reverse from the isids
		// ocontext) for sidorder statement
		item = malloc(sizeof(*item));
		if (item == NULL) {
			log_err("Out of memory");
			rc = -1;
			goto exit;
		}
		item->sid_key = strdup(sid);
		if (!item->sid_key) {
			log_err("Out of memory");
			rc = -1;
			goto exit;
		}
		item->next = head;
		head = item;
	}

	if (head != NULL) {
		cil_printf("(sidorder (");
		for (item = head; item != NULL; item = item->next) {
			cil_printf("%s ", item->sid_key);
		}
		cil_printf("))\n");
	}

	rc = 0;

exit:
	while(head) {
		item = head;
		head = item->next;
		free(item->sid_key);
		free(item);
	}
	return rc;
}

static int ocontext_selinux_isid_to_cil(struct policydb *pdb, struct ocontext *isids)
{
	int rc = -1;

	rc = ocontext_isid_to_cil(pdb, selinux_sid_to_str, SELINUX_SID_SZ, isids);
	if (rc != 0) {
		goto exit;
	}

	return 0;

exit:
	return rc;
}

static int ocontext_selinux_fs_to_cil(struct policydb *UNUSED(pdb), struct ocontext *fss)
{
	if (fss != NULL) {
		log_err("Warning: 'fscon' statement unsupported in CIL. Dropping from output.");
	}

	return 0;
}

static int ocontext_selinux_port_to_cil(struct policydb *pdb, struct ocontext *portcons)
{
	int rc = -1;
	struct ocontext *portcon;
	const char *protocol;
	uint16_t high;
	uint16_t low;

	for (portcon = portcons; portcon != NULL; portcon = portcon->next) {

		switch (portcon->u.port.protocol) {
		case IPPROTO_TCP: protocol = "tcp"; break;
		case IPPROTO_UDP: protocol = "udp"; break;
		case IPPROTO_DCCP: protocol = "dccp"; break;
		case IPPROTO_SCTP: protocol = "sctp"; break;
		default:
			log_err("Unknown portcon protocol: %i", portcon->u.port.protocol);
			rc = -1;
			goto exit;
		}

		low = portcon->u.port.low_port;
		high = portcon->u.port.high_port;

		if (low == high) {
			cil_printf("(portcon %s %i ", protocol, low);
		} else {
			cil_printf("(portcon %s (%i %i) ", protocol, low, high);
		}

		context_to_cil(pdb, &portcon->context[0]);

		cil_printf(")\n");
	}

	return 0;
exit:
	return rc;
}

static int ocontext_selinux_ibpkey_to_cil(struct policydb *pdb,
					struct ocontext *ibpkeycons)
{
	int rc = -1;
	struct ocontext *ibpkeycon;
	char subnet_prefix_str[INET6_ADDRSTRLEN];
	struct in6_addr subnet_prefix = IN6ADDR_ANY_INIT;
	uint16_t high;
	uint16_t low;

	for (ibpkeycon = ibpkeycons; ibpkeycon; ibpkeycon = ibpkeycon->next) {
		low = ibpkeycon->u.ibpkey.low_pkey;
		high = ibpkeycon->u.ibpkey.high_pkey;
		memcpy(&subnet_prefix.s6_addr, &ibpkeycon->u.ibpkey.subnet_prefix,
		       sizeof(ibpkeycon->u.ibpkey.subnet_prefix));

		if (inet_ntop(AF_INET6, &subnet_prefix.s6_addr,
			      subnet_prefix_str, INET6_ADDRSTRLEN) == NULL) {
			log_err("ibpkeycon subnet_prefix is invalid: %m");
			rc = -1;
			goto exit;
		}

		if (low == high)
			cil_printf("(ibpkeycon %s %i ", subnet_prefix_str, low);
		else
			cil_printf("(ibpkeycon %s (%i %i) ", subnet_prefix_str, low,
				   high);

		context_to_cil(pdb, &ibpkeycon->context[0]);

		cil_printf(")\n");
	}
	return 0;
exit:
	return rc;
}

static int ocontext_selinux_netif_to_cil(struct policydb *pdb, struct ocontext *netifs)
{
	struct ocontext *netif;

	for (netif = netifs; netif != NULL; netif = netif->next) {
		cil_printf("(netifcon %s ", netif->u.name);
		context_to_cil(pdb, &netif->context[0]);

		cil_printf(" ");
		context_to_cil(pdb, &netif->context[1]);
		cil_printf(")\n");
	}

	return 0;
}

static int ocontext_selinux_node_to_cil(struct policydb *pdb, struct ocontext *nodes)
{
	int rc = -1;
	struct ocontext *node;
	char addr[INET_ADDRSTRLEN];
	char mask[INET_ADDRSTRLEN];

	for (node = nodes; node != NULL; node = node->next) {
		if (inet_ntop(AF_INET, &node->u.node.addr, addr, INET_ADDRSTRLEN) == NULL) {
			log_err("Nodecon address is invalid: %m");
			rc = -1;
			goto exit;
		}

		if (inet_ntop(AF_INET, &node->u.node.mask, mask, INET_ADDRSTRLEN) == NULL) {
			log_err("Nodecon mask is invalid: %m");
			rc = -1;
			goto exit;
		}

		cil_printf("(nodecon (%s) (%s) ", addr, mask);

		context_to_cil(pdb, &node->context[0]);

		cil_printf(")\n");
	}

	return 0;
exit:
	return rc;
}

static int ocontext_selinux_node6_to_cil(struct policydb *pdb, struct ocontext *nodes)
{
	int rc = -1;
	struct ocontext *node;
	char addr[INET6_ADDRSTRLEN];
	char mask[INET6_ADDRSTRLEN];

	for (node = nodes; node != NULL; node = node->next) {
		if (inet_ntop(AF_INET6, &node->u.node6.addr, addr, INET6_ADDRSTRLEN) == NULL) {
			log_err("Nodecon address is invalid: %m");
			rc = -1;
			goto exit;
		}

		if (inet_ntop(AF_INET6, &node->u.node6.mask, mask, INET6_ADDRSTRLEN) == NULL) {
			log_err("Nodecon mask is invalid: %m");
			rc = -1;
			goto exit;
		}

		cil_printf("(nodecon (%s) (%s) ", addr, mask);

		context_to_cil(pdb, &node->context[0]);

		cil_printf(")\n");
	}

	return 0;
exit:
	return rc;
}

static int ocontext_selinux_ibendport_to_cil(struct policydb *pdb, struct ocontext *ibendports)
{
	struct ocontext *ibendport;

	for (ibendport = ibendports; ibendport; ibendport = ibendport->next) {
		cil_printf("(ibendportcon %s %u ", ibendport->u.ibendport.dev_name, ibendport->u.ibendport.port);
		context_to_cil(pdb, &ibendport->context[0]);

		cil_printf(")\n");
	}

	return 0;
}

static int ocontext_selinux_fsuse_to_cil(struct policydb *pdb, struct ocontext *fsuses)
{
	int rc = -1;
	struct ocontext *fsuse;
	const char *behavior;


	for (fsuse = fsuses; fsuse != NULL; fsuse = fsuse->next) {
		switch (fsuse->v.behavior) {
		case SECURITY_FS_USE_XATTR: behavior = "xattr"; break;
		case SECURITY_FS_USE_TRANS: behavior = "trans"; break;
		case SECURITY_FS_USE_TASK:  behavior = "task"; break;
		default:
			log_err("Unknown fsuse behavior: %i", fsuse->v.behavior);
			rc = -1;
			goto exit;
		}

		cil_printf("(fsuse %s %s ", behavior, fsuse->u.name);

		context_to_cil(pdb, &fsuse->context[0]);

		cil_printf(")\n");

	}

	return 0;
exit:
	return rc;
}


static int ocontext_xen_isid_to_cil(struct policydb *pdb, struct ocontext *isids)
{
	int rc = -1;

	rc = ocontext_isid_to_cil(pdb, xen_sid_to_str, XEN_SID_SZ, isids);
	if (rc != 0) {
		goto exit;
	}

	return 0;

exit:
	return rc;
}

static int ocontext_xen_pirq_to_cil(struct policydb *pdb, struct ocontext *pirqs)
{
	struct ocontext *pirq;

	for (pirq = pirqs; pirq != NULL; pirq = pirq->next) {
		cil_printf("(pirqcon %i ", pirq->u.pirq);
		context_to_cil(pdb, &pirq->context[0]);
		cil_printf(")\n");
	}

	return 0;
}

static int ocontext_xen_ioport_to_cil(struct policydb *pdb, struct ocontext *ioports)
{
	struct ocontext *ioport;
	uint32_t low;
	uint32_t high;

	for (ioport = ioports; ioport != NULL; ioport = ioport->next) {
		low = ioport->u.ioport.low_ioport;
		high = ioport->u.ioport.high_ioport;

		if (low == high) {
			cil_printf("(ioportcon 0x%x ", low);
		} else {
			cil_printf("(ioportcon (0x%x 0x%x) ", low, high);
		}

		context_to_cil(pdb, &ioport->context[0]);

		cil_printf(")\n");
	}

	return 0;
}

static int ocontext_xen_iomem_to_cil(struct policydb *pdb, struct ocontext *iomems)
{
	struct ocontext *iomem;
	uint64_t low;
	uint64_t high;

	for (iomem = iomems; iomem != NULL; iomem = iomem->next) {
		low = iomem->u.iomem.low_iomem;
		high = iomem->u.iomem.high_iomem;

		if (low == high) {
			cil_printf("(iomemcon 0x%"PRIx64" ", low);
		} else {
			cil_printf("(iomemcon (0x%"PRIx64" 0x%"PRIx64") ", low, high);
		}

		context_to_cil(pdb, &iomem->context[0]);

		cil_printf(")\n");
	}

	return 0;
}

static int ocontext_xen_pcidevice_to_cil(struct policydb *pdb, struct ocontext *pcids)
{
	struct ocontext *pcid;

	for (pcid = pcids; pcid != NULL; pcid = pcid->next) {
		cil_printf("(pcidevicecon 0x%lx ", (unsigned long)pcid->u.device);
		context_to_cil(pdb, &pcid->context[0]);
		cil_printf(")\n");
	}

	return 0;
}

static int ocontexts_to_cil(struct policydb *pdb)
{
	int rc = -1;
	int ocon;

	static int (**ocon_funcs)(struct policydb *pdb, struct ocontext *ocon);
	static int (*ocon_selinux_funcs[OCON_NUM])(struct policydb *pdb, struct ocontext *ocon) = {
		ocontext_selinux_isid_to_cil,
		ocontext_selinux_fs_to_cil,
		ocontext_selinux_port_to_cil,
		ocontext_selinux_netif_to_cil,
		ocontext_selinux_node_to_cil,
		ocontext_selinux_fsuse_to_cil,
		ocontext_selinux_node6_to_cil,
		ocontext_selinux_ibpkey_to_cil,
		ocontext_selinux_ibendport_to_cil,
	};
	static int (*ocon_xen_funcs[OCON_NUM])(struct policydb *pdb, struct ocontext *ocon) = {
		ocontext_xen_isid_to_cil,
		ocontext_xen_pirq_to_cil,
		ocontext_xen_ioport_to_cil,
		ocontext_xen_iomem_to_cil,
		ocontext_xen_pcidevice_to_cil,
		NULL,
		NULL,
	};

	switch (pdb->target_platform) {
	case SEPOL_TARGET_SELINUX:
		ocon_funcs = ocon_selinux_funcs;
		break;
	case SEPOL_TARGET_XEN:
		ocon_funcs = ocon_xen_funcs;
		break;
	default:
		log_err("Unknown target platform: %i", pdb->target_platform);
		rc = -1;
		goto exit;
	}

	for (ocon = 0; ocon < OCON_NUM; ocon++) {
		if (ocon_funcs[ocon] != NULL) {
			rc = ocon_funcs[ocon](pdb, pdb->ocontexts[ocon]);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	return 0;
exit:
	return rc;
}

static int genfscon_to_cil(struct policydb *pdb)
{
	struct genfs *genfs;
	struct ocontext *ocon;
	uint32_t sclass;

	for (genfs = pdb->genfs; genfs != NULL; genfs = genfs->next) {
		for (ocon = genfs->head; ocon != NULL; ocon = ocon->next) {
			sclass = ocon->v.sclass;
			if (sclass) {
				const char *file_type;
				const char *class_name = pdb->p_class_val_to_name[sclass-1];
				if (strcmp(class_name, "file") == 0) {
					file_type = "file";
				} else if (strcmp(class_name, "dir") == 0) {
					file_type = "dir";
				} else if (strcmp(class_name, "chr_file") == 0) {
					file_type = "char";
				} else if (strcmp(class_name, "blk_file") == 0) {
					file_type = "block";
				} else if (strcmp(class_name, "sock_file") == 0) {
					file_type = "socket";
				} else if (strcmp(class_name, "fifo_file") == 0) {
					file_type = "pipe";
				} else if (strcmp(class_name, "lnk_file") == 0) {
					file_type = "symlink";
				} else {
					return -1;
				}
				cil_printf("(genfscon %s \"%s\" %s ", genfs->fstype, ocon->u.name, file_type);
			} else {
				cil_printf("(genfscon %s \"%s\" ", genfs->fstype, ocon->u.name);
			}
			context_to_cil(pdb, &ocon->context[0]);
			cil_printf(")\n");
		}
	}

	return 0;
}

static int level_string_to_cil(char *levelstr)
{
	int rc = -1;
	char *sens = NULL;
	char *cats = NULL;
	int matched;
	char *saveptr = NULL;
	char *token = NULL;
	char *ranged = NULL;

	matched = tokenize(levelstr, ':', 2, &sens, &cats);
	if (matched < 1 || matched > 2) {
		log_err("Invalid level: %s", levelstr);
		rc = -1;
		goto exit;
	}

	cil_printf("(%s", sens);

	if (matched == 2) {
		cil_printf("(");
		token = strtok_r(cats, ",", &saveptr);
		while (token != NULL) {
			ranged = strchr(token, '.');
			if (ranged == NULL) {
				cil_printf("%s ", token);
			} else {
				*ranged = '\0';
				cil_printf("(range %s %s) ", token, ranged + 1);
			}
			token = strtok_r(NULL, ",", &saveptr);
		}
		cil_printf(")");
	}

	cil_printf(")");

	rc = 0;
exit:
	free(sens);
	free(cats);
	return rc;
}

static int level_range_string_to_cil(char *levelrangestr)
{
	char *ranged = NULL;
	char *low;
	char *high;

	ranged = strchr(levelrangestr, '-');
	if (ranged == NULL) {
		low = high = levelrangestr;
	} else {
		*ranged = '\0';
		low = levelrangestr;
		high = ranged + 1;
	}

	level_string_to_cil(low);
	cil_printf(" ");
	level_string_to_cil(high);

	return 0;
}

static int context_string_to_cil(char *contextstr)
{
	int rc = -1;
	int matched;
	char *user = NULL;
	char *role = NULL;
	char *type = NULL;
	char *level = NULL;

	matched = tokenize(contextstr, ':', 4, &user, &role, &type, &level);
	if (matched < 3 || matched > 4) {
		log_err("Invalid context: %s", contextstr);
		rc = -1;
		goto exit;
	}

	cil_printf("(%s %s %s (", user, role, type);

	if (matched == 3) {
		cil_printf(DEFAULT_LEVEL);
		cil_printf(" ");
		cil_printf(DEFAULT_LEVEL);
	} else {
		level_range_string_to_cil(level);
	}

	cil_printf("))");

	rc = 0;

exit:
	free(user);
	free(role);
	free(type);
	free(level);

	return rc;
}

static int seusers_to_cil(struct sepol_module_package *mod_pkg)
{
	int rc = -1;
	char *seusers = sepol_module_package_get_seusers(mod_pkg);
	size_t seusers_len = sepol_module_package_get_seusers_len(mod_pkg);
	char *cur = seusers;
	char *end = seusers + seusers_len;
	char *line = NULL;
	char *user = NULL;
	char *seuser = NULL;
	char *level = NULL;
	char *tmp = NULL;
	int matched;

	if (seusers_len == 0) {
		return 0;
	}

	while ((rc = get_line(&cur, end, &line)) > 0) {
		tmp = line;
		while (isspace(*tmp)) {
			tmp++;
		}

		if (tmp[0] == '#' || tmp[0] == '\0') {
			free(line);
			line = NULL;
			continue;
		}

		matched = tokenize(tmp, ':', 3, &user, &seuser, &level);

		if (matched < 2 || matched > 3) {
			log_err("Invalid seuser line: %s", line);
			rc = -1;
			goto exit;
		}

		if (!strcmp(user, "__default__")) {
			cil_printf("(selinuxuserdefault %s (", seuser);
		} else {
			cil_printf("(selinuxuser %s %s (", user, seuser);
		}

		switch (matched) {
		case 2:
			cil_printf("systemlow systemlow");
			break;
		case 3:
			level_range_string_to_cil(level);
			break;
		}

		cil_printf("))\n");

		free(user);
		free(seuser);
		free(level);
		free(line);
		user = seuser = level = NULL;
	}

	if (rc == -1) {
		cil_printf("Failed to read seusers\n");
		goto exit;
	}

	rc = 0;
exit:
	free(line);
	free(user);
	free(seuser);
	free(level);

	return rc;
}

static int netfilter_contexts_to_cil(struct sepol_module_package *mod_pkg)
{
	size_t netcons_len = sepol_module_package_get_netfilter_contexts_len(mod_pkg);

	if (netcons_len > 0) {
		log_err("Warning: netfilter_contexts are unsupported in CIL. Dropping from output.");
	}

	return 0;
}

static int user_extra_to_cil(struct sepol_module_package *mod_pkg)
{
	int rc = -1;
	char *userx = sepol_module_package_get_user_extra(mod_pkg);
	size_t userx_len = sepol_module_package_get_user_extra_len(mod_pkg);
	char *cur = userx;
	char *end = userx + userx_len;
	char *line;
	int matched;
	char *user = NULL;
	char *prefix = NULL;
	int prefix_len = 0;
	char *user_str = NULL;
	char *prefix_str = NULL;
	char *eol = NULL;
	char *tmp = NULL;

	if (userx_len == 0) {
		return 0;
	}

	while ((rc = get_line(&cur, end, &line)) > 0) {
		tmp = line;
		while (isspace(*tmp)) {
			tmp++;
		}

		if (tmp[0] == '#' || tmp[0] == '\0') {
			free(line);
			line = NULL;
			continue;
		}

		matched = tokenize(tmp, ' ', 4, &user_str, &user, &prefix_str, &prefix);
		if (matched != 4) {
			rc = -1;
			log_err("Invalid user extra line: %s", line);
			goto exit;
		}

		prefix_len = strlen(prefix);
		eol = prefix + prefix_len - 1;
		if (*eol != ';' || strcmp(user_str, "user") || strcmp(prefix_str, "prefix")) {
			rc = -1;
			log_err("Invalid user extra line: %s", line);
			goto exit;
		}
		*eol = '\0';

		cil_println(0, "(userprefix %s %s)", user, prefix);
		free(user);
		free(prefix);
		free(line);
		free(user_str);
		free(prefix_str);
		user = prefix = line = user_str = prefix_str = NULL;
	}

	if (rc == -1) {
		cil_printf("Failed to read user_extra\n");
		goto exit;
	}

	rc = 0;
exit:
	free(line);
	free(user);
	free(prefix);

	return rc;
}

static int file_contexts_to_cil(struct sepol_module_package *mod_pkg)
{
	int rc = -1;
	char *fc = sepol_module_package_get_file_contexts(mod_pkg);
	size_t fc_len = sepol_module_package_get_file_contexts_len(mod_pkg);
	char *cur = fc;
	char *end = fc + fc_len;
	char *line = NULL;
	int matched;
	char *regex = NULL;
	char *mode = NULL;
	char *context = NULL;
	const char *cilmode;
	char *tmp = NULL;

	if (fc_len == 0) {
		return 0;
	}

	while ((rc = get_line(&cur, end, &line)) > 0) {
		tmp = line;
		while (isspace(*tmp)) {
			tmp++;
		}

		if (tmp[0] == '#' || tmp[0] == '\0') {
			free(line);
			line = NULL;
			continue;
		}

		matched = tokenize(tmp, ' ', 3, &regex, &mode, &context);
		if (matched < 2 || matched > 3) {
			rc = -1;
			log_err("Invalid file context line: %s", line);
			goto exit;
		}

		if (matched == 2) {
			context = mode;
			mode = NULL;
		}

		if (mode == NULL) {
			cilmode = "any";
		} else if (!strcmp(mode, "--")) {
			cilmode = "file";
		} else if (!strcmp(mode, "-d")) {
			cilmode = "dir";
		} else if (!strcmp(mode, "-c")) {
			cilmode = "char";
		} else if (!strcmp(mode, "-b")) {
			cilmode = "block";
		} else if (!strcmp(mode, "-s")) {
			cilmode = "socket";
		} else if (!strcmp(mode, "-p")) {
			cilmode = "pipe";
		} else if (!strcmp(mode, "-l")) {
			cilmode = "symlink";
		} else {
			rc = -1;
			log_err("Invalid mode in file context line: %s", line);
			goto exit;
		}

		cil_printf("(filecon \"%s\" %s ", regex, cilmode);

		if (!strcmp(context, "<<none>>")) {
			cil_printf("()");
		} else {
			context_string_to_cil(context);
		}

		cil_printf(")\n");

		free(regex);
		free(mode);
		free(context);
		free(line);
		regex = mode = context = line = NULL;
	}

	if (rc == -1) {
		cil_printf("Failed to read file_contexts_to_cil\n");
		goto exit;
	}

	rc = 0;
exit:
	free(line);
	free(regex);
	free(mode);
	free(context);

	return rc;
}


static int (*func_to_cil[SYM_NUM])(int indent, struct policydb *pdb, struct avrule_block *block, struct stack *decl_stack, char *key, void *datum, int scope) = {
	NULL,	// commons, only stored in the global symtab, handled elsewhere
	class_to_cil,
	role_to_cil,
	type_to_cil,
	user_to_cil,
	boolean_to_cil,
	sens_to_cil,
	cat_to_cil
};

static int typealiases_to_cil(int indent, struct policydb *pdb, struct avrule_block *UNUSED(block), struct stack *decl_stack)
{
	struct type_datum *alias_datum;
	char *alias_name;
	char *type_name;
	struct list_node *curr;
	struct avrule_decl *decl = stack_peek(decl_stack);
	struct list *alias_list;
	int rc = -1;

	if (decl == NULL) {
		return -1;
	}

	alias_list = typealias_lists[decl->decl_id];
	if (alias_list == NULL) {
		return 0;
	}

	for (curr = alias_list->head; curr != NULL; curr = curr->next) {
		alias_name = curr->data;
		alias_datum = hashtab_search(pdb->p_types.table, alias_name);
		if (alias_datum == NULL) {
			rc = -1;
			goto exit;
		}
		if (alias_datum->flavor == TYPE_ALIAS) {
			type_name = pdb->p_type_val_to_name[alias_datum->primary - 1];
		} else {
			type_name = pdb->p_type_val_to_name[alias_datum->s.value - 1];
		}
		cil_println(indent, "(typealias %s)", alias_name);
		cil_println(indent, "(typealiasactual %s %s)", alias_name, type_name);
	}

	return 0;

exit:
	return rc;
}

static int declared_scopes_to_cil(int indent, struct policydb *pdb, struct avrule_block *block, struct stack *decl_stack)
{
	int rc = -1;
	struct ebitmap map;
	struct ebitmap_node *node;
	unsigned int i;
	char * key;
	struct scope_datum *scope;
	int sym;
	void *datum;
	struct avrule_decl *decl = stack_peek(decl_stack);

	for (sym = 0; sym < SYM_NUM; sym++) {
		if (func_to_cil[sym] == NULL) {
			continue;
		}

		map = decl->declared.scope[sym];
		ebitmap_for_each_positive_bit(&map, node, i) {
			key = pdb->sym_val_to_name[sym][i];
			datum = hashtab_search(pdb->symtab[sym].table, key);
			if (datum == NULL) {
				rc = -1;
				goto exit;
			}
			scope = hashtab_search(pdb->scope[sym].table, key);
			if (scope == NULL) {
				rc = -1;
				goto exit;
			}
			rc = func_to_cil[sym](indent, pdb, block, decl_stack, key, datum, scope->scope);
			if (rc != 0) {
				goto exit;
			}
		}

		if (sym == SYM_CATS) {
			rc = cat_order_to_cil(indent, pdb, map);
			if (rc != 0) {
				goto exit;
			}
		}

		if (sym == SYM_LEVELS) {
			rc = sens_order_to_cil(indent, pdb, map);
			if (rc != 0) {
				goto exit;
			}
		}

		if (sym == SYM_CLASSES) {
			rc = class_order_to_cil(indent, pdb, map);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	return 0;
exit:
	return rc;
}

static int required_scopes_to_cil(int indent, struct policydb *pdb, struct avrule_block *block, struct stack *decl_stack)
{
	int rc = -1;
	struct ebitmap map;
	struct ebitmap_node *node;
	unsigned int i;
	unsigned int j;
	char * key;
	int sym;
	void *datum;
	struct avrule_decl *decl = stack_peek(decl_stack);
	struct scope_datum *scope_datum;

	for (sym = 0; sym < SYM_NUM; sym++) {
		if (func_to_cil[sym] == NULL) {
			continue;
		}

		map = decl->required.scope[sym];
		ebitmap_for_each_positive_bit(&map, node, i) {
			key = pdb->sym_val_to_name[sym][i];

			scope_datum = hashtab_search(pdb->scope[sym].table, key);
			if (scope_datum == NULL) {
				rc = -1;
				goto exit;
			}
			for (j = 0; j < scope_datum->decl_ids_len; j++) {
				if (scope_datum->decl_ids[j] == decl->decl_id) {
					break;
				}
			}
			if (j >= scope_datum->decl_ids_len) {
				// Symbols required in the global scope are also in the
				// required scope ebitmap of all avrule decls (i.e. required
				// in all optionals). So we need to look at the scopes of each
				// symbol in this avrule_decl to determine if it actually is
				// required in this decl, or if it's just required in the
				// global scope. If we got here, then this symbol is not
				// actually required in this scope, so skip it.
				continue;
			}

			datum = hashtab_search(pdb->symtab[sym].table, key);
			if (datum == NULL) {
				rc = -1;
				goto exit;
			}
			rc = func_to_cil[sym](indent, pdb, block, decl_stack, key, datum, SCOPE_REQ);
			if (rc != 0) {
				goto exit;
			}
		}
	}

	return 0;
exit:
	return rc;
}


static int additive_scopes_to_cil_map(char *key, void *data, void *arg)
{
	int rc = -1;
	struct map_args *args = arg;

	rc = func_to_cil[args->sym_index](args->indent, args->pdb, args->block, args->decl_stack, key, data, SCOPE_REQ);
	if (rc != 0) {
		goto exit;
	}

	return 0;

exit:
	return rc;
}

static int additive_scopes_to_cil(int indent, struct policydb *pdb, struct avrule_block *block, struct stack *decl_stack)
{
	int rc = -1;
	struct avrule_decl *decl = stack_peek(decl_stack);
	struct map_args args;
	args.pdb = pdb;
	args.block = block;
	args.decl_stack = decl_stack;
	args.indent = indent;

	for (args.sym_index = 0; args.sym_index < SYM_NUM; args.sym_index++) {
		if (func_to_cil[args.sym_index] == NULL) {
			continue;
		}
		rc = ksu_hashtab_map(decl->symtab[args.sym_index].table, additive_scopes_to_cil_map, &args);
		if (rc != 0) {
			goto exit;
		}
	}

	return 0;

exit:
	return rc;
}

static int is_scope_superset(struct scope_index *sup, struct scope_index *sub)
{
	// returns 1 if sup is a superset of sub, returns 0 otherwise

	int rc = 0;

	uint32_t i;
	struct ebitmap sup_map;
	struct ebitmap sub_map;
	struct ebitmap res;

	ebitmap_init(&res);

	for (i = 0; i < SYM_NUM; i++) {
		sup_map = sup->scope[i];
		sub_map = sub->scope[i];

		ksu_ebitmap_and(&res, &sup_map, &sub_map);
		if (!ksu_ebitmap_cmp(&res, &sub_map)) {
			goto exit;
		}
		ksu_ebitmap_destroy(&res);
	}

	if (sup->class_perms_len < sub->class_perms_len) {
		goto exit;
	}

	for (i = 0; i < sub->class_perms_len; i++) {
		sup_map = sup->class_perms_map[i];
		sub_map = sub->class_perms_map[i];

		ksu_ebitmap_and(&res, &sup_map, &sub_map);
		if (!ksu_ebitmap_cmp(&res, &sub_map)) {
			goto exit;
		}
		ksu_ebitmap_destroy(&res);
	}

	rc = 1;

exit:

	ksu_ebitmap_destroy(&res);
	return rc;
}

static int block_to_cil(struct policydb *pdb, struct avrule_block *block, struct stack *stack, int indent)
{
	int rc = -1;
	struct avrule_decl *decl;
	struct list *type_attr_list = NULL;
	struct list *role_attr_list = NULL;

	decl = block->branch_list;

	rc = list_init(&type_attr_list);
	if (rc != 0) {
		goto exit;
	}
	rc = list_init(&role_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = typealiases_to_cil(indent, pdb, block, stack);
	if (rc != 0) {
		goto exit;
	}

	rc = declared_scopes_to_cil(indent, pdb, block, stack);
	if (rc != 0) {
		goto exit;
	}

	rc = required_scopes_to_cil(indent, pdb, block, stack);
	if (rc != 0) {
		goto exit;
	}

	rc = additive_scopes_to_cil(indent, pdb, block, stack);
	if (rc != 0) {
		goto exit;
	}

	rc = avrule_list_to_cil(indent, pdb, decl->avrules, type_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = role_trans_to_cil(indent, pdb, decl->role_tr_rules, role_attr_list, type_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = role_allows_to_cil(indent, pdb, decl->role_allow_rules, role_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = range_trans_to_cil(indent, pdb, decl->range_tr_rules, type_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = filename_trans_to_cil(indent, pdb, decl->filename_trans_rules, type_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = cond_list_to_cil(indent, pdb, decl->cond_list, type_attr_list);
	if (rc != 0) {
		goto exit;
	}

	rc = cil_print_attr_list(indent, pdb, type_attr_list);
	if (rc != 0) {
		goto exit;
	}
	rc = cil_print_attr_list(indent, pdb, role_attr_list);
	if (rc != 0) {
		goto exit;
	}

exit:
	attr_list_destroy(&type_attr_list);
	attr_list_destroy(&role_attr_list);

	return rc;
}

static int module_block_to_cil(struct policydb *pdb, struct avrule_block *block, struct stack *stack, int *indent)
{
	int rc = 0;
	struct avrule_decl *decl;
	struct avrule_decl *decl_tmp;

	decl = block->branch_list;
	if (decl == NULL) {
		goto exit;
	}

	if (decl->next != NULL) {
		log_err("Warning: 'else' blocks in optional statements are unsupported in CIL. Dropping from output.");
	}

	if (block->flags & AVRULE_OPTIONAL) {
		while (stack->pos > 0) {
			decl_tmp = stack_peek(stack);
			if (is_scope_superset(&decl->required, &decl_tmp->required)) {
				break;
			}

			stack_pop(stack);
			(*indent)--;
			cil_println(*indent, ")");
		}

		cil_println(*indent, "(optional %s_optional_%i", pdb->name, decl->decl_id);
		(*indent)++;
	}

	stack_push(stack, decl);

	rc = block_to_cil(pdb, block, stack, *indent);
	if (rc != 0) {
		goto exit;
	}

exit:
	return rc;
}

static int global_block_to_cil(struct policydb *pdb, struct avrule_block *block, struct stack *stack)
{
	int rc = 0;
	struct avrule_decl *decl;

	decl = block->branch_list;
	if (decl == NULL) {
		goto exit;
	}

	if (decl->next != NULL) {
		log_err("Warning: 'else' not allowed in global block. Dropping from output.");
	}

	stack_push(stack, decl);

	// type aliases and commons are only stored in the global symtab.
	// However, to get scoping correct, we assume they are in the
	// global block
	rc = ksu_hashtab_map(pdb->p_commons.table, common_to_cil, NULL);
	if (rc != 0) {
		goto exit;
	}

	rc = block_to_cil(pdb, block, stack, 0);
	if (rc != 0) {
		goto exit;
	}

exit:
	return rc;
}

static int blocks_to_cil(struct policydb *pdb)
{
	int rc = -1;
	struct avrule_block *block;
	int indent = 0;
	struct stack *stack = NULL;

	rc = stack_init(&stack);
	if (rc != 0) {
		goto exit;
	}

	block = pdb->global;
	rc = global_block_to_cil(pdb, block, stack);
	if (rc != 0) {
		goto exit;
	}

	for (block = block->next; block != NULL; block = block->next) {
		rc = module_block_to_cil(pdb, block, stack, &indent);
		if (rc != 0) {
			goto exit;
		}
	}

	while (indent > 0) {
		indent--;
		cil_println(indent, ")");
	}

exit:
	stack_destroy(&stack);

	return rc;
}

static int linked_block_to_cil(struct policydb *pdb, struct avrule_block *block, struct stack *stack)
{
	int rc = 0;
	struct avrule_decl *decl;

	decl = block->branch_list;
	if (decl == NULL) {
		goto exit;
	}

	if (!decl->enabled) {
		if (decl->next != NULL) {
			decl = decl->next;
		} else {
			goto exit;
		}
	}

	stack_push(stack, decl);

	rc = block_to_cil(pdb, block, stack, 0);
	if (rc != 0) {
		goto exit;
	}

	stack_pop(stack);

exit:
	return rc;
}

static int linked_blocks_to_cil(struct policydb *pdb)
{
	// Convert base module that has been linked to CIL
	// Since it is linked, all optional blocks have been resolved
	int rc = -1;
	struct avrule_block *block;
	struct stack *stack = NULL;

	rc = stack_init(&stack);
	if (rc != 0) {
		goto exit;
	}

	block = pdb->global;
	rc = global_block_to_cil(pdb, block, stack);
	if (rc != 0) {
		goto exit;
	}

	for (block = block->next; block != NULL; block = block->next) {
		rc = linked_block_to_cil(pdb, block, stack);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	stack_destroy(&stack);

	return rc;
}

static int handle_unknown_to_cil(struct policydb *pdb)
{
	int rc = -1;
	const char *hu;

	switch (pdb->handle_unknown) {
	case SEPOL_DENY_UNKNOWN:
		hu = "deny";
		break;
	case SEPOL_REJECT_UNKNOWN:
		hu = "reject";
		break;
	case SEPOL_ALLOW_UNKNOWN:
		hu = "allow";
		break;
	default:
		log_err("Unknown value for handle-unknown: %i", pdb->handle_unknown);
		rc = -1;
		goto exit;
	}

	cil_println(0, "(handleunknown %s)", hu);

	return 0;

exit:
	return rc;
}

static int generate_mls(struct policydb *pdb)
{
	const char *mls_str = pdb->mls ? "true" : "false";
	cil_println(0, "(mls %s)", mls_str);

	return 0;
}

static int generate_default_level(void)
{
	cil_println(0, "(sensitivity s0)");
	cil_println(0, "(sensitivityorder (s0))");
	cil_println(0, "(level " DEFAULT_LEVEL " (s0))");

	return 0;
}

static int generate_default_object(void)
{
	cil_println(0, "(role " DEFAULT_OBJECT ")");

	return 0;
}

static int generate_builtin_roles(void)
{
	// due to inconsistentencies between policies and CIL not allowing
	// duplicate roles, some roles are always created, regardless of if they
	// are declared in modules or not
	cil_println(0, "(role auditadm_r)");
	cil_println(0, "(role secadm_r)");

	return 0;
}

static int generate_gen_require_attribute(void)
{
	cil_println(0, "(typeattribute " GEN_REQUIRE_ATTR ")");
	cil_println(0, "(roleattribute " GEN_REQUIRE_ATTR ")");

	return 0;
}

static int fix_module_name(struct policydb *pdb)
{
	char *letter;
	int rc = -1;

	// The base module doesn't have its name set, but we use that for some
	// autogenerated names, like optionals and attributes, to prevent naming
	// collisions. However, they sometimes need to be fixed up.

	// the base module isn't given a name, so just call it "base"
	if (pdb->policy_type == POLICY_BASE) {
		pdb->name = strdup("base");
		if (pdb->name == NULL) {
			log_err("Out of memory");
			rc = -1;
			goto exit;
		}
	}

	// CIL is more restrictive in module names than checkmodule. Convert bad
	// characters to underscores
	for (letter = pdb->name; *letter != '\0'; letter++) {
		if (isalnum(*letter)) {
			continue;
		}

		*letter = '_';
	}

	return 0;
exit:
	return rc;
}

int sepol_module_policydb_to_cil(FILE *fp, struct policydb *pdb, int linked)
{
	int rc = -1;

	out_file = fp;

	if (pdb == NULL) {
		rc = 0;
		goto exit;
	}

	if (pdb->policy_type != SEPOL_POLICY_BASE &&
		pdb->policy_type != SEPOL_POLICY_MOD) {
		log_err("Policy package is not a base or module");
		rc = -1;
		goto exit;
	}

	rc = fix_module_name(pdb);
	if (rc != 0) {
		goto exit;
	}

	if (pdb->policy_type == SEPOL_POLICY_BASE && !pdb->mls) {
		// If this is a base non-mls policy, we need to define a default level
		// range that can be used for contexts by other non-mls modules, since
		// CIL requires that all contexts have a range, even if they are
		// ignored as in non-mls policies
		rc = generate_default_level();
		if (rc != 0) {
			goto exit;
		}
	}

	if (pdb->policy_type == SEPOL_POLICY_BASE) {
		// object_r is implicit in checkmodule, but not with CIL, create it
		// as part of base
		rc = generate_default_object();
		if (rc != 0) {
			goto exit;
		}

		rc = generate_builtin_roles();
		if (rc != 0) {
			goto exit;
		}

		// default attribute to be used to mimic gen_require in CIL
		rc = generate_gen_require_attribute();
		if (rc != 0) {
			goto exit;
		}

		// handle_unknown is used from only the base module
		rc = handle_unknown_to_cil(pdb);
		if (rc != 0) {
			goto exit;
		}

		// mls is used from only the base module
		rc = generate_mls(pdb);
		if (rc != 0) {
			goto exit;
		}
	}

	rc = role_list_create(pdb->p_roles.table);
	if (rc != 0) {
		goto exit;
	}

	rc = typealias_list_create(pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = polcaps_to_cil(pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = ocontexts_to_cil(pdb);
	if (rc != 0) {
		goto exit;
	}

	rc = genfscon_to_cil(pdb);
	if (rc != 0) {
		goto exit;
	}

	// now print everything that is scoped
	if (linked) {
		rc = linked_blocks_to_cil(pdb);
	} else {
		rc = blocks_to_cil(pdb);
	}
	if (rc != 0) {
		goto exit;
	}

	rc = 0;

exit:
	role_list_destroy();
	typealias_list_destroy();

	return rc;
}

int sepol_module_package_to_cil(FILE *fp, struct sepol_module_package *mod_pkg)
{
	int rc = -1;
	struct sepol_policydb *pdb;

	out_file = fp;

	pdb = sepol_module_package_get_policy(mod_pkg);
	if (pdb == NULL) {
		log_err("Failed to get policydb");
		rc = -1;
		goto exit;
	}

	rc = sepol_module_policydb_to_cil(fp, &pdb->p, 0);
	if (rc != 0) {
		goto exit;
	}

	rc = seusers_to_cil(mod_pkg);
	if (rc != 0) {
		goto exit;
	}

	rc = netfilter_contexts_to_cil(mod_pkg);
	if (rc != 0) {
		goto exit;
	}

	rc = user_extra_to_cil(mod_pkg);
	if (rc != 0) {
		goto exit;
	}

	rc = file_contexts_to_cil(mod_pkg);
	if (rc != 0) {
		goto exit;
	}

	rc = 0;

exit:
	return rc;
}

static int fp_to_buffer(FILE *fp, char **data, size_t *data_len)
{
	int rc = -1;
	char *d = NULL, *d_tmp;
	size_t d_len = 0;
	size_t read_len = 0;
	size_t max_len = 1 << 17; // start at 128KB, this is enough to hold about half of all the existing pp files

	d = malloc(max_len);
	if (d == NULL) {
		log_err("Out of memory");
		rc = -1;
		goto exit;
	}

	while ((read_len = fread(d + d_len, 1, max_len - d_len, fp)) > 0) {
		d_len += read_len;
		if (d_len == max_len) {
			max_len *= 2;
			d_tmp = realloc(d, max_len);
			if (d_tmp == NULL) {
				log_err("Out of memory");
				rc = -1;
				goto exit;
			}
			d = d_tmp;
		}
	}

	if (ferror(fp) != 0) {
		log_err("Failed to read pp file");
		rc = -1;
		goto exit;
	}

	*data = d;
	*data_len = d_len;

	return 0;

exit:
	free(d);
	return rc;
}

int sepol_ppfile_to_module_package(FILE *fp, struct sepol_module_package **mod_pkg)
{
	int rc = -1;
	struct sepol_policy_file *pf = NULL;
	struct sepol_module_package *pkg = NULL;
	char *data = NULL;
	size_t data_len;
	int fd;
	struct stat sb;

	rc = sepol_policy_file_create(&pf);
	if (rc != 0) {
		log_err("Failed to create policy file");
		goto exit;
	}

	fd = fileno(fp);
	if (fstat(fd, &sb) == -1) {
		rc = -1;
		goto exit;
	}

	if (S_ISFIFO(sb.st_mode) || S_ISSOCK(sb.st_mode)) {
		// libsepol fails when trying to read a policy package from a pipe or a
		// socket due its use of lseek. In this case, read the data into a
		// buffer and provide that to libsepol
		rc = fp_to_buffer(fp, &data, &data_len);
		if (rc != 0) {
			goto exit;
		}

		sepol_policy_file_set_mem(pf, data, data_len);
	} else {
		sepol_policy_file_set_fp(pf, fp);
	}

	rc = sepol_module_package_create(&pkg);
	if (rc != 0) {
		log_err("Failed to create module package");
		goto exit;
	}

	rc = sepol_module_package_read(pkg, pf, 0);
	if (rc != 0) {
		log_err("Failed to read policy package");
		goto exit;
	}

	*mod_pkg = pkg;

exit:
	free(data);

	sepol_policy_file_free(pf);

	if (rc != 0) {
		sepol_module_package_free(pkg);
	}

	return rc;
}

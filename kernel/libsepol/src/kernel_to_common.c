// #include <stdio.h>
// #include <stdlib.h>
// #include <stdarg.h>
#include <linux/string.h>
#include <linux/types.h>
// #include <unistd.h>

// #include <arpa/inet.h>
// #include <netinet/in.h>
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif

#include <sepol/policydb/ebitmap.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/symtab.h>

#include "private.h"
#include "kernel_to_common.h"


void sepol_log_err(const char *fmt, ...)
{
#if 0
	va_list argptr;
	va_start(argptr, fmt);
	if (vfprintf(stderr, fmt, argptr) < 0) {
		_exit(EXIT_FAILURE);
	}
	va_end(argptr);
	if (fprintf(stderr, "\n") < 0) {
		_exit(EXIT_FAILURE);
	}
#endif
}

void sepol_indent(FILE *out, int indent)
{
#if 0
	if (fprintf(out, "%*s", indent * 4, "") < 0) {
		sepol_log_err("Failed to write to output");
	}
#endif
}

void sepol_printf(FILE *out, const char *fmt, ...)
{
#if 0
	va_list argptr;
	va_start(argptr, fmt);
	if (vfprintf(out, fmt, argptr) < 0) {
		sepol_log_err("Failed to write to output");
	}
	va_end(argptr);
#endif
}

__attribute__ ((format(printf, 1, 0)))
static char *create_str_helper(const char *fmt, int num, va_list vargs)
{
	va_list vargs2;
	char *str = NULL;
	char *s;
	size_t len, s_len;
	int i, rc;

	va_copy(vargs2, vargs);

	len = strlen(fmt) + 1; /* +1 for '\0' */

	for (i=0; i<num; i++) {
		s = va_arg(vargs, char *);
		s_len = strlen(s);
		len += s_len > 1 ? s_len - 2 : 0; /* -2 for each %s in fmt */
	}

	str = malloc(len);
	if (!str) {
		sepol_log_err("Out of memory");
		goto exit;
	}

	rc = vsnprintf(str, len, fmt, vargs2);
	if (rc < 0 || rc >= (int)len) {
		goto exit;
	}

	va_end(vargs2);

	return str;

exit:
	free(str);
	va_end(vargs2);
	return NULL;
}

char *create_str(const char *fmt, int num, ...)
{
	char *str = NULL;
	va_list vargs;

	va_start(vargs, num);
	str = create_str_helper(fmt, num, vargs);
	va_end(vargs);

	return str;
}

int strs_init(struct strs **strs, size_t size)
{
	struct strs *new;

	if (size == 0) {
		size = 1;
	}

	*strs = NULL;

	new = malloc(sizeof(struct strs));
	if (!new) {
		sepol_log_err("Out of memory");
		return -1;
	}

	new->list = calloc(size, sizeof(char *));
	if (!new->list) {
		sepol_log_err("Out of memory");
		free(new);
		return -1;
	}

	new->num = 0;
	new->size = size;

	*strs = new;

	return 0;
}

void strs_destroy(struct strs **strs)
{
	if (!strs || !*strs) {
		return;
	}

	free((*strs)->list);
	(*strs)->list = NULL;
	(*strs)->num = 0;
	(*strs)->size = 0;
	free(*strs);
	*strs = NULL;
}

void strs_free_all(struct strs *strs)
{
	if (!strs) {
		return;
	}

	while (strs->num > 0) {
		strs->num--;
		free(strs->list[strs->num]);
	}
}

int strs_add(struct strs *strs, char *s)
{
	if (strs->num + 1 > strs->size) {
		char **new;
		size_t i = strs->size;
		strs->size *= 2;
		new = reallocarray(strs->list, strs->size, sizeof(char *));
		if (!new) {
			sepol_log_err("Out of memory");
			return -1;
		}
		strs->list = new;
		memset(&strs->list[i], 0, sizeof(char *)*(strs->size-i));
	}

	strs->list[strs->num] = s;
	strs->num++;

	return 0;
}

int strs_create_and_add(struct strs *strs, const char *fmt, int num, ...)
{
	char *str;
	va_list vargs;
	int rc;

	va_start(vargs, num);
	str = create_str_helper(fmt, num, vargs);
	va_end(vargs);

	if (!str) {
		rc = -1;
		goto exit;
	}

	rc = strs_add(strs, str);
	if (rc != 0) {
		free(str);
		goto exit;
	}

	return 0;

exit:
	return rc;
}

char *strs_remove_last(struct strs *strs)
{
	if (strs->num == 0) {
		return NULL;
	}
	strs->num--;
	return strs->list[strs->num];
}

int strs_add_at_index(struct strs *strs, char *s, size_t index)
{
	if (index >= strs->size) {
		char **new;
		size_t i = strs->size;
		while (index >= strs->size) {
			strs->size *= 2;
		}
		new = reallocarray(strs->list, strs->size, sizeof(char *));
		if (!new) {
			sepol_log_err("Out of memory");
			return -1;
		}
		strs->list = new;
		memset(&strs->list[i], 0, sizeof(char *)*(strs->size - i));
	}

	strs->list[index] = s;
	if (index >= strs->num) {
		strs->num = index+1;
	}

	return 0;
}

char *strs_read_at_index(struct strs *strs, size_t index)
{
	if (index >= strs->num) {
		return NULL;
	}

	return strs->list[index];
}

static int strs_cmp(const void *a, const void *b)
{
	char *const *aa = a;
	char *const *bb = b;
	return strcmp(*aa,*bb);
}

void strs_sort(struct strs *strs)
{
	if (strs->num == 0) {
		return;
	}
	qsort(strs->list, strs->num, sizeof(char *), strs_cmp);
}

unsigned strs_num_items(const struct strs *strs)
{
	return strs->num;
}

size_t strs_len_items(const struct strs *strs)
{
	unsigned i;
	size_t len = 0;

	for (i=0; i<strs->num; i++) {
		if (!strs->list[i]) continue;
		len += strlen(strs->list[i]);
	}

	return len;
}

char *strs_to_str(const struct strs *strs)
{
	char *str = NULL;
	size_t len = 0;
	char *p;
	unsigned i;
	int rc;

	if (strs->num == 0) {
		goto exit;
	}

	/* strs->num added because either ' ' or '\0' follows each item */
	len = strs_len_items(strs) + strs->num;
	str = malloc(len);
	if (!str) {
		sepol_log_err("Out of memory");
		goto exit;
	}

	p = str;
	for (i=0; i<strs->num; i++) {
		if (!strs->list[i]) continue;
		len = strlen(strs->list[i]);
		rc = snprintf(p, len+1, "%s", strs->list[i]);
		if (rc < 0 || rc > (int)len) {
			free(str);
			str = NULL;
			goto exit;
		}
		p += len;
		if (i < strs->num - 1) {
			*p++ = ' ';
		}
	}

	*p = '\0';

exit:
	return str;
}

void strs_write_each(const struct strs *strs, FILE *out)
{
	unsigned i;

	for (i=0; i<strs->num; i++) {
		if (!strs->list[i]) {
			continue;
		}
		sepol_printf(out, "%s\n",strs->list[i]);
	}
}

void strs_write_each_indented(const struct strs *strs, FILE *out, int indent)
{
	unsigned i;

	for (i=0; i<strs->num; i++) {
		if (!strs->list[i]) {
			continue;
		}
		sepol_indent(out, indent);
		sepol_printf(out, "%s\n",strs->list[i]);
	}
}

int hashtab_ordered_to_strs(char *key, void *data, void *args)
{
	struct strs *strs = (struct strs *)args;
	symtab_datum_t *datum = data;

	return strs_add_at_index(strs, key, datum->value-1);
}

int ebitmap_to_strs(const struct ebitmap *map, struct strs *strs, char **val_to_name)
{
	struct ebitmap_node *node;
	uint32_t i;
	int rc;

	ebitmap_for_each_positive_bit(map, node, i) {
		if (!val_to_name[i])
			continue;

		rc = strs_add(strs, val_to_name[i]);
		if (rc != 0) {
			return -1;
		}
	}

	return 0;
}

char *ebitmap_to_str(const struct ebitmap *map, char **val_to_name, int sort)
{
	struct strs *strs;
	char *str = NULL;
	int rc;

	rc = strs_init(&strs, 32);
	if (rc != 0) {
		goto exit;
	}

	rc = ebitmap_to_strs(map, strs, val_to_name);
	if (rc != 0) {
		goto exit;
	}

	if (sort) {
		strs_sort(strs);
	}

	str = strs_to_str(strs);

exit:
	strs_destroy(&strs);

	return str;
}

int strs_stack_init(struct strs **stack)
{
	return strs_init(stack, STACK_SIZE);
}

void strs_stack_destroy(struct strs **stack)
{
	return strs_destroy(stack);
}

int strs_stack_push(struct strs *stack, char *s)
{
	return strs_add(stack, s);
}

char *strs_stack_pop(struct strs *stack)
{
	return strs_remove_last(stack);
}

int strs_stack_empty(const struct strs *stack)
{
	return strs_num_items(stack) == 0;
}

static int compare_ranges(uint64_t l1, uint64_t h1, uint64_t l2, uint64_t h2)
{
	uint64_t d1, d2;

	d1 = h1-l1;
	d2 = h2-l2;

	if (d1 < d2) {
		return -1;
	} else if (d1 > d2) {
		return 1;
	} else {
		if (l1 < l2) {
			return -1;
		} else if (l1 > l2) {
			return 1;
		}
	}

	return 0;
}

static int fsuse_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	if ((*aa)->v.behavior != (*bb)->v.behavior) {
		if ((*aa)->v.behavior < (*bb)->v.behavior) {
			return -1;
		} else {
			return 1;
		}
	}

	return strcmp((*aa)->u.name, (*bb)->u.name);
}

static int portcon_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;
	int rc;

	rc = compare_ranges((*aa)->u.port.low_port, (*aa)->u.port.high_port,
			    (*bb)->u.port.low_port, (*bb)->u.port.high_port);
	if (rc == 0) {
		if ((*aa)->u.port.protocol < (*bb)->u.port.protocol) {
			rc = -1;
		} else if ((*aa)->u.port.protocol > (*bb)->u.port.protocol) {
			rc = 1;
		}
	}

	return rc;
}

static int netif_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	return strcmp((*aa)->u.name, (*bb)->u.name);
}

static int node_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;
	int rc;

	rc = memcmp(&(*aa)->u.node.mask, &(*bb)->u.node.mask, sizeof((*aa)->u.node.mask));
	if (rc > 0) {
		return -1;
	} else if (rc < 0) {
		return 1;
	}

	return memcmp(&(*aa)->u.node.addr, &(*bb)->u.node.addr, sizeof((*aa)->u.node.addr));
}

static int node6_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;
	int rc;

	rc = memcmp(&(*aa)->u.node6.mask, &(*bb)->u.node6.mask, sizeof((*aa)->u.node6.mask));
	if (rc > 0) {
		return -1;
	} else if (rc < 0) {
		return 1;
	}

	return memcmp(&(*aa)->u.node6.addr, &(*bb)->u.node6.addr, sizeof((*aa)->u.node6.addr));
}

static int ibpkey_data_cmp(const void *a, const void *b)
{
	int rc;
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	rc = (*aa)->u.ibpkey.subnet_prefix - (*bb)->u.ibpkey.subnet_prefix;
	if (rc)
		return rc;

	return compare_ranges((*aa)->u.ibpkey.low_pkey, (*aa)->u.ibpkey.high_pkey,
			      (*bb)->u.ibpkey.low_pkey, (*bb)->u.ibpkey.high_pkey);
}

static int ibendport_data_cmp(const void *a, const void *b)
{
	int rc;
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	rc = strcmp((*aa)->u.ibendport.dev_name, (*bb)->u.ibendport.dev_name);
	if (rc)
		return rc;

	return (*aa)->u.ibendport.port - (*bb)->u.ibendport.port;
}

static int pirq_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	if ((*aa)->u.pirq < (*bb)->u.pirq) {
		return -1;
	} else if ((*aa)->u.pirq > (*bb)->u.pirq) {
		return 1;
	}

	return 0;
}

static int ioport_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	return compare_ranges((*aa)->u.ioport.low_ioport, (*aa)->u.ioport.high_ioport,
			      (*bb)->u.ioport.low_ioport, (*bb)->u.ioport.high_ioport);
}

static int iomem_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	return compare_ranges((*aa)->u.iomem.low_iomem, (*aa)->u.iomem.high_iomem,
			      (*bb)->u.iomem.low_iomem, (*bb)->u.iomem.high_iomem);
}

static int pcid_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	if ((*aa)->u.device < (*bb)->u.device) {
		return -1;
	} else if ((*aa)->u.device > (*bb)->u.device) {
		return 1;
	}

	return 0;
}

static int dtree_data_cmp(const void *a, const void *b)
{
	struct ocontext *const *aa = a;
	struct ocontext *const *bb = b;

	return strcmp((*aa)->u.name, (*bb)->u.name);
}

static int sort_ocontext_data(struct ocontext **ocons, int (*cmp)(const void *, const void *))
{
	struct ocontext *ocon;
	struct ocontext **data;
	unsigned i, num;

	num = 0;
	for (ocon = *ocons; ocon != NULL; ocon = ocon->next) {
		num++;
	}

	if (num == 0) {
		return 0;
	}

	data = calloc(sizeof(*data), num);
	if (!data) {
		sepol_log_err("Out of memory\n");
		return -1;
	}

	i = 0;
	for (ocon = *ocons; ocon != NULL; ocon = ocon->next) {
		data[i] = ocon;
		i++;
	}

	qsort(data, num, sizeof(*data), cmp);

	*ocons = data[0];
	for (i=1; i < num; i++) {
		data[i-1]->next = data[i];
	}
	data[num-1]->next = NULL;

	free(data);

	return 0;
}

int sort_ocontexts(struct policydb *pdb)
{
	int rc = 0;

	if (pdb->target_platform == SEPOL_TARGET_SELINUX) {
		rc = sort_ocontext_data(&pdb->ocontexts[5], fsuse_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[2], portcon_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[3], netif_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[4], node_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[6], node6_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[OCON_IBPKEY], ibpkey_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[OCON_IBENDPORT], ibendport_data_cmp);
		if (rc != 0) {
			goto exit;
		}
	} else if (pdb->target_platform == SEPOL_TARGET_XEN) {
		rc = sort_ocontext_data(&pdb->ocontexts[1], pirq_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[2], ioport_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[3], iomem_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[4], pcid_data_cmp);
		if (rc != 0) {
			goto exit;
		}

		rc = sort_ocontext_data(&pdb->ocontexts[5], dtree_data_cmp);
		if (rc != 0) {
			goto exit;
		}
	}

exit:
	if (rc != 0) {
		sepol_log_err("Error sorting ocontexts\n");
	}

	return rc;
}

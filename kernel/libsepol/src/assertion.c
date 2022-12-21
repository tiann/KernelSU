/* Authors: Joshua Brindle <jbrindle@tresys.com>
 *
 * Assertion checker for avtab entries, taken from
 * checkpolicy.c by Stephen Smalley <sds@tycho.nsa.gov>
 *
 * Copyright (C) 2005 Tresys Technology, LLC
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

#include <sepol/policydb/avtab.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/util.h>

#include "private.h"
#include "debug.h"

struct avtab_match_args {
	sepol_handle_t *handle;
	policydb_t *p;
	avrule_t *avrule;
	avtab_t *avtab;
	unsigned long errors;
};

static const char* policy_name(policydb_t *p) {
	const char *policy_file = "policy.conf";
	if (p->name) {
		policy_file = p->name;
	}
	return policy_file;
}

static void report_failure(sepol_handle_t *handle, policydb_t *p, const avrule_t *avrule,
			   unsigned int stype, unsigned int ttype,
			   const class_perm_node_t *curperm, uint32_t perms)
{
	if (avrule->source_filename) {
		ERR(handle, "neverallow on line %lu of %s (or line %lu of %s) violated by allow %s %s:%s {%s };",
		    avrule->source_line, avrule->source_filename, avrule->line, policy_name(p),
		    p->p_type_val_to_name[stype],
		    p->p_type_val_to_name[ttype],
		    p->p_class_val_to_name[curperm->tclass - 1],
		    sepol_av_to_string(p, curperm->tclass, perms));
	} else if (avrule->line) {
		ERR(handle, "neverallow on line %lu violated by allow %s %s:%s {%s };",
		    avrule->line, p->p_type_val_to_name[stype],
		    p->p_type_val_to_name[ttype],
		    p->p_class_val_to_name[curperm->tclass - 1],
		    sepol_av_to_string(p, curperm->tclass, perms));
	} else {
		ERR(handle, "neverallow violated by allow %s %s:%s {%s };",
		    p->p_type_val_to_name[stype],
		    p->p_type_val_to_name[ttype],
		    p->p_class_val_to_name[curperm->tclass - 1],
		    sepol_av_to_string(p, curperm->tclass, perms));
	}
}

static int match_any_class_permissions(class_perm_node_t *cp, uint32_t class, uint32_t data)
{
	for (; cp; cp = cp->next) {
		if ((cp->tclass == class) && (cp->data & data))
			return 1;
	}

	return 0;
}

static int extended_permissions_and(uint32_t *perms1, uint32_t *perms2) {
	size_t i;
	for (i = 0; i < EXTENDED_PERMS_LEN; i++) {
		if (perms1[i] & perms2[i])
			return 1;
	}

	return 0;
}

static int check_extended_permissions(av_extended_perms_t *neverallow, avtab_extended_perms_t *allow)
{
	int rc = 0;
	if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		if (neverallow->driver == allow->driver)
			rc = extended_permissions_and(neverallow->perms, allow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		rc = xperm_test(neverallow->driver, allow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		rc = xperm_test(allow->driver, neverallow->perms);
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		rc = extended_permissions_and(neverallow->perms, allow->perms);
	}

	return rc;
}

/* Compute which allowed extended permissions violate the neverallow rule */
static void extended_permissions_violated(avtab_extended_perms_t *result,
					av_extended_perms_t *neverallow,
					avtab_extended_perms_t *allow)
{
	size_t i;
	if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		result->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		result->driver = allow->driver;
		for (i = 0; i < EXTENDED_PERMS_LEN; i++)
			result->perms[i] = neverallow->perms[i] & allow->perms[i];
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLFUNCTION)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		result->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		result->driver = neverallow->driver;
		memcpy(result->perms, neverallow->perms, sizeof(result->perms));
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLFUNCTION)) {
		result->specified = AVTAB_XPERMS_IOCTLFUNCTION;
		result->driver = allow->driver;
		memcpy(result->perms, allow->perms, sizeof(result->perms));
	} else if ((neverallow->specified == AVRULE_XPERMS_IOCTLDRIVER)
			&& (allow->specified == AVTAB_XPERMS_IOCTLDRIVER)) {
		result->specified = AVTAB_XPERMS_IOCTLDRIVER;
		for (i = 0; i < EXTENDED_PERMS_LEN; i++)
			result->perms[i] = neverallow->perms[i] & allow->perms[i];
	}
}

/* Same scenarios of interest as check_assertion_extended_permissions */
static int report_assertion_extended_permissions(sepol_handle_t *handle,
				policydb_t *p, const avrule_t *avrule,
				unsigned int stype, unsigned int ttype,
				const class_perm_node_t *curperm, uint32_t perms,
				avtab_key_t *k, avtab_t *avtab)
{
	avtab_ptr_t node;
	avtab_key_t tmp_key;
	avtab_extended_perms_t *xperms;
	avtab_extended_perms_t error;
	ebitmap_t *sattr = &p->type_attr_map[stype];
	ebitmap_t *tattr = &p->type_attr_map[ttype];
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	int rc;
	int found_xperm = 0;
	int errors = 0;

	memcpy(&tmp_key, k, sizeof(avtab_key_t));
	tmp_key.specified = AVTAB_XPERMS_ALLOWED;

	ebitmap_for_each_positive_bit(sattr, snode, i) {
		tmp_key.source_type = i + 1;
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			tmp_key.target_type = j + 1;
			for (node = ksu_avtab_search_node(avtab, &tmp_key);
			     node;
			     node = ksu_avtab_search_node_next(node, tmp_key.specified)) {
				xperms = node->datum.xperms;
				if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
						&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER))
					continue;
				found_xperm = 1;
				rc = check_extended_permissions(avrule->xperms, xperms);
				/* failure on the extended permission check_extended_permissions */
				if (rc) {
					extended_permissions_violated(&error, avrule->xperms, xperms);
					ERR(handle, "neverallowxperm on line %lu of %s (or line %lu of %s) violated by\n"
							"allowxperm %s %s:%s %s;",
							avrule->source_line, avrule->source_filename, avrule->line, policy_name(p),
							p->p_type_val_to_name[i],
							p->p_type_val_to_name[j],
							p->p_class_val_to_name[curperm->tclass - 1],
							sepol_extended_perms_to_string(&error));

					errors++;
				}
			}
		}
	}

	/* failure on the regular permissions */
	if (!found_xperm) {
		ERR(handle, "neverallowxperm on line %lu of %s (or line %lu of %s) violated by\n"
				"allow %s %s:%s {%s };",
				avrule->source_line, avrule->source_filename, avrule->line, policy_name(p),
				p->p_type_val_to_name[stype],
				p->p_type_val_to_name[ttype],
				p->p_class_val_to_name[curperm->tclass - 1],
				sepol_av_to_string(p, curperm->tclass, perms));
		errors++;

	}

	return errors;
}

static int report_assertion_avtab_matches(avtab_key_t *k, avtab_datum_t *d, void *args)
{
	int rc = 0;
	struct avtab_match_args *a = (struct avtab_match_args *)args;
	sepol_handle_t *handle = a->handle;
	policydb_t *p = a->p;
	avtab_t *avtab = a->avtab;
	avrule_t *avrule = a->avrule;
	class_perm_node_t *cp;
	uint32_t perms;
	ebitmap_t src_matches, tgt_matches, self_matches;
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	const int is_avrule_self = (avrule->flags & RULE_SELF) != 0;

	if ((k->specified & AVTAB_ALLOWED) == 0)
		return 0;

	if (!match_any_class_permissions(avrule->perms, k->target_class, d->data))
		return 0;

	ebitmap_init(&src_matches);
	ebitmap_init(&tgt_matches);
	ebitmap_init(&self_matches);

	rc = ksu_ebitmap_and(&src_matches, &avrule->stypes.types,
			 &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (ebitmap_is_empty(&src_matches))
		goto exit;

	rc = ksu_ebitmap_and(&tgt_matches, &avrule->ttypes.types, &p->attr_type_map[k->target_type -1]);
	if (rc < 0)
		goto oom;

	if (is_avrule_self) {
		rc = ksu_ebitmap_and(&self_matches, &src_matches, &p->attr_type_map[k->target_type - 1]);
		if (rc < 0)
			goto oom;

		if (!ebitmap_is_empty(&self_matches)) {
			rc = ebitmap_union(&tgt_matches, &self_matches);
			if (rc < 0)
				goto oom;
		}
	}

	if (ebitmap_is_empty(&tgt_matches))
		goto exit;

	for (cp = avrule->perms; cp; cp = cp->next) {

		perms = cp->data & d->data;
		if ((cp->tclass != k->target_class) || !perms) {
			continue;
		}

		ebitmap_for_each_positive_bit(&src_matches, snode, i) {
			ebitmap_for_each_positive_bit(&tgt_matches, tnode, j) {
				if (is_avrule_self && i != j)
					continue;
				if (avrule->specified == AVRULE_XPERMS_NEVERALLOW) {
					a->errors += report_assertion_extended_permissions(handle,p, avrule,
											i, j, cp, perms, k, avtab);
				} else {
					a->errors++;
					report_failure(handle, p, avrule, i, j, cp, perms);
				}
			}
		}
	}

oom:
exit:
	ksu_ebitmap_destroy(&src_matches);
	ksu_ebitmap_destroy(&tgt_matches);
	ksu_ebitmap_destroy(&self_matches);
	return rc;
}

static int report_assertion_failures(sepol_handle_t *handle, policydb_t *p, avrule_t *avrule)
{
	int rc;
	struct avtab_match_args args;

	args.handle = handle;
	args.p = p;
	args.avrule = avrule;
	args.errors = 0;

	args.avtab =  &p->te_avtab;
	rc = avtab_map(&p->te_avtab, report_assertion_avtab_matches, &args);
	if (rc < 0)
		goto oom;

	args.avtab =  &p->te_cond_avtab;
	rc = avtab_map(&p->te_cond_avtab, report_assertion_avtab_matches, &args);
	if (rc < 0)
		goto oom;

	return args.errors;

oom:
	return rc;
}

/*
 * Look up the extended permissions in avtab and verify that neverallowed
 * permissions are not granted.
 */
static int check_assertion_extended_permissions_avtab(avrule_t *avrule, avtab_t *avtab,
						unsigned int stype, unsigned int ttype,
						avtab_key_t *k, policydb_t *p)
{
	avtab_ptr_t node;
	avtab_key_t tmp_key;
	avtab_extended_perms_t *xperms;
	av_extended_perms_t *neverallow_xperms = avrule->xperms;
	ebitmap_t *sattr = &p->type_attr_map[stype];
	ebitmap_t *tattr = &p->type_attr_map[ttype];
	ebitmap_node_t *snode, *tnode;
	unsigned int i, j;
	int rc = 1;

	memcpy(&tmp_key, k, sizeof(avtab_key_t));
	tmp_key.specified = AVTAB_XPERMS_ALLOWED;

	ebitmap_for_each_positive_bit(sattr, snode, i) {
		tmp_key.source_type = i + 1;
		ebitmap_for_each_positive_bit(tattr, tnode, j) {
			tmp_key.target_type = j + 1;
			for (node = ksu_avtab_search_node(avtab, &tmp_key);
			     node;
			     node = ksu_avtab_search_node_next(node, tmp_key.specified)) {
				xperms = node->datum.xperms;

				if ((xperms->specified != AVTAB_XPERMS_IOCTLFUNCTION)
						&& (xperms->specified != AVTAB_XPERMS_IOCTLDRIVER))
					continue;
				rc = check_extended_permissions(neverallow_xperms, xperms);
				if (rc)
					return rc;
			}
		}
	}

	return rc;
}

/*
 * When the ioctl permission is granted on an avtab entry that matches an
 * avrule neverallowxperm entry, enumerate over the matching
 * source/target/class sets to determine if the extended permissions exist
 * and if the neverallowed ioctls are granted.
 *
 * Four scenarios of interest:
 * 1. PASS - the ioctl permission is not granted for this source/target/class
 *    This case is handled in check_assertion_avtab_match
 * 2. PASS - The ioctl permission is granted AND the extended permission
 *    is NOT granted
 * 3. FAIL - The ioctl permission is granted AND no extended permissions
 *    exist
 * 4. FAIL - The ioctl permission is granted AND the extended permission is
 *    granted
 */
static int check_assertion_extended_permissions(avrule_t *avrule, avtab_t *avtab,
						avtab_key_t *k, policydb_t *p)
{
	ebitmap_t src_matches, tgt_matches, self_matches;
	unsigned int i, j;
	ebitmap_node_t *snode, *tnode;
	const int is_avrule_self = (avrule->flags & RULE_SELF) != 0;
	int rc;

	ebitmap_init(&src_matches);
	ebitmap_init(&tgt_matches);
	ebitmap_init(&self_matches);

	rc = ksu_ebitmap_and(&src_matches, &avrule->stypes.types,
			 &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (ebitmap_is_empty(&src_matches)) {
		rc = 0;
		goto exit;
	}

	rc = ksu_ebitmap_and(&tgt_matches, &avrule->ttypes.types,
			 &p->attr_type_map[k->target_type -1]);
	if (rc < 0)
		goto oom;

	if (is_avrule_self) {
		rc = ksu_ebitmap_and(&self_matches, &src_matches, &p->attr_type_map[k->target_type - 1]);
		if (rc < 0)
			goto oom;

		if (!ebitmap_is_empty(&self_matches)) {
			rc = ebitmap_union(&tgt_matches, &self_matches);
			if (rc < 0)
				goto oom;
		}
	}

	if (ebitmap_is_empty(&tgt_matches)) {
		rc = 0;
		goto exit;
	}

	ebitmap_for_each_positive_bit(&src_matches, snode, i) {
		ebitmap_for_each_positive_bit(&tgt_matches, tnode, j) {
			if (is_avrule_self && i != j)
				continue;
			if (check_assertion_extended_permissions_avtab(avrule, avtab, i, j, k, p)) {
				rc = 1;
				goto exit;
			}
		}
	}

	rc = 0;

oom:
exit:
	ksu_ebitmap_destroy(&src_matches);
	ksu_ebitmap_destroy(&tgt_matches);
	ksu_ebitmap_destroy(&self_matches);
	return rc;
}

static int check_assertion_self_match(avtab_key_t *k, avrule_t *avrule, policydb_t *p)
{
	ebitmap_t src_matches;
	int rc;

	/* The key's target must match something in the matches of the avrule's source
	 * and the key's source.
	 */

	rc = ksu_ebitmap_and(&src_matches, &avrule->stypes.types, &p->attr_type_map[k->source_type - 1]);
	if (rc < 0)
		goto oom;

	if (!ebitmap_match_any(&src_matches, &p->attr_type_map[k->target_type - 1])) {
		rc = 0;
		goto nomatch;
	}

	rc = 1;

oom:
nomatch:
	ksu_ebitmap_destroy(&src_matches);
	return rc;
}

static int check_assertion_avtab_match(avtab_key_t *k, avtab_datum_t *d, void *args)
{
	int rc;
	struct avtab_match_args *a = (struct avtab_match_args *)args;
	policydb_t *p = a->p;
	avrule_t *avrule = a->avrule;
	avtab_t *avtab = a->avtab;

	if ((k->specified & AVTAB_ALLOWED) == 0)
		goto nomatch;

	if (!match_any_class_permissions(avrule->perms, k->target_class, d->data))
		goto nomatch;

	if (!ebitmap_match_any(&avrule->stypes.types, &p->attr_type_map[k->source_type - 1]))
		goto nomatch;

	/* neverallow may have tgts even if it uses SELF */
	if (!ebitmap_match_any(&avrule->ttypes.types, &p->attr_type_map[k->target_type -1])) {
		if (avrule->flags == RULE_SELF) {
			rc = check_assertion_self_match(k, avrule, p);
			if (rc < 0)
				goto oom;
			if (rc == 0)
				goto nomatch;
		} else {
			goto nomatch;
		}
	}

	if (avrule->specified == AVRULE_XPERMS_NEVERALLOW) {
		rc = check_assertion_extended_permissions(avrule, avtab, k, p);
		if (rc < 0)
			goto oom;
		if (rc == 0)
			goto nomatch;
	}
	return 1;

nomatch:
	return 0;

oom:
	return rc;
}

int check_assertion(policydb_t *p, avrule_t *avrule)
{
	int rc;
	struct avtab_match_args args;

	args.handle = NULL;
	args.p = p;
	args.avrule = avrule;
	args.errors = 0;
	args.avtab = &p->te_avtab;

	rc = avtab_map(&p->te_avtab, check_assertion_avtab_match, &args);

	if (rc == 0) {
		args.avtab = &p->te_cond_avtab;
		rc = avtab_map(&p->te_cond_avtab, check_assertion_avtab_match, &args);
	}

	return rc;
}

int check_assertions(sepol_handle_t * handle, policydb_t * p,
		     avrule_t * avrules)
{
	int rc;
	avrule_t *a;
	unsigned long errors = 0;

	if (!avrules) {
		/* Since assertions are stored in avrules, if it is NULL
		   there won't be any to check. This also prevents an invalid
		   free if the avtabs are never initialized */
		return 0;
	}

	for (a = avrules; a != NULL; a = a->next) {
		if (!(a->specified & (AVRULE_NEVERALLOW | AVRULE_XPERMS_NEVERALLOW)))
			continue;
		rc = check_assertion(p, a);
		if (rc < 0) {
			ERR(handle, "Error occurred while checking neverallows");
			return -1;
		}
		if (rc) {
			rc = report_assertion_failures(handle, p, a);
			if (rc < 0) {
				ERR(handle, "Error occurred while checking neverallows");
				return -1;
			}
			errors += rc;
		}
	}

	if (errors)
		ERR(handle, "%lu neverallow failures occurred", errors);

	return errors ? -1 : 0;
}

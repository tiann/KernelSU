/* Author : Stephen Smalley, <sds@tycho.nsa.gov> */
/*
 * Updated: Trusted Computer Solutions, Inc. <dgoeddel@trustedcs.com>
 *
 *	Support for enhanced MLS infrastructure.
 *
 * Copyright (C) 2004-2005 Trusted Computer Solutions, Inc.
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

/* FLASK */

/* 
 * Implementation of the multi-level security (MLS) policy.
 */

#include <sepol/context.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/services.h>
#include <sepol/policydb/context.h>

// #include <stdlib.h>

#include "handle.h"
#include "debug.h"
#include "private.h"
#include "mls.h"

int mls_to_string(sepol_handle_t * handle,
		  const policydb_t * policydb,
		  const context_struct_t * mls, char **str)
{

	char *ptr = NULL, *ptr2 = NULL;

	/* Temporary buffer - length + NULL terminator */
	int len = ksu_mls_compute_context_len(policydb, mls) + 1;

	ptr = (char *)malloc(len);
	if (ptr == NULL)
		goto omem;

	/* Final string w/ ':' cut off */
	ptr2 = (char *)malloc(len - 1);
	if (ptr2 == NULL)
		goto omem;

	ksu_mls_sid_to_context(policydb, mls, &ptr);
	ptr -= len - 1;
	strcpy(ptr2, ptr + 1);

	free(ptr);
	*str = ptr2;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory, could not convert mls context to string");

	free(ptr);
	free(ptr2);
	return STATUS_ERR;

}

int ksu_mls_from_string(sepol_handle_t * handle,
		    const policydb_t * policydb,
		    const char *str, context_struct_t * mls)
{

	char *tmp = strdup(str);
	char *tmp_cp = tmp;
	if (!tmp)
		goto omem;

	if (ksu_mls_context_to_sid(policydb, '$', &tmp_cp, mls) < 0) {
		ERR(handle, "invalid MLS context %s", str);
		free(tmp);
		goto err;
	}

	free(tmp);
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	ERR(handle, "could not construct mls context structure");
	return STATUS_ERR;
}

/*
 * Return the length in bytes for the MLS fields of the
 * security context string representation of `context'.
 */
int ksu_mls_compute_context_len(const policydb_t * policydb,
			    const context_struct_t * context)
{

	unsigned int i, l, len, range;
	ebitmap_node_t *cnode;

	if (!policydb->mls)
		return 0;

	len = 1;		/* for the beginning ":" */
	for (l = 0; l < 2; l++) {
		range = 0;
		len +=
		    strlen(policydb->
			   p_sens_val_to_name[context->range.level[l].sens -
					      1]);

		ebitmap_for_each_bit(&context->range.level[l].cat, cnode, i) {
			if (ebitmap_node_get_bit(cnode, i)) {
				if (range) {
					range++;
					continue;
				}

				len +=
				    strlen(policydb->p_cat_val_to_name[i]) + 1;
				range++;
			} else {
				if (range > 1)
					len +=
					    strlen(policydb->
						   p_cat_val_to_name[i - 1]) +
					    1;
				range = 0;
			}
		}
		/* Handle case where last category is the end of range */
		if (range > 1)
			len += strlen(policydb->p_cat_val_to_name[i - 1]) + 1;

		if (l == 0) {
			if (mls_level_eq(&context->range.level[0],
					 &context->range.level[1]))
				break;
			else
				len++;
		}
	}

	return len;
}

/*
 * Write the security context string representation of
 * the MLS fields of `context' into the string `*scontext'.
 * Update `*scontext' to point to the end of the MLS fields.
 */
void ksu_mls_sid_to_context(const policydb_t * policydb,
			const context_struct_t * context, char **scontext)
{

	char *scontextp;
	unsigned int i, l, range, wrote_sep;
	ebitmap_node_t *cnode;

	if (!policydb->mls)
		return;

	scontextp = *scontext;

	*scontextp = ':';
	scontextp++;

	for (l = 0; l < 2; l++) {
		range = 0;
		wrote_sep = 0;
		strcpy(scontextp,
		       policydb->p_sens_val_to_name[context->range.level[l].
						    sens - 1]);
		scontextp +=
		    strlen(policydb->
			   p_sens_val_to_name[context->range.level[l].sens -
					      1]);
		/* categories */
		ebitmap_for_each_bit(&context->range.level[l].cat, cnode, i) {
			if (ebitmap_node_get_bit(cnode, i)) {
				if (range) {
					range++;
					continue;
				}

				if (!wrote_sep) {
					*scontextp++ = ':';
					wrote_sep = 1;
				} else
					*scontextp++ = ',';
				strcpy(scontextp,
				       policydb->p_cat_val_to_name[i]);
				scontextp +=
				    strlen(policydb->p_cat_val_to_name[i]);
				range++;
			} else {
				if (range > 1) {
					if (range > 2)
						*scontextp++ = '.';
					else
						*scontextp++ = ',';

					strcpy(scontextp,
					       policydb->p_cat_val_to_name[i -
									   1]);
					scontextp +=
					    strlen(policydb->
						   p_cat_val_to_name[i - 1]);
				}
				range = 0;
			}
		}
		/* Handle case where last category is the end of range */
		if (range > 1) {
			if (range > 2)
				*scontextp++ = '.';
			else
				*scontextp++ = ',';

			strcpy(scontextp, policydb->p_cat_val_to_name[i - 1]);
			scontextp += strlen(policydb->p_cat_val_to_name[i - 1]);
		}

		if (l == 0) {
			if (mls_level_eq(&context->range.level[0],
					 &context->range.level[1]))
				break;
			else {
				*scontextp = '-';
				scontextp++;
			}
		}
	}

	*scontext = scontextp;
	return;
}

/*
 * Return 1 if the MLS fields in the security context
 * structure `c' are valid.  Return 0 otherwise.
 */
int ksu_mls_context_isvalid(const policydb_t * p, const context_struct_t * c)
{

	level_datum_t *levdatum;
	user_datum_t *usrdatum;
	unsigned int i, l;
	ebitmap_node_t *cnode;
	hashtab_key_t key;

	if (!p->mls)
		return 1;

	/*
	 * MLS range validity checks: high must dominate low, low level must
	 * be valid (category set <-> sensitivity check), and high level must
	 * be valid (category set <-> sensitivity check)
	 */
	if (!mls_level_dom(&c->range.level[1], &c->range.level[0]))
		/* High does not dominate low. */
		return 0;

	for (l = 0; l < 2; l++) {
		if (!c->range.level[l].sens
		    || c->range.level[l].sens > p->p_levels.nprim)
			return 0;

		key = p->p_sens_val_to_name[c->range.level[l].sens - 1];
		if (!key)
			return 0;

		levdatum = (level_datum_t *) hashtab_search(p->p_levels.table, key);
		if (!levdatum)
			return 0;

		ebitmap_for_each_positive_bit(&c->range.level[l].cat, cnode, i) {
			if (i > p->p_cats.nprim)
				return 0;
			if (!ksu_ebitmap_get_bit(&levdatum->level->cat, i))
				/*
				 * Category may not be associated with
				 * sensitivity in low level.
				 */
				return 0;
		}
	}

	if (c->role == OBJECT_R_VAL)
		return 1;

	/*
	 * User must be authorized for the MLS range.
	 */
	if (!c->user || c->user > p->p_users.nprim)
		return 0;
	usrdatum = p->user_val_to_struct[c->user - 1];
	if (!usrdatum || !mls_range_contains(usrdatum->exp_range, c->range))
		return 0;	/* user may not be associated with range */

	return 1;
}

/*
 * Set the MLS fields in the security context structure
 * `context' based on the string representation in
 * the string `*scontext'.  Update `*scontext' to
 * point to the end of the string representation of
 * the MLS fields.
 *
 * This function modifies the string in place, inserting
 * NULL characters to terminate the MLS fields.
 */
int ksu_mls_context_to_sid(const policydb_t * policydb,
		       char oldc, char **scontext, context_struct_t * context)
{

	char delim;
	char *scontextp, *p, *rngptr;
	level_datum_t *levdatum;
	cat_datum_t *catdatum, *rngdatum;
	unsigned int l;

	if (!policydb->mls)
		return 0;

	/* No MLS component to the security context */
	if (!oldc)
		goto err;

	/* Extract low sensitivity. */
	scontextp = p = *scontext;
	while (*p && *p != ':' && *p != '-')
		p++;

	delim = *p;
	if (delim != 0)
		*p++ = 0;

	for (l = 0; l < 2; l++) {
		levdatum =
		    (level_datum_t *) hashtab_search(policydb->p_levels.table,
						     (hashtab_key_t) scontextp);

		if (!levdatum)
			goto err;

		context->range.level[l].sens = levdatum->level->sens;

		if (delim == ':') {
			/* Extract category set. */
			while (1) {
				scontextp = p;
				while (*p && *p != ',' && *p != '-')
					p++;
				delim = *p;
				if (delim != 0)
					*p++ = 0;

				/* Separate into range if exists */
				if ((rngptr = strchr(scontextp, '.')) != NULL) {
					/* Remove '.' */
					*rngptr++ = 0;
				}

				catdatum =
				    (cat_datum_t *) hashtab_search(policydb->
								   p_cats.table,
								   (hashtab_key_t)
								   scontextp);
				if (!catdatum)
					goto err;

				if (ksu_ebitmap_set_bit
				    (&context->range.level[l].cat,
				     catdatum->s.value - 1, 1))
					goto err;

				/* If range, set all categories in range */
				if (rngptr) {
					unsigned int i;

					rngdatum = (cat_datum_t *)
					    hashtab_search(policydb->p_cats.
							   table,
							   (hashtab_key_t)
							   rngptr);
					if (!rngdatum)
						goto err;

					if (catdatum->s.value >=
					    rngdatum->s.value)
						goto err;

					for (i = catdatum->s.value;
					     i < rngdatum->s.value; i++) {
						if (ksu_ebitmap_set_bit
						    (&context->range.level[l].
						     cat, i, 1))
							goto err;
					}
				}

				if (delim != ',')
					break;
			}
		}
		if (delim == '-') {
			/* Extract high sensitivity. */
			scontextp = p;
			while (*p && *p != ':')
				p++;

			delim = *p;
			if (delim != 0)
				*p++ = 0;
		} else
			break;
	}

	/* High level is missing, copy low level */
	if (l == 0) {
		if (mls_level_cpy(&context->range.level[1],
				  &context->range.level[0]) < 0)
			goto err;
	}
	*scontext = ++p;

	return STATUS_SUCCESS;

      err:
	return STATUS_ERR;
}

/*
 * Copies the MLS range from `src' into `dst'.
 */
static inline int mls_copy_context(context_struct_t * dst,
				   const context_struct_t * src)
{
	int l, rc = 0;

	/* Copy the MLS range from the source context */
	for (l = 0; l < 2; l++) {
		dst->range.level[l].sens = src->range.level[l].sens;
		rc = ksu_ebitmap_cpy(&dst->range.level[l].cat,
				 &src->range.level[l].cat);
		if (rc)
			break;
	}

	return rc;
}

/*
 * Copies the effective MLS range from `src' into `dst'.
 */
static inline int mls_scopy_context(context_struct_t * dst,
				    const context_struct_t * src)
{
	int l, rc = 0;

	/* Copy the MLS range from the source context */
	for (l = 0; l < 2; l++) {
		dst->range.level[l].sens = src->range.level[0].sens;
		rc = ksu_ebitmap_cpy(&dst->range.level[l].cat,
				 &src->range.level[0].cat);
		if (rc)
			break;
	}

	return rc;
}

/*
 * Copies the MLS range `range' into `context'.
 */
static inline int mls_range_set(context_struct_t * context, const mls_range_t * range)
{
	int l, rc = 0;

	/* Copy the MLS range into the  context */
	for (l = 0; l < 2; l++) {
		context->range.level[l].sens = range->level[l].sens;
		rc = ksu_ebitmap_cpy(&context->range.level[l].cat,
				 &range->level[l].cat);
		if (rc)
			break;
	}

	return rc;
}

int ksu_mls_setup_user_range(context_struct_t * fromcon, user_datum_t * user,
			 context_struct_t * usercon, int mls)
{
	if (mls) {
		mls_level_t *fromcon_sen = &(fromcon->range.level[0]);
		mls_level_t *fromcon_clr = &(fromcon->range.level[1]);
		mls_level_t *user_low = &(user->exp_range.level[0]);
		mls_level_t *user_clr = &(user->exp_range.level[1]);
		mls_level_t *user_def = &(user->exp_dfltlevel);
		mls_level_t *usercon_sen = &(usercon->range.level[0]);
		mls_level_t *usercon_clr = &(usercon->range.level[1]);

		/* Honor the user's default level if we can */
		if (mls_level_between(user_def, fromcon_sen, fromcon_clr)) {
			*usercon_sen = *user_def;
		} else if (mls_level_between(fromcon_sen, user_def, user_clr)) {
			*usercon_sen = *fromcon_sen;
		} else if (mls_level_between(fromcon_clr, user_low, user_def)) {
			*usercon_sen = *user_low;
		} else
			return -EINVAL;

		/* Lower the clearance of available contexts
		   if the clearance of "fromcon" is lower than
		   that of the user's default clearance (but
		   only if the "fromcon" clearance dominates
		   the user's computed sensitivity level) */
		if (mls_level_dom(user_clr, fromcon_clr)) {
			*usercon_clr = *fromcon_clr;
		} else if (mls_level_dom(fromcon_clr, user_clr)) {
			*usercon_clr = *user_clr;
		} else
			return -EINVAL;
	}

	return 0;
}

/*
 * Convert the MLS fields in the security context
 * structure `c' from the values specified in the
 * policy `oldp' to the values specified in the policy `newp'.
 */
int ksu_mls_convert_context(policydb_t * oldp,
			policydb_t * newp, context_struct_t * c)
{
	level_datum_t *levdatum;
	cat_datum_t *catdatum;
	ebitmap_t bitmap;
	unsigned int l, i;
	ebitmap_node_t *cnode;

	if (!oldp->mls)
		return 0;

	for (l = 0; l < 2; l++) {
		levdatum =
		    (level_datum_t *) hashtab_search(newp->p_levels.table,
						     oldp->
						     p_sens_val_to_name[c->
									range.
									level
									[l].
									sens -
									1]);

		if (!levdatum)
			return -EINVAL;
		c->range.level[l].sens = levdatum->level->sens;

		ebitmap_init(&bitmap);
		ebitmap_for_each_positive_bit(&c->range.level[l].cat, cnode, i) {
			int rc;

			catdatum =
			    (cat_datum_t *) hashtab_search(newp->p_cats.
							   table,
							   oldp->
							   p_cat_val_to_name
							   [i]);
			if (!catdatum)
				return -EINVAL;
			rc = ksu_ebitmap_set_bit(&bitmap,
					     catdatum->s.value - 1, 1);
			if (rc)
				return rc;
		}
		ksu_ebitmap_destroy(&c->range.level[l].cat);
		c->range.level[l].cat = bitmap;
	}

	return 0;
}

int ksu_mls_compute_sid(policydb_t * policydb,
		    const context_struct_t * scontext,
		    const context_struct_t * tcontext,
		    sepol_security_class_t tclass,
		    uint32_t specified, context_struct_t * newcontext)
{
	range_trans_t rtr;
	struct mls_range *r;
	struct class_datum *cladatum;
	int default_range = 0;

	if (!policydb->mls)
		return 0;

	switch (specified) {
	case AVTAB_TRANSITION:
		/* Look for a range transition rule. */
		rtr.source_type = scontext->type;
		rtr.target_type = tcontext->type;
		rtr.target_class = tclass;
		r = hashtab_search(policydb->range_tr, (hashtab_key_t) &rtr);
		if (r)
			return mls_range_set(newcontext, r);

		if (tclass && tclass <= policydb->p_classes.nprim) {
			cladatum = policydb->class_val_to_struct[tclass - 1];
			if (cladatum)
				default_range = cladatum->default_range;
		}

		switch (default_range) {
		case DEFAULT_SOURCE_LOW:
			return mls_context_cpy_low(newcontext, scontext);
		case DEFAULT_SOURCE_HIGH:
			return mls_context_cpy_high(newcontext, scontext);
		case DEFAULT_SOURCE_LOW_HIGH:
			return mls_context_cpy(newcontext, scontext);
		case DEFAULT_TARGET_LOW:
			return mls_context_cpy_low(newcontext, tcontext);
		case DEFAULT_TARGET_HIGH:
			return mls_context_cpy_high(newcontext, tcontext);
		case DEFAULT_TARGET_LOW_HIGH:
			return mls_context_cpy(newcontext, tcontext);
		case DEFAULT_GLBLUB:
			return mls_context_glblub(newcontext, scontext, tcontext);
		}

		/* Fallthrough */
	case AVTAB_CHANGE:
		if (tclass == policydb->process_class)
			/* Use the process MLS attributes. */
			return mls_copy_context(newcontext, scontext);
		else
			/* Use the process effective MLS attributes. */
			return mls_scopy_context(newcontext, scontext);
	case AVTAB_MEMBER:
		/* Use the process effective MLS attributes. */
		return mls_context_cpy_low(newcontext, scontext);
	default:
		return -EINVAL;
	}
	return -EINVAL;
}

int sepol_mls_contains(sepol_handle_t * handle,
		       const sepol_policydb_t * policydb,
		       const char *mls1, const char *mls2, int *response)
{

	context_struct_t *ctx1 = NULL, *ctx2 = NULL;
	ctx1 = malloc(sizeof(context_struct_t));
	ctx2 = malloc(sizeof(context_struct_t));
	if (ctx1 == NULL || ctx2 == NULL)
		goto omem;
	context_init(ctx1);
	context_init(ctx2);

	if (ksu_mls_from_string(handle, &policydb->p, mls1, ctx1) < 0)
		goto err;

	if (ksu_mls_from_string(handle, &policydb->p, mls2, ctx2) < 0)
		goto err;

	*response = mls_range_contains(ctx1->range, ctx2->range);
	context_destroy(ctx1);
	context_destroy(ctx2);
	free(ctx1);
	free(ctx2);
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	ERR(handle, "could not check if mls context %s contains %s",
	    mls1, mls2);
	context_destroy(ctx1);
	context_destroy(ctx2);
	free(ctx1);
	free(ctx2);
	return STATUS_ERR;
}

int sepol_mls_check(sepol_handle_t * handle,
		    const sepol_policydb_t * policydb, const char *mls)
{

	int ret;
	context_struct_t *con = malloc(sizeof(context_struct_t));
	if (!con) {
		ERR(handle, "out of memory, could not check if "
		    "mls context %s is valid", mls);
		return STATUS_ERR;
	}
	context_init(con);

	ret = ksu_mls_from_string(handle, &policydb->p, mls, con);
	context_destroy(con);
	free(con);
	return ret;
}

void mls_semantic_cat_init(mls_semantic_cat_t * c)
{
	memset(c, 0, sizeof(mls_semantic_cat_t));
}

void mls_semantic_cat_destroy(mls_semantic_cat_t * c __attribute__ ((unused)))
{
	/* it's currently a simple struct - really nothing to destroy */
	return;
}

void mls_semantic_level_init(mls_semantic_level_t * l)
{
	memset(l, 0, sizeof(mls_semantic_level_t));
}

void mls_semantic_level_destroy(mls_semantic_level_t * l)
{
	mls_semantic_cat_t *cur, *next;

	if (l == NULL)
		return;

	next = l->cat;
	while (next) {
		cur = next;
		next = cur->next;
		mls_semantic_cat_destroy(cur);
		free(cur);
	}
}

int mls_semantic_level_cpy(mls_semantic_level_t * dst,
			   const mls_semantic_level_t * src)
{
	const mls_semantic_cat_t *cat;
	mls_semantic_cat_t *newcat, *lnewcat = NULL;

	mls_semantic_level_init(dst);
	dst->sens = src->sens;
	cat = src->cat;
	while (cat) {
		newcat =
		    (mls_semantic_cat_t *) malloc(sizeof(mls_semantic_cat_t));
		if (!newcat)
			goto err;

		mls_semantic_cat_init(newcat);
		if (lnewcat)
			lnewcat->next = newcat;
		else
			dst->cat = newcat;

		newcat->low = cat->low;
		newcat->high = cat->high;

		lnewcat = newcat;
		cat = cat->next;
	}
	return 0;

      err:
	mls_semantic_level_destroy(dst);
	return -1;
}

void mls_semantic_range_init(mls_semantic_range_t * r)
{
	mls_semantic_level_init(&r->level[0]);
	mls_semantic_level_init(&r->level[1]);
}

void mls_semantic_range_destroy(mls_semantic_range_t * r)
{
	mls_semantic_level_destroy(&r->level[0]);
	mls_semantic_level_destroy(&r->level[1]);
}

int mls_semantic_range_cpy(mls_semantic_range_t * dst,
			   const mls_semantic_range_t * src)
{
	if (mls_semantic_level_cpy(&dst->level[0], &src->level[0]) < 0)
		return -1;

	if (mls_semantic_level_cpy(&dst->level[1], &src->level[1]) < 0) {
		mls_semantic_level_destroy(&dst->level[0]);
		return -1;
	}

	return 0;
}

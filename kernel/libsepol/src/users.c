// #include <stdlib.h>
// #include <stddef.h>
#include <linux/string.h>

#include "private.h"
#include "debug.h"
#include "handle.h"

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/expand.h>
#include "user_internal.h"
#include "mls.h"

static int user_to_record(sepol_handle_t * handle,
			  const policydb_t * policydb,
			  int user_idx, sepol_user_t ** record)
{

	const char *name = policydb->p_user_val_to_name[user_idx];
	user_datum_t *usrdatum = policydb->user_val_to_struct[user_idx];
	ebitmap_t *roles;
	ebitmap_node_t *rnode;
	unsigned bit;

	sepol_user_t *tmp_record = NULL;

	if (!usrdatum)
		goto err;

	roles = &(usrdatum->roles.roles);

	if (sepol_user_create(handle, &tmp_record) < 0)
		goto err;

	if (sepol_user_set_name(handle, tmp_record, name) < 0)
		goto err;

	/* Extract roles */
	ebitmap_for_each_positive_bit(roles, rnode, bit) {
		char *role = policydb->p_role_val_to_name[bit];
		if (sepol_user_add_role(handle, tmp_record, role) < 0)
			goto err;
	}

	/* Extract MLS info */
	if (policydb->mls) {
		context_struct_t context;
		char *str;

		context_init(&context);
		if (mls_level_cpy(&context.range.level[0],
				  &usrdatum->exp_dfltlevel) < 0) {
			ERR(handle, "could not copy MLS level");
			context_destroy(&context);
			goto err;
		}
		if (mls_level_cpy(&context.range.level[1],
				  &usrdatum->exp_dfltlevel) < 0) {
			ERR(handle, "could not copy MLS level");
			context_destroy(&context);
			goto err;
		}
		if (mls_to_string(handle, policydb, &context, &str) < 0) {
			context_destroy(&context);
			goto err;
		}
		context_destroy(&context);

		if (sepol_user_set_mlslevel(handle, tmp_record, str) < 0) {
			free(str);
			goto err;
		}
		free(str);

		context_init(&context);
		if (mls_range_cpy(&context.range, &usrdatum->exp_range) < 0) {
			ERR(handle, "could not copy MLS range");
			context_destroy(&context);
			goto err;
		}
		if (mls_to_string(handle, policydb, &context, &str) < 0) {
			context_destroy(&context);
			goto err;
		}
		context_destroy(&context);

		if (sepol_user_set_mlsrange(handle, tmp_record, str) < 0) {
			free(str);
			goto err;
		}
		free(str);
	}

	*record = tmp_record;
	return STATUS_SUCCESS;

      err:
	/* FIXME: handle error */
	sepol_user_free(tmp_record);
	return STATUS_ERR;
}

int sepol_user_modify(sepol_handle_t * handle,
		      sepol_policydb_t * p,
		      const sepol_user_key_t * key, const sepol_user_t * user)
{

	policydb_t *policydb = &p->p;

	/* For user data */
	const char *cname, *cmls_level, *cmls_range;
	char *name = NULL;

	const char **roles = NULL;
	unsigned int num_roles = 0;

	/* Low-level representation */
	user_datum_t *usrdatum = NULL;
	role_datum_t *roldatum;
	unsigned int i;

	context_struct_t context;
	unsigned bit;
	int new = 0;

	ebitmap_node_t *rnode;

	/* First, extract all the data */
	sepol_user_key_unpack(key, &cname);

	cmls_level = sepol_user_get_mlslevel(user);
	cmls_range = sepol_user_get_mlsrange(user);

	/* Make sure that worked properly */
	if (sepol_user_get_roles(handle, user, &roles, &num_roles) < 0)
		goto err;

	/* Now, see if a user exists */
	usrdatum = hashtab_search(policydb->p_users.table, cname);

	/* If it does, we will modify it */
	if (usrdatum) {

		int value_cp = usrdatum->s.value;
		user_datum_destroy(usrdatum);
		user_datum_init(usrdatum);
		usrdatum->s.value = value_cp;

		/* Otherwise, create a new one */
	} else {
		usrdatum = (user_datum_t *) malloc(sizeof(user_datum_t));
		if (!usrdatum)
			goto omem;
		user_datum_init(usrdatum);
		new = 1;
	}

	/* For every role */
	for (i = 0; i < num_roles; i++) {

		/* Search for the role */
		roldatum = hashtab_search(policydb->p_roles.table, roles[i]);
		if (!roldatum) {
			ERR(handle, "undefined role %s for user %s",
			    roles[i], cname);
			goto err;
		}

		/* Set the role and every role it dominates */
		ebitmap_for_each_positive_bit(&roldatum->dominates, rnode, bit) {
			if (ksu_ebitmap_set_bit(&(usrdatum->roles.roles), bit, 1))
				goto omem;
		}
	}

	/* For MLS systems */
	if (policydb->mls) {

		/* MLS level */
		if (cmls_level == NULL) {
			ERR(handle, "MLS is enabled, but no MLS "
			    "default level was defined for user %s", cname);
			goto err;
		}

		context_init(&context);
		if (ksu_mls_from_string(handle, policydb, cmls_level, &context) < 0) {
			context_destroy(&context);
			goto err;
		}
		if (mls_level_cpy(&usrdatum->exp_dfltlevel,
				  &context.range.level[0]) < 0) {
			ERR(handle, "could not copy MLS level %s", cmls_level);
			context_destroy(&context);
			goto err;
		}
		context_destroy(&context);

		/* MLS range */
		if (cmls_range == NULL) {
			ERR(handle, "MLS is enabled, but no MLS"
			    "range was defined for user %s", cname);
			goto err;
		}

		context_init(&context);
		if (ksu_mls_from_string(handle, policydb, cmls_range, &context) < 0) {
			context_destroy(&context);
			goto err;
		}
		if (mls_range_cpy(&usrdatum->exp_range, &context.range) < 0) {
			ERR(handle, "could not copy MLS range %s", cmls_range);
			context_destroy(&context);
			goto err;
		}
		context_destroy(&context);
	} else if (cmls_level != NULL || cmls_range != NULL) {
		ERR(handle, "MLS is disabled, but MLS level/range "
		    "was found for user %s", cname);
		goto err;
	}

	/* If there are no errors, and this is a new user, add the user to policy */
	if (new) {
		void *tmp_ptr;

		/* Ensure reverse lookup array has enough space */
		tmp_ptr = reallocarray(policydb->user_val_to_struct,
				  policydb->p_users.nprim + 1,
				  sizeof(user_datum_t *));
		if (!tmp_ptr)
			goto omem;
		policydb->user_val_to_struct = tmp_ptr;
		policydb->user_val_to_struct[policydb->p_users.nprim] = NULL;

		tmp_ptr = reallocarray(policydb->sym_val_to_name[SYM_USERS],
				  policydb->p_users.nprim + 1,
				  sizeof(char *));
		if (!tmp_ptr)
			goto omem;
		policydb->sym_val_to_name[SYM_USERS] = tmp_ptr;
		policydb->p_user_val_to_name[policydb->p_users.nprim] = NULL;

		/* Need to copy the user name */
		name = strdup(cname);
		if (!name)
			goto omem;

		/* Store user */
		usrdatum->s.value = ++policydb->p_users.nprim;
		if (hashtab_insert(policydb->p_users.table, name,
				   (hashtab_datum_t) usrdatum) < 0)
			goto omem;

		/* Set up reverse entry */
		policydb->p_user_val_to_name[usrdatum->s.value - 1] = name;
		policydb->user_val_to_struct[usrdatum->s.value - 1] = usrdatum;
		name = NULL;

		/* Expand roles */
		if (role_set_expand(&usrdatum->roles, &usrdatum->cache,
				    policydb, NULL, NULL)) {
			ERR(handle, "unable to expand role set");
			goto err;
		}
	}

	free(roles);
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	ERR(handle, "could not load %s into policy", name);

	free(name);
	free(roles);
	if (new && usrdatum) {
		role_set_destroy(&usrdatum->roles);
		free(usrdatum);
	}
	return STATUS_ERR;
}

int sepol_user_exists(sepol_handle_t * handle __attribute__ ((unused)),
		      const sepol_policydb_t * p,
		      const sepol_user_key_t * key, int *response)
{

	const policydb_t *policydb = &p->p;

	const char *cname;
	sepol_user_key_unpack(key, &cname);

	*response = (hashtab_search(policydb->p_users.table, cname) != NULL);

	return STATUS_SUCCESS;
}

int sepol_user_count(sepol_handle_t * handle __attribute__ ((unused)),
		     const sepol_policydb_t * p, unsigned int *response)
{

	const policydb_t *policydb = &p->p;
	*response = policydb->p_users.nprim;

	return STATUS_SUCCESS;
}

int sepol_user_query(sepol_handle_t * handle,
		     const sepol_policydb_t * p,
		     const sepol_user_key_t * key, sepol_user_t ** response)
{

	const policydb_t *policydb = &p->p;
	user_datum_t *usrdatum = NULL;

	const char *cname;
	sepol_user_key_unpack(key, &cname);

	usrdatum = hashtab_search(policydb->p_users.table, cname);

	if (!usrdatum) {
		*response = NULL;
		return STATUS_SUCCESS;
	}

	if (user_to_record(handle, policydb, usrdatum->s.value - 1, response) <
	    0)
		goto err;

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not query user %s", cname);
	return STATUS_ERR;
}

int sepol_user_iterate(sepol_handle_t * handle,
		       const sepol_policydb_t * p,
		       int (*fn) (const sepol_user_t * user,
				  void *fn_arg), void *arg)
{

	const policydb_t *policydb = &p->p;
	unsigned int nusers = policydb->p_users.nprim;
	sepol_user_t *user = NULL;
	unsigned int i;

	/* For each user */
	for (i = 0; i < nusers; i++) {

		int status;

		if (user_to_record(handle, policydb, i, &user) < 0)
			goto err;

		/* Invoke handler */
		status = fn(user, arg);
		if (status < 0)
			goto err;

		sepol_user_free(user);
		user = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not iterate over users");
	sepol_user_free(user);
	return STATUS_ERR;
}

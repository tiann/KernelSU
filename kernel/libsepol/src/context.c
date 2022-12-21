// #include <stdlib.h>
#include <linux/string.h>
// #include <errno.h>

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/services.h>
#include "context_internal.h"

#include "debug.h"
#include "context.h"
#include "handle.h"
#include "mls.h"
#include "private.h"

/* ----- Compatibility ---- */
int ksu_policydb_context_isvalid(const policydb_t * p, const context_struct_t * c)
{

	return context_is_valid(p, c);
}

int sepol_check_context(const char *context)
{

	return sepol_context_to_sid(context,
				    strlen(context) + 1, NULL);
}

/* ---- End compatibility --- */

/*
 * Return 1 if the fields in the security context
 * structure `c' are valid.  Return 0 otherwise.
 */
int context_is_valid(const policydb_t * p, const context_struct_t * c)
{

	role_datum_t *role;
	user_datum_t *usrdatum;
	ebitmap_t types, roles;

	ebitmap_init(&types);
	ebitmap_init(&roles);
	if (!c->role || c->role > p->p_roles.nprim)
		return 0;

	if (!c->user || c->user > p->p_users.nprim)
		return 0;

	if (!c->type || c->type > p->p_types.nprim)
		return 0;

	if (c->role != OBJECT_R_VAL) {
		/*
		 * Role must be authorized for the type.
		 */
		role = p->role_val_to_struct[c->role - 1];
		if (!role || !ksu_ebitmap_get_bit(&role->cache, c->type - 1))
			/* role may not be associated with type */
			return 0;

		/*
		 * User must be authorized for the role.
		 */
		usrdatum = p->user_val_to_struct[c->user - 1];
		if (!usrdatum)
			return 0;

		if (!ksu_ebitmap_get_bit(&usrdatum->cache, c->role - 1))
			/* user may not be associated with role */
			return 0;
	}

	if (!ksu_mls_context_isvalid(p, c))
		return 0;

	return 1;
}

/*
 * Write the security context string representation of
 * the context structure `context' into a dynamically
 * allocated string of the correct size.  Set `*scontext'
 * to point to this string and set `*scontext_len' to
 * the length of the string.
 */
int context_to_string(sepol_handle_t * handle,
		      const policydb_t * policydb,
		      const context_struct_t * context,
		      char **result, size_t * result_len)
{

	char *scontext = NULL;
	size_t scontext_len = 0;
	char *ptr;

	/* Compute the size of the context. */
	scontext_len +=
	    strlen(policydb->p_user_val_to_name[context->user - 1]) + 1;
	scontext_len +=
	    strlen(policydb->p_role_val_to_name[context->role - 1]) + 1;
	scontext_len += strlen(policydb->p_type_val_to_name[context->type - 1]);
	scontext_len += ksu_mls_compute_context_len(policydb, context);

	/* We must null terminate the string */
	scontext_len += 1;

	/* Allocate space for the context; caller must free this space. */
	scontext = malloc(scontext_len);
	if (!scontext)
		goto omem;
	scontext[scontext_len - 1] = '\0';

	/*
	 * Copy the user name, role name and type name into the context.
	 */
	ptr = scontext;
	sprintf(ptr, "%s:%s:%s",
		policydb->p_user_val_to_name[context->user - 1],
		policydb->p_role_val_to_name[context->role - 1],
		policydb->p_type_val_to_name[context->type - 1]);

	ptr +=
	    strlen(policydb->p_user_val_to_name[context->user - 1]) + 1 +
	    strlen(policydb->p_role_val_to_name[context->role - 1]) + 1 +
	    strlen(policydb->p_type_val_to_name[context->type - 1]);

	ksu_mls_sid_to_context(policydb, context, &ptr);

	*result = scontext;
	*result_len = scontext_len;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory, could not convert " "context to string");
	free(scontext);
	return STATUS_ERR;
}

/*
 * Create a context structure from the given record
 */
int context_from_record(sepol_handle_t * handle,
			const policydb_t * policydb,
			context_struct_t ** cptr,
			const sepol_context_t * record)
{

	context_struct_t *scontext = NULL;
	user_datum_t *usrdatum;
	role_datum_t *roldatum;
	type_datum_t *typdatum;

	/* Hashtab keys are not constant - suppress warnings */
	char *user = strdup(sepol_context_get_user(record));
	char *role = strdup(sepol_context_get_role(record));
	char *type = strdup(sepol_context_get_type(record));
	const char *mls = sepol_context_get_mls(record);

	scontext = (context_struct_t *) malloc(sizeof(context_struct_t));
	if (!user || !role || !type || !scontext) {
		ERR(handle, "out of memory");
		goto err;
	}
	context_init(scontext);

	/* User */
	usrdatum = (user_datum_t *) hashtab_search(policydb->p_users.table,
						   (hashtab_key_t) user);
	if (!usrdatum) {
		ERR(handle, "user %s is not defined", user);
		goto err_destroy;
	}
	scontext->user = usrdatum->s.value;

	/* Role */
	roldatum = (role_datum_t *) hashtab_search(policydb->p_roles.table,
						   (hashtab_key_t) role);
	if (!roldatum) {
		ERR(handle, "role %s is not defined", role);
		goto err_destroy;
	}
	scontext->role = roldatum->s.value;

	/* Type */
	typdatum = (type_datum_t *) hashtab_search(policydb->p_types.table,
						   (hashtab_key_t) type);
	if (!typdatum || typdatum->flavor == TYPE_ATTRIB) {
		ERR(handle, "type %s is not defined", type);
		goto err_destroy;
	}
	scontext->type = typdatum->s.value;

	/* MLS */
	if (mls && !policydb->mls) {
		ERR(handle, "MLS is disabled, but MLS context \"%s\" found",
		    mls);
		goto err_destroy;
	} else if (!mls && policydb->mls) {
		ERR(handle, "MLS is enabled, but no MLS context found");
		goto err_destroy;
	}
	if (mls && (ksu_mls_from_string(handle, policydb, mls, scontext) < 0))
		goto err_destroy;

	/* Validity check */
	if (!context_is_valid(policydb, scontext)) {
		if (mls) {
			ERR(handle,
			    "invalid security context: \"%s:%s:%s:%s\"",
			    user, role, type, mls);
		} else {
			ERR(handle,
			    "invalid security context: \"%s:%s:%s\"",
			    user, role, type);
		}
		goto err_destroy;
	}

	*cptr = scontext;
	free(user);
	free(type);
	free(role);
	return STATUS_SUCCESS;

      err_destroy:
	// errno = EINVAL;
	context_destroy(scontext);

      err:
	free(scontext);
	free(user);
	free(type);
	free(role);
	ERR(handle, "could not create context structure");
	return STATUS_ERR;
}

/*
 * Create a record from the given context structure
 */
int context_to_record(sepol_handle_t * handle,
		      const policydb_t * policydb,
		      const context_struct_t * context,
		      sepol_context_t ** record)
{

	sepol_context_t *tmp_record = NULL;
	char *mls = NULL;

	if (sepol_context_create(handle, &tmp_record) < 0)
		goto err;

	if (sepol_context_set_user(handle, tmp_record,
				   policydb->p_user_val_to_name[context->user -
								1]) < 0)
		goto err;

	if (sepol_context_set_role(handle, tmp_record,
				   policydb->p_role_val_to_name[context->role -
								1]) < 0)
		goto err;

	if (sepol_context_set_type(handle, tmp_record,
				   policydb->p_type_val_to_name[context->type -
								1]) < 0)
		goto err;

	if (policydb->mls) {
		if (mls_to_string(handle, policydb, context, &mls) < 0)
			goto err;

		if (sepol_context_set_mls(handle, tmp_record, mls) < 0)
			goto err;
	}

	free(mls);
	*record = tmp_record;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not create context record");
	sepol_context_free(tmp_record);
	free(mls);
	return STATUS_ERR;
}

/*
 * Create a context structure from the provided string.
 */
int context_from_string(sepol_handle_t * handle,
			const policydb_t * policydb,
			context_struct_t ** cptr,
			const char *con_str, size_t con_str_len)
{

	char *con_cpy = NULL;
	sepol_context_t *ctx_record = NULL;

	if (zero_or_saturated(con_str_len)) {
		ERR(handle, "Invalid context length");
		goto err;
	}

	/* sepol_context_from_string expects a NULL-terminated string */
	con_cpy = malloc(con_str_len + 1);
	if (!con_cpy) {
		ERR(handle, "out of memory");
		goto err;
	}

	memcpy(con_cpy, con_str, con_str_len);
	con_cpy[con_str_len] = '\0';

	if (sepol_context_from_string(handle, con_cpy, &ctx_record) < 0)
		goto err;

	/* Now create from the data structure */
	if (context_from_record(handle, policydb, cptr, ctx_record) < 0)
		goto err;

	free(con_cpy);
	sepol_context_free(ctx_record);
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not create context structure");
	free(con_cpy);
	sepol_context_free(ctx_record);
	return STATUS_ERR;
}

int sepol_context_check(sepol_handle_t * handle,
			const sepol_policydb_t * policydb,
			const sepol_context_t * context)
{

	context_struct_t *con = NULL;
	int ret = context_from_record(handle, &policydb->p, &con, context);
	context_destroy(con);
	free(con);
	return ret;
}

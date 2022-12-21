#include <linux/string.h>
// #include <stdlib.h>

#include "handle.h"
#include "private.h"
#include "debug.h"

#include <sepol/booleans.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include "boolean_internal.h"

static int bool_update(sepol_handle_t * handle,
		       policydb_t * policydb,
		       const sepol_bool_key_t * key, const sepol_bool_t * data)
{

	const char *cname;
	char *name;
	int value;
	cond_bool_datum_t *datum;

	sepol_bool_key_unpack(key, &cname);
	name = strdup(cname);
	value = sepol_bool_get_value(data);

	if (!name)
		goto omem;

	datum = hashtab_search(policydb->p_bools.table, name);
	if (!datum) {
		ERR(handle, "boolean %s no longer in policy", name);
		goto err;
	}
	if (value != 0 && value != 1) {
		ERR(handle, "illegal value %d for boolean %s", value, name);
		goto err;
	}

	free(name);
	datum->state = value;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	free(name);
	ERR(handle, "could not update boolean %s", cname);
	return STATUS_ERR;
}

static int bool_to_record(sepol_handle_t * handle,
			  const policydb_t * policydb,
			  int bool_idx, sepol_bool_t ** record)
{

	const char *name = policydb->p_bool_val_to_name[bool_idx];
	cond_bool_datum_t *booldatum = policydb->bool_val_to_struct[bool_idx];
	int value = booldatum->state;

	sepol_bool_t *tmp_record = NULL;

	if (sepol_bool_create(handle, &tmp_record) < 0)
		goto err;

	if (sepol_bool_set_name(handle, tmp_record, name) < 0)
		goto err;

	sepol_bool_set_value(tmp_record, value);

	*record = tmp_record;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not convert boolean %s to record", name);
	sepol_bool_free(tmp_record);
	return STATUS_ERR;
}

int sepol_bool_set(sepol_handle_t * handle,
		   sepol_policydb_t * p,
		   const sepol_bool_key_t * key, const sepol_bool_t * data)
{

	policydb_t *policydb = &p->p;
	const char *name;
	sepol_bool_key_unpack(key, &name);

	if (bool_update(handle, policydb, key, data) < 0)
		goto err;

	if (evaluate_conds(policydb) < 0) {
		ERR(handle, "error while re-evaluating conditionals");
		goto err;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not set boolean %s", name);
	return STATUS_ERR;
}

int sepol_bool_count(sepol_handle_t * handle __attribute__ ((unused)),
		     const sepol_policydb_t * p, unsigned int *response)
{

	const policydb_t *policydb = &p->p;
	*response = policydb->p_bools.nprim;

	return STATUS_SUCCESS;
}

int sepol_bool_exists(sepol_handle_t * handle,
		      const sepol_policydb_t * p,
		      const sepol_bool_key_t * key, int *response)
{

	const policydb_t *policydb = &p->p;

	const char *cname;
	char *name = NULL;
	sepol_bool_key_unpack(key, &cname);
	name = strdup(cname);

	if (!name) {
		ERR(handle, "out of memory, could not check "
		    "if user %s exists", cname);
		return STATUS_ERR;
	}

	*response = (hashtab_search(policydb->p_bools.table, name) != NULL);
	free(name);
	return STATUS_SUCCESS;
}

int sepol_bool_query(sepol_handle_t * handle,
		     const sepol_policydb_t * p,
		     const sepol_bool_key_t * key, sepol_bool_t ** response)
{

	const policydb_t *policydb = &p->p;
	cond_bool_datum_t *booldatum = NULL;

	const char *cname;
	char *name = NULL;
	sepol_bool_key_unpack(key, &cname);
	name = strdup(cname);

	if (!name)
		goto omem;

	booldatum = hashtab_search(policydb->p_bools.table, name);
	if (!booldatum) {
		*response = NULL;
		free(name);
		return STATUS_SUCCESS;
	}

	if (bool_to_record(handle, policydb,
			   booldatum->s.value - 1, response) < 0)
		goto err;

	free(name);
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	ERR(handle, "could not query boolean %s", cname);
	free(name);
	return STATUS_ERR;
}

int sepol_bool_iterate(sepol_handle_t * handle,
		       const sepol_policydb_t * p,
		       int (*fn) (const sepol_bool_t * boolean,
				  void *fn_arg), void *arg)
{

	const policydb_t *policydb = &p->p;
	unsigned int nbools = policydb->p_bools.nprim;
	sepol_bool_t *boolean = NULL;
	unsigned int i;

	/* For each boolean */
	for (i = 0; i < nbools; i++) {

		int status;

		if (bool_to_record(handle, policydb, i, &boolean) < 0)
			goto err;

		/* Invoke handler */
		status = fn(boolean, arg);
		if (status < 0)
			goto err;

		sepol_bool_free(boolean);
		boolean = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not iterate over booleans");
	sepol_bool_free(boolean);
	return STATUS_ERR;
}

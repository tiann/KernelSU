#include <netinet/in.h>
#include <stdlib.h>
#include <inttypes.h>

#include "debug.h"
#include "context.h"
#include "handle.h"

#include <sepol/ibpkey_record.h>
#include <sepol/policydb/policydb.h>
#include "ibpkey_internal.h"

/* Create a low level ibpkey structure from
 * a high level representation
 */
static int ibpkey_from_record(sepol_handle_t *handle,
			      const policydb_t *policydb,
			      ocontext_t **ibpkey, const sepol_ibpkey_t *data)
{
	ocontext_t *tmp_ibpkey = NULL;
	context_struct_t *tmp_con = NULL;
	char *subnet_prefix_buf = NULL;
	int low = sepol_ibpkey_get_low(data);
	int high = sepol_ibpkey_get_high(data);

	tmp_ibpkey = (ocontext_t *)calloc(1, sizeof(*tmp_ibpkey));
	if (!tmp_ibpkey)
		goto omem;

	tmp_ibpkey->u.ibpkey.subnet_prefix = sepol_ibpkey_get_subnet_prefix_bytes(data);

	/* Pkey range */
	tmp_ibpkey->u.ibpkey.low_pkey = low;
	tmp_ibpkey->u.ibpkey.high_pkey = high;
	if (tmp_ibpkey->u.ibpkey.low_pkey > tmp_ibpkey->u.ibpkey.high_pkey) {
		ERR(handle, "low ibpkey %d exceeds high ibpkey %d",
		    tmp_ibpkey->u.ibpkey.low_pkey, tmp_ibpkey->u.ibpkey.high_pkey);
		goto err;
	}

	/* Context */
	if (context_from_record(handle, policydb, &tmp_con,
				sepol_ibpkey_get_con(data)) < 0)
		goto err;
	context_cpy(&tmp_ibpkey->context[0], tmp_con);
	context_destroy(tmp_con);
	free(tmp_con);
	tmp_con = NULL;

	*ibpkey = tmp_ibpkey;
	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");

err:
	if (tmp_ibpkey) {
		context_destroy(&tmp_ibpkey->context[0]);
		free(tmp_ibpkey);
	}
	context_destroy(tmp_con);
	free(tmp_con);
	free(subnet_prefix_buf);
	ERR(handle, "could not create ibpkey structure");
	return STATUS_ERR;
}

static int ibpkey_to_record(sepol_handle_t *handle,
			    const policydb_t *policydb,
			    ocontext_t *ibpkey, sepol_ibpkey_t **record)
{
	context_struct_t *con = &ibpkey->context[0];
	sepol_context_t *tmp_con = NULL;
	sepol_ibpkey_t *tmp_record = NULL;

	if (sepol_ibpkey_create(handle, &tmp_record) < 0)
		goto err;

	sepol_ibpkey_set_subnet_prefix_bytes(tmp_record,
					     ibpkey->u.ibpkey.subnet_prefix);

	sepol_ibpkey_set_range(tmp_record, ibpkey->u.ibpkey.low_pkey,
			       ibpkey->u.ibpkey.high_pkey);

	if (context_to_record(handle, policydb, con, &tmp_con) < 0)
		goto err;

	if (sepol_ibpkey_set_con(handle, tmp_record, tmp_con) < 0)
		goto err;

	sepol_context_free(tmp_con);
	*record = tmp_record;
	return STATUS_SUCCESS;

err:
	ERR(handle, "could not convert ibpkey to record");
	sepol_context_free(tmp_con);
	sepol_ibpkey_free(tmp_record);
	return STATUS_ERR;
}

/* Return the number of ibpkeys */
extern int sepol_ibpkey_count(sepol_handle_t *handle __attribute__ ((unused)),
			      const sepol_policydb_t *p, unsigned int *response)
{
	unsigned int count = 0;
	ocontext_t *c, *head;
	const policydb_t *policydb = &p->p;

	head = policydb->ocontexts[OCON_IBPKEY];
	for (c = head; c; c = c->next)
		count++;

	*response = count;

	return STATUS_SUCCESS;
}

/* Check if a ibpkey exists */
int sepol_ibpkey_exists(sepol_handle_t *handle __attribute__ ((unused)),
			const sepol_policydb_t *p,
			const sepol_ibpkey_key_t *key, int *response)
{
	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	int low, high;
	uint64_t subnet_prefix;

	sepol_ibpkey_key_unpack(key, &subnet_prefix, &low, &high);

	head = policydb->ocontexts[OCON_IBPKEY];
	for (c = head; c; c = c->next) {
		uint64_t subnet_prefix2 = c->u.ibpkey.subnet_prefix;
		uint16_t low2 = c->u.ibpkey.low_pkey;
		uint16_t high2 = c->u.ibpkey.high_pkey;

		if (low2 == low &&
		    high2 == high &&
		    subnet_prefix == subnet_prefix2) {
			*response = 1;
			return STATUS_SUCCESS;
		}
	}

	*response = 0;
	return STATUS_SUCCESS;
}

/* Query a ibpkey */
int sepol_ibpkey_query(sepol_handle_t *handle,
		       const sepol_policydb_t *p,
		       const sepol_ibpkey_key_t *key, sepol_ibpkey_t **response)
{
	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	int low, high;
	uint64_t subnet_prefix;

	sepol_ibpkey_key_unpack(key, &subnet_prefix, &low, &high);

	head = policydb->ocontexts[OCON_IBPKEY];
	for (c = head; c; c = c->next) {
		uint64_t subnet_prefix2 = c->u.ibpkey.subnet_prefix;
		int low2 = c->u.ibpkey.low_pkey;
		int high2 = c->u.ibpkey.high_pkey;

		if (low2 == low &&
		    high2 == high &&
		    subnet_prefix == subnet_prefix2) {
			if (ibpkey_to_record(handle, policydb, c, response) < 0)
				goto err;
			return STATUS_SUCCESS;
		}
	}

	*response = NULL;
	return STATUS_SUCCESS;

err:
	ERR(handle, "could not query ibpkey subnet prefix: %#" PRIx64 " range %u - %u exists",
	    subnet_prefix, low, high);
	return STATUS_ERR;
}

/* Load a ibpkey into policy */
int sepol_ibpkey_modify(sepol_handle_t *handle,
			sepol_policydb_t *p,
			const sepol_ibpkey_key_t *key, const sepol_ibpkey_t *data)
{
	policydb_t *policydb = &p->p;
	ocontext_t *ibpkey = NULL;
	int low, high;
	uint64_t subnet_prefix;

	sepol_ibpkey_key_unpack(key, &subnet_prefix, &low, &high);

	if (ibpkey_from_record(handle, policydb, &ibpkey, data) < 0)
		goto err;

	/* Attach to context list */
	ibpkey->next = policydb->ocontexts[OCON_IBPKEY];
	policydb->ocontexts[OCON_IBPKEY] = ibpkey;

	return STATUS_SUCCESS;

err:
	ERR(handle, "could not load ibpkey subnet prefix: %#" PRIx64 " range %u - %u exists",
	    subnet_prefix, low, high);
	if (ibpkey) {
		context_destroy(&ibpkey->context[0]);
		free(ibpkey);
	}
	return STATUS_ERR;
}

int sepol_ibpkey_iterate(sepol_handle_t *handle,
			 const sepol_policydb_t *p,
			 int (*fn)(const sepol_ibpkey_t *ibpkey,
				   void *fn_arg), void *arg)
{
	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	sepol_ibpkey_t *ibpkey = NULL;

	head = policydb->ocontexts[OCON_IBPKEY];
	for (c = head; c; c = c->next) {
		int status;

		if (ibpkey_to_record(handle, policydb, c, &ibpkey) < 0)
			goto err;

		/* Invoke handler */
		status = fn(ibpkey, arg);
		if (status < 0)
			goto err;

		sepol_ibpkey_free(ibpkey);
		ibpkey = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

err:
	ERR(handle, "could not iterate over ibpkeys");
	sepol_ibpkey_free(ibpkey);
	return STATUS_ERR;
}

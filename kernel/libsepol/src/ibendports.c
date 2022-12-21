#include <netinet/in.h>
#include <stdlib.h>

#include "debug.h"
#include "context.h"
#include "handle.h"

#include <sepol/policydb/policydb.h>
#include "ibendport_internal.h"

/* Create a low level ibendport structure from
 * a high level representation
 */
static int ibendport_from_record(sepol_handle_t *handle,
				 const policydb_t *policydb,
				 ocontext_t **ibendport,
				 const sepol_ibendport_t *data)
{
	ocontext_t *tmp_ibendport = NULL;
	context_struct_t *tmp_con = NULL;
	char *ibdev_name = NULL;
	int port = sepol_ibendport_get_port(data);

	tmp_ibendport = (ocontext_t *)calloc(1, sizeof(ocontext_t));
	if (!tmp_ibendport)
		goto omem;

	if (sepol_ibendport_alloc_ibdev_name(handle,
					     &tmp_ibendport->u.ibendport.dev_name) < 0)
		goto omem;

	if (sepol_ibendport_get_ibdev_name(handle,
					   data,
					   &ibdev_name) < 0)
		goto err;

	strncpy(tmp_ibendport->u.ibendport.dev_name, ibdev_name, IB_DEVICE_NAME_MAX - 1);

	free(ibdev_name);
	ibdev_name = NULL;

	tmp_ibendport->u.ibendport.port = port;

	/* Context */
	if (context_from_record(handle, policydb, &tmp_con,
				sepol_ibendport_get_con(data)) < 0)
		goto err;
	context_cpy(&tmp_ibendport->context[0], tmp_con);
	context_destroy(tmp_con);
	free(tmp_con);
	tmp_con = NULL;

	*ibendport = tmp_ibendport;
	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");

err:
	if (tmp_ibendport) {
		context_destroy(&tmp_ibendport->context[0]);
		free(tmp_ibendport);
	}
	context_destroy(tmp_con);
	free(tmp_con);
	free(ibdev_name);
	ERR(handle, "could not create ibendport structure");
	return STATUS_ERR;
}

static int ibendport_to_record(sepol_handle_t *handle,
			       const policydb_t *policydb,
			       ocontext_t *ibendport,
			       sepol_ibendport_t **record)
{
	int port = ibendport->u.ibendport.port;
	context_struct_t *con = &ibendport->context[0];

	sepol_context_t *tmp_con = NULL;
	sepol_ibendport_t *tmp_record = NULL;

	if (sepol_ibendport_create(handle, &tmp_record) < 0)
		goto err;

	if (sepol_ibendport_set_ibdev_name(handle, tmp_record,
					   ibendport->u.ibendport.dev_name) < 0)
		goto err;

	sepol_ibendport_set_port(tmp_record, port);

	if (context_to_record(handle, policydb, con, &tmp_con) < 0)
		goto err;

	if (sepol_ibendport_set_con(handle, tmp_record, tmp_con) < 0)
		goto err;

	sepol_context_free(tmp_con);
	*record = tmp_record;
	return STATUS_SUCCESS;

err:
	ERR(handle, "could not convert ibendport to record");
	sepol_context_free(tmp_con);
	sepol_ibendport_free(tmp_record);
	return STATUS_ERR;
}

/* Return the number of ibendports */
extern int sepol_ibendport_count(sepol_handle_t *handle __attribute__ ((unused)),
				 const sepol_policydb_t *p, unsigned int *response)
{
	unsigned int count = 0;
	ocontext_t *c, *head;
	const policydb_t *policydb = &p->p;

	head = policydb->ocontexts[OCON_IBENDPORT];
	for (c = head; c; c = c->next)
		count++;

	*response = count;

	return STATUS_SUCCESS;
}

/* Check if a ibendport exists */
int sepol_ibendport_exists(sepol_handle_t *handle __attribute__ ((unused)),
			   const sepol_policydb_t *p,
			   const sepol_ibendport_key_t *key, int *response)
{
	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	int port;
	const char *ibdev_name;

	sepol_ibendport_key_unpack(key, &ibdev_name, &port);

	head = policydb->ocontexts[OCON_IBENDPORT];
	for (c = head; c; c = c->next) {
		const char *ibdev_name2 = c->u.ibendport.dev_name;
		int port2 = c->u.ibendport.port;

		if (port2 == port &&
		    (!strcmp(ibdev_name, ibdev_name2))) {
			*response = 1;
			return STATUS_SUCCESS;
		}
	}

	*response = 0;
	return STATUS_SUCCESS;
}

int sepol_ibendport_query(sepol_handle_t *handle,
			  const sepol_policydb_t *p,
			  const sepol_ibendport_key_t *key,
			  sepol_ibendport_t **response)
{
	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	int port;
	const char *ibdev_name;

	sepol_ibendport_key_unpack(key, &ibdev_name, &port);

	head = policydb->ocontexts[OCON_IBENDPORT];
	for (c = head; c; c = c->next) {
		const char *ibdev_name2 = c->u.ibendport.dev_name;
		int port2 = c->u.ibendport.port;

		if (port2 == port &&
		    (!strcmp(ibdev_name, ibdev_name2))) {
			if (ibendport_to_record(handle, policydb, c, response) < 0)
				goto err;
			return STATUS_SUCCESS;
		}
	}

	*response = NULL;
	return STATUS_SUCCESS;

err:
	ERR(handle, "could not query ibendport, IB device: %s port %u",
	    ibdev_name, port);
	return STATUS_ERR;
}

/* Load a ibendport into policy */
int sepol_ibendport_modify(sepol_handle_t *handle,
			   sepol_policydb_t *p,
			   const sepol_ibendport_key_t *key,
			   const sepol_ibendport_t *data)
{
	policydb_t *policydb = &p->p;
	ocontext_t *ibendport = NULL;
	int port;
	const char *ibdev_name;

	sepol_ibendport_key_unpack(key, &ibdev_name, &port);

	if (ibendport_from_record(handle, policydb, &ibendport, data) < 0)
		goto err;

	/* Attach to context list */
	ibendport->next = policydb->ocontexts[OCON_IBENDPORT];
	policydb->ocontexts[OCON_IBENDPORT] = ibendport;

	return STATUS_SUCCESS;

err:
	ERR(handle, "could not load ibendport %s/%d",
	    ibdev_name, port);
	if (ibendport) {
		context_destroy(&ibendport->context[0]);
		free(ibendport);
	}
	return STATUS_ERR;
}

int sepol_ibendport_iterate(sepol_handle_t *handle,
			    const sepol_policydb_t *p,
			    int (*fn)(const sepol_ibendport_t *ibendport,
				      void *fn_arg), void *arg)
{
	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	sepol_ibendport_t *ibendport = NULL;

	head = policydb->ocontexts[OCON_IBENDPORT];
	for (c = head; c; c = c->next) {
		int status;

		if (ibendport_to_record(handle, policydb, c, &ibendport) < 0)
			goto err;

		/* Invoke handler */
		status = fn(ibendport, arg);
		if (status < 0)
			goto err;

		sepol_ibendport_free(ibendport);
		ibendport = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

err:
	ERR(handle, "could not iterate over ibendports");
	sepol_ibendport_free(ibendport);
	return STATUS_ERR;
}

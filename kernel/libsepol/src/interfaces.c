#include <stdlib.h>

#include "debug.h"
#include "context.h"
#include "handle.h"

#include <sepol/policydb/policydb.h>
#include <sepol/interfaces.h>
#include "iface_internal.h"

/* Create a low level structure from record */
static int iface_from_record(sepol_handle_t * handle,
			     const policydb_t * policydb,
			     ocontext_t ** iface, const sepol_iface_t * record)
{

	ocontext_t *tmp_iface = NULL;
	context_struct_t *tmp_con = NULL;

	tmp_iface = (ocontext_t *) calloc(1, sizeof(ocontext_t));
	if (!tmp_iface)
		goto omem;

	/* Name */
	tmp_iface->u.name = strdup(sepol_iface_get_name(record));
	if (!tmp_iface->u.name)
		goto omem;

	/* Interface Context */
	if (context_from_record(handle, policydb,
				&tmp_con, sepol_iface_get_ifcon(record)) < 0)
		goto err;
	context_cpy(&tmp_iface->context[0], tmp_con);
	context_destroy(tmp_con);
	free(tmp_con);
	tmp_con = NULL;

	/* Message Context */
	if (context_from_record(handle, policydb,
				&tmp_con, sepol_iface_get_msgcon(record)) < 0)
		goto err;
	context_cpy(&tmp_iface->context[1], tmp_con);
	context_destroy(tmp_con);
	free(tmp_con);
	tmp_con = NULL;

	*iface = tmp_iface;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	if (tmp_iface != NULL) {
		free(tmp_iface->u.name);
		context_destroy(&tmp_iface->context[0]);
		context_destroy(&tmp_iface->context[1]);
		free(tmp_iface);
	}
	context_destroy(tmp_con);
	free(tmp_con);
	ERR(handle, "error creating interface structure");
	return STATUS_ERR;
}

static int iface_to_record(sepol_handle_t * handle,
			   const policydb_t * policydb,
			   ocontext_t * iface, sepol_iface_t ** record)
{

	char *name = iface->u.name;
	context_struct_t *ifcon = &iface->context[0];
	context_struct_t *msgcon = &iface->context[1];

	sepol_context_t *tmp_con = NULL;
	sepol_iface_t *tmp_record = NULL;

	if (sepol_iface_create(handle, &tmp_record) < 0)
		goto err;

	if (sepol_iface_set_name(handle, tmp_record, name) < 0)
		goto err;

	if (context_to_record(handle, policydb, ifcon, &tmp_con) < 0)
		goto err;
	if (sepol_iface_set_ifcon(handle, tmp_record, tmp_con) < 0)
		goto err;
	sepol_context_free(tmp_con);
	tmp_con = NULL;

	if (context_to_record(handle, policydb, msgcon, &tmp_con) < 0)
		goto err;
	if (sepol_iface_set_msgcon(handle, tmp_record, tmp_con) < 0)
		goto err;
	sepol_context_free(tmp_con);
	tmp_con = NULL;

	*record = tmp_record;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not convert interface %s to record", name);
	sepol_context_free(tmp_con);
	sepol_iface_free(tmp_record);
	return STATUS_ERR;
}

/* Check if an interface exists */
int sepol_iface_exists(sepol_handle_t * handle __attribute__ ((unused)),
		       const sepol_policydb_t * p,
		       const sepol_iface_key_t * key, int *response)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;

	const char *name;
	sepol_iface_key_unpack(key, &name);

	head = policydb->ocontexts[OCON_NETIF];
	for (c = head; c; c = c->next) {
		if (!strcmp(name, c->u.name)) {
			*response = 1;
			return STATUS_SUCCESS;
		}
	}
	*response = 0;

	return STATUS_SUCCESS;
}

/* Query an interface */
int sepol_iface_query(sepol_handle_t * handle,
		      const sepol_policydb_t * p,
		      const sepol_iface_key_t * key, sepol_iface_t ** response)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;

	const char *name;
	sepol_iface_key_unpack(key, &name);

	head = policydb->ocontexts[OCON_NETIF];
	for (c = head; c; c = c->next) {
		if (!strcmp(name, c->u.name)) {

			if (iface_to_record(handle, policydb, c, response) < 0)
				goto err;

			return STATUS_SUCCESS;
		}
	}

	*response = NULL;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not query interface %s", name);
	return STATUS_ERR;
}

/* Load an interface into policy */
int sepol_iface_modify(sepol_handle_t * handle,
		       sepol_policydb_t * p,
		       const sepol_iface_key_t * key,
		       const sepol_iface_t * data)
{

	policydb_t *policydb = &p->p;
	ocontext_t *head, *prev, *c, *iface = NULL;

	const char *name;
	sepol_iface_key_unpack(key, &name);

	if (iface_from_record(handle, policydb, &iface, data) < 0)
		goto err;

	prev = NULL;
	head = policydb->ocontexts[OCON_NETIF];
	for (c = head; c; c = c->next) {
		if (!strcmp(name, c->u.name)) {

			/* Replace */
			iface->next = c->next;
			if (prev == NULL)
				policydb->ocontexts[OCON_NETIF] = iface;
			else
				prev->next = iface;
			free(c->u.name);
			context_destroy(&c->context[0]);
			context_destroy(&c->context[1]);
			free(c);

			return STATUS_SUCCESS;
		}
		prev = c;
	}

	/* Attach to context list */
	iface->next = policydb->ocontexts[OCON_NETIF];
	policydb->ocontexts[OCON_NETIF] = iface;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "error while loading interface %s", name);

	if (iface != NULL) {
		free(iface->u.name);
		context_destroy(&iface->context[0]);
		context_destroy(&iface->context[1]);
		free(iface);
	}
	return STATUS_ERR;
}

/* Return the number of interfaces */
extern int sepol_iface_count(sepol_handle_t * handle __attribute__ ((unused)),
			     const sepol_policydb_t * p, unsigned int *response)
{

	unsigned int count = 0;
	ocontext_t *c, *head;
	const policydb_t *policydb = &p->p;

	head = policydb->ocontexts[OCON_NETIF];
	for (c = head; c != NULL; c = c->next)
		count++;

	*response = count;

	return STATUS_SUCCESS;
}

int sepol_iface_iterate(sepol_handle_t * handle,
			const sepol_policydb_t * p,
			int (*fn) (const sepol_iface_t * iface,
				   void *fn_arg), void *arg)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	sepol_iface_t *iface = NULL;

	head = policydb->ocontexts[OCON_NETIF];
	for (c = head; c; c = c->next) {
		int status;

		if (iface_to_record(handle, policydb, c, &iface) < 0)
			goto err;

		/* Invoke handler */
		status = fn(iface, arg);
		if (status < 0)
			goto err;

		sepol_iface_free(iface);
		iface = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not iterate over interfaces");
	sepol_iface_free(iface);
	return STATUS_ERR;
}

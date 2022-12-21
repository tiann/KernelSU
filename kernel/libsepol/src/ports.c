// #include <netinet/in.h>
#include <linux/in.h>
#ifndef IPPROTO_DCCP
#define IPPROTO_DCCP 33
#endif
#ifndef IPPROTO_SCTP
#define IPPROTO_SCTP 132
#endif
// #include <stdlib.h>

#include "debug.h"
#include "context.h"
#include "handle.h"

#include <sepol/policydb/policydb.h>
#include "port_internal.h"

static inline int sepol2ipproto(sepol_handle_t * handle, int proto)
{

	switch (proto) {
	case SEPOL_PROTO_TCP:
		return IPPROTO_TCP;
	case SEPOL_PROTO_UDP:
		return IPPROTO_UDP;
	case SEPOL_PROTO_DCCP:
		return IPPROTO_DCCP;
	case SEPOL_PROTO_SCTP:
		return IPPROTO_SCTP;
	default:
		ERR(handle, "unsupported protocol %u", proto);
		return STATUS_ERR;
	}
}

static inline int ipproto2sepol(sepol_handle_t * handle, int proto)
{

	switch (proto) {
	case IPPROTO_TCP:
		return SEPOL_PROTO_TCP;
	case IPPROTO_UDP:
		return SEPOL_PROTO_UDP;
	case IPPROTO_DCCP:
		return SEPOL_PROTO_DCCP;
	case IPPROTO_SCTP:
		return SEPOL_PROTO_SCTP;
	default:
		ERR(handle, "invalid protocol %u " "found in policy", proto);
		return STATUS_ERR;
	}
}

/* Create a low level port structure from
 * a high level representation */
static int port_from_record(sepol_handle_t * handle,
			    const policydb_t * policydb,
			    ocontext_t ** port, const sepol_port_t * data)
{

	ocontext_t *tmp_port = NULL;
	context_struct_t *tmp_con = NULL;
	int tmp_proto;

	int low = sepol_port_get_low(data);
	int high = sepol_port_get_high(data);
	int proto = sepol_port_get_proto(data);

	tmp_port = (ocontext_t *) calloc(1, sizeof(ocontext_t));
	if (!tmp_port)
		goto omem;

	/* Process protocol */
	tmp_proto = sepol2ipproto(handle, proto);
	if (tmp_proto < 0)
		goto err;
	tmp_port->u.port.protocol = tmp_proto;

	/* Port range */
	tmp_port->u.port.low_port = low;
	tmp_port->u.port.high_port = high;
	if (tmp_port->u.port.low_port > tmp_port->u.port.high_port) {
		ERR(handle, "low port %d exceeds high port %d",
		    tmp_port->u.port.low_port, tmp_port->u.port.high_port);
		goto err;
	}

	/* Context */
	if (context_from_record(handle, policydb, &tmp_con,
				sepol_port_get_con(data)) < 0)
		goto err;
	context_cpy(&tmp_port->context[0], tmp_con);
	context_destroy(tmp_con);
	free(tmp_con);
	tmp_con = NULL;

	*port = tmp_port;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	if (tmp_port != NULL) {
		context_destroy(&tmp_port->context[0]);
		free(tmp_port);
	}
	context_destroy(tmp_con);
	free(tmp_con);
	ERR(handle, "could not create port structure for range %u:%u (%s)",
	    low, high, sepol_port_get_proto_str(proto));
	return STATUS_ERR;
}

static int port_to_record(sepol_handle_t * handle,
			  const policydb_t * policydb,
			  ocontext_t * port, sepol_port_t ** record)
{

	int proto = port->u.port.protocol;
	int low = port->u.port.low_port;
	int high = port->u.port.high_port;
	context_struct_t *con = &port->context[0];
	int rec_proto = -1;

	sepol_context_t *tmp_con = NULL;
	sepol_port_t *tmp_record = NULL;

	if (sepol_port_create(handle, &tmp_record) < 0)
		goto err;

	rec_proto = ipproto2sepol(handle, proto);
	if (rec_proto < 0)
		goto err;

	sepol_port_set_proto(tmp_record, rec_proto);
	sepol_port_set_range(tmp_record, low, high);

	if (context_to_record(handle, policydb, con, &tmp_con) < 0)
		goto err;

	if (sepol_port_set_con(handle, tmp_record, tmp_con) < 0)
		goto err;

	sepol_context_free(tmp_con);
	*record = tmp_record;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not convert port range %u - %u (%s) "
	    "to record", low, high, sepol_port_get_proto_str(rec_proto));
	sepol_context_free(tmp_con);
	sepol_port_free(tmp_record);
	return STATUS_ERR;
}

/* Return the number of ports */
extern int sepol_port_count(sepol_handle_t * handle __attribute__ ((unused)),
			    const sepol_policydb_t * p, unsigned int *response)
{

	unsigned int count = 0;
	ocontext_t *c, *head;
	const policydb_t *policydb = &p->p;

	head = policydb->ocontexts[OCON_PORT];
	for (c = head; c != NULL; c = c->next)
		count++;

	*response = count;

	return STATUS_SUCCESS;
}

/* Check if a port exists */
int sepol_port_exists(sepol_handle_t * handle,
		      const sepol_policydb_t * p,
		      const sepol_port_key_t * key, int *response)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;

	int low, high, proto;
	const char *proto_str;
	sepol_port_key_unpack(key, &low, &high, &proto);
	proto_str = sepol_port_get_proto_str(proto);
	proto = sepol2ipproto(handle, proto);
	if (proto < 0)
		goto err;

	head = policydb->ocontexts[OCON_PORT];
	for (c = head; c; c = c->next) {
		int proto2 = c->u.port.protocol;
		int low2 = c->u.port.low_port;
		int high2 = c->u.port.high_port;

		if (proto == proto2 && low2 == low && high2 == high) {
			*response = 1;
			return STATUS_SUCCESS;
		}
	}

	*response = 0;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not check if port range %u - %u (%s) exists",
	    low, high, proto_str);
	return STATUS_ERR;
}

/* Query a port */
int sepol_port_query(sepol_handle_t * handle,
		     const sepol_policydb_t * p,
		     const sepol_port_key_t * key, sepol_port_t ** response)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;

	int low, high, proto;
	const char *proto_str;
	sepol_port_key_unpack(key, &low, &high, &proto);
	proto_str = sepol_port_get_proto_str(proto);
	proto = sepol2ipproto(handle, proto);
	if (proto < 0)
		goto err;

	head = policydb->ocontexts[OCON_PORT];
	for (c = head; c; c = c->next) {
		int proto2 = c->u.port.protocol;
		int low2 = c->u.port.low_port;
		int high2 = c->u.port.high_port;

		if (proto == proto2 && low2 == low && high2 == high) {
			if (port_to_record(handle, policydb, c, response) < 0)
				goto err;
			return STATUS_SUCCESS;
		}
	}

	*response = NULL;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not query port range %u - %u (%s)",
	    low, high, proto_str);
	return STATUS_ERR;

}

/* Load a port into policy */
int sepol_port_modify(sepol_handle_t * handle,
		      sepol_policydb_t * p,
		      const sepol_port_key_t * key, const sepol_port_t * data)
{

	policydb_t *policydb = &p->p;
	ocontext_t *port = NULL;

	int low, high, proto;
	const char *proto_str;

	sepol_port_key_unpack(key, &low, &high, &proto);
	proto_str = sepol_port_get_proto_str(proto);
	proto = sepol2ipproto(handle, proto);
	if (proto < 0)
		goto err;

	if (port_from_record(handle, policydb, &port, data) < 0)
		goto err;

	/* Attach to context list */
	port->next = policydb->ocontexts[OCON_PORT];
	policydb->ocontexts[OCON_PORT] = port;

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not load port range %u - %u (%s)",
	    low, high, proto_str);
	if (port != NULL) {
		context_destroy(&port->context[0]);
		free(port);
	}
	return STATUS_ERR;
}

int sepol_port_iterate(sepol_handle_t * handle,
		       const sepol_policydb_t * p,
		       int (*fn) (const sepol_port_t * port,
				  void *fn_arg), void *arg)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	sepol_port_t *port = NULL;

	head = policydb->ocontexts[OCON_PORT];
	for (c = head; c; c = c->next) {
		int status;

		if (port_to_record(handle, policydb, c, &port) < 0)
			goto err;

		/* Invoke handler */
		status = fn(port, arg);
		if (status < 0)
			goto err;

		sepol_port_free(port);
		port = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not iterate over ports");
	sepol_port_free(port);
	return STATUS_ERR;
}

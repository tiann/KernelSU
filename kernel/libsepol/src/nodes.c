// #include <netinet/in.h>
#include <linux/in.h>
// #include <arpa/inet.h>
#include <linux/inet.h>
// #include <stdlib.h>

#include "debug.h"
#include "context.h"
#include "handle.h"

#include <sepol/policydb/policydb.h>
#include "node_internal.h"

/* Create a low level node structure from
 * a high level representation */
static int node_from_record(sepol_handle_t * handle,
			    const policydb_t * policydb,
			    ocontext_t ** node, const sepol_node_t * data)
{

	ocontext_t *tmp_node = NULL;
	context_struct_t *tmp_con = NULL;
	char *addr_buf = NULL, *mask_buf = NULL;
	size_t addr_bsize, mask_bsize;
	int proto;

	tmp_node = (ocontext_t *) calloc(1, sizeof(ocontext_t));
	if (!tmp_node)
		goto omem;

	/* Address and netmask */
	if (sepol_node_get_addr_bytes(handle, data, &addr_buf, &addr_bsize) < 0)
		goto err;
	if (sepol_node_get_mask_bytes(handle, data, &mask_buf, &mask_bsize) < 0)
		goto err;

	proto = sepol_node_get_proto(data);

	switch (proto) {
	case SEPOL_PROTO_IP4:
		memcpy(&tmp_node->u.node.addr, addr_buf, addr_bsize);
		memcpy(&tmp_node->u.node.mask, mask_buf, mask_bsize);
		break;
	case SEPOL_PROTO_IP6:
		memcpy(tmp_node->u.node6.addr, addr_buf, addr_bsize);
		memcpy(tmp_node->u.node6.mask, mask_buf, mask_bsize);
		break;
	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}
	free(addr_buf);
	free(mask_buf);
	addr_buf = NULL;
	mask_buf = NULL;

	/* Context */
	if (context_from_record(handle, policydb, &tmp_con,
				sepol_node_get_con(data)) < 0)
		goto err;
	context_cpy(&tmp_node->context[0], tmp_con);
	context_destroy(tmp_con);
	free(tmp_con);
	tmp_con = NULL;

	*node = tmp_node;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	if (tmp_node != NULL) {
		context_destroy(&tmp_node->context[0]);
		free(tmp_node);
	}
	context_destroy(tmp_con);
	free(tmp_con);
	free(addr_buf);
	free(mask_buf);
	ERR(handle, "could not create node structure");
	return STATUS_ERR;
}

static int node_to_record(sepol_handle_t * handle,
			  const policydb_t * policydb,
			  ocontext_t * node, int proto, sepol_node_t ** record)
{

	context_struct_t *con = &node->context[0];

	sepol_context_t *tmp_con = NULL;
	sepol_node_t *tmp_record = NULL;

	if (sepol_node_create(handle, &tmp_record) < 0)
		goto err;

	sepol_node_set_proto(tmp_record, proto);

	switch (proto) {

	case SEPOL_PROTO_IP4:
		if (sepol_node_set_addr_bytes(handle, tmp_record,
					      (const char *)&node->u.node.addr,
					      4) < 0)
			goto err;

		if (sepol_node_set_mask_bytes(handle, tmp_record,
					      (const char *)&node->u.node.mask,
					      4) < 0)
			goto err;
		break;

	case SEPOL_PROTO_IP6:
		if (sepol_node_set_addr_bytes(handle, tmp_record,
					      (const char *)&node->u.node6.addr,
					      16) < 0)
			goto err;

		if (sepol_node_set_mask_bytes(handle, tmp_record,
					      (const char *)&node->u.node6.mask,
					      16) < 0)
			goto err;
		break;

	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}

	if (context_to_record(handle, policydb, con, &tmp_con) < 0)
		goto err;

	if (sepol_node_set_con(handle, tmp_record, tmp_con) < 0)
		goto err;

	sepol_context_free(tmp_con);
	*record = tmp_record;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not convert node to record");
	sepol_context_free(tmp_con);
	sepol_node_free(tmp_record);
	return STATUS_ERR;
}

/* Return the number of nodes */
extern int sepol_node_count(sepol_handle_t * handle __attribute__ ((unused)),
			    const sepol_policydb_t * p, unsigned int *response)
{

	unsigned int count = 0;
	ocontext_t *c, *head;
	const policydb_t *policydb = &p->p;

	head = policydb->ocontexts[OCON_NODE];
	for (c = head; c != NULL; c = c->next)
		count++;

	head = policydb->ocontexts[OCON_NODE6];
	for (c = head; c != NULL; c = c->next)
		count++;

	*response = count;

	return STATUS_SUCCESS;
}

/* Check if a node exists */
int sepol_node_exists(sepol_handle_t * handle,
		      const sepol_policydb_t * p,
		      const sepol_node_key_t * key, int *response)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;

	int proto;
	const char *addr, *mask;
	sepol_node_key_unpack(key, &addr, &mask, &proto);

	switch (proto) {

	case SEPOL_PROTO_IP4:
		{
			head = policydb->ocontexts[OCON_NODE];
			for (c = head; c; c = c->next) {
				unsigned int *addr2 = &c->u.node.addr;
				unsigned int *mask2 = &c->u.node.mask;

				if (!memcmp(addr, addr2, 4) &&
				    !memcmp(mask, mask2, 4)) {

					*response = 1;
					return STATUS_SUCCESS;
				}
			}
			break;
		}
	case SEPOL_PROTO_IP6:
		{
			head = policydb->ocontexts[OCON_NODE6];
			for (c = head; c; c = c->next) {
				unsigned int *addr2 = c->u.node6.addr;
				unsigned int *mask2 = c->u.node6.mask;

				if (!memcmp(addr, addr2, 16) &&
				    !memcmp(mask, mask2, 16)) {
					*response = 1;
					return STATUS_SUCCESS;
				}
			}
			break;
		}
	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}

	*response = 0;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not check if node %s/%s (%s) exists",
	    addr, mask, sepol_node_get_proto_str(proto));
	return STATUS_ERR;
}

/* Query a node */
int sepol_node_query(sepol_handle_t * handle,
		     const sepol_policydb_t * p,
		     const sepol_node_key_t * key, sepol_node_t ** response)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;

	int proto;
	const char *addr, *mask;
	sepol_node_key_unpack(key, &addr, &mask, &proto);

	switch (proto) {

	case SEPOL_PROTO_IP4:
		{
			head = policydb->ocontexts[OCON_NODE];
			for (c = head; c; c = c->next) {
				unsigned int *addr2 = &c->u.node.addr;
				unsigned int *mask2 = &c->u.node.mask;

				if (!memcmp(addr, addr2, 4) &&
				    !memcmp(mask, mask2, 4)) {

					if (node_to_record(handle, policydb,
							   c, SEPOL_PROTO_IP4,
							   response) < 0)
						goto err;
					return STATUS_SUCCESS;
				}
			}
			break;
		}
	case SEPOL_PROTO_IP6:
		{
			head = policydb->ocontexts[OCON_NODE6];
			for (c = head; c; c = c->next) {
				unsigned int *addr2 = c->u.node6.addr;
				unsigned int *mask2 = c->u.node6.mask;

				if (!memcmp(addr, addr2, 16) &&
				    !memcmp(mask, mask2, 16)) {

					if (node_to_record(handle, policydb,
							   c, SEPOL_PROTO_IP6,
							   response) < 0)
						goto err;
					return STATUS_SUCCESS;
				}
			}
			break;
		}
	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}
	*response = NULL;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not query node %s/%s (%s)",
	    addr, mask, sepol_node_get_proto_str(proto));
	return STATUS_ERR;

}

/* Load a node into policy */
int sepol_node_modify(sepol_handle_t * handle,
		      sepol_policydb_t * p,
		      const sepol_node_key_t * key, const sepol_node_t * data)
{

	policydb_t *policydb = &p->p;
	ocontext_t *node = NULL;

	int proto;
	const char *addr, *mask;

	sepol_node_key_unpack(key, &addr, &mask, &proto);

	if (node_from_record(handle, policydb, &node, data) < 0)
		goto err;

	switch (proto) {

	case SEPOL_PROTO_IP4:
		{
			/* Attach to context list */
			node->next = policydb->ocontexts[OCON_NODE];
			policydb->ocontexts[OCON_NODE] = node;
			break;
		}
	case SEPOL_PROTO_IP6:
		{
			/* Attach to context list */
			node->next = policydb->ocontexts[OCON_NODE6];
			policydb->ocontexts[OCON_NODE6] = node;
			break;
		}
	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not load node %s/%s (%s)",
	    addr, mask, sepol_node_get_proto_str(proto));
	if (node != NULL) {
		context_destroy(&node->context[0]);
		free(node);
	}
	return STATUS_ERR;
}

int sepol_node_iterate(sepol_handle_t * handle,
		       const sepol_policydb_t * p,
		       int (*fn) (const sepol_node_t * node,
				  void *fn_arg), void *arg)
{

	const policydb_t *policydb = &p->p;
	ocontext_t *c, *head;
	sepol_node_t *node = NULL;
	int status;

	head = policydb->ocontexts[OCON_NODE];
	for (c = head; c; c = c->next) {
		if (node_to_record(handle, policydb, c, SEPOL_PROTO_IP4, &node)
		    < 0)
			goto err;

		/* Invoke handler */
		status = fn(node, arg);
		if (status < 0)
			goto err;

		sepol_node_free(node);
		node = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	head = policydb->ocontexts[OCON_NODE6];
	for (c = head; c; c = c->next) {
		if (node_to_record(handle, policydb, c, SEPOL_PROTO_IP6, &node)
		    < 0)
			goto err;

		/* Invoke handler */
		status = fn(node, arg);
		if (status < 0)
			goto err;

		sepol_node_free(node);
		node = NULL;

		/* Handler requested exit */
		if (status > 0)
			break;
	}

	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not iterate over nodes");
	sepol_node_free(node);
	return STATUS_ERR;
}

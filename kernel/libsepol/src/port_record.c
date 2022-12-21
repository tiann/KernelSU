// #include <stdlib.h>
#include <linux/string.h>

#include "port_internal.h"
#include "context_internal.h"
#include "debug.h"

struct sepol_port {
	/* Low - High range. Same for single ports. */
	int low, high;

	/* Protocol */
	int proto;

	/* Context */
	sepol_context_t *con;
};

struct sepol_port_key {
	/* Low - High range. Same for single ports. */
	int low, high;

	/* Protocol */
	int proto;
};

/* Key */
int sepol_port_key_create(sepol_handle_t * handle,
			  int low, int high, int proto,
			  sepol_port_key_t ** key_ptr)
{

	sepol_port_key_t *tmp_key =
	    (sepol_port_key_t *) malloc(sizeof(sepol_port_key_t));

	if (!tmp_key) {
		ERR(handle, "out of memory, could not create " "port key");
		return STATUS_ERR;
	}

	tmp_key->low = low;
	tmp_key->high = high;
	tmp_key->proto = proto;

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;
}


void sepol_port_key_unpack(const sepol_port_key_t * key,
			   int *low, int *high, int *proto)
{

	*low = key->low;
	*high = key->high;
	*proto = key->proto;
}


int sepol_port_key_extract(sepol_handle_t * handle,
			   const sepol_port_t * port,
			   sepol_port_key_t ** key_ptr)
{

	if (sepol_port_key_create
	    (handle, port->low, port->high, port->proto, key_ptr) < 0) {

		ERR(handle, "could not extract key from port %s %d:%d",
		    sepol_port_get_proto_str(port->proto),
		    port->low, port->high);

		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

void sepol_port_key_free(sepol_port_key_t * key)
{
	free(key);
}

int sepol_port_compare(const sepol_port_t * port, const sepol_port_key_t * key)
{

	if ((port->low == key->low) &&
	    (port->high == key->high) && (port->proto == key->proto))
		return 0;

	if (port->low < key->low)
		return -1;

	else if (key->low < port->low)
		return 1;

	else if (port->high < key->high)
		return -1;

	else if (key->high < port->high)
		return 1;

	else if (port->proto < key->proto)
		return -1;

	else
		return 1;
}

int sepol_port_compare2(const sepol_port_t * port, const sepol_port_t * port2)
{

	if ((port->low == port2->low) &&
	    (port->high == port2->high) && (port->proto == port2->proto))
		return 0;

	if (port->low < port2->low)
		return -1;

	else if (port2->low < port->low)
		return 1;

	else if (port->high < port2->high)
		return -1;

	else if (port2->high < port->high)
		return 1;

	else if (port->proto < port2->proto)
		return -1;

	else
		return 1;
}

/* Port */
int sepol_port_get_low(const sepol_port_t * port)
{

	return port->low;
}


int sepol_port_get_high(const sepol_port_t * port)
{

	return port->high;
}


void sepol_port_set_port(sepol_port_t * port, int port_num)
{

	port->low = port_num;
	port->high = port_num;
}

void sepol_port_set_range(sepol_port_t * port, int low, int high)
{

	port->low = low;
	port->high = high;
}


/* Protocol */
int sepol_port_get_proto(const sepol_port_t * port)
{

	return port->proto;
}


const char *sepol_port_get_proto_str(int proto)
{

	switch (proto) {
	case SEPOL_PROTO_UDP:
		return "udp";
	case SEPOL_PROTO_TCP:
		return "tcp";
	case SEPOL_PROTO_DCCP:
		return "dccp";
	case SEPOL_PROTO_SCTP:
		return "sctp";
	default:
		return "???";
	}
}


void sepol_port_set_proto(sepol_port_t * port, int proto)
{

	port->proto = proto;
}


/* Create */
int sepol_port_create(sepol_handle_t * handle, sepol_port_t ** port)
{

	sepol_port_t *tmp_port = (sepol_port_t *) malloc(sizeof(sepol_port_t));

	if (!tmp_port) {
		ERR(handle, "out of memory, could not create " "port record");
		return STATUS_ERR;
	}

	tmp_port->low = 0;
	tmp_port->high = 0;
	tmp_port->proto = SEPOL_PROTO_UDP;
	tmp_port->con = NULL;
	*port = tmp_port;

	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_port_clone(sepol_handle_t * handle,
		     const sepol_port_t * port, sepol_port_t ** port_ptr)
{

	sepol_port_t *new_port = NULL;
	if (sepol_port_create(handle, &new_port) < 0)
		goto err;

	new_port->low = port->low;
	new_port->high = port->high;
	new_port->proto = port->proto;

	if (port->con &&
	    (sepol_context_clone(handle, port->con, &new_port->con) < 0))
		goto err;

	*port_ptr = new_port;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not clone port record");
	sepol_port_free(new_port);
	return STATUS_ERR;
}

/* Destroy */
void sepol_port_free(sepol_port_t * port)
{

	if (!port)
		return;

	sepol_context_free(port->con);
	free(port);
}


/* Context */
sepol_context_t *sepol_port_get_con(const sepol_port_t * port)
{

	return port->con;
}


int sepol_port_set_con(sepol_handle_t * handle,
		       sepol_port_t * port, sepol_context_t * con)
{

	sepol_context_t *newcon;

	if (sepol_context_clone(handle, con, &newcon) < 0) {
		ERR(handle, "out of memory, could not set port context");
		return STATUS_ERR;
	}

	sepol_context_free(port->con);
	port->con = newcon;
	return STATUS_SUCCESS;
}


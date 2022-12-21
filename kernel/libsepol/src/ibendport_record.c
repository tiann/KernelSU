// #include <stdlib.h>
#include <linux/string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
// #include <errno.h>

#include "sepol/policydb/policydb.h"
#include "ibendport_internal.h"
#include "context_internal.h"
#include "debug.h"

struct sepol_ibendport {
	/* Device Name */
	char *ibdev_name;

	/* Port number */
	int port;

	/* Context */
	sepol_context_t *con;
};

struct sepol_ibendport_key {
	/* Device Name */
	char *ibdev_name;

	/* Port number */
	int port;
};

/* Allocates a sufficiently large string (ibdev_name) */
int sepol_ibendport_alloc_ibdev_name(sepol_handle_t *handle,
				     char **ibdev_name)
{
	*ibdev_name = calloc(1, IB_DEVICE_NAME_MAX);

	if (!*ibdev_name)
		goto omem;

	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");
	ERR(handle, "could not allocate string buffer for ibdev_name");
	return STATUS_ERR;
}

/* Key */
int sepol_ibendport_key_create(sepol_handle_t *handle,
			       const char *ibdev_name,
			       int port,
			       sepol_ibendport_key_t **key_ptr)
{
	sepol_ibendport_key_t *tmp_key =
	    (sepol_ibendport_key_t *)malloc(sizeof(sepol_ibendport_key_t));

	if (!tmp_key) {
		ERR(handle, "out of memory, could not create ibendport key");
		goto omem;
	}

	if (sepol_ibendport_alloc_ibdev_name(handle, &tmp_key->ibdev_name) < 0)
		goto err;

	strncpy(tmp_key->ibdev_name, ibdev_name, IB_DEVICE_NAME_MAX - 1);
	tmp_key->port = port;

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");

err:
	sepol_ibendport_key_free(tmp_key);
	ERR(handle, "could not create ibendport key for IB device %s, port %u",
	    ibdev_name, port);
	return STATUS_ERR;
}


void sepol_ibendport_key_unpack(const sepol_ibendport_key_t *key,
				const char **ibdev_name, int *port)
{
	*ibdev_name = key->ibdev_name;
	*port = key->port;
}


int sepol_ibendport_key_extract(sepol_handle_t *handle,
				const sepol_ibendport_t *ibendport,
				sepol_ibendport_key_t **key_ptr)
{
	if (sepol_ibendport_key_create
	    (handle, ibendport->ibdev_name, ibendport->port, key_ptr) < 0) {
		ERR(handle, "could not extract key from ibendport device %s port %d",
		    ibendport->ibdev_name,
		    ibendport->port);

		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

void sepol_ibendport_key_free(sepol_ibendport_key_t *key)
{
	if (!key)
		return;
	free(key->ibdev_name);
	free(key);
}

int sepol_ibendport_compare(const sepol_ibendport_t *ibendport, const sepol_ibendport_key_t *key)
{
	int rc;

	rc = strcmp(ibendport->ibdev_name, key->ibdev_name);

	if ((ibendport->port == key->port) && !rc)
		return 0;

	if (ibendport->port < key->port)
		return -1;
	else if (key->port < ibendport->port)
		return 1;
	else
		return rc;
}

int sepol_ibendport_compare2(const sepol_ibendport_t *ibendport, const sepol_ibendport_t *ibendport2)
{
	int rc;

	rc = strcmp(ibendport->ibdev_name, ibendport2->ibdev_name);

	if ((ibendport->port == ibendport2->port) && !rc)
		return 0;

	if (ibendport->port < ibendport2->port)
		return -1;
	else if (ibendport2->port < ibendport->port)
		return 1;
	else
		return rc;
}

int sepol_ibendport_get_port(const sepol_ibendport_t *ibendport)
{
	return ibendport->port;
}


void sepol_ibendport_set_port(sepol_ibendport_t *ibendport, int port)
{
	ibendport->port = port;
}


int sepol_ibendport_get_ibdev_name(sepol_handle_t *handle,
				   const sepol_ibendport_t *ibendport,
				   char **ibdev_name)
{
	char *tmp_ibdev_name = NULL;

	if (sepol_ibendport_alloc_ibdev_name(handle, &tmp_ibdev_name) < 0)
		goto err;

	strncpy(tmp_ibdev_name, ibendport->ibdev_name, IB_DEVICE_NAME_MAX - 1);
	*ibdev_name = tmp_ibdev_name;
	return STATUS_SUCCESS;

err:
	free(tmp_ibdev_name);
	ERR(handle, "could not get ibendport ibdev_name");
	return STATUS_ERR;
}


int sepol_ibendport_set_ibdev_name(sepol_handle_t *handle,
				   sepol_ibendport_t *ibendport,
				   const char *ibdev_name)
{
	char *tmp = NULL;

	if (sepol_ibendport_alloc_ibdev_name(handle, &tmp) < 0)
		goto err;

	strncpy(tmp, ibdev_name, IB_DEVICE_NAME_MAX - 1);
	free(ibendport->ibdev_name);
	ibendport->ibdev_name = tmp;
	return STATUS_SUCCESS;

err:
	free(tmp);
	ERR(handle, "could not set ibendport subnet prefix to %s", ibdev_name);
	return STATUS_ERR;
}


/* Create */
int sepol_ibendport_create(sepol_handle_t *handle, sepol_ibendport_t **ibendport)
{
	sepol_ibendport_t *tmp_ibendport = (sepol_ibendport_t *)malloc(sizeof(sepol_ibendport_t));

	if (!tmp_ibendport) {
		ERR(handle, "out of memory, could not create ibendport record");
		return STATUS_ERR;
	}

	tmp_ibendport->ibdev_name = NULL;
	tmp_ibendport->port = 0;
	tmp_ibendport->con = NULL;
	*ibendport = tmp_ibendport;

	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_ibendport_clone(sepol_handle_t *handle,
			  const sepol_ibendport_t *ibendport,
			  sepol_ibendport_t **ibendport_ptr)
{
	sepol_ibendport_t *new_ibendport = NULL;

	if (sepol_ibendport_create(handle, &new_ibendport) < 0)
		goto err;

	if (sepol_ibendport_alloc_ibdev_name(handle, &new_ibendport->ibdev_name) < 0)
		goto omem;

	strncpy(new_ibendport->ibdev_name, ibendport->ibdev_name, IB_DEVICE_NAME_MAX - 1);
	new_ibendport->port = ibendport->port;

	if (ibendport->con &&
	    (sepol_context_clone(handle, ibendport->con, &new_ibendport->con) < 0))
		goto err;

	*ibendport_ptr = new_ibendport;
	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");

err:
	ERR(handle, "could not clone ibendport record");
	sepol_ibendport_free(new_ibendport);
	return STATUS_ERR;
}

/* Destroy */
void sepol_ibendport_free(sepol_ibendport_t *ibendport)
{
	if (!ibendport)
		return;

	free(ibendport->ibdev_name);
	sepol_context_free(ibendport->con);
	free(ibendport);
}


/* Context */
sepol_context_t *sepol_ibendport_get_con(const sepol_ibendport_t *ibendport)
{
	return ibendport->con;
}


int sepol_ibendport_set_con(sepol_handle_t *handle,
			    sepol_ibendport_t *ibendport, sepol_context_t *con)
{
	sepol_context_t *newcon;

	if (sepol_context_clone(handle, con, &newcon) < 0) {
		ERR(handle, "out of memory, could not set ibendport context");
		return STATUS_ERR;
	}

	sepol_context_free(ibendport->con);
	ibendport->con = newcon;
	return STATUS_SUCCESS;
}


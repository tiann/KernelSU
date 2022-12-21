#include <stdlib.h>
#include <string.h>

#include "iface_internal.h"
#include "context_internal.h"
#include "debug.h"

struct sepol_iface {

	/* Interface name */
	char *name;

	/* Interface context */
	sepol_context_t *netif_con;

	/* Message context */
	sepol_context_t *netmsg_con;
};

struct sepol_iface_key {

	/* Interface name */
	char *name;
};

/* Key */
int sepol_iface_key_create(sepol_handle_t * handle,
			   const char *name, sepol_iface_key_t ** key_ptr)
{

	sepol_iface_key_t *tmp_key =
	    (sepol_iface_key_t *) malloc(sizeof(sepol_iface_key_t));

	if (!tmp_key) {
		ERR(handle, "out of memory, could not create interface key");
		return STATUS_ERR;
	}

	tmp_key->name = strdup(name);
	if (!tmp_key->name) {
		ERR(handle, "out of memory, could not create interface key");
		free(tmp_key);
		return STATUS_ERR;
	}

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;
}


void sepol_iface_key_unpack(const sepol_iface_key_t * key, const char **name)
{

	*name = key->name;
}


int sepol_iface_key_extract(sepol_handle_t * handle,
			    const sepol_iface_t * iface,
			    sepol_iface_key_t ** key_ptr)
{

	if (sepol_iface_key_create(handle, iface->name, key_ptr) < 0) {
		ERR(handle, "could not extract key from "
		    "interface %s", iface->name);
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

void sepol_iface_key_free(sepol_iface_key_t * key)
{
	if (!key)
		return;
	free(key->name);
	free(key);
}

int sepol_iface_compare(const sepol_iface_t * iface,
			const sepol_iface_key_t * key)
{

	return strcmp(iface->name, key->name);
}

int sepol_iface_compare2(const sepol_iface_t * iface,
			 const sepol_iface_t * iface2)
{

	return strcmp(iface->name, iface2->name);
}

/* Create */
int sepol_iface_create(sepol_handle_t * handle, sepol_iface_t ** iface)
{

	sepol_iface_t *tmp_iface =
	    (sepol_iface_t *) malloc(sizeof(sepol_iface_t));

	if (!tmp_iface) {
		ERR(handle, "out of memory, could not create "
		    "interface record");
		return STATUS_ERR;
	}

	tmp_iface->name = NULL;
	tmp_iface->netif_con = NULL;
	tmp_iface->netmsg_con = NULL;
	*iface = tmp_iface;

	return STATUS_SUCCESS;
}


/* Name */
const char *sepol_iface_get_name(const sepol_iface_t * iface)
{

	return iface->name;
}


int sepol_iface_set_name(sepol_handle_t * handle,
			 sepol_iface_t * iface, const char *name)
{

	char *tmp_name = strdup(name);
	if (!tmp_name) {
		ERR(handle, "out of memory, " "could not set interface name");
		return STATUS_ERR;
	}
	free(iface->name);
	iface->name = tmp_name;
	return STATUS_SUCCESS;
}


/* Interface Context */
sepol_context_t *sepol_iface_get_ifcon(const sepol_iface_t * iface)
{

	return iface->netif_con;
}


int sepol_iface_set_ifcon(sepol_handle_t * handle,
			  sepol_iface_t * iface, sepol_context_t * con)
{

	sepol_context_t *newcon;

	if (sepol_context_clone(handle, con, &newcon) < 0) {
		ERR(handle, "out of memory, could not set interface context");
		return STATUS_ERR;
	}

	sepol_context_free(iface->netif_con);
	iface->netif_con = newcon;
	return STATUS_SUCCESS;
}


/* Message Context */
sepol_context_t *sepol_iface_get_msgcon(const sepol_iface_t * iface)
{

	return iface->netmsg_con;
}


int sepol_iface_set_msgcon(sepol_handle_t * handle,
			   sepol_iface_t * iface, sepol_context_t * con)
{

	sepol_context_t *newcon;
	if (sepol_context_clone(handle, con, &newcon) < 0) {
		ERR(handle, "out of memory, could not set message context");
		return STATUS_ERR;
	}

	sepol_context_free(iface->netmsg_con);
	iface->netmsg_con = newcon;
	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_iface_clone(sepol_handle_t * handle,
		      const sepol_iface_t * iface, sepol_iface_t ** iface_ptr)
{

	sepol_iface_t *new_iface = NULL;
	if (sepol_iface_create(handle, &new_iface) < 0)
		goto err;

	if (sepol_iface_set_name(handle, new_iface, iface->name) < 0)
		goto err;

	if (iface->netif_con &&
	    (sepol_context_clone
	     (handle, iface->netif_con, &new_iface->netif_con) < 0))
		goto err;

	if (iface->netmsg_con &&
	    (sepol_context_clone
	     (handle, iface->netmsg_con, &new_iface->netmsg_con) < 0))
		goto err;

	*iface_ptr = new_iface;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not clone interface record");
	sepol_iface_free(new_iface);
	return STATUS_ERR;
}

/* Destroy */
void sepol_iface_free(sepol_iface_t * iface)
{

	if (!iface)
		return;

	free(iface->name);
	sepol_context_free(iface->netif_con);
	sepol_context_free(iface->netmsg_con);
	free(iface);
}


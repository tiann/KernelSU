// #include <errno.h>
// #include <stdlib.h>
#include <linux/string.h>

#include "boolean_internal.h"
#include "debug.h"
#include "kernel.h"

struct sepol_bool {
	/* This boolean's name */
	char *name;

	/* Its value */
	int value;
};

struct sepol_bool_key {
	/* This boolean's name */
	char *name;
};

int sepol_bool_key_create(sepol_handle_t * handle,
			  const char *name, sepol_bool_key_t ** key_ptr)
{

	sepol_bool_key_t *tmp_key =
	    (sepol_bool_key_t *) malloc(sizeof(struct sepol_bool_key));

	if (!tmp_key) {
		ERR(handle, "out of memory, " "could not create boolean key");
		return STATUS_ERR;
	}

	tmp_key->name = strdup(name);
	if (!tmp_key->name) {
		ERR(handle, "out of memory, " "could not create boolean key");
		free(tmp_key);
		return STATUS_ERR;
	}

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;
}


void sepol_bool_key_unpack(const sepol_bool_key_t * key, const char **name)
{

	*name = key->name;
}


int sepol_bool_key_extract(sepol_handle_t * handle,
			   const sepol_bool_t * boolean,
			   sepol_bool_key_t ** key_ptr)
{

	if (sepol_bool_key_create(handle, boolean->name, key_ptr) < 0) {
		ERR(handle, "could not extract key from boolean %s",
		    boolean->name);
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

void sepol_bool_key_free(sepol_bool_key_t * key)
{
	if (!key)
		return;
	free(key->name);
	free(key);
}

int sepol_bool_compare(const sepol_bool_t * boolean,
		       const sepol_bool_key_t * key)
{

	return strcmp(boolean->name, key->name);
}

int sepol_bool_compare2(const sepol_bool_t * boolean,
			const sepol_bool_t * boolean2)
{

	return strcmp(boolean->name, boolean2->name);
}

/* Name */
const char *sepol_bool_get_name(const sepol_bool_t * boolean)
{

	return boolean->name;
}


int sepol_bool_set_name(sepol_handle_t * handle,
			sepol_bool_t * boolean, const char *name)
{

	char *tmp_name = strdup(name);
	if (!tmp_name) {
		ERR(handle, "out of memory, could not set boolean name");
		return STATUS_ERR;
	}
	free(boolean->name);
	boolean->name = tmp_name;
	return STATUS_SUCCESS;
}


/* Value */
int sepol_bool_get_value(const sepol_bool_t * boolean)
{

	return boolean->value;
}


void sepol_bool_set_value(sepol_bool_t * boolean, int value)
{

	boolean->value = value;
}


/* Create */
int sepol_bool_create(sepol_handle_t * handle, sepol_bool_t ** bool_ptr)
{

	sepol_bool_t *boolean = (sepol_bool_t *) malloc(sizeof(sepol_bool_t));

	if (!boolean) {
		ERR(handle, "out of memory, "
		    "could not create boolean record");
		return STATUS_ERR;
	}

	boolean->name = NULL;
	boolean->value = 0;

	*bool_ptr = boolean;
	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_bool_clone(sepol_handle_t * handle,
		     const sepol_bool_t * boolean, sepol_bool_t ** bool_ptr)
{

	sepol_bool_t *new_bool = NULL;

	if (sepol_bool_create(handle, &new_bool) < 0)
		goto err;

	if (sepol_bool_set_name(handle, new_bool, boolean->name) < 0)
		goto err;

	new_bool->value = boolean->value;

	*bool_ptr = new_bool;
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not clone boolean record");
	sepol_bool_free(new_bool);
	return STATUS_ERR;
}

/* Destroy */
void sepol_bool_free(sepol_bool_t * boolean)
{

	if (!boolean)
		return;

	free(boolean->name);
	free(boolean);
}


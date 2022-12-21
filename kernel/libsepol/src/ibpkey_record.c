#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sepol/ibpkey_record.h>

#include "ibpkey_internal.h"
#include "context_internal.h"
#include "debug.h"

struct sepol_ibpkey {
	/* Subnet prefix */
	uint64_t subnet_prefix;

	/* Low - High range. Same for single ibpkeys. */
	int low, high;

	/* Context */
	sepol_context_t *con;
};

struct sepol_ibpkey_key {
	/* Subnet prefix */
	uint64_t subnet_prefix;

	/* Low - High range. Same for single ibpkeys. */
	int low, high;
};

/* Converts a string represtation (subnet_prefix_str)
 * to a numeric representation (subnet_prefix_bytes)
 */
static int ibpkey_parse_subnet_prefix(sepol_handle_t *handle,
				      const char *subnet_prefix_str,
				      uint64_t *subnet_prefix)
{
	struct in6_addr in_addr;

	if (inet_pton(AF_INET6, subnet_prefix_str, &in_addr) <= 0) {
		ERR(handle, "could not parse IPv6 address for ibpkey subnet prefix %s: %m",
		    subnet_prefix_str);
		return STATUS_ERR;
	}

	memcpy(subnet_prefix, in_addr.s6_addr, sizeof(*subnet_prefix));

	return STATUS_SUCCESS;
}

/* Converts a numeric representation (subnet_prefix_bytes)
 * to a string representation (subnet_prefix_str)
 */

static int ibpkey_expand_subnet_prefix(sepol_handle_t *handle,
				       uint64_t subnet_prefix,
				       char *subnet_prefix_str)
{
	struct in6_addr addr;

	memset(&addr, 0, sizeof(struct in6_addr));
	memcpy(&addr.s6_addr[0], &subnet_prefix, sizeof(subnet_prefix));

	if (inet_ntop(AF_INET6, &addr, subnet_prefix_str,
		      INET6_ADDRSTRLEN) == NULL) {
		ERR(handle,
		    "could not expand IPv6 address to string: %m");
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

/* Allocates a sufficiently large string (subnet_prefix)
 * for an IPV6 address for the subnet prefix
 */
static int ibpkey_alloc_subnet_prefix_string(sepol_handle_t *handle,
					     char **subnet_prefix)
{
	char *tmp_subnet_prefix = NULL;

	tmp_subnet_prefix = malloc(INET6_ADDRSTRLEN);

	if (!tmp_subnet_prefix)
		goto omem;

	*subnet_prefix = tmp_subnet_prefix;
	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");

	ERR(handle, "could not allocate string buffer for subnet_prefix");
	return STATUS_ERR;
}

/* Key */
int sepol_ibpkey_key_create(sepol_handle_t *handle,
			    const char *subnet_prefix,
			    int low, int high,
			    sepol_ibpkey_key_t **key_ptr)
{
	sepol_ibpkey_key_t *tmp_key =
	    (sepol_ibpkey_key_t *)malloc(sizeof(sepol_ibpkey_key_t));

	if (!tmp_key) {
		ERR(handle, "out of memory, could not create ibpkey key");
		goto omem;
	}

	if (ibpkey_parse_subnet_prefix(handle, subnet_prefix, &tmp_key->subnet_prefix) < 0)
		goto err;

	tmp_key->low = low;
	tmp_key->high = high;

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;

omem:
	ERR(handle, "out of memory");

err:
	sepol_ibpkey_key_free(tmp_key);
	ERR(handle, "could not create ibpkey key for subnet prefix%s, range %u, %u",
	    subnet_prefix, low, high);
	return STATUS_ERR;
}


void sepol_ibpkey_key_unpack(const sepol_ibpkey_key_t *key,
			     uint64_t *subnet_prefix, int *low, int *high)
{
	*subnet_prefix = key->subnet_prefix;
	*low = key->low;
	*high = key->high;
}


int sepol_ibpkey_key_extract(sepol_handle_t *handle,
			     const sepol_ibpkey_t *ibpkey,
			     sepol_ibpkey_key_t **key_ptr)
{
	char subnet_prefix_str[INET6_ADDRSTRLEN];

	ibpkey_expand_subnet_prefix(handle, ibpkey->subnet_prefix, subnet_prefix_str);

	if (sepol_ibpkey_key_create
	    (handle, subnet_prefix_str, ibpkey->low, ibpkey->high, key_ptr) < 0) {
		ERR(handle, "could not extract key from ibpkey %s %d:%d",
		    subnet_prefix_str,
		    ibpkey->low, ibpkey->high);

		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

void sepol_ibpkey_key_free(sepol_ibpkey_key_t *key)
{
	if (!key)
		return;
	free(key);
}

int sepol_ibpkey_compare(const sepol_ibpkey_t *ibpkey, const sepol_ibpkey_key_t *key)
{
	if (ibpkey->subnet_prefix < key->subnet_prefix)
		return -1;
	if (key->subnet_prefix < ibpkey->subnet_prefix)
		return 1;

	if (ibpkey->low < key->low)
		return -1;
	if (key->low < ibpkey->low)
		return 1;

	if (ibpkey->high < key->high)
		return -1;
	if (key->high < ibpkey->high)
		return 1;

	return 0;
}

int sepol_ibpkey_compare2(const sepol_ibpkey_t *ibpkey, const sepol_ibpkey_t *ibpkey2)
{
	if (ibpkey->subnet_prefix < ibpkey2->subnet_prefix)
		return -1;
	if (ibpkey2->subnet_prefix < ibpkey->subnet_prefix)
		return 1;

	if (ibpkey->low < ibpkey2->low)
		return -1;
	if (ibpkey2->low < ibpkey->low)
		return 1;

	if (ibpkey->high < ibpkey2->high)
		return -1;
	if (ibpkey2->high < ibpkey->high)
		return 1;

	return 0;
}

/* Pkey */
int sepol_ibpkey_get_low(const sepol_ibpkey_t *ibpkey)
{
	return ibpkey->low;
}


int sepol_ibpkey_get_high(const sepol_ibpkey_t *ibpkey)
{
	return ibpkey->high;
}


void sepol_ibpkey_set_pkey(sepol_ibpkey_t *ibpkey, int pkey_num)
{
	ibpkey->low = pkey_num;
	ibpkey->high = pkey_num;
}

void sepol_ibpkey_set_range(sepol_ibpkey_t *ibpkey, int low, int high)
{
	ibpkey->low = low;
	ibpkey->high = high;
}


int sepol_ibpkey_get_subnet_prefix(sepol_handle_t *handle,
				   const sepol_ibpkey_t *ibpkey,
				   char **subnet_prefix)
{
	char *tmp_subnet_prefix = NULL;

	if (ibpkey_alloc_subnet_prefix_string(handle, &tmp_subnet_prefix) < 0)
		goto err;

	if (ibpkey_expand_subnet_prefix(handle, ibpkey->subnet_prefix, tmp_subnet_prefix) < 0)
		goto err;

	*subnet_prefix = tmp_subnet_prefix;
	return STATUS_SUCCESS;

err:
	free(tmp_subnet_prefix);
	ERR(handle, "could not get ibpkey subnet_prefix");
	return STATUS_ERR;
}


/* Subnet prefix */
uint64_t sepol_ibpkey_get_subnet_prefix_bytes(const sepol_ibpkey_t *ibpkey)
{
	return ibpkey->subnet_prefix;
}


int sepol_ibpkey_set_subnet_prefix(sepol_handle_t *handle,
				   sepol_ibpkey_t *ibpkey,
				   const char *subnet_prefix_str)
{
	uint64_t tmp = 0;

	if (ibpkey_parse_subnet_prefix(handle, subnet_prefix_str, &tmp) < 0)
		goto err;

	ibpkey->subnet_prefix = tmp;
	return STATUS_SUCCESS;

err:
	ERR(handle, "could not set ibpkey subnet prefix to %s", subnet_prefix_str);
	return STATUS_ERR;
}


void sepol_ibpkey_set_subnet_prefix_bytes(sepol_ibpkey_t *ibpkey,
					  uint64_t subnet_prefix)
{
	ibpkey->subnet_prefix = subnet_prefix;
}


/* Create */
int sepol_ibpkey_create(sepol_handle_t *handle, sepol_ibpkey_t **ibpkey)
{
	sepol_ibpkey_t *tmp_ibpkey = (sepol_ibpkey_t *)malloc(sizeof(sepol_ibpkey_t));

	if (!tmp_ibpkey) {
		ERR(handle, "out of memory, could not create ibpkey record");
		return STATUS_ERR;
	}

	tmp_ibpkey->subnet_prefix = 0;
	tmp_ibpkey->low = 0;
	tmp_ibpkey->high = 0;
	tmp_ibpkey->con = NULL;
	*ibpkey = tmp_ibpkey;

	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_ibpkey_clone(sepol_handle_t *handle,
		       const sepol_ibpkey_t *ibpkey, sepol_ibpkey_t **ibpkey_ptr)
{
	sepol_ibpkey_t *new_ibpkey = NULL;

	if (sepol_ibpkey_create(handle, &new_ibpkey) < 0)
		goto err;

	new_ibpkey->subnet_prefix = ibpkey->subnet_prefix;
	new_ibpkey->low = ibpkey->low;
	new_ibpkey->high = ibpkey->high;

	if (ibpkey->con &&
	    (sepol_context_clone(handle, ibpkey->con, &new_ibpkey->con) < 0))
		goto err;

	*ibpkey_ptr = new_ibpkey;
	return STATUS_SUCCESS;

err:
	ERR(handle, "could not clone ibpkey record");
	sepol_ibpkey_free(new_ibpkey);
	return STATUS_ERR;
}

/* Destroy */
void sepol_ibpkey_free(sepol_ibpkey_t *ibpkey)
{
	if (!ibpkey)
		return;

	sepol_context_free(ibpkey->con);
	free(ibpkey);
}


/* Context */
sepol_context_t *sepol_ibpkey_get_con(const sepol_ibpkey_t *ibpkey)
{
	return ibpkey->con;
}


int sepol_ibpkey_set_con(sepol_handle_t *handle,
			 sepol_ibpkey_t *ibpkey, sepol_context_t *con)
{
	sepol_context_t *newcon;

	if (sepol_context_clone(handle, con, &newcon) < 0) {
		ERR(handle, "out of memory, could not set ibpkey context");
		return STATUS_ERR;
	}

	sepol_context_free(ibpkey->con);
	ibpkey->con = newcon;
	return STATUS_SUCCESS;
}


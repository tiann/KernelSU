// #include <stdlib.h>
// #include <stddef.h>
#include <linux/string.h>
// #include <netinet/in.h>
#include <linux/in.h>
// #include <arpa/inet.h>
#include <linux/inet.h>
// #include <errno.h>

#include "node_internal.h"
#include "context_internal.h"
#include "debug.h"

struct sepol_node {

	/* Network address and mask */
	char *addr;
	size_t addr_sz;

	char *mask;
	size_t mask_sz;

	/* Protocol */
	int proto;

	/* Context */
	sepol_context_t *con;
};

struct sepol_node_key {

	/* Network address and mask */
	char *addr;
	size_t addr_sz;

	char *mask;
	size_t mask_sz;

	/* Protocol */
	int proto;
};

/* Converts a string represtation (addr_str)
 * to a numeric representation (addr_bytes) */

static int node_parse_addr(sepol_handle_t * handle,
			   const char *addr_str, int proto, char *addr_bytes)
{

	switch (proto) {

	case SEPOL_PROTO_IP4:
		{
			struct in_addr in_addr;

			if (inet_pton(AF_INET, addr_str, &in_addr) <= 0) {
				ERR(handle, "could not parse IPv4 address "
				    "%s: %m", addr_str);
				return STATUS_ERR;
			}

			memcpy(addr_bytes, &in_addr.s_addr, 4);
			break;
		}
	case SEPOL_PROTO_IP6:
		{
			struct in6_addr in_addr;

			if (inet_pton(AF_INET6, addr_str, &in_addr) <= 0) {
				ERR(handle, "could not parse IPv6 address "
				    "%s: %m", addr_str);
				return STATUS_ERR;
			}

			memcpy(addr_bytes, in_addr.s6_addr, 16);
			break;
		}
	default:
		ERR(handle, "unsupported protocol %u, could not "
		    "parse address", proto);
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

/* Allocates a sufficiently large buffer (addr, addr_sz)
 * according to the protocol */

static int node_alloc_addr(sepol_handle_t * handle,
			   int proto, char **addr, size_t * addr_sz)
{

	char *tmp_addr = NULL;
	size_t tmp_addr_sz;

	switch (proto) {

	case SEPOL_PROTO_IP4:
		tmp_addr_sz = 4;
		tmp_addr = malloc(4);
		if (!tmp_addr)
			goto omem;
		break;

	case SEPOL_PROTO_IP6:
		tmp_addr_sz = 16;
		tmp_addr = malloc(16);
		if (!tmp_addr)
			goto omem;
		break;

	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}

	*addr = tmp_addr;
	*addr_sz = tmp_addr_sz;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	free(tmp_addr);
	ERR(handle, "could not allocate address of protocol %s",
	    sepol_node_get_proto_str(proto));
	return STATUS_ERR;
}

/* Converts a numeric representation (addr_bytes)
 * to a string representation (addr_str), according to 
 * the protocol */

static int node_expand_addr(sepol_handle_t * handle,
			    char *addr_bytes, int proto, char *addr_str)
{

	switch (proto) {

	case SEPOL_PROTO_IP4:
		{
			struct in_addr addr;
			memset(&addr, 0, sizeof(struct in_addr));
			memcpy(&addr.s_addr, addr_bytes, 4);

			if (inet_ntop(AF_INET, &addr, addr_str,
				      INET_ADDRSTRLEN) == NULL) {

				ERR(handle,
				    "could not expand IPv4 address to string: %m");
				return STATUS_ERR;
			}
			break;
		}

	case SEPOL_PROTO_IP6:
		{
			struct in6_addr addr;
			memset(&addr, 0, sizeof(struct in6_addr));
			memcpy(&addr.s6_addr[0], addr_bytes, 16);
			if (inet_ntop(AF_INET6, &addr, addr_str,
				      INET6_ADDRSTRLEN) == NULL) {

				ERR(handle,
				    "could not expand IPv6 address to string: %m");
				return STATUS_ERR;
			}
			break;
		}

	default:
		ERR(handle, "unsupported protocol %u, could not"
		    " expand address to string", proto);
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

/* Allocates a sufficiently large address string (addr)
 * according to the protocol */

static int node_alloc_addr_string(sepol_handle_t * handle,
				  int proto, char **addr)
{

	char *tmp_addr = NULL;

	switch (proto) {

	case SEPOL_PROTO_IP4:
		tmp_addr = malloc(INET_ADDRSTRLEN);
		if (!tmp_addr)
			goto omem;
		break;

	case SEPOL_PROTO_IP6:
		tmp_addr = malloc(INET6_ADDRSTRLEN);
		if (!tmp_addr)
			goto omem;
		break;

	default:
		ERR(handle, "unsupported protocol %u", proto);
		goto err;
	}

	*addr = tmp_addr;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	free(tmp_addr);
	ERR(handle, "could not allocate string buffer for "
	    "address of protocol %s", sepol_node_get_proto_str(proto));
	return STATUS_ERR;
}

/* Key */
int sepol_node_key_create(sepol_handle_t * handle,
			  const char *addr,
			  const char *mask,
			  int proto, sepol_node_key_t ** key_ptr)
{

	sepol_node_key_t *tmp_key =
	    (sepol_node_key_t *) calloc(1, sizeof(sepol_node_key_t));
	if (!tmp_key)
		goto omem;

	if (node_alloc_addr(handle, proto, &tmp_key->addr, &tmp_key->addr_sz) <
	    0)
		goto err;
	if (node_parse_addr(handle, addr, proto, tmp_key->addr) < 0)
		goto err;

	if (node_alloc_addr(handle, proto, &tmp_key->mask, &tmp_key->mask_sz) <
	    0)
		goto err;
	if (node_parse_addr(handle, mask, proto, tmp_key->mask) < 0)
		goto err;

	tmp_key->proto = proto;

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	sepol_node_key_free(tmp_key);
	ERR(handle, "could not create node key for (%s, %s, %s)",
	    addr, mask, sepol_node_get_proto_str(proto));
	return STATUS_ERR;
}


void sepol_node_key_unpack(const sepol_node_key_t * key,
			   const char **addr, const char **mask, int *proto)
{

	*addr = key->addr;
	*mask = key->mask;
	*proto = key->proto;
}


int sepol_node_key_extract(sepol_handle_t * handle,
			   const sepol_node_t * node,
			   sepol_node_key_t ** key_ptr)
{

	sepol_node_key_t *tmp_key =
	    (sepol_node_key_t *) calloc(1, sizeof(sepol_node_key_t));
	if (!tmp_key)
		goto omem;

	tmp_key->addr = malloc(node->addr_sz);
	tmp_key->mask = malloc(node->mask_sz);

	if (!tmp_key->addr || !tmp_key->mask)
		goto omem;

	memcpy(tmp_key->addr, node->addr, node->addr_sz);
	memcpy(tmp_key->mask, node->mask, node->mask_sz);
	tmp_key->addr_sz = node->addr_sz;
	tmp_key->mask_sz = node->mask_sz;
	tmp_key->proto = node->proto;

	*key_ptr = tmp_key;
	return STATUS_SUCCESS;

      omem:
	sepol_node_key_free(tmp_key);
	ERR(handle, "out of memory, could not extract node key");
	return STATUS_ERR;
}

void sepol_node_key_free(sepol_node_key_t * key)
{

	if (!key)
		return;

	free(key->addr);
	free(key->mask);
	free(key);
}


int sepol_node_compare(const sepol_node_t * node, const sepol_node_key_t * key)
{

	int rc1, rc2;

	if ((node->addr_sz < key->addr_sz) || (node->mask_sz < key->mask_sz))
		return -1;

	else if ((node->addr_sz > key->addr_sz) ||
		 (node->mask_sz > key->mask_sz))
		return 1;

	rc1 = memcmp(node->addr, key->addr, node->addr_sz);
	rc2 = memcmp(node->mask, key->mask, node->mask_sz);

	return (rc2 != 0) ? rc2 : rc1;
}

int sepol_node_compare2(const sepol_node_t * node, const sepol_node_t * node2)
{

	int rc1, rc2;

	if ((node->addr_sz < node2->addr_sz) ||
	    (node->mask_sz < node2->mask_sz))
		return -1;

	else if ((node->addr_sz > node2->addr_sz) ||
		 (node->mask_sz > node2->mask_sz))
		return 1;

	rc1 = memcmp(node->addr, node2->addr, node->addr_sz);
	rc2 = memcmp(node->mask, node2->mask, node->mask_sz);

	return (rc2 != 0) ? rc2 : rc1;
}

/* Addr */
int sepol_node_get_addr(sepol_handle_t * handle,
			const sepol_node_t * node, char **addr)
{

	char *tmp_addr = NULL;

	if (node_alloc_addr_string(handle, node->proto, &tmp_addr) < 0)
		goto err;

	if (node_expand_addr(handle, node->addr, node->proto, tmp_addr) < 0)
		goto err;

	*addr = tmp_addr;
	return STATUS_SUCCESS;

      err:
	free(tmp_addr);
	ERR(handle, "could not get node address");
	return STATUS_ERR;
}


int sepol_node_get_addr_bytes(sepol_handle_t * handle,
			      const sepol_node_t * node,
			      char **buffer, size_t * bsize)
{

	char *tmp_buf = malloc(node->addr_sz);
	if (!tmp_buf) {
		ERR(handle, "out of memory, could not get address bytes");
		return STATUS_ERR;
	}

	memcpy(tmp_buf, node->addr, node->addr_sz);
	*buffer = tmp_buf;
	*bsize = node->addr_sz;
	return STATUS_SUCCESS;
}


int sepol_node_set_addr(sepol_handle_t * handle,
			sepol_node_t * node, int proto, const char *addr)
{

	char *tmp_addr = NULL;
	size_t tmp_addr_sz;

	if (node_alloc_addr(handle, proto, &tmp_addr, &tmp_addr_sz) < 0)
		goto err;

	if (node_parse_addr(handle, addr, proto, tmp_addr) < 0)
		goto err;

	free(node->addr);
	node->addr = tmp_addr;
	node->addr_sz = tmp_addr_sz;
	return STATUS_SUCCESS;

      err:
	free(tmp_addr);
	ERR(handle, "could not set node address to %s", addr);
	return STATUS_ERR;
}


int sepol_node_set_addr_bytes(sepol_handle_t * handle,
			      sepol_node_t * node,
			      const char *addr, size_t addr_sz)
{

	char *tmp_addr = malloc(addr_sz);
	if (!tmp_addr) {
		ERR(handle, "out of memory, could not " "set node address");
		return STATUS_ERR;
	}

	memcpy(tmp_addr, addr, addr_sz);
	free(node->addr);
	node->addr = tmp_addr;
	node->addr_sz = addr_sz;
	return STATUS_SUCCESS;
}


/* Mask */
int sepol_node_get_mask(sepol_handle_t * handle,
			const sepol_node_t * node, char **mask)
{

	char *tmp_mask = NULL;

	if (node_alloc_addr_string(handle, node->proto, &tmp_mask) < 0)
		goto err;

	if (node_expand_addr(handle, node->mask, node->proto, tmp_mask) < 0)
		goto err;

	*mask = tmp_mask;
	return STATUS_SUCCESS;

      err:
	free(tmp_mask);
	ERR(handle, "could not get node netmask");
	return STATUS_ERR;
}


int sepol_node_get_mask_bytes(sepol_handle_t * handle,
			      const sepol_node_t * node,
			      char **buffer, size_t * bsize)
{

	char *tmp_buf = malloc(node->mask_sz);
	if (!tmp_buf) {
		ERR(handle, "out of memory, could not get netmask bytes");
		return STATUS_ERR;
	}

	memcpy(tmp_buf, node->mask, node->mask_sz);
	*buffer = tmp_buf;
	*bsize = node->mask_sz;
	return STATUS_SUCCESS;
}


int sepol_node_set_mask(sepol_handle_t * handle,
			sepol_node_t * node, int proto, const char *mask)
{

	char *tmp_mask = NULL;
	size_t tmp_mask_sz;

	if (node_alloc_addr(handle, proto, &tmp_mask, &tmp_mask_sz) < 0)
		goto err;

	if (node_parse_addr(handle, mask, proto, tmp_mask) < 0)
		goto err;

	free(node->mask);
	node->mask = tmp_mask;
	node->mask_sz = tmp_mask_sz;
	return STATUS_SUCCESS;

      err:
	free(tmp_mask);
	ERR(handle, "could not set node netmask to %s", mask);
	return STATUS_ERR;
}


int sepol_node_set_mask_bytes(sepol_handle_t * handle,
			      sepol_node_t * node,
			      const char *mask, size_t mask_sz)
{

	char *tmp_mask = malloc(mask_sz);
	if (!tmp_mask) {
		ERR(handle, "out of memory, could not " "set node netmask");
		return STATUS_ERR;
	}
	memcpy(tmp_mask, mask, mask_sz);
	free(node->mask);
	node->mask = tmp_mask;
	node->mask_sz = mask_sz;
	return STATUS_SUCCESS;
}


/* Protocol */
int sepol_node_get_proto(const sepol_node_t * node)
{

	return node->proto;
}


void sepol_node_set_proto(sepol_node_t * node, int proto)
{

	node->proto = proto;
}


const char *sepol_node_get_proto_str(int proto)
{

	switch (proto) {
	case SEPOL_PROTO_IP4:
		return "ipv4";
	case SEPOL_PROTO_IP6:
		return "ipv6";
	default:
		return "???";
	}
}


/* Create */
int sepol_node_create(sepol_handle_t * handle, sepol_node_t ** node)
{

	sepol_node_t *tmp_node = (sepol_node_t *) malloc(sizeof(sepol_node_t));

	if (!tmp_node) {
		ERR(handle, "out of memory, could not create " "node record");
		return STATUS_ERR;
	}

	tmp_node->addr = NULL;
	tmp_node->addr_sz = 0;
	tmp_node->mask = NULL;
	tmp_node->mask_sz = 0;
	tmp_node->proto = SEPOL_PROTO_IP4;
	tmp_node->con = NULL;
	*node = tmp_node;

	return STATUS_SUCCESS;
}


/* Deep copy clone */
int sepol_node_clone(sepol_handle_t * handle,
		     const sepol_node_t * node, sepol_node_t ** node_ptr)
{

	sepol_node_t *new_node = NULL;
	if (sepol_node_create(handle, &new_node) < 0)
		goto err;

	/* Copy address, mask, protocol */
	new_node->addr = malloc(node->addr_sz);
	new_node->mask = malloc(node->mask_sz);
	if (!new_node->addr || !new_node->mask)
		goto omem;

	memcpy(new_node->addr, node->addr, node->addr_sz);
	memcpy(new_node->mask, node->mask, node->mask_sz);
	new_node->addr_sz = node->addr_sz;
	new_node->mask_sz = node->mask_sz;
	new_node->proto = node->proto;

	/* Copy context */
	if (node->con &&
	    (sepol_context_clone(handle, node->con, &new_node->con) < 0))
		goto err;

	*node_ptr = new_node;
	return STATUS_SUCCESS;

      omem:
	ERR(handle, "out of memory");

      err:
	ERR(handle, "could not clone node record");
	sepol_node_free(new_node);
	return STATUS_ERR;
}

/* Destroy */
void sepol_node_free(sepol_node_t * node)
{

	if (!node)
		return;

	sepol_context_free(node->con);
	free(node->addr);
	free(node->mask);
	free(node);
}


/* Context */
sepol_context_t *sepol_node_get_con(const sepol_node_t * node)
{

	return node->con;
}


int sepol_node_set_con(sepol_handle_t * handle,
		       sepol_node_t * node, sepol_context_t * con)
{

	sepol_context_t *newcon;

	if (sepol_context_clone(handle, con, &newcon) < 0) {
		ERR(handle, "out of memory, could not set node context");
		return STATUS_ERR;
	}

	sepol_context_free(node->con);
	node->con = newcon;
	return STATUS_SUCCESS;
}


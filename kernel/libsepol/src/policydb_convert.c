// #include <stdlib.h>

#include "private.h"
#include "debug.h"

#include <sepol/policydb/policydb.h>

/* Construct a policydb from the supplied (data, len) pair */

int policydb_from_image(sepol_handle_t * handle,
			void *data, size_t len, policydb_t * policydb)
{

	policy_file_t pf;

	policy_file_init(&pf);
	pf.type = PF_USE_MEMORY;
	pf.data = data;
	pf.len = len;
	pf.handle = handle;

	if (ksu_policydb_read(policydb, &pf, 0)) {
		ksu_policydb_destroy(policydb);
		ERR(handle, "policy image is invalid");
		// errno = EINVAL;
		return STATUS_ERR;
	}

	return STATUS_SUCCESS;
}

/* Write a policydb to a memory region, and return the (data, len) pair. */

int policydb_to_image(sepol_handle_t * handle,
		      policydb_t * policydb, void **newdata, size_t * newlen)
{

	void *tmp_data = NULL;
	size_t tmp_len;
	policy_file_t pf;
	struct policydb tmp_policydb;

	/* Compute the length for the new policy image. */
	policy_file_init(&pf);
	pf.type = PF_LEN;
	pf.handle = handle;
	if (ksu_policydb_write(policydb, &pf)) {
		ERR(handle, "could not compute policy length");
		// errno = EINVAL;
		goto err;
	}

	/* Allocate the new policy image. */
	pf.type = PF_USE_MEMORY;
	pf.data = malloc(pf.len);
	if (!pf.data) {
		ERR(handle, "out of memory");
		goto err;
	}

	/* Need to save len and data prior to modification by ksu_policydb_write. */
	tmp_len = pf.len;
	tmp_data = pf.data;

	/* Write out the new policy image. */
	if (ksu_policydb_write(policydb, &pf)) {
		ERR(handle, "could not write policy");
		// errno = EINVAL;
		goto err;
	}

	/* Verify the new policy image. */
	pf.type = PF_USE_MEMORY;
	pf.data = tmp_data;
	pf.len = tmp_len;
	if (policydb_init(&tmp_policydb)) {
		ERR(handle, "Out of memory");
		// errno = ENOMEM;
		goto err;
	}
	if (ksu_policydb_read(&tmp_policydb, &pf, 0)) {
		ERR(handle, "new policy image is invalid");
		// errno = EINVAL;
		goto err;
	}
	ksu_policydb_destroy(&tmp_policydb);

	/* Update (newdata, newlen) */
	*newdata = tmp_data;
	*newlen = tmp_len;

	/* Recover */
	return STATUS_SUCCESS;

      err:
	ERR(handle, "could not create policy image");

	/* Recover */
	free(tmp_data);
	return STATUS_ERR;
}

/*
 * Author: Mary Garvin <mgarvin@tresys.com>
 *
 * Copyright (C) 2007-2008 Tresys Technology, LLC
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "test-downgrade.h"
#include "parse_util.h"
#include "helpers.h"

#include <sepol/debug.h>
#include <sepol/handle.h>
#include <sepol/policydb/policydb.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/expand.h>
#include <sepol/policydb/conditional.h>
#include <limits.h>
#include <CUnit/Basic.h>

#define POLICY_BIN_HI	"policies/test-downgrade/policy.hi"
#define POLICY_BIN_LO	"policies/test-downgrade/policy.lo"

static policydb_t policydb;

/*
 * Function Name:  downgrade_test_init
 *
 * Input: None
 *
 * Output: None
 *
 * Description: Initialize the policydb (policy data base structure)
 */
int downgrade_test_init(void)
{
	/* Initialize the policydb_t structure */
	if (policydb_init(&policydb)) {
		fprintf(stderr, "%s:  Out of memory!\n", __FUNCTION__);
		return -1;
	}

	return 0;
}

/*
 * Function Name:  downgrade_test_cleanup
 *
 * Input: None
 *
 * Output: None
 *
 * Description: Destroys policydb structure
 */
int downgrade_test_cleanup(void)
{
	ksu_policydb_destroy(&policydb);

	return 0;
}

/*
 * Function Name: downgrade_add_tests
 *
 * Input: CU_pSuite
 *
 * Output: Returns 0 upon success.  Returns a CUnit error value on failure.
 *
 * Description:  Add the given downgrade tests to the downgrade suite.
 */
int downgrade_add_tests(CU_pSuite suite)
{
	if (CU_add_test(suite, "downgrade", test_downgrade) == NULL)
		return CU_get_error();

	return 0;
}

/*
 * Function Name:  test_downgrade_possible
 *
 * Input: None
 *
 * Output: None
 *
 * Description:
 * Tests the backward compatibility of MLS and Non-MLS binary policy versions.
 */
void test_downgrade(void)
{
	if (do_downgrade_test(0) < 0)
		fprintf(stderr,
		        "\nError during downgrade testing of Non-MLS policy\n");


	if (do_downgrade_test(1) < 0)
		fprintf(stderr,
			"\nError during downgrade testing of MLS policy\n");
}

/*
 * Function Name:  do_downgrade_test
 *
 * Input: 0 for Non-MLS policy and 1 for MLS policy downgrade testing
 *
 * Output: 0 on success, negative number upon failure
 *
 * Description: This function handles the downgrade testing.
 *              A binary policy is read into the policydb structure, the
 *              policy version is decreased by a specific amount, written
 *              back out and then read back in again.  The process is
 *              repeated until the minimum policy version is reached.
 */
int do_downgrade_test(int mls)
{
	policydb_t policydb_tmp;
	int hi, lo, version;

	/* Reset policydb for re-use */
	ksu_policydb_destroy(&policydb);
	downgrade_test_init();

	/* Read in the hi policy from file */
	if (read_binary_policy(POLICY_BIN_HI, &policydb) != 0) {
		fprintf(stderr, "error reading %spolicy binary\n", mls ? "mls " : "");
		CU_FAIL("Unable to read the binary policy");
		return -1;
	}

	/* Change MLS value based on parameter */
	policydb.mls = mls ? 1 : 0;

	for (hi = policydb.policyvers; hi >= POLICYDB_VERSION_MIN; hi--) {
		/* Stash old version number */
		version = policydb.policyvers;

		/* Try downgrading to each possible version. */
		for (lo = hi - 1; lo >= POLICYDB_VERSION_MIN; lo--) {

			/* Reduce policy version */
			policydb.policyvers = lo;

			/* Write out modified binary policy */
			if (write_binary_policy(POLICY_BIN_LO, &policydb) != 0) {
				/*
				 * Error from MLS to pre-MLS is expected due
				 * to MLS re-implementation in version 19.
				 */
				if (mls && lo < POLICYDB_VERSION_MLS)
					continue;

				fprintf(stderr, "error writing %spolicy binary, version %d (downgraded from %d)\n", mls ? "mls " : "", lo, hi);
				CU_FAIL("Failed to write downgraded binary policy");
					return -1;
			}

			/* Make sure we can read back what we wrote. */
			if (policydb_init(&policydb_tmp)) {
				fprintf(stderr, "%s:  Out of memory!\n",
					__FUNCTION__);
				return -1;
			}
			if (read_binary_policy(POLICY_BIN_LO, &policydb_tmp) != 0) {
				fprintf(stderr, "error reading %spolicy binary, version %d (downgraded from %d)\n", mls ? "mls " : "", lo, hi);
				CU_FAIL("Unable to read downgraded binary policy");
				return -1;
			}
			ksu_policydb_destroy(&policydb_tmp);
		}
		/* Restore version number */
		policydb.policyvers = version;
    }

    return 0;
}

/*
 * Function Name: read_binary_policy
 *
 * Input: char * which is the path to the file containing the binary policy
 *
 * Output: Returns 0 upon success.  Upon failure, -1 is returned.
 *	   Possible failures are, filename with given path does not exist,
 *	   a failure to open the file, or a failure from prolicydb_read
 *	   function call.
 *
 * Description:  Get a filename, open file and read binary policy into policydb
 * 				 structure.
 */
int read_binary_policy(const char *path, policydb_t *p)
{
	FILE *in_fp = NULL;
	struct policy_file f;
	int rc;

	/* Open the binary policy file */
	if ((in_fp = fopen(path, "rb")) == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n", path,
			strerror(errno));
		return -1;
	}

	/* Read in the binary policy.  */
	memset(&f, 0, sizeof(struct policy_file));
	f.type = PF_USE_STDIO;
	f.fp = in_fp;
	rc = ksu_policydb_read(p, &f, 0);

	fclose(in_fp);
	return rc;
}

/*
 * Function Name: write_binary_policy
 *
 * Input: char * which is the path to the file containing the binary policy
 *
 * Output: Returns 0 upon success.  Upon failure, -1 is returned.
 *	   Possible failures are, filename with given path does not exist,
 *	   a failure to open the file, or a failure from prolicydb_read
 *	   function call.
 *
 * Description:  open file and write the binary policy from policydb structure.
 */
int write_binary_policy(const char *path, policydb_t *p)
{
	FILE *out_fp = NULL;
	struct policy_file f;
	sepol_handle_t *handle;
	int rc;

	/* We don't want libsepol to print warnings to stderr */
	handle = sepol_handle_create();
	if (handle == NULL) {
		fprintf(stderr, "Out of memory!\n");
		return -1;
	}
	sepol_msg_set_callback(handle, NULL, NULL);

	/* Open the binary policy file for writing */
	if ((out_fp = fopen(path, "w" )) == NULL) {
		fprintf(stderr, "Unable to open %s: %s\n", path,
			strerror(errno));
		sepol_handle_destroy(handle);
		return -1;
	}

	/* Write the binary policy */
	memset(&f, 0, sizeof(struct policy_file));
	f.type = PF_USE_STDIO;
	f.fp = out_fp;
	f.handle = handle;
	rc = ksu_policydb_write(p, &f);

	sepol_handle_destroy(f.handle);
	fclose(out_fp);
	return rc;
}

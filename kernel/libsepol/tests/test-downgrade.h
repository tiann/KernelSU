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

#ifndef __TEST_DOWNGRADE_H__
#define __TEST_DOWNGRADE_H__

#include <CUnit/Basic.h>
#include <sepol/policydb/policydb.h>

/*
 * Function Name:  downgrade_test_init
 * 
 * Input: None
 * 
 * Output: None
 * 
 * Description: Initialize the policydb (policy data base structure)
 */
int downgrade_test_init(void);

/*
 * Function Name:  downgrade_test_cleanup
 * 
 * Input: None
 * 
 * Output: None
 * 
 * Description: Destroys policydb structure
 */
int downgrade_test_cleanup(void);

/*
 * Function Name: downgrade_add_tests
 * 
 * Input: CU_pSuite
 * 
 * Output: Returns 0 upon success.  Upon failure, a CUnit testing error
 *	   value is returned
 * 
 * Description:  Add the given downgrade tests to the downgrade suite.
 */
int downgrade_add_tests(CU_pSuite suite);

/*
 * Function Name: test_downgrade_possible
 * 
 * Input: None
 * 
 * Output: None
 * 
 * Description: Tests the backward compatibility of MLS and Non-MLS binary
 *		policy versions. 
 */
void test_downgrade(void);

/*
 * Function Name:  do_downgrade_test
 * 
 * Input: int that represents a 0 for Non-MLS policy and a 
 * 		 1 for MLS policy downgrade testing
 * 
 * Output: (int) 0 on success, negative number upon failure
 * 
 * Description: This function handles the downgrade testing.  A binary policy
 *		is read into the policydb structure, the policy version is
 *		decreased by a specific amount, written back out and then read
 *		back in again. The process is iterative until the minimum
 *		policy version is reached. 
 */
int do_downgrade_test(int mls);

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
 * Description: Get a filename, open file and read in the binary policy
 *		into the policydb structure.
 */
int read_binary_policy(const char *path, policydb_t *);

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
 * Description: Get a filename, open file and read in the binary policy
 *		into the policydb structure.
 */
int write_binary_policy(const char *path, policydb_t *);

#endif

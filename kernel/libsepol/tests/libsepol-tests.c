/*
 * Author: Karl MacMillan <kmacmillan@tresys.com>
 *
 * Copyright (C) 2006 Tresys Technology, LLC
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

#include "test-cond.h"
#include "test-linker.h"
#include "test-expander.h"
#include "test-deps.h"
#include "test-downgrade.h"

#include <CUnit/Basic.h>
#include <CUnit/Console.h>
#include <CUnit/TestDB.h>

#include <stdbool.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

int mls;

#define DECLARE_SUITE(name) \
	do { \
		suite = CU_add_suite(#name, name##_test_init, name##_test_cleanup); \
		if (NULL == suite) { \
			CU_cleanup_registry(); \
			return CU_get_error(); \
		} \
		if (name##_add_tests(suite)) { \
			CU_cleanup_registry(); \
			return CU_get_error(); \
		} \
	} while (0)

static void usage(char *progname)
{
	printf("usage:  %s [options]\n", progname);
	printf("options:\n");
	printf("\t-v, --verbose\t\t\tverbose output\n");
	printf("\t-i, --interactive\t\tinteractive console\n");
}

static bool do_tests(int interactive, int verbose)
{
	CU_pSuite suite = NULL;
	unsigned int num_failures;

	if (CUE_SUCCESS != CU_initialize_registry())
		return CU_get_error();

	DECLARE_SUITE(cond);
	DECLARE_SUITE(linker);
	DECLARE_SUITE(expander);
	DECLARE_SUITE(deps);
	DECLARE_SUITE(downgrade);

	if (verbose)
		CU_basic_set_mode(CU_BRM_VERBOSE);
	else
		CU_basic_set_mode(CU_BRM_NORMAL);

	if (interactive)
		CU_console_run_tests();
	else
		CU_basic_run_tests();
	num_failures = CU_get_number_of_tests_failed();
	CU_cleanup_registry();
	return CU_get_error() == CUE_SUCCESS && num_failures == 0;

}

int main(int argc, char **argv)
{
	int i, verbose = 1, interactive = 0;

	struct option opts[] = {
		{"verbose", 0, NULL, 'v'},
		{"interactive", 0, NULL, 'i'},
		{NULL, 0, NULL, 0}
	};

	while ((i = getopt_long(argc, argv, "vi", opts, NULL)) != -1) {
		switch (i) {
		case 'v':
			verbose = 1;
			break;
		case 'i':
			interactive = 1;
			break;
		case 'h':
		default:{
				usage(argv[0]);
				exit(1);
			}
		}
	}

	/* first do the non-mls tests */
	mls = 0;
	if (!do_tests(interactive, verbose))
		return -1;

	/* then with mls */
	mls = 1;
	if (!do_tests(interactive, verbose))
		return -1;

	return 0;
}

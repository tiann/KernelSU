/*
 * Copyright 2011 Tresys Technology, LLC. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *    2. Redistributions in binary form must reproduce the above copyright notice,
 *       this list of conditions and the following disclaimer in the documentation
 *       and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY TRESYS TECHNOLOGY, LLC ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL TRESYS TECHNOLOGY, LLC OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#include <sepol/policydb/policydb.h>

#include "CuTest.h"
#include "test_integration.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void test_integration(CuTest *tc) {
	int status = 0, status1 = 0, status2 = 0;

	status = system("./secilc -M -c 24 test/integration.cil &> /dev/null");

	if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGINT || WTERMSIG(status) == SIGQUIT))
		printf("Call to system for secilc failed.\n");
	
	status1 = system("checkpolicy -M -c 24 -o policy.conf.24 test/policy.conf &> /dev/null");

	if (WIFSIGNALED(status1) && (WTERMSIG(status1) == SIGINT || WTERMSIG(status1) == SIGQUIT))
		printf("Call to checkpolicy failed.\n");
	
	status2 = system("sediff -q policy.24 \\; policy.conf.24 &> /dev/null");

	if (WIFSIGNALED(status2) && (WTERMSIG(status2) == SIGINT || WTERMSIG(status2) == SIGQUIT))
		printf("Call to sediff for secilc failed.\n");
	
	CuAssertIntEquals(tc, 1, WIFEXITED(status));
	CuAssertIntEquals(tc, 0, WEXITSTATUS(status));
	CuAssertIntEquals(tc, 1, WIFEXITED(status1));
	CuAssertIntEquals(tc, 0, WEXITSTATUS(status1));
	CuAssertIntEquals(tc, 1, WIFEXITED(status2));
	CuAssertIntEquals(tc, 0, WEXITSTATUS(status2));
}

void test_min_policy(CuTest *tc) {
	int status = 0;

	status = system("./secilc -M -c 24 test/policy.cil &> /dev/null");

	if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGINT || WTERMSIG(status) == SIGQUIT))
		printf("Call to system for secilc failed.\n");
	
	CuAssertIntEquals(tc, 1, WIFEXITED(status));
	CuAssertIntEquals(tc, 0, WEXITSTATUS(status));
}

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

#include <stdio.h>
#include "CuTest.h"
#include "../../src/cil_log.h"

CuSuite* CilTreeGetSuite(void);
CuSuite* CilTreeGetResolveSuite(void);
CuSuite* CilTreeGetBuildSuite(void);
CuSuite* CilTestFullCil(void);

void RunAllTests(void) {
    /* disable cil log output */
    cil_set_log_level(0);

    CuString *output  = CuStringNew();
    CuSuite* suite = CuSuiteNew();
    CuSuite* suiteResolve = CuSuiteNew();
    CuSuite* suiteBuild = CuSuiteNew(); 
    CuSuite* suiteIntegration = CuSuiteNew();

    CuSuiteAddSuite(suite, CilTreeGetSuite());
    CuSuiteAddSuite(suiteResolve, CilTreeGetResolveSuite());
    CuSuiteAddSuite(suiteBuild, CilTreeGetBuildSuite());
    CuSuiteAddSuite(suiteIntegration, CilTestFullCil());

    CuSuiteRun(suite);
    CuSuiteDetails(suite, output);
    CuSuiteSummary(suite, output);

    CuSuiteRun(suiteResolve);
    CuSuiteDetails(suiteResolve, output);
    CuSuiteSummary(suiteResolve, output);

    CuSuiteRun(suiteBuild);
    CuSuiteDetails(suiteBuild, output);
    CuSuiteSummary(suiteBuild, output);

    CuSuiteRun(suiteIntegration);
    CuSuiteDetails(suiteIntegration, output);
    CuSuiteSummary(suiteIntegration, output);
    printf("\n%s\n", output->buffer);
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[]) {
    RunAllTests();

    return 0;
}

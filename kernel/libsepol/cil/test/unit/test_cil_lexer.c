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
#include "test_cil_lexer.h"

#include "../../src/cil_lexer.h"

void test_cil_lexer_setup(CuTest *tc) {
   char *test_str = "(test \"qstring\");comment\n";
   uint32_t str_size = strlen(test_str);
   char *buffer = malloc(str_size + 2);

   memset(buffer+str_size, 0, 2);
   strncpy(buffer, test_str, str_size);

   int rc = cil_lexer_setup(buffer, str_size + 2);
   CuAssertIntEquals(tc, SEPOL_OK, rc);

   free(buffer);
}

void test_cil_lexer_next(CuTest *tc) {
   char *test_str = "(test \"qstring\") ;comment\n";
   uint32_t str_size = strlen(test_str);
   char *buffer = malloc(str_size + 2);

   memset(buffer+str_size, 0, 2);
   strcpy(buffer, test_str);

   cil_lexer_setup(buffer, str_size + 2);

   struct token test_tok;

   int rc = cil_lexer_next(&test_tok);
   CuAssertIntEquals(tc, SEPOL_OK, rc);

   CuAssertIntEquals(tc, OPAREN, test_tok.type);
   CuAssertStrEquals(tc, "(", test_tok.value);
   CuAssertIntEquals(tc, 1, test_tok.line);

   rc = cil_lexer_next(&test_tok);
   CuAssertIntEquals(tc, SEPOL_OK, rc);
   
   CuAssertIntEquals(tc, SYMBOL, test_tok.type);
   CuAssertStrEquals(tc, "test", test_tok.value);
   CuAssertIntEquals(tc, 1, test_tok.line);
 
   rc = cil_lexer_next(&test_tok);
   CuAssertIntEquals(tc, SEPOL_OK, rc);
   
   CuAssertIntEquals(tc, QSTRING, test_tok.type);
   CuAssertStrEquals(tc, "\"qstring\"", test_tok.value);
   CuAssertIntEquals(tc, 1, test_tok.line);
 
   rc = cil_lexer_next(&test_tok);
   CuAssertIntEquals(tc, SEPOL_OK, rc);
   
   CuAssertIntEquals(tc, CPAREN, test_tok.type);
   CuAssertStrEquals(tc, ")", test_tok.value);
   CuAssertIntEquals(tc, 1, test_tok.line);

   rc = cil_lexer_next(&test_tok);
   CuAssertIntEquals(tc, SEPOL_OK, rc);
  
   CuAssertIntEquals(tc, COMMENT, test_tok.type);
   CuAssertStrEquals(tc, ";comment", test_tok.value);
   CuAssertIntEquals(tc, 1, test_tok.line);

   free(buffer);
}


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

#ifndef TEST_CIL_POLICY_H_
#define TEST_CIL_POLICY_H_

#include "CuTest.h"

void test_cil_post_filecon_compare_meta_a_not_b(CuTest *tc);
void test_cil_post_filecon_compare_meta_b_not_a(CuTest *tc);
void test_cil_post_filecon_compare_meta_a_and_b_strlen_a_greater_b(CuTest *tc);
void test_cil_post_filecon_compare_meta_a_and_b_strlen_b_greater_a(CuTest *tc);
void test_cil_post_filecon_compare_type_atype_greater_btype(CuTest *tc);
void test_cil_post_filecon_compare_type_btype_greater_atype(CuTest *tc);
void test_cil_post_filecon_compare_stemlen_a_greater_b(CuTest *tc);
void test_cil_post_filecon_compare_stemlen_b_greater_a(CuTest *tc);
void test_cil_post_filecon_compare_equal(CuTest *tc);

void test_cil_post_portcon_compare_atotal_greater_btotal(CuTest *tc);
void test_cil_post_portcon_compare_btotal_greater_atotal(CuTest *tc);
void test_cil_post_portcon_compare_aportlow_greater_bportlow(CuTest *tc);
void test_cil_post_portcon_compare_bportlow_greater_aportlow(CuTest *tc);
void test_cil_post_portcon_compare_equal(CuTest *tc);

void test_cil_post_genfscon_compare_atypestr_greater_btypestr(CuTest *tc);
void test_cil_post_genfscon_compare_btypestr_greater_atypestr(CuTest *tc);
void test_cil_post_genfscon_compare_apathstr_greater_bpathstr(CuTest *tc);
void test_cil_post_genfscon_compare_bpathstr_greater_apathstr(CuTest *tc);
void test_cil_post_genfscon_compare_equal(CuTest *tc);

void test_cil_post_netifcon_compare_a_greater_b(CuTest *tc);
void test_cil_post_netifcon_compare_b_greater_a(CuTest *tc);
void test_cil_post_netifcon_compare_equal(CuTest *tc);

void test_cil_post_nodecon_compare_aipv4_bipv6(CuTest *tc);
void test_cil_post_nodecon_compare_aipv6_bipv4(CuTest *tc);
void test_cil_post_nodecon_compare_aipv4_greaterthan_bipv4(CuTest *tc);
void test_cil_post_nodecon_compare_aipv4_lessthan_bipv4(CuTest *tc);
void test_cil_post_nodecon_compare_amaskipv4_greaterthan_bmaskipv4(CuTest *tc);
void test_cil_post_nodecon_compare_amaskipv4_lessthan_bmaskipv4(CuTest *tc);
void test_cil_post_nodecon_compare_aipv6_greaterthan_bipv6(CuTest *tc);
void test_cil_post_nodecon_compare_aipv6_lessthan_bipv6(CuTest *tc);
void test_cil_post_nodecon_compare_amaskipv6_greaterthan_bmaskipv6(CuTest *tc);
void test_cil_post_nodecon_compare_amaskipv6_lessthan_bmaskipv6(CuTest *tc);

void test_cil_post_fsuse_compare_type_a_greater_b(CuTest *tc);
void test_cil_post_fsuse_compare_type_b_greater_a(CuTest *tc);
void test_cil_post_fsuse_compare_fsstr_a_greater_b(CuTest *tc);
void test_cil_post_fsuse_compare_fsstr_b_greater_a(CuTest *tc);
void test_cil_post_fsuse_compare_equal(CuTest *tc);

#endif


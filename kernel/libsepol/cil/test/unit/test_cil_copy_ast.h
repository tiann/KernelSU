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

#ifndef TEST_CIL_COPY_AST_H_
#define TEST_CIL_COPY_AST_H_

#include "CuTest.h"

void test_cil_copy_list(CuTest *);
void test_cil_copy_list_sublist(CuTest *);
void test_cil_copy_list_sublist_extra(CuTest *);
void test_cil_copy_list_orignull_neg(CuTest *);

void test_cil_copy_block(CuTest *);
void test_cil_copy_node_helper_block(CuTest *tc); 
void test_cil_copy_node_helper_block_merge(CuTest *tc); 

void test_cil_copy_perm(CuTest *);
void test_cil_copy_node_helper_perm(CuTest *tc); 
void test_cil_copy_node_helper_perm_neg(CuTest *tc); 

void test_cil_copy_class(CuTest *);
void test_cil_copy_node_helper_class(CuTest *tc); 
void test_cil_copy_node_helper_class_dup_neg(CuTest *tc); 

void test_cil_copy_common(CuTest *);
void test_cil_copy_node_helper_common(CuTest *tc); 
void test_cil_copy_node_helper_common_dup_neg(CuTest *tc); 

void test_cil_copy_classcommon(CuTest *);
void test_cil_copy_node_helper_classcommon(CuTest *tc); 

void test_cil_copy_sid(CuTest *);
void test_cil_copy_node_helper_sid(CuTest *tc); 
void test_cil_copy_node_helper_sid_merge(CuTest *tc); 

void test_cil_copy_sidcontext(CuTest *);
void test_cil_copy_node_helper_sidcontext(CuTest *tc); 

void test_cil_copy_user(CuTest *);
void test_cil_copy_node_helper_user(CuTest *tc); 
void test_cil_copy_node_helper_user_merge(CuTest *tc); 

void test_cil_copy_role(CuTest *);
void test_cil_copy_node_helper_role(CuTest *tc); 
void test_cil_copy_node_helper_role_merge(CuTest *tc); 

void test_cil_copy_userrole(CuTest *);
void test_cil_copy_node_helper_userrole(CuTest *tc); 

void test_cil_copy_type(CuTest *);
void test_cil_copy_node_helper_type(CuTest *tc); 
void test_cil_copy_node_helper_type_merge(CuTest *tc); 

void test_cil_copy_typeattribute(CuTest *);
void test_cil_copy_node_helper_typeattribute(CuTest *tc); 
void test_cil_copy_node_helper_typeattribute_merge(CuTest *tc); 

void test_cil_copy_typealias(CuTest *);
void test_cil_copy_node_helper_typealias(CuTest *tc); 
void test_cil_copy_node_helper_typealias_dup_neg(CuTest *tc); 

void test_cil_copy_bool(CuTest *);
void test_cil_copy_node_helper_bool(CuTest *tc); 
void test_cil_copy_node_helper_bool_dup_neg(CuTest *tc); 

void test_cil_copy_avrule(CuTest *);
void test_cil_copy_node_helper_avrule(CuTest *tc); 

void test_cil_copy_type_rule(CuTest *);
void test_cil_copy_node_helper_type_rule(CuTest *tc); 

void test_cil_copy_sens(CuTest *);
void test_cil_copy_node_helper_sens(CuTest *tc); 
void test_cil_copy_node_helper_sens_merge(CuTest *tc); 

void test_cil_copy_sensalias(CuTest *);
void test_cil_copy_node_helper_sensalias(CuTest *tc); 
void test_cil_copy_node_helper_sensalias_dup_neg(CuTest *tc); 

void test_cil_copy_cat(CuTest *);
void test_cil_copy_node_helper_cat(CuTest *tc); 
void test_cil_copy_node_helper_cat_merge(CuTest *tc); 

void test_cil_copy_catalias(CuTest *);
void test_cil_copy_node_helper_catalias(CuTest *tc); 
void test_cil_copy_node_helper_catalias_dup_neg(CuTest *tc); 

void test_cil_copy_senscat(CuTest *);
void test_cil_copy_node_helper_senscat(CuTest *tc); 

void test_cil_copy_catorder(CuTest *);
void test_cil_copy_node_helper_catorder(CuTest *tc); 

void test_cil_copy_dominance(CuTest *);
void test_cil_copy_node_helper_dominance(CuTest *tc); 

void test_cil_copy_level(CuTest *);
void test_cil_copy_node_helper_level(CuTest *tc); 
void test_cil_copy_node_helper_level_dup_neg(CuTest *tc); 

void test_cil_copy_fill_level(CuTest *);

void test_cil_copy_context(CuTest *);
void test_cil_copy_node_helper_context(CuTest *tc); 
void test_cil_copy_node_helper_context_dup_neg(CuTest *tc); 

void test_cil_copy_netifcon(CuTest *);
void test_cil_copy_netifcon_nested(CuTest *);
void test_cil_copy_node_helper_netifcon(CuTest *tc); 
void test_cil_copy_node_helper_netifcon_merge(CuTest *tc); 

void test_cil_copy_fill_context(CuTest *);
void test_cil_copy_fill_context_anonrange(CuTest *);

void test_cil_copy_call(CuTest *);
void test_cil_copy_node_helper_call(CuTest *tc); 

void test_cil_copy_optional(CuTest *);
void test_cil_copy_node_helper_optional(CuTest *tc); 
void test_cil_copy_node_helper_optional_merge(CuTest *tc); 

void test_cil_copy_nodecon(CuTest *);
void test_cil_copy_nodecon_anon(CuTest *);

void test_cil_copy_fill_ipaddr(CuTest *);

void test_cil_copy_ipaddr(CuTest *);
void test_cil_copy_node_helper_ipaddr(CuTest *tc); 
void test_cil_copy_node_helper_ipaddr_dup_neg(CuTest *tc); 

void test_cil_copy_conditional(CuTest *);

void test_cil_copy_boolif(CuTest *);
void test_cil_copy_node_helper_boolif(CuTest *tc); 

void test_cil_copy_constrain(CuTest *);
void test_cil_copy_node_helper_mlsconstrain(CuTest *tc); 

void test_cil_copy_ast(CuTest *);
void test_cil_copy_ast_neg(CuTest *);

void test_cil_copy_node_helper_orignull_neg(CuTest *tc); 
void test_cil_copy_node_helper_extraargsnull_neg(CuTest *tc); 

void test_cil_copy_data_helper(CuTest *tc); 
void test_cil_copy_data_helper_getparentsymtab_neg(CuTest *tc); 
void test_cil_copy_data_helper_duplicatedb_neg(CuTest *tc); 

#endif

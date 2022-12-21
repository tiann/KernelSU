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

#ifndef TEST_CIL_BUILD_AST_H_
#define TEST_CIL_BUILD_AST_H_

#include "CuTest.h"

void test_cil_parse_to_list(CuTest *);
void test_cil_parse_to_list_currnull_neg(CuTest *);
void test_cil_parse_to_list_listnull_neg(CuTest *);

void test_cil_set_to_list(CuTest *);
void test_cil_set_to_list_tree_node_null_neg(CuTest *);
void test_cil_set_to_list_cl_head_null_neg(CuTest *);
void test_cil_set_to_list_listnull_neg(CuTest *);

void test_cil_gen_block(CuTest *);
void test_cil_gen_block_justblock_neg(CuTest *);
void test_cil_gen_block_noname_neg(CuTest *);
void test_cil_gen_block_dbnull_neg(CuTest *);
void test_cil_gen_block_treenull_neg(CuTest *);
void test_cil_gen_block_nodenull_neg(CuTest *);
void test_cil_gen_block_nodeparentnull_neg(CuTest *);
void test_cil_destroy_block(CuTest *);

void test_cil_gen_blockinherit(CuTest *);
void test_cil_gen_blockinherit_namelist_neg(CuTest *);
void test_cil_gen_blockinherit_namenull_neg(CuTest *);
void test_cil_gen_blockinherit_extra_neg(CuTest *);
void test_cil_gen_blockinherit_dbnull_neg(CuTest *);
void test_cil_gen_blockinherit_currnull_neg(CuTest *);
void test_cil_gen_blockinherit_astnull_neg(CuTest *);

void test_cil_gen_perm(CuTest *);
void test_cil_gen_perm_noname_neg(CuTest *);
void test_cil_gen_perm_dbnull_neg(CuTest *);
void test_cil_gen_perm_currnull_neg(CuTest *);
void test_cil_gen_perm_astnull_neg(CuTest *);
void test_cil_gen_perm_nodenull_neg(CuTest *);

void test_cil_gen_permset(CuTest *);
void test_cil_gen_permset_noname_neg(CuTest *);
void test_cil_gen_permset_nameinparens_neg(CuTest *);
void test_cil_gen_permset_noperms_neg(CuTest *);
void test_cil_gen_permset_emptyperms_neg(CuTest *);
void test_cil_gen_permset_extra_neg(CuTest *);
void test_cil_gen_permset_dbnull_neg(CuTest *);
void test_cil_gen_permset_currnull_neg(CuTest *);
void test_cil_gen_permset_astnull_neg(CuTest *);

void test_cil_gen_perm_nodes(CuTest *);
void test_cil_gen_perm_nodes_failgen_neg(CuTest *);
void test_cil_gen_perm_nodes_inval_perm_neg(CuTest *);

void test_cil_fill_permset(CuTest *);
void test_cil_fill_permset_sublist_neg(CuTest *);
void test_cil_fill_permset_startpermnull_neg(CuTest *);
void test_cil_fill_permset_permsetnull_neg(CuTest *);

void test_cil_gen_in(CuTest *);
void test_cil_gen_in_blockstrnull_neg(CuTest *);
void test_cil_gen_in_extra_neg(CuTest *);
void test_cil_gen_in_dbnull_neg(CuTest *);
void test_cil_gen_in_currnull_neg(CuTest *);
void test_cil_gen_in_astnull_neg(CuTest *);

void test_cil_gen_class(CuTest *);
void test_cil_gen_class_noname_neg(CuTest *);
void test_cil_gen_class_nodenull_neg(CuTest *);
void test_cil_gen_class_dbnull_neg(CuTest *);
void test_cil_gen_class_currnull_neg(CuTest *);
void test_cil_gen_class_noclass_neg(CuTest *);
void test_cil_gen_class_noclassname_neg(CuTest *);
void test_cil_gen_class_namesublist_neg(CuTest *);
void test_cil_gen_class_noperms(CuTest *);
void test_cil_gen_class_permsnotinlist_neg(CuTest *);
void test_cil_gen_class_extrapermlist_neg(CuTest *);
void test_cil_gen_class_listinlist_neg(CuTest *);

void test_cil_fill_classpermset_anonperms(CuTest *);
void test_cil_fill_classpermset_anonperms_neg(CuTest *);
void test_cil_fill_classpermset_namedperms(CuTest *);
void test_cil_fill_classpermset_extra_neg(CuTest *);
void test_cil_fill_classpermset_emptypermslist_neg(CuTest *);
void test_cil_fill_classpermset_noperms_neg(CuTest *);
void test_cil_fill_classpermset_noclass_neg(CuTest *);
void test_cil_fill_classpermset_classnodenull_neg(CuTest *);
void test_cil_fill_classpermset_cpsnull_neg(CuTest *);

void test_cil_gen_classpermset(CuTest *);
void test_cil_gen_classpermset_noname_neg(CuTest *);
void test_cil_gen_classpermset_nameinparens_neg(CuTest *);
void test_cil_gen_classpermset_noclass_neg(CuTest *);
void test_cil_gen_classpermset_noperms_neg(CuTest *);
void test_cil_gen_classpermset_emptyperms_neg(CuTest *);
void test_cil_gen_classpermset_extra_neg(CuTest *);
void test_cil_gen_classpermset_dbnull_neg(CuTest *);
void test_cil_gen_classpermset_currnull_neg(CuTest *);
void test_cil_gen_classpermset_astnull_neg(CuTest *);

void test_cil_gen_classmap_perm(CuTest *);
void test_cil_gen_classmap_perm_dupeperm_neg(CuTest *);
void test_cil_gen_classmap_perm_dbnull_neg(CuTest *tc);
void test_cil_gen_classmap_perm_currnull_neg(CuTest *tc);
void test_cil_gen_classmap_perm_astnull_neg(CuTest *tc);

void test_cil_gen_classmap(CuTest *);
void test_cil_gen_classmap_extra_neg(CuTest *);
void test_cil_gen_classmap_noname_neg(CuTest *);
void test_cil_gen_classmap_emptyperms_neg(CuTest *);
void test_cil_gen_classmap_dbnull_neg(CuTest *tc);
void test_cil_gen_classmap_currnull_neg(CuTest *tc);
void test_cil_gen_classmap_astnull_neg(CuTest *tc);

void test_cil_gen_classmapping_anonpermset(CuTest *);
void test_cil_gen_classmapping_anonpermset_neg(CuTest *);
void test_cil_gen_classmapping_namedpermset(CuTest *);
void test_cil_gen_classmapping_noclassmapname_neg(CuTest *);
void test_cil_gen_classmapping_noclassmapperm_neg(CuTest *);
void test_cil_gen_classmapping_nopermissionsets_neg(CuTest *);
void test_cil_gen_classmapping_emptyperms_neg(CuTest *);
void test_cil_gen_classmapping_dbnull_neg(CuTest *tc);
void test_cil_gen_classmapping_currnull_neg(CuTest *tc);
void test_cil_gen_classmapping_astnull_neg(CuTest *tc);

void test_cil_gen_common(CuTest *);
void test_cil_gen_common_dbnull_neg(CuTest *tc);
void test_cil_gen_common_currnull_neg(CuTest *tc);
void test_cil_gen_common_astnull_neg(CuTest *tc);
void test_cil_gen_common_noname_neg(CuTest *tc);
void test_cil_gen_common_twoperms_neg(CuTest *tc);
void test_cil_gen_common_permsublist_neg(CuTest *tc);
void test_cil_gen_common_noperms_neg(CuTest *tc);

void test_cil_gen_sid(CuTest *);
void test_cil_gen_sid_noname_neg(CuTest *);
void test_cil_gen_sid_nameinparens_neg(CuTest *);
void test_cil_gen_sid_extra_neg(CuTest *);
void test_cil_gen_sid_dbnull_neg(CuTest *);
void test_cil_gen_sid_currnull_neg(CuTest *);
void test_cil_gen_sid_astnull_neg(CuTest *);

void test_cil_gen_sidcontext(CuTest *);
void test_cil_gen_sidcontext_namedcontext(CuTest *);
void test_cil_gen_sidcontext_halfcontext_neg(CuTest *);
void test_cil_gen_sidcontext_noname_neg(CuTest *);
void test_cil_gen_sidcontext_empty_neg(CuTest *);
void test_cil_gen_sidcontext_nocontext_neg(CuTest *);
void test_cil_gen_sidcontext_dblname_neg(CuTest *);
void test_cil_gen_sidcontext_dbnull_neg(CuTest *);
void test_cil_gen_sidcontext_pcurrnull_neg(CuTest *);
void test_cil_gen_sidcontext_astnodenull_neg(CuTest *);

void test_cil_gen_type(CuTest *);
void test_cil_gen_type_neg(CuTest *);
void test_cil_gen_type_dbnull_neg(CuTest *tc);
void test_cil_gen_type_currnull_neg(CuTest *tc);
void test_cil_gen_type_astnull_neg(CuTest *tc);
void test_cil_gen_type_extra_neg(CuTest *tc);

void test_cil_gen_typeattribute(CuTest *);
void test_cil_gen_typeattribute_dbnull_neg(CuTest *tc);
void test_cil_gen_typeattribute_currnull_neg(CuTest *tc);
void test_cil_gen_typeattribute_astnull_neg(CuTest *tc);
void test_cil_gen_typeattribute_extra_neg(CuTest *tc);

void test_cil_gen_typeattr(CuTest *);
void test_cil_gen_typeattr_dbnull_neg(CuTest *);
void test_cil_gen_typeattr_currnull_neg(CuTest *);
void test_cil_gen_typeattr_astnull_neg(CuTest *);
void test_cil_gen_typeattr_typenull_neg(CuTest *);
void test_cil_gen_typeattr_attrnull_neg(CuTest *);
void test_cil_gen_typeattr_attrlist_neg(CuTest *);
void test_cil_gen_typeattr_extra_neg(CuTest *);

void test_cil_gen_typebounds(CuTest *);
void test_cil_gen_typebounds_notype1_neg(CuTest *);
void test_cil_gen_typebounds_type1inparens_neg(CuTest *);
void test_cil_gen_typebounds_notype2_neg(CuTest *);
void test_cil_gen_typebounds_type2inparens_neg(CuTest *);
void test_cil_gen_typebounds_extra_neg(CuTest *);
void test_cil_gen_typebounds_dbnull_neg(CuTest *);
void test_cil_gen_typebounds_currnull_neg(CuTest *);
void test_cil_gen_typebounds_astnull_neg(CuTest *);

void test_cil_gen_typepermissive(CuTest *);
void test_cil_gen_typepermissive_noname_neg(CuTest *);
void test_cil_gen_typepermissive_typeinparens_neg(CuTest *);
void test_cil_gen_typepermissive_extra_neg(CuTest *);
void test_cil_gen_typepermissive_dbnull_neg(CuTest *);
void test_cil_gen_typepermissive_currnull_neg(CuTest *);
void test_cil_gen_typepermissive_astnull_neg(CuTest *);

void test_cil_gen_nametypetransition(CuTest *);
void test_cil_gen_nametypetransition_nostr_neg(CuTest *);
void test_cil_gen_nametypetransition_strinparens_neg(CuTest *);
void test_cil_gen_nametypetransition_nosrc_neg(CuTest *);
void test_cil_gen_nametypetransition_srcinparens_neg(CuTest *);
void test_cil_gen_nametypetransition_notgt_neg(CuTest *);
void test_cil_gen_nametypetransition_tgtinparens_neg(CuTest *);
void test_cil_gen_nametypetransition_noclass_neg(CuTest *);
void test_cil_gen_nametypetransition_classinparens_neg(CuTest *);
void test_cil_gen_nametypetransition_nodest_neg(CuTest *);
void test_cil_gen_nametypetransition_destinparens_neg(CuTest *);
void test_cil_gen_nametypetransition_extra_neg(CuTest *);
void test_cil_gen_nametypetransition_dbnull_neg(CuTest *);
void test_cil_gen_nametypetransition_currnull_neg(CuTest *);
void test_cil_gen_nametypetransition_astnull_neg(CuTest *);

void test_cil_gen_rangetransition(CuTest *);
void test_cil_gen_rangetransition_namedtransition(CuTest *);
void test_cil_gen_rangetransition_anon_low_l(CuTest *);
void test_cil_gen_rangetransition_anon_low_l_neg(CuTest *);
void test_cil_gen_rangetransition_anon_high_l(CuTest *);
void test_cil_gen_rangetransition_anon_high_l_neg(CuTest *);
void test_cil_gen_rangetransition_dbnull_neg(CuTest *);
void test_cil_gen_rangetransition_currnull_neg(CuTest *);
void test_cil_gen_rangetransition_astnull_neg(CuTest *);
void test_cil_gen_rangetransition_nofirsttype_neg(CuTest *);
void test_cil_gen_rangetransition_firsttype_inparens_neg(CuTest *);
void test_cil_gen_rangetransition_nosecondtype_neg(CuTest *);
void test_cil_gen_rangetransition_secondtype_inparens_neg(CuTest *);
void test_cil_gen_rangetransition_noclass_neg(CuTest *);
void test_cil_gen_rangetransition_class_inparens_neg(CuTest *);
void test_cil_gen_rangetransition_nolevel_l_neg(CuTest *);
void test_cil_gen_rangetransition_nolevel_h_neg(CuTest *);
void test_cil_gen_rangetransition_extra_neg(CuTest *);

void test_cil_gen_expr_stack_and(CuTest *);
void test_cil_gen_expr_stack_or(CuTest *);
void test_cil_gen_expr_stack_xor(CuTest *);
void test_cil_gen_expr_stack_not(CuTest *);
void test_cil_gen_expr_stack_not_noexpr_neg(CuTest *);
void test_cil_gen_expr_stack_not_extraexpr_neg(CuTest *);
void test_cil_gen_expr_stack_eq(CuTest *);
void test_cil_gen_expr_stack_neq(CuTest *);
void test_cil_gen_expr_stack_nested(CuTest *);
void test_cil_gen_expr_stack_nested_neg(CuTest *);
void test_cil_gen_expr_stack_nested_emptyargs_neg(CuTest *);
void test_cil_gen_expr_stack_nested_missingoperator_neg(CuTest *);
void test_cil_gen_expr_stack_arg1null_neg(CuTest *);
void test_cil_gen_expr_stack_arg2null_neg(CuTest *);
void test_cil_gen_expr_stack_extraarg_neg(CuTest *);
void test_cil_gen_expr_stack_currnull_neg(CuTest *);
void test_cil_gen_expr_stack_stacknull_neg(CuTest *);

void test_cil_gen_boolif_multiplebools_true(CuTest *);
void test_cil_gen_boolif_multiplebools_false(CuTest *);
void test_cil_gen_boolif_multiplebools_unknowncond_neg(CuTest *);
void test_cil_gen_boolif_true(CuTest *);
void test_cil_gen_boolif_false(CuTest *);
void test_cil_gen_boolif_unknowncond_neg(CuTest *);
void test_cil_gen_boolif_nested(CuTest *);
void test_cil_gen_boolif_nested_neg(CuTest *);
void test_cil_gen_boolif_extra_neg(CuTest *);
void test_cil_gen_boolif_extra_parens_neg(CuTest *);
void test_cil_gen_boolif_nocond(CuTest *);
void test_cil_gen_boolif_neg(CuTest *);
void test_cil_gen_boolif_dbnull_neg(CuTest *);
void test_cil_gen_boolif_currnull_neg(CuTest *);
void test_cil_gen_boolif_astnull_neg(CuTest *);
void test_cil_gen_boolif_nocond_neg(CuTest *);
void test_cil_gen_boolif_notruelist_neg(CuTest *);
void test_cil_gen_boolif_empty_cond_neg(CuTest *);

void test_cil_gen_else(CuTest *);
void test_cil_gen_else_neg(CuTest *);
void test_cil_gen_else_dbnull_neg(CuTest *);
void test_cil_gen_else_currnull_neg(CuTest *);
void test_cil_gen_else_astnull_neg(CuTest *);

void test_cil_gen_tunif_multiplebools_true(CuTest *);
void test_cil_gen_tunif_multiplebools_false(CuTest *);
void test_cil_gen_tunif_multiplebools_unknowncond_neg(CuTest *);
void test_cil_gen_tunif_true(CuTest *);
void test_cil_gen_tunif_false(CuTest *);
void test_cil_gen_tunif_unknowncond_neg(CuTest *);
void test_cil_gen_tunif_nocond(CuTest *);
void test_cil_gen_tunif_nested(CuTest *);
void test_cil_gen_tunif_nested_neg(CuTest *);
void test_cil_gen_tunif_extra_neg(CuTest *);
void test_cil_gen_tunif_extra_parens_neg(CuTest *);
void test_cil_gen_tunif_neg(CuTest *);
void test_cil_gen_tunif_dbnull_neg(CuTest *);
void test_cil_gen_tunif_currnull_neg(CuTest *);
void test_cil_gen_tunif_astnull_neg(CuTest *);
void test_cil_gen_tunif_nocond_neg(CuTest *);
void test_cil_gen_tunif_notruelist_neg(CuTest *);

void test_cil_gen_condblock_true(CuTest *);
void test_cil_gen_condblock_false(CuTest *);
void test_cil_gen_condblock_dbnull_neg(CuTest *);
void test_cil_gen_condblock_currnull_neg(CuTest *);
void test_cil_gen_condblock_astnull_neg(CuTest *);
void test_cil_gen_condblock_nocond_neg(CuTest *);
void test_cil_gen_condblock_extra_neg(CuTest *);

void test_cil_gen_typealias(CuTest *);
void test_cil_gen_typealias_incomplete_neg(CuTest *);
void test_cil_gen_typealias_incomplete_neg2(CuTest *);
void test_cil_gen_typealias_extratype_neg(CuTest *);
void test_cil_gen_typealias_dbnull_neg(CuTest *tc);
void test_cil_gen_typealias_currnull_neg(CuTest *tc);
void test_cil_gen_typealias_astnull_neg(CuTest *tc);

void test_cil_gen_typeattributeset(CuTest *);
void test_cil_gen_typeattributeset_and_two_types(CuTest *);
void test_cil_gen_typeattributeset_not(CuTest *);
void test_cil_gen_typeattributeset_exclude_attr(CuTest *);
void test_cil_gen_typeattributeset_exclude_neg(CuTest *);
void test_cil_gen_typeattributeset_dbnull_neg(CuTest *);
void test_cil_gen_typeattributeset_currnull_neg(CuTest *);
void test_cil_gen_typeattributeset_astnull_neg(CuTest *);
void test_cil_gen_typeattributeset_noname_neg(CuTest *);
void test_cil_gen_typeattributeset_nameinparens_neg(CuTest *);
void test_cil_gen_typeattributeset_emptylists_neg(CuTest *);
void test_cil_gen_typeattributeset_listinparens_neg(CuTest *);
void test_cil_gen_typeattributeset_extra_neg(CuTest *);

void test_cil_gen_userbounds(CuTest *);
void test_cil_gen_userbounds_notype1_neg(CuTest *);
void test_cil_gen_userbounds_type1_inparens_neg(CuTest *);
void test_cil_gen_userbounds_notype2_neg(CuTest *);
void test_cil_gen_userbounds_type2_inparens_neg(CuTest *);
void test_cil_gen_userbounds_extra_neg(CuTest *);
void test_cil_gen_userbounds_dbnull_neg(CuTest *);
void test_cil_gen_userbounds_currnull_neg(CuTest *);
void test_cil_gen_userbounds_astnull_neg(CuTest *);

void test_cil_gen_role(CuTest *);
void test_cil_gen_role_dbnull_neg(CuTest *tc);
void test_cil_gen_role_currnull_neg(CuTest *tc);
void test_cil_gen_role_astnull_neg(CuTest *tc);
void test_cil_gen_role_extrarole_neg(CuTest *tc);
void test_cil_gen_role_noname_neg(CuTest *tc);

void test_cil_gen_roletransition(CuTest *);
void test_cil_gen_roletransition_currnull_neg(CuTest *);
void test_cil_gen_roletransition_astnull_neg(CuTest *);
void test_cil_gen_roletransition_srcnull_neg(CuTest *);
void test_cil_gen_roletransition_tgtnull_neg(CuTest *);
void test_cil_gen_roletransition_resultnull_neg(CuTest *);
void test_cil_gen_roletransition_extra_neg(CuTest *);

void test_cil_gen_bool_true(CuTest *);
void test_cil_gen_bool_tunable_true(CuTest *);
void test_cil_gen_bool_false(CuTest *);
void test_cil_gen_bool_tunable_false(CuTest *);
void test_cil_gen_bool_none_neg(CuTest *);
void test_cil_gen_bool_dbnull_neg(CuTest *);
void test_cil_gen_bool_currnull_neg(CuTest *);
void test_cil_gen_bool_astnull_neg(CuTest *);
void test_cil_gen_bool_notbool_neg(CuTest *);
void test_cil_gen_bool_boolname_neg(CuTest *);
void test_cil_gen_bool_extraname_false_neg(CuTest *);
void test_cil_gen_bool_extraname_true_neg(CuTest *);

void test_cil_gen_constrain_expr_stack_eq2_t1type(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_t1t1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_t2type(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_t2t2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_r1role(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_r1r1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_r2role(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_r2r2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_t1t2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_r1r2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_r1r2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_u1u2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_u1user(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_u1u1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_u2user(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_u2u2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l2h2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l1l2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l1h1(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l1h2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_h1l2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_h1h2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_h1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l1l1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_l1l2_constrain_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_l1l2_constrain_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_leftkeyword_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_noexpr1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_expr1inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_noexpr2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_expr2inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_noexpr1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_expr1inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_noexpr2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_expr2inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_eq2_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_noteq(CuTest *);
void test_cil_gen_constrain_expr_stack_noteq_noexpr1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_noteq_expr1inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_noteq_noexpr2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_noteq_expr2inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_noteq_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_not(CuTest *);
void test_cil_gen_constrain_expr_stack_not_noexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_not_emptyparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_not_extraparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_or(CuTest *);
void test_cil_gen_constrain_expr_stack_or_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_or_noexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_or_emptyfirstparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_or_missingsecondexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_or_emptysecondparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_or_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_and(CuTest *);
void test_cil_gen_constrain_expr_stack_and_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_and_noexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_and_emptyfirstparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_and_missingsecondexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_and_emptysecondparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_and_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_dom(CuTest *);
void test_cil_gen_constrain_expr_stack_dom_noexpr1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_dom_expr1inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_dom_noexpr2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_dom_expr2inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_dom_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_domby(CuTest *);
void test_cil_gen_constrain_expr_stack_domby_noexpr1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_domby_expr1inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_domby_noexpr2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_domby_expr2inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_domby_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_incomp(CuTest *);
void test_cil_gen_constrain_expr_stack_incomp_noexpr1_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_incomp_expr1inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_incomp_noexpr2_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_incomp_expr2inparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_incomp_extraexpr_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_currnull_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_stacknull_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_operatorinparens_neg(CuTest *);
void test_cil_gen_constrain_expr_stack_incorrectcall_neg(CuTest *);

void test_cil_gen_roleallow(CuTest *);
void test_cil_gen_roleallow_dbnull_neg(CuTest *);
void test_cil_gen_roleallow_currnull_neg(CuTest *);
void test_cil_gen_roleallow_astnull_neg(CuTest *);
void test_cil_gen_roleallow_srcnull_neg(CuTest *);
void test_cil_gen_roleallow_tgtnull_neg(CuTest *);
void test_cil_gen_roleallow_extra_neg(CuTest *);

void test_cil_gen_rolebounds(CuTest *);
void test_cil_gen_rolebounds_norole1_neg(CuTest *);
void test_cil_gen_rolebounds_role1_inparens_neg(CuTest *);
void test_cil_gen_rolebounds_norole2_neg(CuTest *);
void test_cil_gen_rolebounds_role2_inparens_neg(CuTest *);
void test_cil_gen_rolebounds_extra_neg(CuTest *);
void test_cil_gen_rolebounds_dbnull_neg(CuTest *);
void test_cil_gen_rolebounds_currnull_neg(CuTest *);
void test_cil_gen_rolebounds_astnull_neg(CuTest *);

void test_cil_gen_avrule(CuTest *);
void test_cil_gen_avrule_permset(CuTest *);
void test_cil_gen_avrule_permset_anon(CuTest *);
void test_cil_gen_avrule_extra_neg(CuTest *);
void test_cil_gen_avrule_sourceparens(CuTest *);
void test_cil_gen_avrule_sourceemptyparen_neg(CuTest *);
void test_cil_gen_avrule_targetparens(CuTest *);
void test_cil_gen_avrule_targetemptyparen_neg(CuTest *);
void test_cil_gen_avrule_currnull_neg(CuTest *tc);
void test_cil_gen_avrule_astnull_neg(CuTest *tc);
void test_cil_gen_avrule_sourcedomainnull_neg(CuTest *tc);
void test_cil_gen_avrule_targetdomainnull_neg(CuTest *tc);
void test_cil_gen_avrule_objectclassnull_neg(CuTest *tc);
void test_cil_gen_avrule_permsnull_neg(CuTest *tc);
void test_cil_gen_avrule_twolists_neg(CuTest *);
//TODO: add cases to cover  parse_current->next->cl_head != NULL || parse_current->next->next->cl_head != NULL 
// || parse_current->next->next->next->cl_head != NULL

void test_cil_gen_type_rule_transition(CuTest *);
void test_cil_gen_type_rule_transition_currnull_neg(CuTest *);
void test_cil_gen_type_rule_transition_astnull_neg(CuTest *);
void test_cil_gen_type_rule_transition_srcnull_neg(CuTest *);
void test_cil_gen_type_rule_transition_tgtnull_neg(CuTest *);
void test_cil_gen_type_rule_transition_objnull_neg(CuTest *);
void test_cil_gen_type_rule_transition_resultnull_neg(CuTest *);
void test_cil_gen_type_rule_transition_extra_neg(CuTest *);

void test_cil_gen_type_rule_change(CuTest *);
void test_cil_gen_type_rule_change_currnull_neg(CuTest *);
void test_cil_gen_type_rule_change_astnull_neg(CuTest *);
void test_cil_gen_type_rule_change_srcnull_neg(CuTest *);
void test_cil_gen_type_rule_change_tgtnull_neg(CuTest *);
void test_cil_gen_type_rule_change_objnull_neg(CuTest *);
void test_cil_gen_type_rule_change_resultnull_neg(CuTest *);
void test_cil_gen_type_rule_change_extra_neg(CuTest *);

void test_cil_gen_type_rule_member(CuTest *); 
void test_cil_gen_type_rule_member_currnull_neg(CuTest *);
void test_cil_gen_type_rule_member_astnull_neg(CuTest *);
void test_cil_gen_type_rule_member_srcnull_neg(CuTest *);
void test_cil_gen_type_rule_member_tgtnull_neg(CuTest *);
void test_cil_gen_type_rule_member_objnull_neg(CuTest *);
void test_cil_gen_type_rule_member_resultnull_neg(CuTest *);
void test_cil_gen_type_rule_member_extra_neg(CuTest *);

void test_cil_gen_user(CuTest *);
void test_cil_gen_user_dbnull_neg(CuTest *);
void test_cil_gen_user_currnull_neg(CuTest *);
void test_cil_gen_user_astnull_neg(CuTest *);
void test_cil_gen_user_nouser_neg(CuTest *);
void test_cil_gen_user_xsinfo_neg(CuTest *);

void test_cil_gen_userlevel(CuTest *);
void test_cil_gen_userlevel_anon_level(CuTest *);
void test_cil_gen_userlevel_anon_level_neg(CuTest *);
void test_cil_gen_userlevel_usernull_neg(CuTest *);
void test_cil_gen_userlevel_userrange_neg(CuTest *);
void test_cil_gen_userlevel_levelnull_neg(CuTest *);
void test_cil_gen_userlevel_levelrangeempty_neg(CuTest *);
void test_cil_gen_userlevel_extra_neg(CuTest *);
void test_cil_gen_userlevel_dbnull_neg(CuTest *);
void test_cil_gen_userlevel_currnull_neg(CuTest *);
void test_cil_gen_userlevel_astnull_neg(CuTest *);

void test_cil_gen_userrange_named(CuTest *);
void test_cil_gen_userrange_anon(CuTest *);
void test_cil_gen_userrange_usernull_neg(CuTest *);
void test_cil_gen_userrange_anonuser_neg(CuTest *);
void test_cil_gen_userrange_rangenamenull_neg(CuTest *);
void test_cil_gen_userrange_anonrangeinvalid_neg(CuTest *);
void test_cil_gen_userrange_anonrangeempty_neg(CuTest *);
void test_cil_gen_userrange_extra_neg(CuTest *);
void test_cil_gen_userrange_dbnull_neg(CuTest *);
void test_cil_gen_userrange_currnull_neg(CuTest *);
void test_cil_gen_userrange_astnull_neg(CuTest *);

void test_cil_gen_sensitivity(CuTest *);
void test_cil_gen_sensitivity_dbnull_neg(CuTest *);
void test_cil_gen_sensitivity_currnull_neg(CuTest *);
void test_cil_gen_sensitivity_astnull_neg(CuTest *);
void test_cil_gen_sensitivity_sensnull_neg(CuTest *);
void test_cil_gen_sensitivity_senslist_neg(CuTest *);
void test_cil_gen_sensitivity_extra_neg(CuTest *);

void test_cil_gen_sensalias(CuTest *);
void test_cil_gen_sensalias_dbnull_neg(CuTest *);
void test_cil_gen_sensalias_currnull_neg(CuTest *);
void test_cil_gen_sensalias_astnull_neg(CuTest *);
void test_cil_gen_sensalias_sensnull_neg(CuTest *);
void test_cil_gen_sensalias_senslist_neg(CuTest *);
void test_cil_gen_sensalias_aliasnull_neg(CuTest *);
void test_cil_gen_sensalias_aliaslist_neg(CuTest *);
void test_cil_gen_sensalias_extra_neg(CuTest *);

void test_cil_gen_category(CuTest *);
void test_cil_gen_category_dbnull_neg(CuTest *); 
void test_cil_gen_category_astnull_neg(CuTest *);
void test_cil_gen_category_currnull_neg(CuTest *);
void test_cil_gen_category_catnull_neg(CuTest *);
void test_cil_gen_category_catlist_neg(CuTest *);
void test_cil_gen_category_extra_neg(CuTest *);

void test_cil_gen_catset(CuTest *);
void test_cil_gen_catset_dbnull_neg(CuTest *);
void test_cil_gen_catset_currnull_neg(CuTest *);
void test_cil_gen_catset_astnull_neg(CuTest *);
void test_cil_gen_catset_namenull_neg(CuTest *);
void test_cil_gen_catset_setnull_neg(CuTest *);
void test_cil_gen_catset_namelist_neg(CuTest *);
void test_cil_gen_catset_extra_neg(CuTest *);
void test_cil_gen_catset_nodefail_neg(CuTest *);
void test_cil_gen_catset_notset_neg(CuTest *);
void test_cil_gen_catset_settolistfail_neg(CuTest *);

void test_cil_gen_catalias(CuTest *);
void test_cil_gen_catalias_dbnull_neg(CuTest *);
void test_cil_gen_catalias_currnull_neg(CuTest *);
void test_cil_gen_catalias_astnull_neg(CuTest *);
void test_cil_gen_catalias_catnull_neg(CuTest *);
void test_cil_gen_catalias_aliasnull_neg(CuTest *);
void test_cil_gen_catalias_extra_neg(CuTest *);

void test_cil_gen_catrange(CuTest *);
void test_cil_gen_catrange_noname_neg(CuTest *);
void test_cil_gen_catrange_norange_neg(CuTest *);
void test_cil_gen_catrange_emptyrange_neg(CuTest *);
void test_cil_gen_catrange_extrarange_neg(CuTest *);
void test_cil_gen_catrange_dbnull_neg(CuTest *);
void test_cil_gen_catrange_currnull_neg(CuTest *);
void test_cil_gen_catrange_astnull_neg(CuTest *);
void test_cil_gen_catrange_extra_neg(CuTest *);

void test_cil_gen_roletype(CuTest *tc);
void test_cil_gen_roletype_currnull_neg(CuTest *tc);
void test_cil_gen_roletype_dbnull_neg(CuTest *tc);
void test_cil_gen_roletype_astnull_neg(CuTest *tc);
void test_cil_gen_roletype_empty_neg(CuTest *tc);
void test_cil_gen_roletype_rolelist_neg(CuTest *tc);
void test_cil_gen_roletype_roletype_sublist_neg(CuTest *tc);
void test_cil_gen_roletype_typelist_neg(CuTest *tc);

void test_cil_gen_userrole(CuTest *tc);
void test_cil_gen_userrole_currnull_neg(CuTest *tc);
void test_cil_gen_userrole_dbnull_neg(CuTest *tc);
void test_cil_gen_userrole_astnull_neg(CuTest *tc);
void test_cil_gen_userrole_empty_neg(CuTest *tc);
void test_cil_gen_userrole_userlist_neg(CuTest *tc);
void test_cil_gen_userrole_userrole_sublist_neg(CuTest *tc);
void test_cil_gen_userrole_rolelist_neg(CuTest *tc);

void test_cil_gen_classcommon(CuTest *tc);
void test_cil_gen_classcommon_dbnull_neg(CuTest *tc);
void test_cil_gen_classcommon_currnull_neg(CuTest *tc);
void test_cil_gen_classcommon_astnull_neg(CuTest *tc);
void test_cil_gen_classcommon_missingclassname_neg(CuTest *tc);
void test_cil_gen_classcommon_noperms_neg(CuTest *tc);
void test_cil_gen_classcommon_extraperms_neg(CuTest *tc);

void test_cil_gen_catorder(CuTest *tc);
void test_cil_gen_catorder_dbnull_neg(CuTest *tc);
void test_cil_gen_catorder_currnull_neg(CuTest *tc);
void test_cil_gen_catorder_astnull_neg(CuTest *tc);
void test_cil_gen_catorder_missingcats_neg(CuTest *tc);
void test_cil_gen_catorder_nosublist_neg(CuTest *tc);
void test_cil_gen_catorder_nestedcat_neg(CuTest *tc);

void test_cil_gen_dominance(CuTest *tc);
void test_cil_gen_dominance_dbnull_neg(CuTest *tc);
void test_cil_gen_dominance_currnull_neg(CuTest *tc);
void test_cil_gen_dominance_astnull_neg(CuTest *tc);
void test_cil_gen_dominance_nosensitivities_neg(CuTest *tc);
void test_cil_gen_dominance_nosublist_neg(CuTest *tc);

void test_cil_gen_senscat(CuTest *tc);
void test_cil_gen_senscat_nosublist(CuTest *);
void test_cil_gen_senscat_dbnull_neg(CuTest *tc);
void test_cil_gen_senscat_currnull_neg(CuTest *tc);
void test_cil_gen_senscat_astnull_neg(CuTest *tc);
void test_cil_gen_senscat_nosensitivities_neg(CuTest *tc);
void test_cil_gen_senscat_sublist_neg(CuTest *);
void test_cil_gen_senscat_nocat_neg(CuTest *);

void test_cil_fill_level(CuTest *tc);
void test_cil_fill_level_sensnull_neg(CuTest *tc);
void test_cil_fill_level_levelnull_neg(CuTest *tc);
void test_cil_fill_level_nocat(CuTest *tc);
void test_cil_fill_level_emptycat_neg(CuTest *tc);

void test_cil_gen_level(CuTest *tc);
void test_cil_gen_level_nameinparens_neg(CuTest *tc);
void test_cil_gen_level_emptysensparens_neg(CuTest *tc);
void test_cil_gen_level_extra_neg(CuTest *tc);
void test_cil_gen_level_emptycat_neg(CuTest *tc);
void test_cil_gen_level_noname_neg(CuTest *tc);
void test_cil_gen_level_nosens_neg(CuTest *tc);
void test_cil_gen_level_dbnull_neg(CuTest *tc);
void test_cil_gen_level_currnull_neg(CuTest *tc);
void test_cil_gen_level_astnull_neg(CuTest *tc);

void test_cil_gen_levelrange(CuTest *tc);
void test_cil_gen_levelrange_rangeinvalid_neg(CuTest *tc);
void test_cil_gen_levelrange_namenull_neg(CuTest *tc);
void test_cil_gen_levelrange_rangenull_neg(CuTest *tc);
void test_cil_gen_levelrange_rangeempty_neg(CuTest *tc);
void test_cil_gen_levelrange_extra_neg(CuTest *tc);
void test_cil_gen_levelrange_dbnull_neg(CuTest *tc);
void test_cil_gen_levelrange_currnull_neg(CuTest *tc);
void test_cil_gen_levelrange_astnull_neg(CuTest *tc);

void test_cil_gen_constrain(CuTest *tc);
void test_cil_gen_constrain_neg(CuTest *tc);
void test_cil_gen_constrain_classset_neg(CuTest *tc);
void test_cil_gen_constrain_classset_noclass_neg(CuTest *tc);
void test_cil_gen_constrain_classset_noperm_neg(CuTest *tc);
void test_cil_gen_constrain_permset_neg(CuTest *tc);
void test_cil_gen_constrain_permset_noclass_neg(CuTest *tc);
void test_cil_gen_constrain_permset_noperm_neg(CuTest *tc);
void test_cil_gen_constrain_expression_neg(CuTest *tc);
void test_cil_gen_constrain_dbnull_neg(CuTest *tc);
void test_cil_gen_constrain_currnull_neg(CuTest *tc);
void test_cil_gen_constrain_astnull_neg(CuTest *tc);

void test_cil_fill_context(CuTest *tc);
void test_cil_fill_context_unnamedlvl(CuTest *tc);
void test_cil_fill_context_nocontext_neg(CuTest *tc);
void test_cil_fill_context_nouser_neg(CuTest *tc);
void test_cil_fill_context_norole_neg(CuTest *tc);
void test_cil_fill_context_notype_neg(CuTest *tc);
void test_cil_fill_context_nolowlvl_neg(CuTest *tc);
void test_cil_fill_context_nohighlvl_neg(CuTest *tc);
void test_cil_fill_context_unnamedlvl_nocontextlow_neg(CuTest *tc);
void test_cil_fill_context_unnamedlvl_nocontexthigh_neg(CuTest *tc);

void test_cil_gen_context(CuTest *tc);
void test_cil_gen_context_notinparens_neg(CuTest *tc);
void test_cil_gen_context_extralevel_neg(CuTest *tc);
void test_cil_gen_context_emptycontext_neg(CuTest *tc);
void test_cil_gen_context_extra_neg(CuTest *tc);
void test_cil_gen_context_doubleparen_neg(CuTest *tc);
void test_cil_gen_context_norole_neg(CuTest *tc);
void test_cil_gen_context_roleinparens_neg(CuTest *tc);
void test_cil_gen_context_notype_neg(CuTest *tc);
void test_cil_gen_context_typeinparens_neg(CuTest *tc);
void test_cil_gen_context_nolevels_neg(CuTest *tc);
void test_cil_gen_context_nosecondlevel_neg(CuTest *tc);
void test_cil_gen_context_noname_neg(CuTest *tc);
void test_cil_gen_context_nouser_neg(CuTest *tc);
void test_cil_gen_context_dbnull_neg(CuTest *tc);
void test_cil_gen_context_currnull_neg(CuTest *tc);
void test_cil_gen_context_astnull_neg(CuTest *tc);

void test_cil_gen_filecon_file(CuTest *tc);
void test_cil_gen_filecon_dir(CuTest *tc);
void test_cil_gen_filecon_char(CuTest *tc);
void test_cil_gen_filecon_block(CuTest *tc);
void test_cil_gen_filecon_socket(CuTest *tc);
void test_cil_gen_filecon_pipe(CuTest *tc);
void test_cil_gen_filecon_symlink(CuTest *tc);
void test_cil_gen_filecon_any(CuTest *tc);
void test_cil_gen_filecon_neg(CuTest *tc);
void test_cil_gen_filecon_anon_context(CuTest *tc);
void test_cil_gen_filecon_dbnull_neg(CuTest *tc);
void test_cil_gen_filecon_currnull_neg(CuTest *tc);
void test_cil_gen_filecon_astnull_neg(CuTest *tc);
void test_cil_gen_filecon_str1null_neg(CuTest *tc);
void test_cil_gen_filecon_str1_inparens_neg(CuTest *tc);
void test_cil_gen_filecon_str2null_neg(CuTest *tc);
void test_cil_gen_filecon_str2_inparens_neg(CuTest *tc);
void test_cil_gen_filecon_classnull_neg(CuTest *tc);
void test_cil_gen_filecon_class_inparens_neg(CuTest *tc);
void test_cil_gen_filecon_contextnull_neg(CuTest *tc);
void test_cil_gen_filecon_context_neg(CuTest *tc);
void test_cil_gen_filecon_extra_neg(CuTest *tc);

void test_cil_gen_portcon_udp(CuTest *tc);
void test_cil_gen_portcon_tcp(CuTest *tc);
void test_cil_gen_portcon_unknownprotocol_neg(CuTest *tc);
void test_cil_gen_portcon_anon_context(CuTest *tc);
void test_cil_gen_portcon_portrange(CuTest *tc);
void test_cil_gen_portcon_portrange_one_neg(CuTest *tc);
void test_cil_gen_portcon_portrange_morethanone_neg(CuTest *tc);
void test_cil_gen_portcon_singleport_neg(CuTest *tc);
void test_cil_gen_portcon_lowport_neg(CuTest *tc);
void test_cil_gen_portcon_highport_neg(CuTest *tc);
void test_cil_gen_portcon_dbnull_neg(CuTest *tc);
void test_cil_gen_portcon_currnull_neg(CuTest *tc);
void test_cil_gen_portcon_astnull_neg(CuTest *tc);
void test_cil_gen_portcon_str1null_neg(CuTest *tc);
void test_cil_gen_portcon_str1parens_neg(CuTest *tc);
void test_cil_gen_portcon_portnull_neg(CuTest *tc);
void test_cil_gen_portcon_contextnull_neg(CuTest *tc);
void test_cil_gen_portcon_context_neg(CuTest *tc);
void test_cil_gen_portcon_extra_neg(CuTest *tc);

void test_cil_fill_ipaddr(CuTest *tc);
void test_cil_fill_ipaddr_addrnodenull_neg(CuTest *tc);
void test_cil_fill_ipaddr_addrnull_neg(CuTest *tc);
void test_cil_fill_ipaddr_addrinparens_neg(CuTest *tc);
void test_cil_fill_ipaddr_extra_neg(CuTest *tc);

void test_cil_gen_nodecon(CuTest *tc);
void test_cil_gen_nodecon_anon_context(CuTest *tc);
void test_cil_gen_nodecon_dbnull_neg(CuTest *tc);
void test_cil_gen_nodecon_currnull_neg(CuTest *tc);
void test_cil_gen_nodecon_astnull_neg(CuTest *tc);
void test_cil_gen_nodecon_ipnull_neg(CuTest *tc);
void test_cil_gen_nodecon_ipanon(CuTest *tc);
void test_cil_gen_nodecon_ipanon_neg(CuTest *tc);
void test_cil_gen_nodecon_netmasknull_neg(CuTest *tc);
void test_cil_gen_nodecon_netmaskanon(CuTest *tc);
void test_cil_gen_nodecon_netmaskanon_neg(CuTest *tc);
void test_cil_gen_nodecon_contextnull_neg(CuTest *tc);
void test_cil_gen_nodecon_context_neg(CuTest *tc);
void test_cil_gen_nodecon_extra_neg(CuTest *tc);

void test_cil_gen_genfscon(CuTest *tc);
void test_cil_gen_genfscon_anon_context(CuTest *tc);
void test_cil_gen_genfscon_dbnull_neg(CuTest *tc);
void test_cil_gen_genfscon_currnull_neg(CuTest *tc);
void test_cil_gen_genfscon_astnull_neg(CuTest *tc);
void test_cil_gen_genfscon_typenull_neg(CuTest *tc);
void test_cil_gen_genfscon_typeparens_neg(CuTest *tc);
void test_cil_gen_genfscon_pathnull_neg(CuTest *tc);
void test_cil_gen_genfscon_pathparens_neg(CuTest *tc);
void test_cil_gen_genfscon_contextnull_neg(CuTest *tc);
void test_cil_gen_genfscon_context_neg(CuTest *tc);
void test_cil_gen_genfscon_extra_neg(CuTest *tc);

void test_cil_gen_netifcon(CuTest *tc);
void test_cil_gen_netifcon_nested(CuTest *tc);
void test_cil_gen_netifcon_nested_neg(CuTest *tc);
void test_cil_gen_netifcon_nested_emptysecondlist_neg(CuTest *tc);
void test_cil_gen_netifcon_extra_nested_secondlist_neg(CuTest *tc);
void test_cil_gen_netifcon_nested_missingobjects_neg(CuTest *tc);
void test_cil_gen_netifcon_nested_secondnested_missingobjects_neg(CuTest *tc);
void test_cil_gen_netifcon_dbnull_neg(CuTest *tc);
void test_cil_gen_netifcon_currnull_neg(CuTest *tc);
void test_cil_gen_netifcon_astnull_neg(CuTest *tc);
void test_cil_gen_netifcon_ethmissing_neg(CuTest *tc);
void test_cil_gen_netifcon_interfacemissing_neg(CuTest *tc);
void test_cil_gen_netifcon_packetmissing_neg(CuTest *tc);

void test_cil_gen_pirqcon(CuTest *tc);
void test_cil_gen_pirqcon_pirqnotint_neg(CuTest *tc);
void test_cil_gen_pirqcon_nopirq_neg(CuTest *tc);
void test_cil_gen_pirqcon_pirqrange_neg(CuTest *tc);
void test_cil_gen_pirqcon_nocontext_neg(CuTest *tc);
void test_cil_gen_pirqcon_anoncontext_neg(CuTest *tc);
void test_cil_gen_pirqcon_extra_neg(CuTest *tc);
void test_cil_gen_pirqcon_dbnull_neg(CuTest *tc);
void test_cil_gen_pirqcon_currnull_neg(CuTest *tc);
void test_cil_gen_pirqcon_astnull_neg(CuTest *tc);

void test_cil_gen_iomemcon(CuTest *tc);
void test_cil_gen_iomemcon_iomemrange(CuTest *tc);
void test_cil_gen_iomemcon_iomemrange_firstnotint_neg(CuTest *tc);
void test_cil_gen_iomemcon_iomemrange_secondnotint_neg(CuTest *tc);
void test_cil_gen_iomemcon_iomemrange_empty_neg(CuTest *tc);
void test_cil_gen_iomemcon_iomemrange_singleiomem_neg(CuTest *tc);
void test_cil_gen_iomemcon_iomemrange_morethantwoiomem_neg(CuTest *tc);
void test_cil_gen_iomemcon_iomemnotint_neg(CuTest *tc);
void test_cil_gen_iomemcon_noiomem_neg(CuTest *tc);
void test_cil_gen_iomemcon_nocontext_neg(CuTest *tc);
void test_cil_gen_iomemcon_anoncontext_neg(CuTest *tc);
void test_cil_gen_iomemcon_extra_neg(CuTest *tc);
void test_cil_gen_iomemcon_dbnull_neg(CuTest *tc);
void test_cil_gen_iomemcon_currnull_neg(CuTest *tc);
void test_cil_gen_iomemcon_astnull_neg(CuTest *tc);

void test_cil_gen_ioportcon(CuTest *tc);
void test_cil_gen_ioportcon_ioportrange(CuTest *tc);
void test_cil_gen_ioportcon_ioportrange_firstnotint_neg(CuTest *tc);
void test_cil_gen_ioportcon_ioportrange_secondnotint_neg(CuTest *tc);
void test_cil_gen_ioportcon_ioportrange_empty_neg(CuTest *tc);
void test_cil_gen_ioportcon_ioportrange_singleioport_neg(CuTest *tc);
void test_cil_gen_ioportcon_ioportrange_morethantwoioport_neg(CuTest *tc);
void test_cil_gen_ioportcon_ioportnotint_neg(CuTest *tc);
void test_cil_gen_ioportcon_noioport_neg(CuTest *tc);
void test_cil_gen_ioportcon_nocontext_neg(CuTest *tc);
void test_cil_gen_ioportcon_anoncontext_neg(CuTest *tc);
void test_cil_gen_ioportcon_extra_neg(CuTest *tc);
void test_cil_gen_ioportcon_dbnull_neg(CuTest *tc);
void test_cil_gen_ioportcon_currnull_neg(CuTest *tc);
void test_cil_gen_ioportcon_astnull_neg(CuTest *tc);

void test_cil_gen_pcidevicecon(CuTest *tc);
void test_cil_gen_pcidevicecon_pcidevicenotint_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_nopcidevice_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_pcidevicerange_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_nocontext_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_anoncontext_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_extra_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_dbnull_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_currnull_neg(CuTest *tc);
void test_cil_gen_pcidevicecon_astnull_neg(CuTest *tc);

void test_cil_gen_fsuse_anoncontext(CuTest *tc);
void test_cil_gen_fsuse_anoncontext_neg(CuTest *tc);
void test_cil_gen_fsuse_xattr(CuTest *tc);
void test_cil_gen_fsuse_task(CuTest *tc);
void test_cil_gen_fsuse_transition(CuTest *tc);
void test_cil_gen_fsuse_invalidtype_neg(CuTest *tc);
void test_cil_gen_fsuse_notype_neg(CuTest *tc);
void test_cil_gen_fsuse_typeinparens_neg(CuTest *tc);
void test_cil_gen_fsuse_nofilesystem_neg(CuTest *tc);
void test_cil_gen_fsuse_filesysteminparens_neg(CuTest *tc);
void test_cil_gen_fsuse_nocontext_neg(CuTest *tc);
void test_cil_gen_fsuse_emptyconparens_neg(CuTest *tc);
void test_cil_gen_fsuse_extra_neg(CuTest *tc);
void test_cil_gen_fsuse_dbnull_neg(CuTest *tc);
void test_cil_gen_fsuse_currnull_neg(CuTest *tc);
void test_cil_gen_fsuse_astnull_neg(CuTest *tc);

void test_cil_gen_macro_noparams(CuTest *tc);
void test_cil_gen_macro_type(CuTest *tc);
void test_cil_gen_macro_role(CuTest *tc);
void test_cil_gen_macro_user(CuTest *tc);
void test_cil_gen_macro_sensitivity(CuTest *tc);
void test_cil_gen_macro_category(CuTest *tc);
void test_cil_gen_macro_catset(CuTest *tc);
void test_cil_gen_macro_level(CuTest *tc);
void test_cil_gen_macro_class(CuTest *tc);
void test_cil_gen_macro_classmap(CuTest *tc);
void test_cil_gen_macro_permset(CuTest *tc);
void test_cil_gen_macro_duplicate(CuTest *tc);
void test_cil_gen_macro_duplicate_neg(CuTest *tc);
void test_cil_gen_macro_unknown_neg(CuTest *tc);
void test_cil_gen_macro_dbnull_neg(CuTest *tc);
void test_cil_gen_macro_currnull_neg(CuTest *tc);
void test_cil_gen_macro_astnull_neg(CuTest *tc);
void test_cil_gen_macro_unnamed_neg(CuTest *tc);
void test_cil_gen_macro_noparam_name_neg(CuTest *tc);
void test_cil_gen_macro_noparam_neg(CuTest *tc);
void test_cil_gen_macro_nosecondparam_neg(CuTest *tc);
void test_cil_gen_macro_emptyparam_neg(CuTest *tc);
void test_cil_gen_macro_paramcontainsperiod_neg(CuTest *tc);

void test_cil_gen_call(CuTest *tc);
void test_cil_gen_call_noargs(CuTest *tc);
void test_cil_gen_call_anon(CuTest *tc);
void test_cil_gen_call_empty_call_neg(CuTest *tc);
void test_cil_gen_call_dbnull_neg(CuTest *tc);
void test_cil_gen_call_currnull_neg(CuTest *tc);
void test_cil_gen_call_astnull_neg(CuTest *tc);
void test_cil_gen_call_name_inparens_neg(CuTest *tc);
void test_cil_gen_call_noname_neg(CuTest *tc);

void test_cil_gen_optional(CuTest *tc);
void test_cil_gen_optional_emptyoptional(CuTest *tc);
void test_cil_gen_optional_dbnull_neg(CuTest *tc);
void test_cil_gen_optional_currnull_neg(CuTest *tc);
void test_cil_gen_optional_astnull_neg(CuTest *tc);
void test_cil_gen_optional_unnamed_neg(CuTest *tc);
void test_cil_gen_optional_extra_neg(CuTest *tc);
void test_cil_gen_optional_nameinparens_neg(CuTest *tc);
void test_cil_gen_optional_norule_neg(CuTest *tc);

void test_cil_gen_policycap(CuTest *tc);
void test_cil_gen_policycap_noname_neg(CuTest *tc);
void test_cil_gen_policycap_nameinparens_neg(CuTest *tc);
void test_cil_gen_policycap_extra_neg(CuTest *tc);
void test_cil_gen_policycap_dbnull_neg(CuTest *tc);
void test_cil_gen_policycap_currnull_neg(CuTest *tc);
void test_cil_gen_policycap_astnull_neg(CuTest *tc);

void test_cil_gen_ipaddr_ipv4(CuTest *tc);
void test_cil_gen_ipaddr_ipv4_neg(CuTest *tc);
void test_cil_gen_ipaddr_ipv6(CuTest *tc);
void test_cil_gen_ipaddr_ipv6_neg(CuTest *tc);
void test_cil_gen_ipaddr_noname_neg(CuTest *tc);
void test_cil_gen_ipaddr_nameinparens_neg(CuTest *tc);
void test_cil_gen_ipaddr_noip_neg(CuTest *tc);
void test_cil_gen_ipaddr_ipinparens_neg(CuTest *tc);
void test_cil_gen_ipaddr_extra_neg(CuTest *tc);
void test_cil_gen_ipaddr_dbnull_neg(CuTest *tc);
void test_cil_gen_ipaddr_currnull_neg(CuTest *tc);
void test_cil_gen_ipaddr_astnull_neg(CuTest *tc);
/*
cil_build_ast test cases
*/
void test_cil_build_ast(CuTest *);
void test_cil_build_ast_dbnull_neg(CuTest *);
void test_cil_build_ast_astnull_neg(CuTest *);
void test_cil_build_ast_suberr_neg(CuTest *);
void test_cil_build_ast_treenull_neg(CuTest *);

void test_cil_build_ast_node_helper_block(CuTest *);
void test_cil_build_ast_node_helper_block_neg(CuTest *);

void test_cil_build_ast_node_helper_blockinherit(CuTest *);
void test_cil_build_ast_node_helper_blockinherit_neg(CuTest *);

void test_cil_build_ast_node_helper_permset(CuTest *);
void test_cil_build_ast_node_helper_permset_neg(CuTest *);

void test_cil_build_ast_node_helper_in(CuTest *);
void test_cil_build_ast_node_helper_in_neg(CuTest *);

void test_cil_build_ast_node_helper_class(CuTest *);
void test_cil_build_ast_node_helper_class_neg(CuTest *);

void test_cil_build_ast_node_helper_classpermset(CuTest *);
void test_cil_build_ast_node_helper_classpermset_neg(CuTest *);

void test_cil_build_ast_node_helper_classmap(CuTest *);
void test_cil_build_ast_node_helper_classmap_neg(CuTest *);

void test_cil_build_ast_node_helper_classmapping(CuTest *);
void test_cil_build_ast_node_helper_classmapping_neg(CuTest *);

void test_cil_build_ast_node_helper_common(CuTest *);
void test_cil_build_ast_node_helper_common_neg(CuTest *);

void test_cil_build_ast_node_helper_sid(CuTest *);
void test_cil_build_ast_node_helper_sid_neg(CuTest *);

void test_cil_build_ast_node_helper_sidcontext(CuTest *);
void test_cil_build_ast_node_helper_sidcontext_neg(CuTest *);

void test_cil_build_ast_node_helper_user(CuTest *);
void test_cil_build_ast_node_helper_user_neg(CuTest *);

void test_cil_build_ast_node_helper_userlevel(CuTest *);
void test_cil_build_ast_node_helper_userlevel_neg(CuTest *);

void test_cil_build_ast_node_helper_userrange(CuTest *);
void test_cil_build_ast_node_helper_userrange_neg(CuTest *);

void test_cil_build_ast_node_helper_type(CuTest *);
void test_cil_build_ast_node_helper_type_neg(CuTest *);

void test_cil_build_ast_node_helper_typeattribute(CuTest *);
void test_cil_build_ast_node_helper_typeattribute_neg(CuTest *);

void test_cil_build_ast_node_helper_boolif(CuTest *);
void test_cil_build_ast_node_helper_boolif_neg(CuTest *);

void test_cil_build_ast_node_helper_tunif(CuTest *);
void test_cil_build_ast_node_helper_tunif_neg(CuTest *);

void test_cil_build_ast_node_helper_condblock_true(CuTest *);
void test_cil_build_ast_node_helper_condblock_true_neg(CuTest *);
void test_cil_build_ast_node_helper_condblock_false(CuTest *);
void test_cil_build_ast_node_helper_condblock_false_neg(CuTest *);

void test_cil_build_ast_node_helper_typealias(CuTest *);
void test_cil_build_ast_node_helper_typealias_notype_neg(CuTest *);

void test_cil_build_ast_node_helper_typebounds(CuTest *);
void test_cil_build_ast_node_helper_typebounds_neg(CuTest *);

void test_cil_build_ast_node_helper_typepermissive(CuTest *);
void test_cil_build_ast_node_helper_typepermissive_neg(CuTest *);

void test_cil_build_ast_node_helper_nametypetransition(CuTest *);
void test_cil_build_ast_node_helper_nametypetransition_neg(CuTest *);

void test_cil_build_ast_node_helper_rangetransition(CuTest *);
void test_cil_build_ast_node_helper_rangetransition_neg(CuTest *);

void test_cil_build_ast_node_helper_typeattributeset(CuTest *);
void test_cil_build_ast_node_helper_typeattributeset_neg(CuTest *);

void test_cil_build_ast_node_helper_userbounds(CuTest *);
void test_cil_build_ast_node_helper_userbounds_neg(CuTest *);

void test_cil_build_ast_node_helper_role(CuTest *);
void test_cil_build_ast_node_helper_role_neg(CuTest *);

void test_cil_build_ast_node_helper_roletransition(CuTest *);
void test_cil_build_ast_node_helper_roletransition_neg(CuTest *);

void test_cil_build_ast_node_helper_roleallow(CuTest *);
void test_cil_build_ast_node_helper_roleallow_neg(CuTest *);

void test_cil_build_ast_node_helper_rolebounds(CuTest *);
void test_cil_build_ast_node_helper_rolebounds_neg(CuTest *);

void test_cil_build_ast_node_helper_avrule_allow(CuTest *);
void test_cil_build_ast_node_helper_avrule_allow_neg(CuTest *);

void test_cil_build_ast_node_helper_avrule_auditallow(CuTest *);
void test_cil_build_ast_node_helper_avrule_auditallow_neg(CuTest *);

void test_cil_build_ast_node_helper_avrule_dontaudit(CuTest *);
void test_cil_build_ast_node_helper_avrule_dontaudit_neg(CuTest *);

void test_cil_build_ast_node_helper_avrule_neverallow(CuTest *);
void test_cil_build_ast_node_helper_avrule_neverallow_neg(CuTest *);

void test_cil_build_ast_node_helper_type_rule_transition(CuTest *);
void test_cil_build_ast_node_helper_type_rule_transition_neg(CuTest *);

void test_cil_build_ast_node_helper_type_rule_change(CuTest *);
void test_cil_build_ast_node_helper_type_rule_change_neg(CuTest *);

void test_cil_build_ast_node_helper_type_rule_member(CuTest *);
void test_cil_build_ast_node_helper_type_rule_member_neg(CuTest *);

void test_cil_build_ast_node_helper_bool(CuTest *);
void test_cil_build_ast_node_helper_bool_neg(CuTest *);

void test_cil_build_ast_node_helper_bool_tunable(CuTest *);
void test_cil_build_ast_node_helper_bool_tunable_neg(CuTest *);

void test_cil_build_ast_node_helper_else(CuTest *);
void test_cil_build_ast_node_helper_else_neg(CuTest *);

void test_cil_build_ast_node_helper_sensitivity(CuTest *);
void test_cil_build_ast_node_helper_sensitivity_neg(CuTest *);

void test_cil_build_ast_node_helper_sensalias(CuTest *);
void test_cil_build_ast_node_helper_sensalias_neg(CuTest *);

void test_cil_build_ast_node_helper_category(CuTest *);
void test_cil_build_ast_node_helper_category_neg(CuTest *);

void test_cil_build_ast_node_helper_catset(CuTest *tc);
void test_cil_build_ast_node_helper_catset_neg(CuTest *tc);

void test_cil_build_ast_node_helper_catorder(CuTest *tc);
void test_cil_build_ast_node_helper_catorder_neg(CuTest *tc);

void test_cil_build_ast_node_helper_catalias(CuTest *tc);
void test_cil_build_ast_node_helper_catalias_neg(CuTest *tc);

void test_cil_build_ast_node_helper_catrange(CuTest *tc);
void test_cil_build_ast_node_helper_catrange_neg(CuTest *tc);

void test_cil_build_ast_node_helper_roletype(CuTest *tc);
void test_cil_build_ast_node_helper_roletype_neg(CuTest *tc);

void test_cil_build_ast_node_helper_userrole(CuTest *tc);
void test_cil_build_ast_node_helper_userrole_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_classcommon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_classcommon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_dominance(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_dominance_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_senscat(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_senscat_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_level(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_level_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_levelrange(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_levelrange_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_constrain(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_constrain_neg(CuTest *tc);
void test_cil_build_ast_node_helper_gen_mlsconstrain(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_mlsconstrain_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_context(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_context_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_filecon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_filecon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_portcon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_portcon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_nodecon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_nodecon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_genfscon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_genfscon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_netifcon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_netifcon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_pirqcon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_pirqcon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_iomemcon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_iomemcon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_ioportcon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_ioportcon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_pcidevicecon(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_pcidevicecon_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_fsuse(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_fsuse_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_macro(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_macro_neg(CuTest *tc);
void test_cil_build_ast_node_helper_gen_macro_nested_macro_neg(CuTest *tc);
void test_cil_build_ast_node_helper_gen_macro_nested_tunif_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_call(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_call_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_optional(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_optional_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_policycap(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_policycap_neg(CuTest *tc);

void test_cil_build_ast_node_helper_gen_ipaddr(CuTest *tc); 
void test_cil_build_ast_node_helper_gen_ipaddr_neg(CuTest *tc);

void test_cil_build_ast_node_helper_extraargsnull_neg(CuTest *);

void test_cil_build_ast_last_child_helper(CuTest *);
void test_cil_build_ast_last_child_helper_extraargsnull_neg(CuTest *);
#endif

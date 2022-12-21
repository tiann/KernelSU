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

 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of Tresys Technology, LLC.
 */

#ifndef TEST_CIL_RESOLVE_AST_H_
#define TEST_CIL_RESOLVE_AST_H_

#include "CuTest.h"

void test_cil_resolve_name(CuTest *);
void test_cil_resolve_name_invalid_type_neg(CuTest *);

void test_cil_resolve_ast_curr_null_neg(CuTest *);


/*
	cil_resolve test cases
*/

void test_cil_resolve_roleallow(CuTest *);
void test_cil_resolve_roleallow_srcdecl_neg(CuTest *);
void test_cil_resolve_roleallow_tgtdecl_neg(CuTest *);

void test_cil_resolve_rolebounds(CuTest *tc);
void test_cil_resolve_rolebounds_exists_neg(CuTest *tc);
void test_cil_resolve_rolebounds_role1_neg(CuTest *tc);
void test_cil_resolve_rolebounds_role2_neg(CuTest *tc);

void test_cil_resolve_sensalias(CuTest *);
void test_cil_resolve_sensalias_sensdecl_neg(CuTest *);

void test_cil_resolve_catalias(CuTest *);
void test_cil_resolve_catalias_catdecl_neg(CuTest *);

void test_cil_resolve_catorder(CuTest *);
void test_cil_resolve_catorder_neg(CuTest *);

void test_cil_resolve_dominance(CuTest *);
void test_cil_resolve_dominance_neg(CuTest *);

void test_cil_resolve_cat_list(CuTest *);
void test_cil_resolve_cat_list_catlistnull_neg(CuTest *);
void test_cil_resolve_cat_list_rescatlistnull_neg(CuTest *);
void test_cil_resolve_cat_list_catrange(CuTest *);
void test_cil_resolve_cat_list_catrange_neg(CuTest *);
void test_cil_resolve_cat_list_catname_neg(CuTest *);

void test_cil_resolve_catset(CuTest *);
void test_cil_resolve_catset_catlist_neg(CuTest *);

void test_cil_resolve_catrange(CuTest *);
void test_cil_resolve_catrange_catloworder_neg(CuTest *);
void test_cil_resolve_catrange_cathighorder_neg(CuTest *);
void test_cil_resolve_catrange_cat1_neg(CuTest *);
void test_cil_resolve_catrange_cat2_neg(CuTest *);

void test_cil_resolve_senscat(CuTest *);
void test_cil_resolve_senscat_catrange_neg(CuTest *);
void test_cil_resolve_senscat_catsetname(CuTest *);
void test_cil_resolve_senscat_catsetname_neg(CuTest *);
void test_cil_resolve_senscat_sublist(CuTest *);
void test_cil_resolve_senscat_missingsens_neg(CuTest *);
void test_cil_resolve_senscat_sublist_neg(CuTest *);
void test_cil_resolve_senscat_category_neg(CuTest *);
void test_cil_resolve_senscat_currrangecat(CuTest *);
void test_cil_resolve_senscat_currrangecat_neg(CuTest *);

void test_cil_resolve_level(CuTest *);
void test_cil_resolve_level_catlist(CuTest *);
void test_cil_resolve_level_catset(CuTest *);
void test_cil_resolve_level_catset_name_neg(CuTest *);
void test_cil_resolve_level_sens_neg(CuTest *);
void test_cil_resolve_level_cat_neg(CuTest *);
void test_cil_resolve_level_senscat_neg(CuTest *);

void test_cil_resolve_levelrange_namedlvl(CuTest *);
void test_cil_resolve_levelrange_namedlvl_low_neg(CuTest *);
void test_cil_resolve_levelrange_namedlvl_high_neg(CuTest *);
void test_cil_resolve_levelrange_anonlvl(CuTest *);
void test_cil_resolve_levelrange_anonlvl_low_neg(CuTest *);
void test_cil_resolve_levelrange_anonlvl_high_neg(CuTest *);

void test_cil_resolve_constrain(CuTest *);
void test_cil_resolve_constrain_class_neg(CuTest *);
void test_cil_resolve_constrain_perm_neg(CuTest *);
void test_cil_resolve_constrain_perm_resolve_neg(CuTest *);

void test_cil_resolve_context(CuTest *);
void test_cil_resolve_context_macro(CuTest *);
void test_cil_resolve_context_macro_neg(CuTest *);
void test_cil_resolve_context_namedrange(CuTest *);
void test_cil_resolve_context_namedrange_neg(CuTest *);
void test_cil_resolve_context_macro_namedrange_anon(CuTest *);
void test_cil_resolve_context_user_neg(CuTest *);
void test_cil_resolve_context_role_neg(CuTest *);
void test_cil_resolve_context_type_neg(CuTest *);
void test_cil_resolve_context_anon_level_neg(CuTest *);

void test_cil_resolve_roletransition(CuTest *);
void test_cil_resolve_roletransition_srcdecl_neg(CuTest *);
void test_cil_resolve_roletransition_tgtdecl_neg(CuTest *);
void test_cil_resolve_roletransition_resultdecl_neg(CuTest *);

void test_cil_resolve_typeattributeset_type_in_multiple_attrs(CuTest *);
void test_cil_resolve_typeattributeset_multiple_excludes_with_not(CuTest *);
void test_cil_resolve_typeattributeset_multiple_types_with_and(CuTest *);
void test_cil_resolve_typeattributeset_using_attr(CuTest *);
void test_cil_resolve_typeattributeset_name_neg(CuTest *);
void test_cil_resolve_typeattributeset_undef_type_neg(CuTest *);
void test_cil_resolve_typeattributeset_not(CuTest *);
void test_cil_resolve_typeattributeset_undef_type_not_neg(CuTest *);

void test_cil_resolve_typealias(CuTest *);
void test_cil_resolve_typealias_neg(CuTest *);

void test_cil_resolve_typebounds(CuTest *);
void test_cil_resolve_typebounds_repeatbind_neg(CuTest *);
void test_cil_resolve_typebounds_type1_neg(CuTest *);
void test_cil_resolve_typebounds_type2_neg(CuTest *);

void test_cil_resolve_typepermissive(CuTest *);
void test_cil_resolve_typepermissive_neg(CuTest *);

void test_cil_resolve_nametypetransition(CuTest *);
void test_cil_resolve_nametypetransition_src_neg(CuTest *);
void test_cil_resolve_nametypetransition_tgt_neg(CuTest *);
void test_cil_resolve_nametypetransition_class_neg(CuTest *);
void test_cil_resolve_nametypetransition_dest_neg(CuTest *);

void test_cil_resolve_rangetransition(CuTest *);
void test_cil_resolve_rangetransition_namedrange(CuTest *);
void test_cil_resolve_rangetransition_namedrange_anon(CuTest *);
void test_cil_resolve_rangetransition_namedrange_anon_neg(CuTest *);
void test_cil_resolve_rangetransition_namedrange_neg(CuTest *);
void test_cil_resolve_rangetransition_type1_neg(CuTest *);
void test_cil_resolve_rangetransition_type2_neg(CuTest *);
void test_cil_resolve_rangetransition_class_neg(CuTest *);
void test_cil_resolve_rangetransition_call_level_l_anon(CuTest *);
void test_cil_resolve_rangetransition_call_level_l_anon_neg(CuTest *);
void test_cil_resolve_rangetransition_call_level_h_anon(CuTest *);
void test_cil_resolve_rangetransition_call_level_h_anon_neg(CuTest *);
void test_cil_resolve_rangetransition_level_l_neg(CuTest *);
void test_cil_resolve_rangetransition_level_h_neg(CuTest *);
void test_cil_resolve_rangetransition_anon_level_l(CuTest *);
void test_cil_resolve_rangetransition_anon_level_l_neg(CuTest *);
void test_cil_resolve_rangetransition_anon_level_h(CuTest *);
void test_cil_resolve_rangetransition_anon_level_h_neg(CuTest *);

void test_cil_resolve_classcommon(CuTest *);
void test_cil_resolve_classcommon_no_class_neg(CuTest *);
void test_cil_resolve_classcommon_neg(CuTest *);
void test_cil_resolve_classcommon_no_common_neg(CuTest *);

void test_cil_resolve_classmapping_named(CuTest *);
void test_cil_resolve_classmapping_anon(CuTest *);
void test_cil_resolve_classmapping_anon_inmacro(CuTest *);
void test_cil_resolve_classmapping_anon_inmacro_neg(CuTest *);
void test_cil_resolve_classmapping_named_classmapname_neg(CuTest *);
void test_cil_resolve_classmapping_anon_classmapname_neg(CuTest *);
void test_cil_resolve_classmapping_anon_permset_neg(CuTest *);

void test_cil_resolve_classpermset_named(CuTest *);
void test_cil_resolve_classpermset_named_namedpermlist(CuTest *);
void test_cil_resolve_classpermset_named_permlist_neg(CuTest *);
void test_cil_resolve_classpermset_named_unnamedcps_neg(CuTest *);
void test_cil_resolve_classpermset_anon(CuTest *);
void test_cil_resolve_classpermset_anon_namedpermlist(CuTest *);
void test_cil_resolve_classpermset_anon_permlist_neg(CuTest *);

void test_cil_resolve_avrule(CuTest *);
void test_cil_resolve_avrule_permset(CuTest *);
void test_cil_resolve_avrule_permset_neg(CuTest *);
void test_cil_resolve_avrule_permset_permdne_neg(CuTest *);
void test_cil_resolve_avrule_firsttype_neg(CuTest *);
void test_cil_resolve_avrule_secondtype_neg(CuTest *);
void test_cil_resolve_avrule_class_neg(CuTest *);
void test_cil_resolve_avrule_perm_neg(CuTest *);

void test_cil_resolve_type_rule_transition(CuTest *);
void test_cil_resolve_type_rule_transition_srcdecl_neg(CuTest *);
void test_cil_resolve_type_rule_transition_tgtdecl_neg(CuTest *);
void test_cil_resolve_type_rule_transition_objdecl_neg(CuTest *);
void test_cil_resolve_type_rule_transition_resultdecl_neg(CuTest *);

void test_cil_resolve_type_rule_change(CuTest *);
void test_cil_resolve_type_rule_change_srcdecl_neg(CuTest *);
void test_cil_resolve_type_rule_change_tgtdecl_neg(CuTest *);
void test_cil_resolve_type_rule_change_objdecl_neg(CuTest *);
void test_cil_resolve_type_rule_change_resultdecl_neg(CuTest *);

void test_cil_resolve_type_rule_member(CuTest *);
void test_cil_resolve_type_rule_member_srcdecl_neg(CuTest *);
void test_cil_resolve_type_rule_member_tgtdecl_neg(CuTest *);
void test_cil_resolve_type_rule_member_objdecl_neg(CuTest *);
void test_cil_resolve_type_rule_member_resultdecl_neg(CuTest *);

void test_cil_resolve_filecon(CuTest *);
void test_cil_resolve_filecon_neg(CuTest *);
void test_cil_resolve_filecon_anon_context(CuTest *);
void test_cil_resolve_filecon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_filecon(CuTest *tc);
void test_cil_resolve_ast_node_helper_filecon_neg(CuTest *tc);

void test_cil_resolve_portcon(CuTest *);
void test_cil_resolve_portcon_neg(CuTest *);
void test_cil_resolve_portcon_anon_context(CuTest *);
void test_cil_resolve_portcon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_portcon(CuTest *tc);
void test_cil_resolve_ast_node_helper_portcon_neg(CuTest *tc);

void test_cil_resolve_genfscon(CuTest *);
void test_cil_resolve_genfscon_neg(CuTest *);
void test_cil_resolve_genfscon_anon_context(CuTest *);
void test_cil_resolve_genfscon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_genfscon(CuTest *tc);
void test_cil_resolve_ast_node_helper_genfscon_neg(CuTest *tc);

void test_cil_resolve_nodecon_ipv4(CuTest *);
void test_cil_resolve_nodecon_ipv6(CuTest *);
void test_cil_resolve_nodecon_anonipaddr_ipv4(CuTest *);
void test_cil_resolve_nodecon_anonnetmask_ipv4(CuTest *);
void test_cil_resolve_nodecon_anonipaddr_ipv6(CuTest *);
void test_cil_resolve_nodecon_anonnetmask_ipv6(CuTest *);
void test_cil_resolve_nodecon_diffipfam_neg(CuTest *);
void test_cil_resolve_nodecon_context_neg(CuTest *);
void test_cil_resolve_nodecon_ipaddr_neg(CuTest *);
void test_cil_resolve_nodecon_netmask_neg(CuTest *);
void test_cil_resolve_nodecon_anon_context(CuTest *);
void test_cil_resolve_nodecon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_nodecon(CuTest *tc);
void test_cil_resolve_ast_node_helper_nodecon_ipaddr_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_nodecon_netmask_neg(CuTest *tc);

void test_cil_resolve_netifcon(CuTest *);
void test_cil_resolve_netifcon_otf_neg(CuTest *);
void test_cil_resolve_netifcon_interface_neg(CuTest *);
void test_cil_resolve_netifcon_unnamed(CuTest *);
void test_cil_resolve_netifcon_unnamed_packet_neg(CuTest *);
void test_cil_resolve_netifcon_unnamed_otf_neg(CuTest *);
void test_cil_resolve_ast_node_helper_netifcon(CuTest *tc);
void test_cil_resolve_ast_node_helper_netifcon_neg(CuTest *tc);

void test_cil_resolve_pirqcon(CuTest *);
void test_cil_resolve_pirqcon_context_neg(CuTest *);
void test_cil_resolve_pirqcon_anon_context(CuTest *);
void test_cil_resolve_pirqcon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_pirqcon(CuTest *tc);
void test_cil_resolve_ast_node_helper_pirqcon_neg(CuTest *tc);

void test_cil_resolve_iomemcon(CuTest *);
void test_cil_resolve_iomemcon_context_neg(CuTest *);
void test_cil_resolve_iomemcon_anon_context(CuTest *);
void test_cil_resolve_iomemcon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_iomemcon(CuTest *tc);
void test_cil_resolve_ast_node_helper_iomemcon_neg(CuTest *tc);

void test_cil_resolve_ioportcon(CuTest *);
void test_cil_resolve_ioportcon_context_neg(CuTest *);
void test_cil_resolve_ioportcon_anon_context(CuTest *);
void test_cil_resolve_ioportcon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_ioportcon(CuTest *tc);
void test_cil_resolve_ast_node_helper_ioportcon_neg(CuTest *tc);

void test_cil_resolve_pcidevicecon(CuTest *);
void test_cil_resolve_pcidevicecon_context_neg(CuTest *);
void test_cil_resolve_pcidevicecon_anon_context(CuTest *);
void test_cil_resolve_pcidevicecon_anon_context_neg(CuTest *);
void test_cil_resolve_ast_node_helper_pcidevicecon(CuTest *tc);
void test_cil_resolve_ast_node_helper_pcidevicecon_neg(CuTest *tc);

void test_cil_resolve_fsuse(CuTest *);
void test_cil_resolve_fsuse_neg(CuTest *);
void test_cil_resolve_fsuse_anon(CuTest *);
void test_cil_resolve_fsuse_anon_neg(CuTest *);
void test_cil_resolve_ast_node_helper_fsuse(CuTest *tc);
void test_cil_resolve_ast_node_helper_fsuse_neg(CuTest *tc);

void test_cil_resolve_sidcontext(CuTest *);
void test_cil_resolve_sidcontext_named_levels(CuTest *);
void test_cil_resolve_sidcontext_named_context(CuTest *);
void test_cil_resolve_sidcontext_named_context_wrongname_neg(CuTest *tc);
void test_cil_resolve_sidcontext_named_context_invaliduser_neg(CuTest *tc);
void test_cil_resolve_sidcontext_named_context_sidcontextnull_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_sidcontext(CuTest *tc);
void test_cil_resolve_ast_node_helper_sidcontext_neg(CuTest *tc);

void test_cil_resolve_blockinherit(CuTest *);
void test_cil_resolve_blockinherit_blockstrdne_neg(CuTest *);
void test_cil_resolve_ast_node_helper_blockinherit(CuTest *tc);

void test_cil_resolve_in_block(CuTest *);
void test_cil_resolve_in_blockstrdne_neg(CuTest *);
void test_cil_resolve_in_macro(CuTest *);
void test_cil_resolve_in_optional(CuTest *);

void test_cil_resolve_call1_noparam(CuTest *);
void test_cil_resolve_call1_type(CuTest *);
void test_cil_resolve_call1_role(CuTest *);
void test_cil_resolve_call1_user(CuTest *);
void test_cil_resolve_call1_sens(CuTest *);
void test_cil_resolve_call1_cat(CuTest *);
void test_cil_resolve_call1_catset(CuTest *);
void test_cil_resolve_call1_catset_anon(CuTest *);
void test_cil_resolve_call1_catset_anon_neg(CuTest *);
void test_cil_resolve_call1_level(CuTest *);
void test_cil_resolve_call1_class(CuTest *);
void test_cil_resolve_call1_classmap(CuTest *);
void test_cil_resolve_call1_permset(CuTest *);
void test_cil_resolve_call1_permset_anon(CuTest *);
void test_cil_resolve_call1_classpermset_named(CuTest *);
void test_cil_resolve_call1_classpermset_anon(CuTest *);
void test_cil_resolve_call1_classpermset_anon_neg(CuTest *);
void test_cil_resolve_call1_level(CuTest *);
void test_cil_resolve_call1_level_anon(CuTest *);
void test_cil_resolve_call1_level_anon_neg(CuTest *);
void test_cil_resolve_call1_ipaddr(CuTest *);
void test_cil_resolve_call1_ipaddr_anon(CuTest *);
void test_cil_resolve_call1_ipaddr_anon_neg(CuTest *);
void test_cil_resolve_call1_unknown_neg(CuTest *);
void test_cil_resolve_call1_unknowncall_neg(CuTest *);
void test_cil_resolve_call1_extraargs_neg(CuTest *);
void test_cil_resolve_call1_copy_dup(CuTest *);
void test_cil_resolve_call1_missing_arg_neg(CuTest *);
void test_cil_resolve_call1_paramsflavor_neg(CuTest *);
void test_cil_resolve_call1_unknownflavor_neg(CuTest *);

void test_cil_resolve_call2_type(CuTest *);
void test_cil_resolve_call2_role(CuTest *);
void test_cil_resolve_call2_user(CuTest *);
void test_cil_resolve_call2_sens(CuTest *);
void test_cil_resolve_call2_cat(CuTest *);
void test_cil_resolve_call2_catset(CuTest *);
void test_cil_resolve_call2_catset_anon(CuTest *);
void test_cil_resolve_call2_permset(CuTest *);
void test_cil_resolve_call2_permset_anon(CuTest *);
void test_cil_resolve_call2_classpermset_named(CuTest *);
void test_cil_resolve_call2_classpermset_anon(CuTest *);
void test_cil_resolve_call2_class(CuTest *);
void test_cil_resolve_call2_classmap(CuTest *);
void test_cil_resolve_call2_level(CuTest *);
void test_cil_resolve_call2_level_anon(CuTest *);
void test_cil_resolve_call2_ipaddr(CuTest *);
void test_cil_resolve_call2_ipaddr_anon(CuTest *);
void test_cil_resolve_call2_unknown_neg(CuTest *);

void test_cil_resolve_name_call_args(CuTest *);
void test_cil_resolve_name_call_args_multipleparams(CuTest *);
void test_cil_resolve_name_call_args_diffflavor(CuTest *);
void test_cil_resolve_name_call_args_callnull_neg(CuTest *);
void test_cil_resolve_name_call_args_namenull_neg(CuTest *);
void test_cil_resolve_name_call_args_callargsnull_neg(CuTest *);
void test_cil_resolve_name_call_args_name_neg(CuTest *);

void test_cil_resolve_expr_stack_bools(CuTest *);
void test_cil_resolve_expr_stack_tunables(CuTest *);
void test_cil_resolve_expr_stack_type(CuTest *);
void test_cil_resolve_expr_stack_role(CuTest *);
void test_cil_resolve_expr_stack_user(CuTest *);
void test_cil_resolve_expr_stack_neg(CuTest *);
void test_cil_resolve_expr_stack_emptystr_neg(CuTest *);

void test_cil_resolve_boolif(CuTest *);
void test_cil_resolve_boolif_neg(CuTest *);

void test_cil_evaluate_expr_stack_and(CuTest *);
void test_cil_evaluate_expr_stack_not(CuTest *);
void test_cil_evaluate_expr_stack_or(CuTest *);
void test_cil_evaluate_expr_stack_xor(CuTest *);
void test_cil_evaluate_expr_stack_eq(CuTest *);
void test_cil_evaluate_expr_stack_neq(CuTest *);
void test_cil_evaluate_expr_stack_oper1(CuTest *);
void test_cil_evaluate_expr_stack_oper2(CuTest *);
void test_cil_evaluate_expr_stack_neg(CuTest *);

void test_cil_resolve_tunif_false(CuTest *);
void test_cil_resolve_tunif_true(CuTest *);
void test_cil_resolve_tunif_resolveexpr_neg(CuTest *);
void test_cil_resolve_tunif_evaluateexpr_neg(CuTest *);

void test_cil_resolve_userbounds(CuTest *tc);
void test_cil_resolve_userbounds_exists_neg(CuTest *tc);
void test_cil_resolve_userbounds_user1_neg(CuTest *tc);
void test_cil_resolve_userbounds_user2_neg(CuTest *tc);

void test_cil_resolve_roletype(CuTest *tc);
void test_cil_resolve_roletype_type_neg(CuTest *tc);
void test_cil_resolve_roletype_role_neg(CuTest *tc);

void test_cil_resolve_userrole(CuTest *tc);
void test_cil_resolve_userrole_user_neg(CuTest *tc);
void test_cil_resolve_userrole_role_neg(CuTest *tc);

void test_cil_resolve_userlevel(CuTest *tc);
void test_cil_resolve_userlevel_macro(CuTest *tc);
void test_cil_resolve_userlevel_macro_neg(CuTest *tc);
void test_cil_resolve_userlevel_level_anon(CuTest *tc);
void test_cil_resolve_userlevel_level_anon_neg(CuTest *tc);
void test_cil_resolve_userlevel_user_neg(CuTest *tc);
void test_cil_resolve_userlevel_level_neg(CuTest *tc);

void test_cil_resolve_userrange(CuTest *tc);
void test_cil_resolve_userrange_macro(CuTest *tc);
void test_cil_resolve_userrange_macro_neg(CuTest *tc);
void test_cil_resolve_userrange_range_anon(CuTest *tc);
void test_cil_resolve_userrange_range_anon_neg(CuTest *tc);
void test_cil_resolve_userrange_user_neg(CuTest *tc);
void test_cil_resolve_userrange_range_neg(CuTest *tc);

void test_cil_disable_children_helper_optional_enabled(CuTest *tc);
void test_cil_disable_children_helper_optional_disabled(CuTest *tc);
void test_cil_disable_children_helper_block(CuTest *tc);
void test_cil_disable_children_helper_user(CuTest *tc);
void test_cil_disable_children_helper_role(CuTest *tc);
void test_cil_disable_children_helper_type(CuTest *tc);
void test_cil_disable_children_helper_typealias(CuTest *tc);
void test_cil_disable_children_helper_common(CuTest *tc);
void test_cil_disable_children_helper_class(CuTest *tc);
void test_cil_disable_children_helper_bool(CuTest *tc);
void test_cil_disable_children_helper_sens(CuTest *tc);
void test_cil_disable_children_helper_cat(CuTest *tc);
void test_cil_disable_children_helper_catset(CuTest *tc);
void test_cil_disable_children_helper_sid(CuTest *tc);
void test_cil_disable_children_helper_macro(CuTest *tc);
void test_cil_disable_children_helper_context(CuTest *tc);
void test_cil_disable_children_helper_level(CuTest *tc);
void test_cil_disable_children_helper_policycap(CuTest *tc);
void test_cil_disable_children_helper_perm(CuTest *tc);
void test_cil_disable_children_helper_catalias(CuTest *tc);
void test_cil_disable_children_helper_sensalias(CuTest *tc);
void test_cil_disable_children_helper_tunable(CuTest *tc);
void test_cil_disable_children_helper_unknown(CuTest *tc);

/*
	__cil_resolve_ast_node_helper test cases
*/

void test_cil_resolve_ast_node_helper_call1(CuTest *);
void test_cil_resolve_ast_node_helper_call1_neg(CuTest *);

void test_cil_resolve_ast_node_helper_call2(CuTest *);
void test_cil_resolve_ast_node_helper_call2_neg(CuTest *);

void test_cil_resolve_ast_node_helper_boolif(CuTest *);
void test_cil_resolve_ast_node_helper_boolif_neg(CuTest *);

void test_cil_resolve_ast_node_helper_tunif(CuTest *);
void test_cil_resolve_ast_node_helper_tunif_neg(CuTest *);

void test_cil_resolve_ast_node_helper_catorder(CuTest *);
void test_cil_resolve_ast_node_helper_catorder_neg(CuTest *);

void test_cil_resolve_ast_node_helper_dominance(CuTest *);
void test_cil_resolve_ast_node_helper_dominance_neg(CuTest *);

void test_cil_resolve_ast_node_helper_roleallow(CuTest *);
void test_cil_resolve_ast_node_helper_roleallow_neg(CuTest *);

void test_cil_resolve_ast_node_helper_rolebounds(CuTest *tc);
void test_cil_resolve_ast_node_helper_rolebounds_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_sensalias(CuTest *);
void test_cil_resolve_ast_node_helper_sensalias_neg(CuTest *);

void test_cil_resolve_ast_node_helper_catalias(CuTest *);
void test_cil_resolve_ast_node_helper_catalias_neg(CuTest *);

void test_cil_resolve_ast_node_helper_catset(CuTest *);
void test_cil_resolve_ast_node_helper_catset_catlist_neg(CuTest *);

void test_cil_resolve_ast_node_helper_level(CuTest *);
void test_cil_resolve_ast_node_helper_level_neg(CuTest *);

void test_cil_resolve_ast_node_helper_levelrange(CuTest *);
void test_cil_resolve_ast_node_helper_levelrange_neg(CuTest *);

void test_cil_resolve_ast_node_helper_constrain(CuTest *);
void test_cil_resolve_ast_node_helper_constrain_neg(CuTest *);
void test_cil_resolve_ast_node_helper_mlsconstrain(CuTest *);
void test_cil_resolve_ast_node_helper_mlsconstrain_neg(CuTest *);

void test_cil_resolve_ast_node_helper_context(CuTest *);
void test_cil_resolve_ast_node_helper_context_neg(CuTest *);

void test_cil_resolve_ast_node_helper_catrange(CuTest *tc);
void test_cil_resolve_ast_node_helper_catrange_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_senscat(CuTest *tc);
void test_cil_resolve_ast_node_helper_senscat_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_roletransition(CuTest *);
void test_cil_resolve_ast_node_helper_roletransition_srcdecl_neg(CuTest *);
void test_cil_resolve_ast_node_helper_roletransition_tgtdecl_neg(CuTest *);
void test_cil_resolve_ast_node_helper_roletransition_resultdecl_neg(CuTest *);

void test_cil_resolve_ast_node_helper_typeattributeset(CuTest *);
void test_cil_resolve_ast_node_helper_typeattributeset_undef_type_neg(CuTest *);

void test_cil_resolve_ast_node_helper_typealias(CuTest *);
void test_cil_resolve_ast_node_helper_typealias_notype_neg(CuTest *);

void test_cil_resolve_ast_node_helper_typebounds(CuTest *);
void test_cil_resolve_ast_node_helper_typebounds_neg(CuTest *);

void test_cil_resolve_ast_node_helper_typepermissive(CuTest *);
void test_cil_resolve_ast_node_helper_typepermissive_neg(CuTest *);

void test_cil_resolve_ast_node_helper_nametypetransition(CuTest *);
void test_cil_resolve_ast_node_helper_nametypetransition_neg(CuTest *);

void test_cil_resolve_ast_node_helper_rangetransition(CuTest *);
void test_cil_resolve_ast_node_helper_rangetransition_neg(CuTest *);

void test_cil_resolve_ast_node_helper_avrule(CuTest *);
void test_cil_resolve_ast_node_helper_avrule_src_nores_neg(CuTest *);
void test_cil_resolve_ast_node_helper_avrule_tgt_nores_neg(CuTest *);
void test_cil_resolve_ast_node_helper_avrule_class_nores_neg(CuTest *);
void test_cil_resolve_ast_node_helper_avrule_datum_null_neg(CuTest *);

void test_cil_resolve_ast_node_helper_type_rule_transition(CuTest *);
void test_cil_resolve_ast_node_helper_type_rule_transition_neg(CuTest *);

void test_cil_resolve_ast_node_helper_type_rule_change(CuTest *);
void test_cil_resolve_ast_node_helper_type_rule_change_neg(CuTest *);

void test_cil_resolve_ast_node_helper_type_rule_member(CuTest *);
void test_cil_resolve_ast_node_helper_type_rule_member_neg(CuTest *);

void test_cil_resolve_ast_node_helper_userbounds(CuTest *tc);
void test_cil_resolve_ast_node_helper_userbounds_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_roletype(CuTest *tc);
void test_cil_resolve_ast_node_helper_roletype_role_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_roletype_type_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_userrole(CuTest *tc);
void test_cil_resolve_ast_node_helper_userrole_user_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_userrole_role_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_userlevel(CuTest *tc);
void test_cil_resolve_ast_node_helper_userlevel_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_userlevel(CuTest *tc);
void test_cil_resolve_ast_node_helper_userlevel_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_userrange(CuTest *tc);
void test_cil_resolve_ast_node_helper_userrange_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_classcommon(CuTest *tc);
void test_cil_resolve_ast_node_helper_classcommon_neg(CuTest *tc);

void test_cil_resolve_ast_node_helper_callstack(CuTest *tc);
void test_cil_resolve_ast_node_helper_call(CuTest *tc);
void test_cil_resolve_ast_node_helper_optional(CuTest *tc);
void test_cil_resolve_ast_node_helper_macro(CuTest *tc);
void test_cil_resolve_ast_node_helper_optstack(CuTest *tc);
void test_cil_resolve_ast_node_helper_optstack_tunable_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_optstack_macro_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_nodenull_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_extraargsnull_neg(CuTest *tc);
void test_cil_resolve_ast_node_helper_optfailedtoresolve(CuTest *tc);
#endif

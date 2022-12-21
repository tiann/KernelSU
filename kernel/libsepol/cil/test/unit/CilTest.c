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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sepol/policydb/symtab.h>
#include <sepol/policydb/policydb.h>

#include "CuTest.h"
#include "CilTest.h"

#include "../../src/cil_internal.h"

#include "test_cil.h"
#include "test_cil_tree.h"
#include "test_cil_list.h"
#include "test_cil_symtab.h"
#include "test_cil_parser.h"
#include "test_cil_lexer.h"
#include "test_cil_build_ast.h"
#include "test_cil_resolve_ast.h"
#include "test_cil_fqn.h"
#include "test_cil_copy_ast.h"
#include "test_cil_post.h"
#include "test_integration.h"

void set_cil_file_data(struct cil_file_data **data) {
	struct cil_file_data *new_data = malloc(sizeof(*new_data));
	FILE *file;
	struct stat filedata;
	uint32_t file_size;
	char *buffer;

	file = fopen("test/policy.cil", "r");
	if (!file) {
	    fprintf(stderr, "Could not open file\n");
	    exit(1);
	}
	if (stat("test/policy.cil", &filedata) == -1) {
	    printf("Could not stat file\n");
	    exit(1);
	}
	file_size = filedata.st_size;

	buffer = malloc(file_size + 2);
	if(fread(buffer, file_size, 1, file) < 1) {
		exit(1);
	}
	memset(buffer+file_size, 0, 2);
	fclose(file);


	new_data->buffer = buffer;
	new_data->file_size = file_size;

	*data = new_data;

}

void gen_test_tree(struct cil_tree **test_root, char *line[]) {
	struct cil_tree *new_tree = malloc(sizeof(*new_tree));
	struct cil_tree_node *node, *item, *current;

	cil_tree_init(&new_tree);
	new_tree->root->flavor = CIL_ROOT;
	current = new_tree->root;
	
	char **i = line;
	do {
	    if (*i[0] == '(') {
	        cil_tree_node_init(&node);
	        node->parent = current;
	        node->flavor = CIL_PARSE_NODE;
	        node->line = 0;
	        if (current->cl_head == NULL)
	            current->cl_head = node;
	        else
	            current->cl_tail->next = node;
	        current->cl_tail = node;
	        current = node;
	    }
	    else if (*i[0] == ')')
	        current = current->parent;
	    else {
	        cil_tree_node_init(&item);
	        item->parent = current;
	        item->data = cil_strdup(*i);
	        item->flavor = CIL_PARSE_NODE;
	        item->line = 0;
	        if (current->cl_head == NULL) {
	            current->cl_head = item;
	        }
	        else {
	            current->cl_tail->next = item;
	        }
	        current->cl_tail = item;
	    }
	    i++;
	} while(*i != NULL);

	*test_root = new_tree;
}

void test_symtab_init(CuTest *tc) {
	struct cil_db *test_new_db;
	test_new_db = malloc(sizeof(*test_new_db));

	uint32_t rc = 0, i =0;
	
	for (i = 0; i < CIL_SYM_NUM; i++) {
	    rc = ksu_symtab_init(&test_new_db->symtab[i], cil_sym_sizes[CIL_SYM_ARRAY_ROOT][i]);
	    CuAssertIntEquals(tc, 0, rc);
	    // TODO CDS add checks to make sure the symtab looks correct
	}

	free(test_new_db);
}

void test_symtab_init_no_table_neg(CuTest *tc) {
	struct cil_db *test_new_db;
	test_new_db = malloc(sizeof(*test_new_db));

	int rc = ksu_symtab_init(&test_new_db->symtab[0], (uint32_t)SIZE_MAX);
	CuAssertIntEquals(tc, -1, rc);

	free(test_new_db);
}

CuSuite* CilTreeGetResolveSuite(void) {
	CuSuite* suite = CuSuiteNew();
	
	/* test_cil_resolve_ast.c */
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_curr_null_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_perm_nodes_inval_perm_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_name);
	SUITE_ADD_TEST(suite, test_cil_resolve_name_invalid_type_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_type_in_multiple_attrs);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_multiple_excludes_with_not);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_multiple_types_with_and);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_using_attr);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_name_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_undef_type_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_not);
	SUITE_ADD_TEST(suite, test_cil_resolve_typeattributeset_undef_type_not_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typeattributeset);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typeattributeset_undef_type_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_typealias);
	SUITE_ADD_TEST(suite, test_cil_resolve_typealias_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typealias);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typealias_notype_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_typebounds);
	SUITE_ADD_TEST(suite, test_cil_resolve_typebounds_repeatbind_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_typebounds_type1_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_typebounds_type2_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typebounds);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typebounds_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_typepermissive);
	SUITE_ADD_TEST(suite, test_cil_resolve_typepermissive_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typepermissive);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_typepermissive_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_nametypetransition);
	SUITE_ADD_TEST(suite, test_cil_resolve_nametypetransition_src_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nametypetransition_tgt_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nametypetransition_class_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nametypetransition_dest_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_nametypetransition);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_nametypetransition_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_namedrange);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_namedrange_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_namedrange_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_namedrange_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_type1_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_type2_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_class_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_call_level_l_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_call_level_l_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_call_level_h_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_call_level_h_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_level_l_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_level_h_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_anon_level_l);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_anon_level_l_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_anon_level_h);
	SUITE_ADD_TEST(suite, test_cil_resolve_rangetransition_anon_level_h_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_rangetransition);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_rangetransition_neg); 
	
	SUITE_ADD_TEST(suite, test_cil_resolve_classcommon);
	SUITE_ADD_TEST(suite, test_cil_resolve_classcommon_no_class_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_classcommon_no_common_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_classcommon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_classcommon_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_named);
	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_anon_inmacro);
	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_anon_inmacro_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_named_classmapname_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_anon_classmapname_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_classmapping_anon_permset_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_rolebounds);
	SUITE_ADD_TEST(suite, test_cil_resolve_rolebounds_exists_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rolebounds_role1_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_rolebounds_role2_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_rolebounds);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_rolebounds_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_sensalias);
	SUITE_ADD_TEST(suite, test_cil_resolve_sensalias_sensdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_sensalias);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_sensalias_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_catalias);
	SUITE_ADD_TEST(suite, test_cil_resolve_catalias_catdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catalias);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catalias_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_catorder);
	SUITE_ADD_TEST(suite, test_cil_resolve_catorder_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catorder);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catorder_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_dominance);
	SUITE_ADD_TEST(suite, test_cil_resolve_dominance_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_dominance);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_dominance_neg);
	//TODO: test for __cil_set_order

	SUITE_ADD_TEST(suite, test_cil_resolve_catset);
	SUITE_ADD_TEST(suite, test_cil_resolve_catset_catlist_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catset);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catset_catlist_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_catrange);
	SUITE_ADD_TEST(suite, test_cil_resolve_catrange_catloworder_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_catrange_cathighorder_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_catrange_cat1_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_catrange_cat2_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catrange);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_catrange_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_catrange_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_catsetname);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_catsetname_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_sublist);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_missingsens_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_category_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_senscat_currrangecat);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_senscat);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_senscat_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_level);
	SUITE_ADD_TEST(suite, test_cil_resolve_level_catlist);
	SUITE_ADD_TEST(suite, test_cil_resolve_level_catset);
	SUITE_ADD_TEST(suite, test_cil_resolve_level_catset_name_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_level_sens_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_level_cat_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_level_senscat_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_level);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_level_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_levelrange_namedlvl);
	SUITE_ADD_TEST(suite, test_cil_resolve_levelrange_namedlvl_low_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_levelrange_namedlvl_high_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_levelrange_anonlvl);
	SUITE_ADD_TEST(suite, test_cil_resolve_levelrange_anonlvl_low_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_levelrange_anonlvl_high_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_levelrange);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_levelrange_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_constrain);
	SUITE_ADD_TEST(suite, test_cil_resolve_constrain_class_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_constrain_perm_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_constrain_perm_resolve_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_constrain);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_constrain_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_mlsconstrain);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_mlsconstrain_neg); 
	
	SUITE_ADD_TEST(suite, test_cil_resolve_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_macro);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_macro_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_namedrange);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_namedrange_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_user_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_role_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_type_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_context_anon_level_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_context_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_roletransition);
	SUITE_ADD_TEST(suite, test_cil_resolve_roletransition_srcdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_roletransition_tgtdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_roletransition_resultdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletransition);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletransition_srcdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletransition_tgtdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletransition_resultdecl_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_roleallow);
	SUITE_ADD_TEST(suite, test_cil_resolve_roleallow_srcdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_roleallow_tgtdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roleallow);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roleallow_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_named);
	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_named_namedpermlist);
	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_named_permlist_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_named_unnamedcps_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_anon_namedpermlist);
	SUITE_ADD_TEST(suite, test_cil_resolve_classpermset_anon_permlist_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_permset);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_permset_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_permset_permdne_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_firsttype_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_secondtype_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_class_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_avrule_perm_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_avrule);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_avrule_src_nores_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_avrule_tgt_nores_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_avrule_class_nores_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_avrule_datum_null_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_transition);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_transition_srcdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_transition_tgtdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_transition_objdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_transition_resultdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_type_rule_transition);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_type_rule_transition_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_change);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_change_srcdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_change_tgtdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_change_objdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_change_resultdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_type_rule_change);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_type_rule_change_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_member);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_member_srcdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_member_tgtdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_member_objdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_type_rule_member_resultdecl_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_type_rule_member);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_type_rule_member_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_filecon);
	SUITE_ADD_TEST(suite, test_cil_resolve_filecon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_filecon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_filecon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_filecon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_filecon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_portcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_portcon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_portcon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_portcon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_portcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_portcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_genfscon);
	SUITE_ADD_TEST(suite, test_cil_resolve_genfscon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_genfscon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_genfscon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_genfscon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_genfscon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_ipv4);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_ipv6);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_anonipaddr_ipv4);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_anonnetmask_ipv4);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_anonipaddr_ipv6);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_anonnetmask_ipv6);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_diffipfam_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_ipaddr_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_netmask_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_nodecon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_nodecon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_nodecon_ipaddr_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_nodecon_netmask_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_netifcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_netifcon_otf_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_netifcon_interface_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_netifcon_unnamed);
	SUITE_ADD_TEST(suite, test_cil_resolve_netifcon_unnamed_packet_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_netifcon_unnamed_otf_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_netifcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_netifcon_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_pirqcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_pirqcon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_pirqcon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_pirqcon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_pirqcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_pirqcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_iomemcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_iomemcon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_iomemcon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_iomemcon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_iomemcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_iomemcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_ioportcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ioportcon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ioportcon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_ioportcon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_ioportcon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_ioportcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_pcidevicecon);
	SUITE_ADD_TEST(suite, test_cil_resolve_pcidevicecon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_pcidevicecon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_pcidevicecon_anon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_pcidevicecon);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_pcidevicecon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_fsuse);
	SUITE_ADD_TEST(suite, test_cil_resolve_fsuse_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_fsuse_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_fsuse_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_fsuse);
	//SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_fsuse_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_sidcontext);
	SUITE_ADD_TEST(suite, test_cil_resolve_sidcontext_named_levels);
	SUITE_ADD_TEST(suite, test_cil_resolve_sidcontext_named_context);
	SUITE_ADD_TEST(suite, test_cil_resolve_sidcontext_named_context_wrongname_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_sidcontext_named_context_invaliduser_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_sidcontext); 
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_sidcontext_neg); 

	SUITE_ADD_TEST(suite, test_cil_resolve_blockinherit);
	SUITE_ADD_TEST(suite, test_cil_resolve_blockinherit_blockstrdne_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_blockinherit);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_in_block);
	SUITE_ADD_TEST(suite, test_cil_resolve_in_blockstrdne_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_in_macro);
	SUITE_ADD_TEST(suite, test_cil_resolve_in_optional);
	
	//SUITE_ADD_TEST(suite, test_cil_resolve_call1_noparam);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_type);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_role);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_user);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_sens);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_cat);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_catset);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_catset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_catset_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_class);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_classmap);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_permset);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_permset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_classpermset_named);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_classpermset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_classpermset_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_level);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_level_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_level_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_ipaddr);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_ipaddr_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_ipaddr_anon_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_unknown_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_unknowncall_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_extraargs_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_copy_dup);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_missing_arg_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_paramsflavor_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_call1_unknownflavor_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_call1);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_call1_neg); 
	
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_type);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_role);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_user);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_sens);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_cat);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_catset);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_catset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_permset);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_permset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_classpermset_named);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_classpermset_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_class);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_classmap);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_level);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_level_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_ipaddr);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_ipaddr_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_call2_unknown_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_call2);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_call2_neg); 
	
	SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args);
	SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args_multipleparams);
	SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args_diffflavor);
	SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args_callnull_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args_namenull_neg);
	//SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args_callargsnull_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_name_call_args_name_neg);
	
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_bools);
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_tunables);
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_type);
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_role);
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_user);
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_neg);
//	SUITE_ADD_TEST(suite, test_cil_resolve_expr_stack_emptystr_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_boolif);
	SUITE_ADD_TEST(suite, test_cil_resolve_boolif_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_boolif);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_boolif_neg); 
	
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_and);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_not);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_or);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_xor);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_eq);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_neq);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_oper1);
	SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_oper2);
	//SUITE_ADD_TEST(suite, test_cil_evaluate_expr_stack_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_tunif_false);
	SUITE_ADD_TEST(suite, test_cil_resolve_tunif_true);
	SUITE_ADD_TEST(suite, test_cil_resolve_tunif_resolveexpr_neg);
	//SUITE_ADD_TEST(suite, test_cil_resolve_tunif_evaluateexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_tunif);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_tunif_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_userbounds);
	SUITE_ADD_TEST(suite, test_cil_resolve_userbounds_exists_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_userbounds_user1_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_userbounds_user2_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userbounds);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userbounds_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_roletype);
	SUITE_ADD_TEST(suite, test_cil_resolve_roletype_type_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_roletype_role_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletype);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletype_role_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_roletype_type_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_userrole);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userrole_user_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_userrole_role_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userrole);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userrole_user_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userrole_role_neg);

	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel_macro);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel_macro_neg);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel_level_anon);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel_level_anon_neg);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel_user_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_userlevel_level_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userlevel);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userlevel_neg);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange_macro);
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange_macro_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange_range_anon);
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange_range_anon_neg);	
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange_user_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_userrange_range_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userrange);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_userrange_neg); 
	
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_optional_enabled);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_optional_disabled);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_block);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_user);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_role);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_type);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_typealias);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_common);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_class);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_bool);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_sens);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_cat);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_catset);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_sid);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_macro);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_context);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_level);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_policycap);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_perm);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_catalias);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_sensalias);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_tunable);
	SUITE_ADD_TEST(suite, test_cil_disable_children_helper_unknown);
	
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_callstack);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_call);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_optional);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_macro);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_optstack);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_optstack_tunable_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_optstack_macro_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_nodenull_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_extraargsnull_neg);
	SUITE_ADD_TEST(suite, test_cil_resolve_ast_node_helper_optfailedtoresolve);

	return suite;
}

CuSuite* CilTreeGetBuildSuite(void) {
	CuSuite* suite = CuSuiteNew();

	/* test_cil_build_ast.c */
	SUITE_ADD_TEST(suite, test_cil_build_ast);
	SUITE_ADD_TEST(suite, test_cil_build_ast_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_treenull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_suberr_neg);

	SUITE_ADD_TEST(suite, test_cil_parse_to_list);
	SUITE_ADD_TEST(suite, test_cil_parse_to_list_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_parse_to_list_listnull_neg);

	SUITE_ADD_TEST(suite, test_cil_set_to_list);
	SUITE_ADD_TEST(suite, test_cil_set_to_list_listnull_neg);
	SUITE_ADD_TEST(suite, test_cil_set_to_list_tree_node_null_neg);
	SUITE_ADD_TEST(suite, test_cil_set_to_list_cl_head_null_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_block);
	SUITE_ADD_TEST(suite, test_cil_gen_block_justblock_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_block_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_block_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_block_treenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_block_nodenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_block_nodeparentnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_block);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_block_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit);
	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit_namelist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit_namenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_blockinherit_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_blockinherit);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_blockinherit_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_perm);
	SUITE_ADD_TEST(suite, test_cil_gen_perm_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_perm_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_perm_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_perm_nodenull_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_permset);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_noperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_emptyperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_permset_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_permset);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_permset_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_perm_nodes);
	SUITE_ADD_TEST(suite, test_cil_gen_perm_nodes_failgen_neg);

	SUITE_ADD_TEST(suite, test_cil_fill_permset);
	SUITE_ADD_TEST(suite, test_cil_fill_permset_sublist_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_permset_startpermnull_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_permset_permsetnull_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_in);
	SUITE_ADD_TEST(suite, test_cil_gen_in_blockstrnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_in_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_in_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_in_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_in_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_in);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_in_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_class);
	SUITE_ADD_TEST(suite, test_cil_gen_class_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_nodenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_noclassname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_namesublist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_noperms);
	SUITE_ADD_TEST(suite, test_cil_gen_class_permsnotinlist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_extrapermlist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_class_listinlist_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_class);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_class_neg);

	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_anonperms);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_anonperms_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_namedperms);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_emptypermslist_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_noperms_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_noclass_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_classnodenull_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_classpermset_cpsnull_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_noclass_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_noperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_emptyperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classpermset_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_classpermset);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_classpermset_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_perm);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_perm_dupeperm_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_perm_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_perm_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_perm_astnull_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_classmap);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_emptyperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmap_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_classmap);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_classmap_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_anonpermset);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_anonpermset_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_namedpermset);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_noclassmapname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_noclassmapperm_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_nopermissionsets_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classmapping_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_classmapping);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_classmapping_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_common);
	SUITE_ADD_TEST(suite, test_cil_gen_common_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_common_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_common_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_common_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_common_twoperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_common_permsublist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_common_noperms_neg);
	
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_common);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_common_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_sid);
	SUITE_ADD_TEST(suite, test_cil_gen_sid_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sid_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sid_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sid_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sid_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sid_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sid);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sid_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_namedcontext);
//	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_halfcontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_empty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_dblname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_pcurrnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sidcontext_astnodenull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sidcontext);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sidcontext_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_type);
	SUITE_ADD_TEST(suite, test_cil_gen_type_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_typeattribute);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattribute_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattribute_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattribute_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattribute_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typeattribute);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typeattribute_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_typebounds);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_notype1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_type1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_notype2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_type2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typebounds_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typebounds);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typebounds_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive);
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive_typeinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typepermissive_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typepermissive);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typepermissive_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_nostr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_strinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_nosrc_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_srcinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_notgt_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_tgtinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_noclass_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_classinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_nodest_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_destinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nametypetransition_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_nametypetransition);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_nametypetransition_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_namedtransition);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_anon_low_l);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_anon_low_l_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_anon_high_l);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_anon_high_l_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_nofirsttype_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_firsttype_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_nosecondtype_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_secondtype_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_noclass_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_class_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_nolevel_l_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_nolevel_h_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rangetransition_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_rangetransition);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_rangetransition_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_and);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_or);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_xor);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_not);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_not_noexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_not_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_eq);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_neq);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_nested);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_nested_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_nested_emptyargs_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_nested_missingoperator_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_arg1null_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_arg2null_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_extraarg_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_expr_stack_stacknull_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_multiplebools_true);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_multiplebools_false);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_multiplebools_unknowncond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_true);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_false);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_unknowncond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_nested);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_nested_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_extra_parens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_nocond);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_nocond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_notruelist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_boolif_empty_cond_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_boolif);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_boolif_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_multiplebools_true);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_multiplebools_false);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_multiplebools_unknowncond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_true);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_false);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_unknowncond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_nocond);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_nested);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_nested_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_extra_parens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_nocond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_tunif_notruelist_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_tunif);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_tunif_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_true);
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_false);
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_nocond_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_condblock_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_condblock_true);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_condblock_true_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_condblock_false);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_condblock_false_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_typealias);
	SUITE_ADD_TEST(suite, test_cil_gen_typealias_incomplete_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typealias_incomplete_neg2);
	SUITE_ADD_TEST(suite, test_cil_gen_typealias_extratype_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typealias);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typealias_notype_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typealias_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typealias_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typealias_astnull_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_and_two_types);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_not);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_exclude_attr);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_exclude_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_emptylists_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_listinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_typeattributeset_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typeattributeset);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_typeattributeset_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_notype1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_type1_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_notype2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_type2_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userbounds_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userbounds);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userbounds_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_role);
	SUITE_ADD_TEST(suite, test_cil_gen_role_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_role_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_role_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_role_extrarole_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_role_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_role);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_role_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_roletransition);
	SUITE_ADD_TEST(suite, test_cil_gen_roletransition_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletransition_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletransition_srcnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletransition_tgtnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletransition_resultnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletransition_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_roletransition);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_roletransition_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_bool_true);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_tunable_true);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_false);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_tunable_false);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_none_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_notbool_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_boolname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_extraname_false_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_bool_extraname_true_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_bool);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_bool_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_bool_tunable);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_bool_tunable_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_t1type);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_t1t1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_t2type);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_t2t2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_r1role);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_r1r1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_r2role);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_r2r2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_t1t2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_r1r2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_r1r2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_u1u2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_u1user);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_u1u1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_u2user);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_u2u2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l2h2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l1l2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l1h1);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l1h2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_h1l2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_h1h2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_h1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l1l1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_l1l2_constrain_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_l1l2_constrain_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_leftkeyword_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_noexpr1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_expr1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_noexpr2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_expr2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_noexpr1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_expr1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_noexpr2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_expr2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_eq2_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_noteq);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_noteq_noexpr1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_noteq_expr1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_noteq_noexpr2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_noteq_expr2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_noteq_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_not);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_not_noexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_not_emptyparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_not_extraparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or_noexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or_emptyfirstparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or_missingsecondexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or_emptysecondparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_or_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and_noexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and_emptyfirstparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and_missingsecondexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and_emptysecondparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_and_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_dom);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_dom_noexpr1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_dom_expr1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_dom_noexpr2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_dom_expr2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_dom_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_domby);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_domby_noexpr1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_domby_expr1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_domby_noexpr2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_domby_expr2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_domby_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incomp);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incomp_noexpr1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incomp_expr1inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incomp_noexpr2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incomp_expr2inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incomp_extraexpr_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_stacknull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_operatorinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expr_stack_incorrectcall_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_roleallow);
	SUITE_ADD_TEST(suite, test_cil_gen_roleallow_dbnull_neg);
        SUITE_ADD_TEST(suite, test_cil_gen_roleallow_currnull_neg);
        SUITE_ADD_TEST(suite, test_cil_gen_roleallow_astnull_neg);
        SUITE_ADD_TEST(suite, test_cil_gen_roleallow_srcnull_neg);
        SUITE_ADD_TEST(suite, test_cil_gen_roleallow_tgtnull_neg);
        SUITE_ADD_TEST(suite, test_cil_gen_roleallow_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_roleallow);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_roleallow_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_norole1_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_role1_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_norole2_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_role2_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_rolebounds_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_rolebounds);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_rolebounds_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_avrule);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_permset);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_permset_anon);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_sourceparens);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_sourceemptyparen_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_targetparens);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_targetemptyparen_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_sourcedomainnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_targetdomainnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_objectclassnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_permsnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_avrule_twolists_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_allow);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_allow_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_auditallow);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_auditallow_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_dontaudit);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_dontaudit_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_neverallow);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_avrule_neverallow_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_srcnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_tgtnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_objnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_resultnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_transition_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_rule_transition);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_rule_transition_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_srcnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_tgtnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_objnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_resultnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_change_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_rule_change);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_rule_change_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_srcnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_tgtnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_objnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_resultnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_type_rule_member_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_rule_member);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_type_rule_member_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_user);
	SUITE_ADD_TEST(suite, test_cil_gen_user_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_user_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_user_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_user_nouser_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_user_xsinfo_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_user);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_user_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_userlevel);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_anon_level);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_anon_level_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_usernull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_userrange_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_levelnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_levelrangeempty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userlevel_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userlevel);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userlevel_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_named);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_anon);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_usernull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_anonuser_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_rangenamenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_anonrangeinvalid_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_anonrangeempty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrange_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userrange);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userrange_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity);
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity_sensnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity_senslist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensitivity_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sensitivity);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sensitivity_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_sensalias);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_sensnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_senslist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_aliasnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_aliaslist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_sensalias_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sensalias);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_sensalias_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_category);
	SUITE_ADD_TEST(suite, test_cil_gen_category_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_category_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_category_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_category_catnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_category_catlist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_category_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_category);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_category_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_catset);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_namenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_setnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_namelist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_nodefail_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_notset_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catset_settolistfail_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catset);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catset_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_catalias);
	SUITE_ADD_TEST(suite, test_cil_gen_catalias_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catalias_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catalias_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catalias_catnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catalias_aliasnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catalias_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catalias);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catalias_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_catrange);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_norange_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_emptyrange_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_extrarange_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catrange_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catrange);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catrange_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_roletype);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_empty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_rolelist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_roletype_sublist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_roletype_typelist_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_roletype);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_roletype_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_userrole);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_empty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_userlist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_userrole_sublist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_userrole_rolelist_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userrole);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_userrole_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_classcommon);
	SUITE_ADD_TEST(suite, test_cil_gen_classcommon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classcommon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classcommon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classcommon_missingclassname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classcommon_noperms_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_classcommon_extraperms_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_classcommon);	
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_classcommon_neg);	

	SUITE_ADD_TEST(suite, test_cil_gen_catorder);
	SUITE_ADD_TEST(suite, test_cil_gen_catorder_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catorder_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catorder_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catorder_missingcats_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catorder_nosublist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_catorder_nestedcat_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catorder);	
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_catorder_neg);	

	SUITE_ADD_TEST(suite, test_cil_gen_dominance);
	SUITE_ADD_TEST(suite, test_cil_gen_dominance_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_dominance_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_dominance_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_dominance_nosensitivities_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_dominance_nosublist_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_dominance);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_dominance_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_senscat);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_nosublist);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_nosensitivities_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_sublist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_senscat_nocat_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_senscat);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_senscat_neg);
	
	SUITE_ADD_TEST(suite, test_cil_fill_level);
	SUITE_ADD_TEST(suite, test_cil_fill_level_sensnull_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_level_levelnull_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_level_nocat);
	SUITE_ADD_TEST(suite, test_cil_fill_level_emptycat_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_level);
	SUITE_ADD_TEST(suite, test_cil_gen_level_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_emptysensparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_emptycat_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_nosens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_level_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_level);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_level_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_rangeinvalid_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_namenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_rangenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_rangeempty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_levelrange_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_levelrange);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_levelrange_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_constrain);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_classset_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_classset_noperm_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_classset_noclass_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_permset_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_permset_noclass_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_permset_noperm_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_expression_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_constrain_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_constrain);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_constrain_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_mlsconstrain);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_mlsconstrain_neg);
	
	SUITE_ADD_TEST(suite, test_cil_fill_context);
	SUITE_ADD_TEST(suite, test_cil_fill_context_unnamedlvl);
	SUITE_ADD_TEST(suite, test_cil_fill_context_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_nouser_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_norole_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_notype_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_nolowlvl_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_nohighlvl_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_unnamedlvl_nocontextlow_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_context_unnamedlvl_nocontexthigh_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_context);
	SUITE_ADD_TEST(suite, test_cil_gen_context_notinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_extralevel_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_emptycontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_doubleparen_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_norole_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_roleinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_notype_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_typeinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_nolevels_neg);
//	SUITE_ADD_TEST(suite, test_cil_gen_context_nosecondlevel_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_nouser_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_context_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_context);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_context_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_file);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_dir);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_char);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_block);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_socket);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_pipe);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_symlink);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_any);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_str1null_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_str1_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_str2null_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_str2_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_classnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_class_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_contextnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_filecon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_filecon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_filecon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_udp);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_tcp);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_unknownprotocol_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_portrange);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_portrange_one_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_portrange_morethanone_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_singleport_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_lowport_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_highport_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_str1null_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_str1parens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_portnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_contextnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_portcon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_portcon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_portcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_fill_ipaddr);
	SUITE_ADD_TEST(suite, test_cil_fill_ipaddr_addrnodenull_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_ipaddr_addrnull_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_ipaddr_addrinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_fill_ipaddr_extra_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_ipnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_ipanon);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_ipanon_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_netmasknull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_netmaskanon);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_netmaskanon_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_contextnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_nodecon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_nodecon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_nodecon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_anon_context);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_typenull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_typeparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_pathnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_pathparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_contextnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_context_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_genfscon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_genfscon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_genfscon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_nested);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_nested_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_nested_emptysecondlist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_extra_nested_secondlist_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_nested_missingobjects_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_nested_secondnested_missingobjects_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_ethmissing_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_interfacemissing_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_netifcon_packetmissing_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_netifcon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_netifcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_pirqnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_nopirq_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_pirqrange_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_anoncontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pirqcon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_pirqcon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_pirqcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemrange);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemrange_firstnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemrange_secondnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemrange_empty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemrange_singleiomem_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemrange_morethantwoiomem_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_iomemnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_noiomem_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_anoncontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_iomemcon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_iomemcon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_iomemcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportrange);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportrange_firstnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportrange_secondnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportrange_empty_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportrange_singleioport_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportrange_morethantwoioport_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_ioportnotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_noioport_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_anoncontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ioportcon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_ioportcon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_ioportcon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_pcidevicenotint_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_nopcidevice_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_pcidevicerange_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_anoncontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_pcidevicecon_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_pcidevicecon);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_pcidevicecon_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_anoncontext);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_anoncontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_xattr);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_task);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_transition);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_invalidtype_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_notype_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_typeinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_nofilesystem_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_filesysteminparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_nocontext_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_emptyconparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_fsuse_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_fsuse);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_fsuse_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_macro_noparams);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_type);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_role);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_user);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_sensitivity);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_category);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_catset);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_level);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_class);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_classmap);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_permset);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_duplicate);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_duplicate_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_unknown_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_unnamed_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_noparam_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_nosecondparam_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_noparam_name_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_emptyparam_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_macro_paramcontainsperiod_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_macro);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_macro_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_macro_nested_macro_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_macro_nested_tunif_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_call);
	SUITE_ADD_TEST(suite, test_cil_gen_call_noargs);
	SUITE_ADD_TEST(suite, test_cil_gen_call_anon);
	SUITE_ADD_TEST(suite, test_cil_gen_call_empty_call_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_call_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_call_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_call_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_call_name_inparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_call_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_call);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_call_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_optional);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_emptyoptional);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_unnamed_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_optional_norule_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_optional);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_optional_neg);

	SUITE_ADD_TEST(suite, test_cil_gen_policycap);
	SUITE_ADD_TEST(suite, test_cil_gen_policycap_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_policycap_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_policycap_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_policycap_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_policycap_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_policycap_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_policycap);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_policycap_neg);
	
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_ipv4);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_ipv4_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_ipv6);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_ipv6_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_noname_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_nameinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_noip_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_ipinparens_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_extra_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_dbnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_currnull_neg);
	SUITE_ADD_TEST(suite, test_cil_gen_ipaddr_astnull_neg);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_ipaddr);
	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_gen_ipaddr_neg);

	SUITE_ADD_TEST(suite, test_cil_build_ast_node_helper_extraargsnull_neg);
	
	SUITE_ADD_TEST(suite, test_cil_build_ast_last_child_helper);
	SUITE_ADD_TEST(suite, test_cil_build_ast_last_child_helper_extraargsnull_neg);
	
	return suite;
}

CuSuite* CilTreeGetSuite(void) {
	CuSuite* suite = CuSuiteNew();

	/* CilTest.c */
	SUITE_ADD_TEST(suite, test_symtab_init);
	SUITE_ADD_TEST(suite, test_symtab_init_no_table_neg);


	/* test_cil.c */
	SUITE_ADD_TEST(suite, test_cil_symtab_array_init);

	SUITE_ADD_TEST(suite, test_cil_db_init);

	SUITE_ADD_TEST(suite, test_cil_get_symtab_block);
	SUITE_ADD_TEST(suite, test_cil_get_symtab_class);
	SUITE_ADD_TEST(suite, test_cil_get_symtab_root);
	SUITE_ADD_TEST(suite, test_cil_get_symtab_flavor_neg);
	SUITE_ADD_TEST(suite, test_cil_get_symtab_null_neg);
	SUITE_ADD_TEST(suite, test_cil_get_symtab_node_null_neg);
	SUITE_ADD_TEST(suite, test_cil_get_symtab_parent_null_neg);


	/* test_cil_list.c */
	SUITE_ADD_TEST(suite, test_cil_list_append_item);
	SUITE_ADD_TEST(suite, test_cil_list_append_item_append);
	SUITE_ADD_TEST(suite, test_cil_list_append_item_append_extra);
	SUITE_ADD_TEST(suite, test_cil_list_append_item_listnull_neg);
	SUITE_ADD_TEST(suite, test_cil_list_append_item_itemnull_neg);
	SUITE_ADD_TEST(suite, test_cil_list_prepend_item_prepend);
	SUITE_ADD_TEST(suite, test_cil_list_prepend_item_prepend_neg);
	SUITE_ADD_TEST(suite, test_cil_list_prepend_item_listnull_neg);
	SUITE_ADD_TEST(suite, test_cil_list_prepend_item_itemnull_neg);


	/* test_cil_symtab.c */
	SUITE_ADD_TEST(suite, test_cil_symtab_insert);


	/* test_cil_tree.c */
	SUITE_ADD_TEST(suite, test_cil_tree_init);
	SUITE_ADD_TEST(suite, test_cil_tree_node_init);


	/* test_cil_lexer.c */
	SUITE_ADD_TEST(suite, test_cil_lexer_setup);
	SUITE_ADD_TEST(suite, test_cil_lexer_next);


	/* test_cil_parser.c */
	SUITE_ADD_TEST(suite, test_cil_parser);


	/* test_cil_fqn.c */
	SUITE_ADD_TEST(suite, test_cil_qualify_name);
	SUITE_ADD_TEST(suite, test_cil_qualify_name_cil_flavor);

	/* test cil_copy_ast.c */
	SUITE_ADD_TEST(suite, test_cil_copy_list);
	SUITE_ADD_TEST(suite, test_cil_copy_list_sublist);
	SUITE_ADD_TEST(suite, test_cil_copy_list_sublist_extra);
	SUITE_ADD_TEST(suite, test_cil_copy_list_orignull_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_block);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_block);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_block_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_perm);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_perm);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_perm_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_class);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_class);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_class_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_common);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_common);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_common_dup_neg);

	SUITE_ADD_TEST(suite, test_cil_copy_classcommon);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_classcommon);
	
	SUITE_ADD_TEST(suite, test_cil_copy_sid);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sid);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sid_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_sidcontext);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sidcontext);
	
	SUITE_ADD_TEST(suite, test_cil_copy_user);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_user);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_user_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_role);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_role);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_role_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_userrole);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_userrole);
	
	SUITE_ADD_TEST(suite, test_cil_copy_type);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_type);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_type_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_typeattribute);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_typeattribute);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_typeattribute_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_typealias);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_typealias);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_typealias_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_bool);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_bool);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_bool_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_avrule);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_avrule);
	
	SUITE_ADD_TEST(suite, test_cil_copy_type_rule);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_type_rule);
	
	SUITE_ADD_TEST(suite, test_cil_copy_sens);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sens);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sens_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_sensalias);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sensalias);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_sensalias_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_cat);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_cat);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_cat_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_catalias);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_catalias);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_catalias_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_senscat);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_senscat);
	
	SUITE_ADD_TEST(suite, test_cil_copy_catorder);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_catorder);
	
	SUITE_ADD_TEST(suite, test_cil_copy_dominance);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_dominance);
	
	SUITE_ADD_TEST(suite, test_cil_copy_level);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_level);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_level_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_fill_level);
	
	SUITE_ADD_TEST(suite, test_cil_copy_context);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_context);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_context_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_netifcon);
	SUITE_ADD_TEST(suite, test_cil_copy_netifcon_nested);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_netifcon);
	
	SUITE_ADD_TEST(suite, test_cil_copy_fill_context);
	SUITE_ADD_TEST(suite, test_cil_copy_fill_context_anonrange);
	
	SUITE_ADD_TEST(suite, test_cil_copy_call);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_call);
	
	SUITE_ADD_TEST(suite, test_cil_copy_optional);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_optional);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_optional_merge);
	
	SUITE_ADD_TEST(suite, test_cil_copy_nodecon);
	SUITE_ADD_TEST(suite, test_cil_copy_nodecon_anon);
	
	SUITE_ADD_TEST(suite, test_cil_copy_fill_ipaddr);
	
	SUITE_ADD_TEST(suite, test_cil_copy_ipaddr);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_ipaddr);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_ipaddr_dup_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_conditional);
	
	SUITE_ADD_TEST(suite, test_cil_copy_boolif);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_boolif);
	
	SUITE_ADD_TEST(suite, test_cil_copy_constrain);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_mlsconstrain);

	//SUITE_ADD_TEST(suite, test_cil_copy_ast);
	//SUITE_ADD_TEST(suite, test_cil_copy_ast_neg);
	
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_orignull_neg);
	SUITE_ADD_TEST(suite, test_cil_copy_node_helper_extraargsnull_neg);
	
	/* test_post.c */
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_meta_a_not_b);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_meta_b_not_a);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_meta_a_and_b_strlen_a_greater_b);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_meta_a_and_b_strlen_b_greater_a);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_type_atype_greater_btype);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_type_btype_greater_atype);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_stemlen_a_greater_b);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_stemlen_b_greater_a);
	SUITE_ADD_TEST(suite, test_cil_post_filecon_compare_equal);
	
	SUITE_ADD_TEST(suite, test_cil_post_genfscon_compare_atypestr_greater_btypestr);
	SUITE_ADD_TEST(suite, test_cil_post_genfscon_compare_btypestr_greater_atypestr);
	SUITE_ADD_TEST(suite, test_cil_post_genfscon_compare_apathstr_greater_bpathstr);
	SUITE_ADD_TEST(suite, test_cil_post_genfscon_compare_bpathstr_greater_apathstr);
	SUITE_ADD_TEST(suite, test_cil_post_genfscon_compare_equal);
	
	SUITE_ADD_TEST(suite, test_cil_post_netifcon_compare_a_greater_b);
	SUITE_ADD_TEST(suite, test_cil_post_netifcon_compare_b_greater_a);
	SUITE_ADD_TEST(suite, test_cil_post_netifcon_compare_equal);
	
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_aipv4_bipv6);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_aipv6_bipv4);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_aipv4_greaterthan_bipv4);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_aipv4_lessthan_bipv4);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_amaskipv4_greaterthan_bmaskipv4);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_amaskipv4_lessthan_bmaskipv4);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_aipv6_greaterthan_bipv6);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_aipv6_lessthan_bipv6);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_amaskipv6_greaterthan_bmaskipv6);
	SUITE_ADD_TEST(suite, test_cil_post_nodecon_compare_amaskipv6_lessthan_bmaskipv6);
	
	SUITE_ADD_TEST(suite, test_cil_post_fsuse_compare_type_a_greater_b);
	SUITE_ADD_TEST(suite, test_cil_post_fsuse_compare_type_b_greater_a);
	SUITE_ADD_TEST(suite, test_cil_post_fsuse_compare_fsstr_a_greater_b);
	SUITE_ADD_TEST(suite, test_cil_post_fsuse_compare_fsstr_b_greater_a);
	SUITE_ADD_TEST(suite, test_cil_post_fsuse_compare_equal);
	
	return suite;
}

CuSuite* CilTestFullCil(void) {
	CuSuite* suite = CuSuiteNew();
	SUITE_ADD_TEST(suite, test_min_policy);
	SUITE_ADD_TEST(suite, test_integration);

	return suite;
}

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

#ifndef CIL_VERIFY_H_
#define CIL_VERIFY_H_

#include <stdint.h>

#include "cil_internal.h"
#include "cil_flavor.h"
#include "cil_tree.h"
#include "cil_list.h"

enum cil_syntax {
	CIL_SYN_STRING      = 1 << 0,
	CIL_SYN_LIST        = 1 << 1,
	CIL_SYN_EMPTY_LIST  = 1 << 2,
	CIL_SYN_N_LISTS     = 1 << 3,
	CIL_SYN_N_STRINGS   = 1 << 4,
	CIL_SYN_END         = 1 << 5
};

struct cil_args_verify {
	struct cil_db *db;
	struct cil_complex_symtab *csymtab;
	int *avrule_cnt;
	int *handleunknown;
	int *mls;
	int *nseuserdflt;
	int *pass;
};

int cil_verify_name(const struct cil_db *db, const char *name, enum cil_flavor flavor);
int __cil_verify_syntax(struct cil_tree_node *parse_current, enum cil_syntax s[], size_t len);
int cil_verify_expr_syntax(struct cil_tree_node *current, enum cil_flavor op, enum cil_flavor expr_flavor);
int cil_verify_constraint_leaf_expr_syntax(enum cil_flavor l_flavor, enum cil_flavor r_flavor, enum cil_flavor op, enum cil_flavor expr_flavor);
int cil_verify_constraint_expr_syntax(struct cil_tree_node *current, enum cil_flavor op);
int cil_verify_conditional_blocks(struct cil_tree_node *current);
int cil_verify_decl_does_not_shadow_macro_parameter(struct cil_macro *macro, struct cil_tree_node *node, const char *name);
int __cil_verify_ranges(struct cil_list *list);
int __cil_verify_ordered_node_helper(struct cil_tree_node *node, uint32_t *finished, void *extra_args);
int __cil_verify_ordered(struct cil_tree_node *current, enum cil_flavor flavor);
int __cil_verify_initsids(struct cil_list *sids);
int __cil_verify_senscat(struct cil_sens *sens, struct cil_cat *cat);
int __cil_verify_helper(struct cil_tree_node *node, __attribute__((unused)) uint32_t *finished, void *extra_args);
int __cil_pre_verify_helper(struct cil_tree_node *node, __attribute__((unused)) uint32_t *finished, void *extra_args);

#endif

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

#ifndef CIL_RESOLVE_AST_H_
#define CIL_RESOLVE_AST_H_

#include <stdint.h>

#include "cil_internal.h"
#include "cil_tree.h"

int cil_resolve_classorder(struct cil_tree_node *current, void *extra_args);
int cil_resolve_classperms(struct cil_tree_node *current, struct cil_classperms *cp, void *extra_args);
int cil_resolve_classpermissionset(struct cil_tree_node *current, struct cil_classpermissionset *cps, void *extra_args);
int cil_resolve_classperms_list(struct cil_tree_node *current, struct cil_list *cp_list, void *extra_args);
int cil_resolve_avrule(struct cil_tree_node *current, void *extra_args);
int cil_resolve_type_rule(struct cil_tree_node *current, void *extra_args);
int cil_resolve_typeattributeset(struct cil_tree_node *current, void *extra_args);
int cil_resolve_typealias(struct cil_tree_node *current, void *extra_args);
int cil_resolve_typebounds(struct cil_tree_node *current, void *extra_args);
int cil_resolve_typepermissive(struct cil_tree_node *current, void *extra_args);
int cil_resolve_nametypetransition(struct cil_tree_node *current, void *extra_args);
int cil_resolve_rangetransition(struct cil_tree_node *current, void *extra_args);
int cil_resolve_classcommon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_classmapping(struct cil_tree_node *current, void *extra_args);
int cil_resolve_userrole(struct cil_tree_node *current, void *extra_args);
int cil_resolve_userlevel(struct cil_tree_node *current, void *extra_args);
int cil_resolve_userrange(struct cil_tree_node *current, void *extra_args);
int cil_resolve_userbounds(struct cil_tree_node *current, void *extra_args);
int cil_resolve_userprefix(struct cil_tree_node *current, void *extra_args);
int cil_resolve_userattributeset(struct cil_tree_node *current, void *extra_args);
int cil_resolve_selinuxuser(struct cil_tree_node *current, void *extra_args);
int cil_resolve_roletype(struct cil_tree_node *current, void *extra_args);
int cil_resolve_roletransition(struct cil_tree_node *current, void *extra_args);
int cil_resolve_roleallow(struct cil_tree_node *current, void *extra_args);
int cil_resolve_roleattributeset(struct cil_tree_node *current, void *extra_args);
int cil_resolve_rolebounds(struct cil_tree_node *current, void *extra_args);
int cil_resolve_sensalias(struct cil_tree_node *current, void *extra_args);
int cil_resolve_catalias(struct cil_tree_node *current, void *extra_args);
int cil_resolve_catorder(struct cil_tree_node *current, void *extra_args);
int cil_resolve_sensitivityorder(struct cil_tree_node *current, void *extra_args);
int cil_resolve_cat_list(struct cil_tree_node *current, struct cil_list *cat_list, struct cil_list *res_cat_list, void *extra_args);
int cil_resolve_catset(struct cil_tree_node *current, struct cil_catset *catset, void *extra_args);
int cil_resolve_senscat(struct cil_tree_node *current, void *extra_args);
int cil_resolve_level(struct cil_tree_node *current, struct cil_level *level, void *extra_args); 
int cil_resolve_levelrange(struct cil_tree_node *current, struct cil_levelrange *levelrange, void *extra_args); 
int cil_resolve_constrain(struct cil_tree_node *current, void *extra_args);
int cil_resolve_validatetrans(struct cil_tree_node *current, void *extra_args);
int cil_resolve_context(struct cil_tree_node *current, struct cil_context *context, void *extra_args);
int cil_resolve_filecon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_ibpkeycon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_ibendportcon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_portcon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_genfscon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_nodecon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_netifcon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_pirqcon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_iomemcon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_ioportcon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_pcidevicecon(struct cil_tree_node *current, void *extra_args);
int cil_resolve_fsuse(struct cil_tree_node *current, void *extra_args);
int cil_resolve_sidcontext(struct cil_tree_node *current, void *extra_args);
int cil_resolve_sidorder(struct cil_tree_node *current, void *extra_args);
int cil_resolve_blockinherit(struct cil_tree_node *current, void *extra_args);
int cil_resolve_in(struct cil_tree_node *current, void *extra_args);
int cil_resolve_call1(struct cil_tree_node *current, void *extra_args);
int cil_resolve_call2(struct cil_tree_node *, void *extra_args);
int cil_resolve_name_call_args(struct cil_call *call, char *name, enum cil_sym_index sym_index, struct cil_symtab_datum **datum);
int cil_resolve_expr(enum cil_flavor expr_type, struct cil_list *str_expr, struct cil_list **datum_expr, struct cil_tree_node *parent, void *extra_args);
int cil_resolve_boolif(struct cil_tree_node *current, void *extra_args);
int cil_evaluate_expr(struct cil_list *datum_expr, uint16_t *result);
int cil_resolve_tunif(struct cil_tree_node *current, void *extra_args);

int cil_resolve_ast(struct cil_db *db, struct cil_tree_node *current);
int cil_resolve_name(struct cil_tree_node *ast_node, char *name, enum cil_sym_index sym_index, void *extra_args, struct cil_symtab_datum **datum);
int cil_resolve_name_keep_aliases(struct cil_tree_node *ast_node, char *name, enum cil_sym_index sym_index, void *extra_args, struct cil_symtab_datum **datum);

#endif /* CIL_RESOLVE_AST_H_ */

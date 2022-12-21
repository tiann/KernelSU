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

#include "cil_internal.h"
#include "cil_log.h"
#include "cil_mem.h"
#include "cil_tree.h"
#include "cil_list.h"
#include "cil_symtab.h"
#include "cil_copy_ast.h"
#include "cil_build_ast.h"
#include "cil_strpool.h"
#include "cil_verify.h"

struct cil_args_copy {
	struct cil_tree_node *orig_dest;
	struct cil_tree_node *dest;
	struct cil_db *db;
};

void cil_copy_list(struct cil_list *data, struct cil_list **copy)
{
	struct cil_list *new;
	struct cil_list_item *orig_item;

	cil_list_init(&new, data->flavor);

	cil_list_for_each(orig_item, data) {
		switch (orig_item->flavor) {
		case CIL_STRING:
			cil_list_append(new, CIL_STRING, orig_item->data);
			break;
		case CIL_LIST: {
			struct cil_list *new_sub = NULL;
			cil_copy_list((struct cil_list*)orig_item->data, &new_sub);
			cil_list_append(new, CIL_LIST, new_sub);
			break;
		}
		case CIL_PARAM: {
			struct cil_param *po = orig_item->data;
			struct cil_param *pn;
			cil_param_init(&pn);
			pn->str = po->str;
			pn->flavor = po->flavor;
			cil_list_append(new, CIL_PARAM, pn);
		}
			break;

		default:
			cil_list_append(new, orig_item->flavor, orig_item->data);
			break;
		}
	}

	*copy = new;
}

static int cil_copy_node(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	char *new = NULL;

	if (data != NULL) {
		new = data;
	}
	*copy = new;

	return SEPOL_OK;
}

int cil_copy_block(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_block *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		if (FLAVOR(datum) != CIL_BLOCK) {
			cil_tree_log(NODE(orig), CIL_ERR, "Block %s being copied", key);
			cil_tree_log(NODE(datum), CIL_ERR, "  Conflicts with %s already declared", cil_node_to_string(NODE(datum)));
			return SEPOL_ERR;
		}
		cil_tree_log(NODE(orig), CIL_WARN, "Block %s being copied", key);
		cil_tree_log(NODE(datum), CIL_WARN, "  Previously declared");
		*copy = datum;
	} else {
		struct cil_block *new;
		cil_block_init(&new);
		*copy = new;
	}

	return SEPOL_OK;
}

int cil_copy_blockabstract(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_blockabstract *orig = data;
	struct cil_blockabstract *new = NULL;

	cil_blockabstract_init(&new);

	new->block_str = orig->block_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_blockinherit(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_blockinherit *orig = data;
	struct cil_blockinherit *new = NULL;

	cil_blockinherit_init(&new);

	new->block_str = orig->block_str;
	new->block = orig->block;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_policycap(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_policycap *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_policycap *new;
		cil_policycap_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_perm(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_perm *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_perm *new;
		cil_perm_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

void cil_copy_classperms(struct cil_classperms *orig, struct cil_classperms **new)
{
	cil_classperms_init(new);
	(*new)->class_str = orig->class_str;
	cil_copy_list(orig->perm_strs, &((*new)->perm_strs));
}

void cil_copy_classperms_set(struct cil_classperms_set *orig, struct cil_classperms_set **new)
{
	cil_classperms_set_init(new);
	(*new)->set_str = orig->set_str;
}

void cil_copy_classperms_list(struct cil_list *orig, struct cil_list **new)
{
	struct cil_list_item *orig_item;

	if (orig == NULL) {
		return;
	}

	cil_list_init(new, CIL_LIST_ITEM);
	cil_list_for_each(orig_item, orig) {
		if (orig_item->flavor == CIL_CLASSPERMS) {
			struct cil_classperms *cp;
			cil_copy_classperms(orig_item->data, &cp);
			cil_list_append(*new, CIL_CLASSPERMS, cp);
		} else {
			struct cil_classperms_set *cp_set;
			cil_copy_classperms_set(orig_item->data, &cp_set);
			cil_list_append(*new, CIL_CLASSPERMS_SET, cp_set);
		}
	}
}

int cil_copy_classmapping(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_classmapping *orig = data;
	struct cil_classmapping *new = NULL;

	cil_classmapping_init(&new);

	new->map_class_str = orig->map_class_str;
	new->map_perm_str = orig->map_perm_str;

	cil_copy_classperms_list(orig->classperms, &new->classperms);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_class(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_class *orig = data;
	struct cil_class *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_class: class cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_class_init(&new);

	new->common = NULL;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_classorder(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_classorder *orig = data;
	struct cil_classorder *new = NULL;

	cil_classorder_init(&new);
	if (orig->class_list_str != NULL) {
		cil_copy_list(orig->class_list_str, &new->class_list_str);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_classpermission(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_classpermission *orig = data;
	struct cil_classpermission *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	if (key != NULL) {
		cil_symtab_get_datum(symtab, key, &datum);
		if (datum != NULL) {
			cil_log(CIL_INFO, "classpermission cannot be redefined\n");
			return SEPOL_ERR;
		}
	}

	cil_classpermission_init(&new);

	cil_copy_classperms_list(orig->classperms, &new->classperms);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_classpermissionset(__attribute__((unused)) struct cil_db *db, void *data, void **copy,  __attribute__((unused)) symtab_t *symtab)
{
	struct cil_classpermissionset *orig = data;
	struct cil_classpermissionset *new = NULL;

	cil_classpermissionset_init(&new);

	new->set_str = orig->set_str;

	cil_copy_classperms_list(orig->classperms, &new->classperms);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_classcommon(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_classcommon *orig = data;
	struct cil_classcommon *new = NULL;

	cil_classcommon_init(&new);

	new->class_str = orig->class_str;
	new->common_str = orig->common_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_sid(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_sid *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_sid *new;
		cil_sid_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_sidcontext(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_sidcontext *orig = data;
	struct cil_sidcontext *new = NULL;

	cil_sidcontext_init(&new);

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_sidorder(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_sidorder *orig = data;
	struct cil_sidorder *new = NULL;

	cil_sidorder_init(&new);
	if (orig->sid_list_str != NULL) {
		cil_copy_list(orig->sid_list_str, &new->sid_list_str);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_user(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_user *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_user *new;
		cil_user_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_userattribute(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_userattribute *orig = data;
	struct cil_userattribute *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		cil_userattribute_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_userattributeset(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_userattributeset *orig = data;
	struct cil_userattributeset *new = NULL;

	cil_userattributeset_init(&new);

	new->attr_str = orig->attr_str;

	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_userrole(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_userrole *orig = data;
	struct cil_userrole *new = NULL;

	cil_userrole_init(&new);

	new->user_str = orig->user_str;
	new->role_str = orig->role_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_userlevel(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_userlevel *orig = data;
	struct cil_userlevel *new = NULL;

	cil_userlevel_init(&new);

	new->user_str = orig->user_str;

	if (orig->level_str != NULL) {
		new->level_str = orig->level_str;
	} else {
		cil_copy_fill_level(db, orig->level, &new->level);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_userrange(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_userrange *orig = data;
	struct cil_userrange *new = NULL;

	cil_userrange_init(&new);

	new->user_str = orig->user_str;

	if (orig->range_str != NULL) {
		new->range_str = orig->range_str;
	} else {
		cil_levelrange_init(&new->range);
		cil_copy_fill_levelrange(db, orig->range, new->range);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_userprefix(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_userprefix *orig = data;
	struct cil_userprefix *new = NULL;

	cil_userprefix_init(&new);

	new->user_str = orig->user_str;
	new->prefix_str = orig->prefix_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_role(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_role *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_role *new;
		cil_role_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_roletype(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_roletype *orig = data;
	struct cil_roletype *new = NULL;

	cil_roletype_init(&new);

	new->role_str = orig->role_str;
	new->type_str = orig->type_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_roleattribute(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_roleattribute *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_roleattribute *new;
		cil_roleattribute_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_roleattributeset(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_roleattributeset *orig = data;
	struct cil_roleattributeset *new = NULL;

	cil_roleattributeset_init(&new);

	new->attr_str = orig->attr_str;
	
	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_roleallow(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_roleallow *orig = data;
	struct cil_roleallow *new = NULL;

	cil_roleallow_init(&new);

	new->src_str = orig->src_str;
	new->tgt_str = orig->tgt_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_type(__attribute__((unused)) struct cil_db *db, __attribute__((unused)) void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_type *new;

	cil_type_init(&new);
	*copy = new;

	return SEPOL_OK;
}

int cil_copy_typepermissive(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_typepermissive *orig = data;
	struct cil_typepermissive *new = NULL;

	cil_typepermissive_init(&new);

	new->type_str = orig->type_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_typeattribute(__attribute__((unused)) struct cil_db *db, __attribute__((unused)) void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_typeattribute *new;

	cil_typeattribute_init(&new);
	*copy = new;

	return SEPOL_OK;
}

int cil_copy_typeattributeset(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_typeattributeset *orig = data;
	struct cil_typeattributeset *new = NULL;

	cil_typeattributeset_init(&new);

	new->attr_str = orig->attr_str;

	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_expandtypeattribute(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_expandtypeattribute *orig = data;
	struct cil_expandtypeattribute *new = NULL;

	cil_expandtypeattribute_init(&new);

	if (orig->attr_strs != NULL) {
		cil_copy_list(orig->attr_strs, &new->attr_strs);
	}

	if (orig->attr_datums != NULL) {
		cil_copy_list(orig->attr_datums, &new->attr_datums);
	}

	new->expand = orig->expand;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_alias(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_alias *orig = data;
	struct cil_alias *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_alias: alias cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_alias_init(&new);

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_aliasactual(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused))symtab_t *symtab)
{
	struct cil_aliasactual *orig = data;
	struct cil_aliasactual *new = NULL;

	cil_aliasactual_init(&new);

	new->alias_str = orig->alias_str;
	new->actual_str = orig->actual_str;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_roletransition(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_roletransition *orig = data;
	struct cil_roletransition *new = NULL;

	cil_roletransition_init(&new);

	new->src_str = orig->src_str;
	new->tgt_str = orig->tgt_str;
	new->obj_str = orig->obj_str;
	new->result_str = orig->result_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_nametypetransition(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_nametypetransition *orig = data;
	struct cil_nametypetransition *new = NULL;

	cil_nametypetransition_init(&new);

	new->src_str = orig->src_str;
	new->tgt_str = orig->tgt_str;
	new->obj_str = orig->obj_str;
	new->name_str = orig->name_str;
	new->result_str = orig->result_str;


	*copy = new;

	return SEPOL_OK;
}

int cil_copy_rangetransition(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_rangetransition *orig = data;
	struct cil_rangetransition *new = NULL;

	cil_rangetransition_init(&new);

	new->src_str = orig->src_str;
	new->exec_str = orig->exec_str;
	new->obj_str = orig->obj_str;

	if (orig->range_str != NULL) {
		new->range_str = orig->range_str;
	} else {
		cil_levelrange_init(&new->range);
		cil_copy_fill_levelrange(db, orig->range, new->range);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_bool(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_bool *orig = data;
	struct cil_bool *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_bool: boolean cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_bool_init(&new);
	new->value = orig->value;
	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_tunable(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_tunable *orig = data;
	struct cil_tunable *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_tunable: tunable cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_tunable_init(&new);
	new->value = orig->value;
	*copy = new;

	return SEPOL_OK;
}

static void cil_copy_fill_permissionx(struct cil_db *db, struct cil_permissionx *orig, struct cil_permissionx *new)
{
	new->kind = orig->kind;
	new->obj_str = orig->obj_str;
	cil_copy_expr(db, orig->expr_str, &new->expr_str);
}

int cil_copy_avrule(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_avrule *orig = data;
	struct cil_avrule *new = NULL;

	cil_avrule_init(&new);

	new->is_extended = orig->is_extended;
	new->rule_kind = orig->rule_kind;
	new->src_str = orig->src_str;
	new->tgt_str = orig->tgt_str;

	if (!new->is_extended) {
		cil_copy_classperms_list(orig->perms.classperms, &new->perms.classperms);
	} else {
		if (orig->perms.x.permx_str != NULL) {
			new->perms.x.permx_str = orig->perms.x.permx_str;
		} else {
			cil_permissionx_init(&new->perms.x.permx);
			cil_copy_fill_permissionx(db, orig->perms.x.permx, new->perms.x.permx);
		}
	}

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_permissionx(struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_permissionx *orig = data;
	struct cil_permissionx *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;


	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_permissionx: permissionx cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_permissionx_init(&new);
	cil_copy_fill_permissionx(db, orig, new);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_type_rule(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_type_rule  *orig = data;
	struct cil_type_rule *new = NULL;

	cil_type_rule_init(&new);

	new->rule_kind = orig->rule_kind;
	new->src_str = orig->src_str;
	new->tgt_str = orig->tgt_str;
	new->obj_str = orig->obj_str;
	new->result_str = orig->result_str;

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_sens(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_sens *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_sens *new;
		cil_sens_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

int cil_copy_cat(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_cat *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum == NULL) {
		struct cil_cat *new;
		cil_cat_init(&new);
		*copy = new;
	} else {
		*copy = datum;
	}

	return SEPOL_OK;
}

static void cil_copy_cats(struct cil_db *db, struct cil_cats *orig, struct cil_cats **new)
{
	cil_cats_init(new);
	cil_copy_expr(db, orig->str_expr, &(*new)->str_expr);
	cil_copy_expr(db, orig->datum_expr, &(*new)->datum_expr);
}

int cil_copy_catset(struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_catset *orig = data;
	struct cil_catset *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_catset: categoryset cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_catset_init(&new);

	cil_copy_cats(db, orig->cats, &new->cats);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_senscat(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_senscat *orig = data;
	struct cil_senscat *new = NULL;

	cil_senscat_init(&new);

	new->sens_str = orig->sens_str;

	cil_copy_cats(db, orig->cats, &new->cats);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_catorder(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_catorder *orig = data;
	struct cil_catorder *new = NULL;

	cil_catorder_init(&new);
	if (orig->cat_list_str != NULL) {
		cil_copy_list(orig->cat_list_str, &new->cat_list_str);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_sensitivityorder(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_sensorder *orig = data;
	struct cil_sensorder *new = NULL;

	cil_sensorder_init(&new);
	if (orig->sens_list_str != NULL) {
		cil_copy_list(orig->sens_list_str, &new->sens_list_str);
	}

	*copy = new;

	return SEPOL_OK;
}

void cil_copy_fill_level(struct cil_db *db, struct cil_level *orig, struct cil_level **new)
{
	cil_level_init(new);

	(*new)->sens_str = orig->sens_str;

	if (orig->cats != NULL) {
		cil_copy_cats(db, orig->cats, &(*new)->cats);
	}
}

int cil_copy_level(struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_level *orig = data;
	struct cil_level *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	if (key != NULL) {
		cil_symtab_get_datum(symtab, key, &datum);
		if (datum != NULL) {
			cil_log(CIL_INFO, "cil_copy_level: level cannot be redefined\n");
			return SEPOL_ERR;
		}
	}

	cil_copy_fill_level(db, orig, &new);

	*copy = new;

	return SEPOL_OK;
}

void cil_copy_fill_levelrange(struct cil_db *db, struct cil_levelrange *data, struct cil_levelrange *new)
{
	if (data->low_str != NULL) {
		new->low_str = data->low_str;
	} else {
		cil_copy_fill_level(db, data->low, &new->low);
	}

	if (data->high_str != NULL) {
		new->high_str = data->high_str;
	} else {
		cil_copy_fill_level(db, data->high, &new->high);
	}
}

int cil_copy_levelrange(struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_levelrange *orig = data;
	struct cil_levelrange *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	if (key != NULL) {
		cil_symtab_get_datum(symtab, key, &datum);
		if (datum != NULL) {
			cil_log(CIL_INFO, "cil_copy_levelrange: levelrange cannot be redefined\n");
			return SEPOL_ERR;
		}
	}

	cil_levelrange_init(&new);
	cil_copy_fill_levelrange(db, orig, new);

	*copy = new;

	return SEPOL_OK;
}

void cil_copy_fill_context(struct cil_db *db, struct cil_context *data, struct cil_context *new)
{
	new->user_str = data->user_str;
	new->role_str = data->role_str;
	new->type_str = data->type_str;

	if (data->range_str != NULL) {
		new->range_str = data->range_str;
	} else {
		cil_levelrange_init(&new->range);
		cil_copy_fill_levelrange(db, data->range, new->range);
	}
}

int cil_copy_context(struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_context *orig = data;
	struct cil_context *new = NULL;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	if (key != NULL) {
		cil_symtab_get_datum(symtab, key, &datum);
		if (datum != NULL) {
			cil_log(CIL_INFO, "cil_copy_context: context cannot be redefined\n");
			return SEPOL_ERR;
		}
	}

	cil_context_init(&new);
	cil_copy_fill_context(db, orig, new);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_netifcon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_netifcon *orig = data;
	struct cil_netifcon *new = NULL;

	cil_netifcon_init(&new);

	new->interface_str = orig->interface_str;

	if (orig->if_context_str != NULL) {
		new->if_context_str = orig->if_context_str;
	} else {
		cil_context_init(&new->if_context);
		cil_copy_fill_context(db, orig->if_context, new->if_context);
	}

	if (orig->packet_context_str != NULL) {
		new->packet_context_str = orig->packet_context_str;
	} else {
		cil_context_init(&new->packet_context);
		cil_copy_fill_context(db, orig->packet_context, new->packet_context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_genfscon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_genfscon *orig = data;
	struct cil_genfscon *new = NULL;

	cil_genfscon_init(&new);

	new->fs_str = orig->fs_str;
	new->path_str = orig->path_str;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_filecon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_filecon *orig = data;
	struct cil_filecon *new = NULL;

	cil_filecon_init(&new);

	new->path_str = orig->path_str;
	new->type = orig->type;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else if (orig->context != NULL) {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_nodecon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_nodecon *orig = data;
	struct cil_nodecon *new = NULL;

	cil_nodecon_init(&new);

	if (orig->addr_str != NULL) {
		new->addr_str = orig->addr_str;
	} else {
		cil_ipaddr_init(&new->addr);
		cil_copy_fill_ipaddr(orig->addr, new->addr);
	}

	if (orig->mask_str != NULL) {
		new->mask_str = orig->mask_str;
	} else {
		cil_ipaddr_init(&new->mask);
		cil_copy_fill_ipaddr(orig->mask, new->mask);
	}

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_ibpkeycon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_ibpkeycon *orig = data;
	struct cil_ibpkeycon *new = NULL;

	cil_ibpkeycon_init(&new);

	new->subnet_prefix_str = orig->subnet_prefix_str;
	new->pkey_low = orig->pkey_low;
	new->pkey_high = orig->pkey_high;

	if (orig->context_str) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_ibendportcon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_ibendportcon *orig = data;
	struct cil_ibendportcon *new = NULL;

	cil_ibendportcon_init(&new);

	new->dev_name_str = orig->dev_name_str;
	new->port = orig->port;

	if (orig->context_str) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_portcon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_portcon *orig = data;
	struct cil_portcon *new = NULL;

	cil_portcon_init(&new);

	new->proto = orig->proto;
	new->port_low = orig->port_low;
	new->port_high = orig->port_high;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_pirqcon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_pirqcon *orig = data;
	struct cil_pirqcon *new = NULL;

	cil_pirqcon_init(&new);

	new->pirq = orig->pirq;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_iomemcon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_iomemcon *orig = data;
	struct cil_iomemcon *new = NULL;

	cil_iomemcon_init(&new);

	new->iomem_low = orig->iomem_low;
	new->iomem_high = orig->iomem_high;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_ioportcon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_ioportcon *orig = data;
	struct cil_ioportcon *new = NULL;

	cil_ioportcon_init(&new);

	new->ioport_low = orig->ioport_low;
	new->ioport_high = orig->ioport_high;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_pcidevicecon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_pcidevicecon *orig = data;
	struct cil_pcidevicecon *new = NULL;

	cil_pcidevicecon_init(&new);

	new->dev = orig->dev;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_devicetreecon(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_devicetreecon *orig = data;
	struct cil_devicetreecon *new = NULL;

	cil_devicetreecon_init(&new);

	new->path = orig->path;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_fsuse(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_fsuse *orig = data;
	struct cil_fsuse *new = NULL;

	cil_fsuse_init(&new);

	new->type = orig->type;
	new->fs_str = orig->fs_str;

	if (orig->context_str != NULL) {
		new->context_str = orig->context_str;
	} else {
		cil_context_init(&new->context);
		cil_copy_fill_context(db, orig->context, new->context);
	}

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_expr(struct cil_db *db, struct cil_list *orig, struct cil_list **new)
{
	struct cil_list_item *curr;

	if (orig == NULL) {
		*new = NULL;
		return SEPOL_OK;
	}

	cil_list_init(new, orig->flavor);

	cil_list_for_each(curr, orig) {
		switch (curr->flavor) {
		case CIL_LIST: {
			struct cil_list *sub_list;
			cil_copy_expr(db, curr->data, &sub_list);
			cil_list_append(*new, CIL_LIST, sub_list);
			break;
		}
		case CIL_STRING:
			cil_list_append(*new, CIL_STRING, curr->data);
			break;
		case CIL_DATUM:
			cil_list_append(*new, curr->flavor, curr->data);
			break;
		case CIL_OP:
			cil_list_append(*new, curr->flavor, curr->data);
			break;
		case CIL_CONS_OPERAND:
			cil_list_append(*new, curr->flavor, curr->data);
			break;
		default:
			cil_log(CIL_INFO, "Unknown flavor %d in expression being copied\n",curr->flavor);
			cil_list_append(*new, curr->flavor, curr->data);
			break;
		}
	}

	return SEPOL_OK;
}

int cil_copy_constrain(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_constrain *orig = data;
	struct cil_constrain *new = NULL;

	cil_constrain_init(&new);
	cil_copy_classperms_list(orig->classperms, &new->classperms);

	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_validatetrans(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_validatetrans *orig = data;
	struct cil_validatetrans *new = NULL;

	cil_validatetrans_init(&new);

	new->class_str = orig->class_str;

	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);

	*copy = new;

	return SEPOL_OK;
}

int cil_copy_call(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_call *orig = data;
	struct cil_call *new = NULL;
	int rc = SEPOL_ERR;

	cil_call_init(&new);

	new->macro_str = orig->macro_str;
	new->macro = orig->macro;

	if (orig->args_tree != NULL) {
		cil_tree_init(&new->args_tree);
		rc = cil_copy_ast(db, orig->args_tree->root, new->args_tree->root);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}
	
	new->copied = orig->copied;

	*copy = new;

	return SEPOL_OK;

exit:
	cil_destroy_call(new);
	return rc;
}

static int cil_copy_macro(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_macro *orig = data;
	char *key = orig->datum.name;
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		if (FLAVOR(datum) != CIL_MACRO) {
			cil_tree_log(NODE(orig), CIL_ERR, "Macro %s being copied", key);
			cil_tree_log(NODE(datum), CIL_ERR, "  Conflicts with %s already declared", cil_node_to_string(NODE(datum)));
			return SEPOL_ERR;
		}
		cil_tree_log(NODE(orig), CIL_WARN, "Skipping macro %s", key);
		cil_tree_log(NODE(datum), CIL_WARN, "  Previously declared");
		*copy = NULL;
	} else {
		struct cil_macro *new;
		cil_macro_init(&new);
		if (orig->params != NULL) {
			cil_copy_list(orig->params, &new->params);
		}
		*copy = new;
	}

	return SEPOL_OK;
}

int cil_copy_optional(__attribute__((unused)) struct cil_db *db, __attribute__((unused)) void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_optional *new;

	cil_optional_init(&new);
	*copy = new;

	return SEPOL_OK;
}

void cil_copy_fill_ipaddr(struct cil_ipaddr *data, struct cil_ipaddr *new)
{
	new->family = data->family;
	memcpy(&new->ip, &data->ip, sizeof(data->ip));
}

int cil_copy_ipaddr(__attribute__((unused)) struct cil_db *db, void *data, void **copy, symtab_t *symtab)
{
	struct cil_ipaddr *orig = data;
	struct cil_ipaddr *new = NULL;
	char * key = orig->datum.name;	
	struct cil_symtab_datum *datum = NULL;

	cil_symtab_get_datum(symtab, key, &datum);
	if (datum != NULL) {
		cil_log(CIL_INFO, "cil_copy_ipaddr: ipaddress cannot be redefined\n");
		return SEPOL_ERR;
	}

	cil_ipaddr_init(&new);
	cil_copy_fill_ipaddr(orig, new);

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_condblock(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_condblock *orig = data;
	struct cil_condblock *new = *copy;
	cil_condblock_init(&new);
	new->flavor = orig->flavor;
	*copy = new;

	return SEPOL_OK;
}

int cil_copy_boolif(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_booleanif *orig = data;
	struct cil_booleanif *new = NULL;

	cil_boolif_init(&new);

	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);
	new->preserved_tunable = orig->preserved_tunable;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_tunif(struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_tunableif *orig = data;
	struct cil_tunableif *new = NULL;

	cil_tunif_init(&new);

	cil_copy_expr(db, orig->str_expr, &new->str_expr);
	cil_copy_expr(db, orig->datum_expr, &new->datum_expr);

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_default(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_default *orig = data;
	struct cil_default *new = NULL;

	cil_default_init(&new);

	new->flavor = orig->flavor;

	if (orig->class_strs != NULL) {
		cil_copy_list(orig->class_strs, &new->class_strs);
	}

	new->object = orig->object;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_defaultrange(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_defaultrange *orig = data;
	struct cil_defaultrange *new = NULL;

	cil_defaultrange_init(&new);

	if (orig->class_strs != NULL) {
		cil_copy_list(orig->class_strs, &new->class_strs);
	}

	new->object_range = orig->object_range;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_handleunknown(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_handleunknown *orig = data;
	struct cil_handleunknown *new = NULL;

	cil_handleunknown_init(&new);
	new->handle_unknown = orig->handle_unknown;
	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_mls(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_mls *orig = data;
	struct cil_mls *new = NULL;

	cil_mls_init(&new);
	new->value = orig->value;
	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_bounds(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_bounds *orig = data;
	struct cil_bounds *new = NULL;

	cil_bounds_init(&new);

	new->parent_str = orig->parent_str;
	new->child_str = orig->child_str;

	*copy = new;

	return SEPOL_OK;
}

static int cil_copy_src_info(__attribute__((unused)) struct cil_db *db, void *data, void **copy, __attribute__((unused)) symtab_t *symtab)
{
	struct cil_src_info *orig = data;
	struct cil_src_info *new = NULL;

	cil_src_info_init(&new);

	new->kind = orig->kind;
	new->hll_line = orig->hll_line;
	new->path = orig->path;

	*copy = new;

	return SEPOL_OK;
}

static int __cil_copy_node_helper(struct cil_tree_node *orig, uint32_t *finished, void *extra_args)
{
	int rc = SEPOL_ERR;
	struct cil_tree_node *parent = NULL;
	struct cil_tree_node *new = NULL;
	struct cil_db *db = NULL;
	struct cil_args_copy *args = NULL;
	struct cil_tree_node *namespace = NULL;
	enum cil_sym_index sym_index = CIL_SYM_UNKNOWN;
	symtab_t *symtab = NULL;
	void *data = NULL;
	int (*copy_func)(struct cil_db *db, void *data, void **copy, symtab_t *symtab) = NULL;
	struct cil_blockinherit *blockinherit = NULL;

	if (orig == NULL || extra_args == NULL) {
		goto exit;
	}

	args = extra_args;
	parent = args->dest;
	db = args->db;


	switch (orig->flavor) {
	case CIL_BLOCK:
		copy_func = &cil_copy_block;
		break;
	case CIL_BLOCKABSTRACT:
		if (args->orig_dest->flavor == CIL_BLOCKINHERIT) {
			/* When inheriting a block, don't copy any blockabstract
			 * statements. Inheriting a block from a block that was
			 * just inherited never worked. */
			return SEPOL_OK;
		}
		copy_func = &cil_copy_blockabstract;
		break;
	case CIL_BLOCKINHERIT:
		copy_func = &cil_copy_blockinherit;
		break;
	case CIL_POLICYCAP:
		copy_func = &cil_copy_policycap;
		break;
	case CIL_PERM:
	case CIL_MAP_PERM:
		copy_func = &cil_copy_perm;
		break;
	case CIL_CLASSMAPPING:
		copy_func = &cil_copy_classmapping;
		break;
	case CIL_CLASS:
	case CIL_COMMON:
	case CIL_MAP_CLASS:
		copy_func = &cil_copy_class;
		break;
	case CIL_CLASSORDER:
		copy_func = &cil_copy_classorder;
		break;
	case CIL_CLASSPERMISSION:
		copy_func = &cil_copy_classpermission;
		break;
	case CIL_CLASSPERMISSIONSET:
		copy_func = &cil_copy_classpermissionset;
		break;
	case CIL_CLASSCOMMON:
		copy_func = &cil_copy_classcommon;
		break;
	case CIL_SID:
		copy_func = &cil_copy_sid;
		break;
	case CIL_SIDCONTEXT:
		copy_func = &cil_copy_sidcontext;
		break;
	case CIL_SIDORDER:
		copy_func = &cil_copy_sidorder;
		break;
	case CIL_USER:
		copy_func = &cil_copy_user;
		break;
	case CIL_USERATTRIBUTE:
		copy_func = &cil_copy_userattribute;
		break;
	case CIL_USERATTRIBUTESET:
		copy_func = &cil_copy_userattributeset;
		break;
	case CIL_USERROLE:
		copy_func = &cil_copy_userrole;
		break;
	case CIL_USERLEVEL:
		copy_func = &cil_copy_userlevel;
		break;
	case CIL_USERRANGE:
		copy_func = &cil_copy_userrange;
		break;
	case CIL_USERBOUNDS:
		copy_func = &cil_copy_bounds;
		break;
	case CIL_USERPREFIX:
		copy_func = &cil_copy_userprefix;
		break;
	case CIL_ROLE:
		copy_func = &cil_copy_role;
		break;
	case CIL_ROLETYPE:
		copy_func = &cil_copy_roletype;
		break;
	case CIL_ROLEBOUNDS:
		copy_func = &cil_copy_bounds;
		break;
	case CIL_ROLEATTRIBUTE:
		copy_func = &cil_copy_roleattribute;
		break;
	case CIL_ROLEATTRIBUTESET:
		copy_func = &cil_copy_roleattributeset;
		break;
	case CIL_ROLEALLOW:
		copy_func = &cil_copy_roleallow;
		break;
	case CIL_TYPE:
		copy_func = &cil_copy_type;
		break;
	case CIL_TYPEBOUNDS:
		copy_func = &cil_copy_bounds;
		break;
	case CIL_TYPEPERMISSIVE:
		copy_func = cil_copy_typepermissive;
		break;
	case CIL_TYPEATTRIBUTE:
		copy_func = &cil_copy_typeattribute;
		break;
	case CIL_TYPEATTRIBUTESET:
		copy_func = &cil_copy_typeattributeset;
		break;
	case CIL_EXPANDTYPEATTRIBUTE:
		copy_func = &cil_copy_expandtypeattribute;
		break;
	case CIL_TYPEALIAS:
		copy_func = &cil_copy_alias;
		break;
	case CIL_TYPEALIASACTUAL:
		copy_func = &cil_copy_aliasactual;
		break;
	case CIL_ROLETRANSITION:
		copy_func = &cil_copy_roletransition;
		break;
	case CIL_NAMETYPETRANSITION:
		copy_func = &cil_copy_nametypetransition;
		break;
	case CIL_RANGETRANSITION:
		copy_func = &cil_copy_rangetransition;
		break;
	case CIL_TUNABLE:
		copy_func = &cil_copy_tunable;
		break;
	case CIL_BOOL:
		copy_func = &cil_copy_bool;
		break;
	case CIL_AVRULE:
	case CIL_AVRULEX:
		copy_func = &cil_copy_avrule;
		break;
	case CIL_PERMISSIONX:
		copy_func = &cil_copy_permissionx;
		break;
	case CIL_TYPE_RULE:
		copy_func = &cil_copy_type_rule;
		break;
	case CIL_SENS:
		copy_func = &cil_copy_sens;
		break;
	case CIL_SENSALIAS:
		copy_func = &cil_copy_alias;
		break;
	case CIL_SENSALIASACTUAL:
		copy_func = &cil_copy_aliasactual;
		break;
	case CIL_CAT:
		copy_func = &cil_copy_cat;
		break;
	case CIL_CATALIAS:
		copy_func = &cil_copy_alias;
		break;
	case CIL_CATALIASACTUAL:
		copy_func = &cil_copy_aliasactual;
		break;
	case CIL_CATSET:
		copy_func = &cil_copy_catset;
		break;
	case CIL_SENSCAT:
		copy_func = &cil_copy_senscat;
		break;
	case CIL_CATORDER:
		copy_func = &cil_copy_catorder;
		break;
	case CIL_SENSITIVITYORDER:
		copy_func = &cil_copy_sensitivityorder;
		break;
	case CIL_LEVEL:
		copy_func = &cil_copy_level;
		break;
	case CIL_LEVELRANGE:
		copy_func = &cil_copy_levelrange;
		break;
	case CIL_CONTEXT:
		copy_func = &cil_copy_context;
		break;
	case CIL_NETIFCON:
		copy_func = &cil_copy_netifcon;
		break;
	case CIL_GENFSCON:
		copy_func = &cil_copy_genfscon;
		break;
	case CIL_FILECON:
		copy_func = &cil_copy_filecon;
		break;
	case CIL_NODECON:
		copy_func = &cil_copy_nodecon;
		break;
	case CIL_IBPKEYCON:
		copy_func = &cil_copy_ibpkeycon;
		break;
	case CIL_IBENDPORTCON:
		copy_func = &cil_copy_ibendportcon;
		break;
	case CIL_PORTCON:
		copy_func = &cil_copy_portcon;
		break;
	case CIL_PIRQCON:
		copy_func = &cil_copy_pirqcon;
		break;
	case CIL_IOMEMCON:
		copy_func = &cil_copy_iomemcon;
		break;
	case CIL_IOPORTCON:
		copy_func = &cil_copy_ioportcon;
		break;
	case CIL_PCIDEVICECON:
		copy_func = &cil_copy_pcidevicecon;
		break;
	case CIL_DEVICETREECON:
		copy_func = &cil_copy_devicetreecon;
		break;
	case CIL_FSUSE:
		copy_func = &cil_copy_fsuse;
		break;
	case CIL_CONSTRAIN:
	case CIL_MLSCONSTRAIN:
		copy_func = &cil_copy_constrain;
		break;
	case CIL_VALIDATETRANS:
	case CIL_MLSVALIDATETRANS:
		copy_func = &cil_copy_validatetrans;
		break;
	case CIL_CALL:
		copy_func = &cil_copy_call;
		break;
	case CIL_MACRO:
		copy_func = &cil_copy_macro;
		break;
	case CIL_NODE:
		copy_func = &cil_copy_node;
		break;
	case CIL_OPTIONAL:
		copy_func = &cil_copy_optional;
		break;
	case CIL_IPADDR:
		copy_func = &cil_copy_ipaddr;
		break;
	case CIL_CONDBLOCK:
		copy_func = &cil_copy_condblock;
		break;
	case CIL_BOOLEANIF:
		copy_func = &cil_copy_boolif;
		break;
	case CIL_TUNABLEIF:
		copy_func = &cil_copy_tunif;
		break;
	case CIL_DEFAULTUSER:
	case CIL_DEFAULTROLE:
	case CIL_DEFAULTTYPE:
		copy_func = &cil_copy_default;
		break;
	case CIL_DEFAULTRANGE:
		copy_func = &cil_copy_defaultrange;
		break;
	case CIL_HANDLEUNKNOWN:
		copy_func = &cil_copy_handleunknown;
		break;
	case CIL_MLS:
		copy_func = &cil_copy_mls;
		break;
	case CIL_SRC_INFO:
		copy_func = &cil_copy_src_info;
		break;
	default:
		goto exit;
	}

	if (orig->flavor >= CIL_MIN_DECLARATIVE) {
		rc = cil_flavor_to_symtab_index(orig->flavor, &sym_index);
		if (rc != SEPOL_OK) {
			goto exit;
		}

		rc = cil_get_symtab(parent, &symtab, sym_index);
		if (rc != SEPOL_OK) {
			goto exit;
		}
	}

	rc = (*copy_func)(db, orig->data, &data, symtab);
	if (rc == SEPOL_OK) {
		if (orig->flavor == CIL_MACRO && data == NULL) {
			/* Skipping macro re-declaration */
			if (args->orig_dest->flavor != CIL_BLOCKINHERIT) {
				cil_log(CIL_ERR, "  Re-declaration of macro is only allowed when inheriting a block\n");
				return SEPOL_ERR;
			}
			*finished = CIL_TREE_SKIP_HEAD;
			return SEPOL_OK;
		}

		cil_tree_node_init(&new);

		new->parent = parent;
		new->line = orig->line;
		new->hll_offset = orig->hll_offset;
		new->flavor = orig->flavor;
		new->data = data;

		if (orig->flavor == CIL_BLOCK && DATUM(data)->nodes->head != NULL) {
			/* Duplicate block */
			if (args->orig_dest->flavor != CIL_BLOCKINHERIT) {
				cil_log(CIL_ERR, "  Re-declaration of block is only allowed when inheriting a block\n");
				rc = SEPOL_ERR;
				goto exit;
			}
			cil_list_append(DATUM(new->data)->nodes, CIL_NODE, new);
		} else if (orig->flavor >= CIL_MIN_DECLARATIVE) {
			/* Check the flavor of data if was found in the destination symtab */
			if (DATUM(data)->nodes->head && FLAVOR(data) != orig->flavor) {
				cil_tree_log(orig, CIL_ERR, "Incompatible flavor when trying to copy %s", DATUM(data)->name);
				cil_tree_log(NODE(data), CIL_ERR, "Note: conflicting declaration");
				new->flavor = FLAVOR(data);
				rc = SEPOL_ERR;
				goto exit;
			}

			rc = cil_add_decl_to_symtab(db, symtab, DATUM(orig->data)->name, DATUM(data), new);
			if (rc != SEPOL_OK) {
				if (rc == SEPOL_EEXIST) {
					cil_symtab_datum_destroy(data);
					free(data);
					data = NULL;
					rc = SEPOL_OK;
				} else {
					goto exit;
				}
			}

			namespace = new;
			while (namespace->flavor != CIL_MACRO && namespace->flavor != CIL_BLOCK && namespace->flavor != CIL_ROOT) {
				namespace = namespace->parent;
			}

			if (namespace->flavor == CIL_MACRO) {
				rc = cil_verify_decl_does_not_shadow_macro_parameter(namespace->data, orig, DATUM(orig->data)->name);
				if (rc != SEPOL_OK) {
					goto exit;
				}
			}
		}

		if (new->flavor == CIL_BLOCKINHERIT) {
			blockinherit = new->data;
			// if a blockinherit statement is copied before blockinherit are
			// resolved (like in an in-statement), the block will not have been
			// resolved yet, so there's nothing to append yet. This is fine,
			// the copied blockinherit statement will be handled later, as if
			// it wasn't in an in-statement
			if (blockinherit->block != NULL) {
				cil_list_append(blockinherit->block->bi_nodes, CIL_NODE, new);
			}
		}

		if (parent->cl_head == NULL) {
			parent->cl_head = new;
			parent->cl_tail = new;
		} else {
			parent->cl_tail->next = new;
			parent->cl_tail = new;
		}

		if (orig->cl_head != NULL) {
			args->dest = new;
		}
	} else {
		cil_tree_log(orig, CIL_ERR, "Problem copying %s node", cil_node_to_string(orig));
		goto exit;
	}

	return SEPOL_OK;

exit:
	cil_tree_node_destroy(&new);
	return rc;
}

static int __cil_copy_last_child_helper(__attribute__((unused)) struct cil_tree_node *orig, void *extra_args)
{
	struct cil_tree_node *node = NULL;
	struct cil_args_copy *args = NULL;

	args = extra_args;
	node = args->dest;

	if (node->flavor != CIL_ROOT) {
		args->dest = node->parent;
	}

	return SEPOL_OK;
}

// dest is the parent node to copy into
// if the copy is for a call to a macro, dest should be a pointer to the call
int cil_copy_ast(struct cil_db *db, struct cil_tree_node *orig, struct cil_tree_node *dest)
{
	int rc = SEPOL_ERR;
	struct cil_args_copy extra_args;

	extra_args.orig_dest = dest;
	extra_args.dest = dest;
	extra_args.db = db;

	rc = cil_tree_walk(orig, __cil_copy_node_helper, NULL,  __cil_copy_last_child_helper, &extra_args);
	if (rc != SEPOL_OK) {
		cil_tree_log(dest, CIL_ERR, "Failed to copy %s to %s", cil_node_to_string(orig), cil_node_to_string(dest));
		goto exit;
	}

	return SEPOL_OK;

exit:
	return rc;
}


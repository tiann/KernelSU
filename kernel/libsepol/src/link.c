/* Authors: Karl MacMillan <kmacmillan@mentalrootkit.com>
 *	    Joshua Brindle <jbrindle@tresys.com>
 *          Jason Tang <jtang@tresys.com>
 *
 * Copyright (C) 2004-2005 Tresys Technology, LLC
 * Copyright (C) 2007 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <sepol/policydb/policydb.h>
#include <sepol/policydb/conditional.h>
#include <sepol/policydb/hashtab.h>
#include <sepol/policydb/avrule_block.h>
#include <sepol/policydb/link.h>
#include <sepol/policydb/util.h>

// #include <stdlib.h>
// #include <stdarg.h>
// #include <stdio.h>
#include <linux/string.h>
// #include <assert.h>

#include "debug.h"
#include "private.h"

#undef min
#define min(a,b) (((a) < (b)) ? (a) : (b))

typedef struct policy_module {
	policydb_t *policy;
	uint32_t num_decls;
	uint32_t *map[SYM_NUM];
	uint32_t *avdecl_map;
	uint32_t **perm_map;
	uint32_t *perm_map_len;

	/* a pointer to within the base module's avrule_block chain to
	 * where this module's global now resides */
	avrule_block_t *base_global;
} policy_module_t;

typedef struct link_state {
	int verbose;
	policydb_t *base;
	avrule_block_t *last_avrule_block, *last_base_avrule_block;
	uint32_t next_decl_id, current_decl_id;

	/* temporary variables, used during ksu_hashtab_map() calls */
	policy_module_t *cur;
	char *cur_mod_name;
	avrule_decl_t *dest_decl;
	class_datum_t *src_class, *dest_class;
	char *dest_class_name;
	char dest_class_req;	/* flag indicating the class was not declared */
	uint32_t symbol_num;
	/* used to report the name of the module if dependency error occurs */
	policydb_t **decl_to_mod;

	/* error reporting fields */
	sepol_handle_t *handle;
} link_state_t;

typedef struct missing_requirement {
	uint32_t symbol_type;
	uint32_t symbol_value;
	uint32_t perm_value;
} missing_requirement_t;

static const char * const symtab_names[SYM_NUM] = {
	"common", "class", "role", "type/attribute", "user",
	"bool", "level", "category"
};

/* Deallocates all elements within a module, but NOT the policydb_t
 * structure within, as well as the pointer itself. */
static void policy_module_destroy(policy_module_t * mod)
{
	unsigned int i;
	if (mod == NULL) {
		return;
	}
	for (i = 0; i < SYM_NUM; i++) {
		free(mod->map[i]);
	}
	for (i = 0; mod->perm_map != NULL && i < mod->policy->p_classes.nprim;
	     i++) {
		free(mod->perm_map[i]);
	}
	free(mod->perm_map);
	free(mod->perm_map_len);
	free(mod->avdecl_map);
	free(mod);
}

/***** functions that copy identifiers from a module to base *****/

/* Note: there is currently no scoping for permissions, which causes some
 * strange side-effects. The current approach is this:
 *
 * a) perm is required and the class _and_ perm are declared in base: only add a mapping.
 * b) perm is required and the class and perm are _not_ declared in base: simply add the permissions
 *    to the object class. This means that the requirements for the decl are the union of the permissions
 *    required for all decls, but who cares.
 * c) perm is required, the class is declared in base, but the perm is not present. Nothing we can do
 *    here because we can't mark a single permission as required, so we bail with a requirement error
 *    _even_ if we are in an optional.
 *
 * A is correct behavior, b is wrong but not too bad, c is totall wrong for optionals. Fixing this requires
 * a format change.
 */
static int permission_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
				    void *data)
{
	char *perm_id = key, *new_id = NULL;
	perm_datum_t *perm, *new_perm = NULL, *dest_perm;
	link_state_t *state = (link_state_t *) data;

	class_datum_t *src_class = state->src_class;
	class_datum_t *dest_class = state->dest_class;
	policy_module_t *mod = state->cur;
	uint32_t sclassi = src_class->s.value - 1;
	int ret;

	perm = (perm_datum_t *) datum;
	dest_perm = hashtab_search(dest_class->permissions.table, perm_id);
	if (dest_perm == NULL && dest_class->comdatum != NULL) {
		dest_perm =
		    hashtab_search(dest_class->comdatum->permissions.table,
				   perm_id);
	}

	if (dest_perm == NULL) {
		/* If the object class was not declared in the base, add the perm
		 * to the object class. */
		if (state->dest_class_req) {
			/* If the class was required (not declared), insert the new permission */
			new_id = strdup(perm_id);
			if (new_id == NULL) {
				ERR(state->handle, "Memory error");
				ret = SEPOL_ERR;
				goto err;
			}
			new_perm =
			    (perm_datum_t *) calloc(1, sizeof(perm_datum_t));
			if (new_perm == NULL) {
				ERR(state->handle, "Memory error");
				ret = SEPOL_ERR;
				goto err;
			}
			ret = hashtab_insert(dest_class->permissions.table,
					     (hashtab_key_t) new_id,
					     (hashtab_datum_t) new_perm);
			if (ret) {
				ERR(state->handle,
				    "could not insert permission into class");
				goto err;
			}
			new_perm->s.value = dest_class->permissions.nprim + 1;
			dest_perm = new_perm;
		} else {
			/* this is case c from above */
			ERR(state->handle,
			    "Module %s depends on permission %s in class %s, not satisfied",
			    state->cur_mod_name, perm_id,
			    state->dest_class_name);
			return SEPOL_EREQ;
		}
	}

	/* build the mapping for permissions encompassing this class.
	 * unlike symbols, the permission map translates between
	 * module permission bit to target permission bit.  that bit
	 * may have originated from the class -or- it could be from
	 * the class's common parent.*/
	if (perm->s.value > mod->perm_map_len[sclassi]) {
		uint32_t *newmap = calloc(perm->s.value, sizeof(*newmap));
		if (newmap == NULL) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
		if (mod->perm_map_len[sclassi] > 0) {
			memcpy(newmap, mod->perm_map[sclassi], mod->perm_map_len[sclassi] * sizeof(*newmap));
		}
		free(mod->perm_map[sclassi]);
		mod->perm_map[sclassi] = newmap;
		mod->perm_map_len[sclassi] = perm->s.value;
	}
	mod->perm_map[sclassi][perm->s.value - 1] = dest_perm->s.value;

	return 0;
      err:
	free(new_id);
	free(new_perm);
	return ret;
}

static int class_copy_default_new_object(link_state_t *state,
					 class_datum_t *olddatum,
					 class_datum_t *newdatum)
{
	if (olddatum->default_user) {
		if (newdatum->default_user && olddatum->default_user != newdatum->default_user) {
			ERR(state->handle, "Found conflicting default user definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_user = olddatum->default_user;
	}
	if (olddatum->default_role) {
		if (newdatum->default_role && olddatum->default_role != newdatum->default_role) {
			ERR(state->handle, "Found conflicting default role definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_role = olddatum->default_role;
	}
	if (olddatum->default_type) {
		if (newdatum->default_type && olddatum->default_type != newdatum->default_type) {
			ERR(state->handle, "Found conflicting default type definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_type = olddatum->default_type;
	}
	if (olddatum->default_range) {
		if (newdatum->default_range && olddatum->default_range != newdatum->default_range) {
			ERR(state->handle, "Found conflicting default range definitions");
			return SEPOL_ENOTSUP;
		}
		newdatum->default_range = olddatum->default_range;
	}
	return 0;
}

static int class_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			       void *data)
{
	char *id = key, *new_id = NULL;
	class_datum_t *cladatum, *new_class = NULL;
	link_state_t *state = (link_state_t *) data;
	scope_datum_t *scope = NULL;
	int ret;

	cladatum = (class_datum_t *) datum;
	state->dest_class_req = 0;

	new_class = hashtab_search(state->base->p_classes.table, id);
	/* If there is not an object class already in the base symtab that means
	 * that either a) a module is trying to declare a new object class (which
	 * the compiler should prevent) or b) an object class was required that is
	 * not in the base.
	 */
	if (new_class == NULL) {
		scope =
		    hashtab_search(state->cur->policy->p_classes_scope.table,
				   id);
		if (scope == NULL) {
			ret = SEPOL_ERR;
			goto err;
		}
		if (scope->scope == SCOPE_DECL) {
			/* disallow declarations in modules */
			ERR(state->handle,
			    "%s: Modules may not yet declare new classes.",
			    state->cur_mod_name);
			ret = SEPOL_ENOTSUP;
			goto err;
		} else {
			/* It would be nice to error early here because the requirement is
			 * not met, but we cannot because the decl might be optional (in which
			 * case we should record the requirement so that it is just turned
			 * off). Note: this will break horribly if modules can declare object
			 * classes because the class numbers will be all wrong (i.e., they
			 * might be assigned in the order they were required rather than the
			 * current scheme which ensures correct numbering by ordering the 
			 * declarations properly). This can't be fixed until some infrastructure
			 * for querying the object class numbers is in place. */
			state->dest_class_req = 1;
			new_class =
			    (class_datum_t *) calloc(1, sizeof(class_datum_t));
			if (new_class == NULL) {
				ERR(state->handle, "Memory error");
				ret = SEPOL_ERR;
				goto err;
			}
			if (ksu_symtab_init
			    (&new_class->permissions, PERM_SYMTAB_SIZE)) {
				ret = SEPOL_ERR;
				goto err;
			}
			new_id = strdup(id);
			if (new_id == NULL) {
				ERR(state->handle, "Memory error");
				symtab_destroy(&new_class->permissions);
				ret = SEPOL_ERR;
				goto err;
			}
			ret = hashtab_insert(state->base->p_classes.table,
					     (hashtab_key_t) new_id,
					     (hashtab_datum_t) new_class);
			if (ret) {
				ERR(state->handle,
				    "could not insert new class into symtab");
				symtab_destroy(&new_class->permissions);
				goto err;
			}
			new_class->s.value = ++(state->base->p_classes.nprim);
		}
	}

	state->cur->map[SYM_CLASSES][cladatum->s.value - 1] =
	    new_class->s.value;

	/* copy permissions */
	state->src_class = cladatum;
	state->dest_class = new_class;
	state->dest_class_name = (char *)key;

	/* copy default new object rules */
	ret = class_copy_default_new_object(state, cladatum, new_class);
	if (ret)
		return ret;

	ret =
	    ksu_hashtab_map(cladatum->permissions.table, permission_copy_callback,
			state);
	if (ret != 0) {
		return ret;
	}

	return 0;
      err:
	free(new_class);
	free(new_id);
	return ret;
}

static int role_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id = key, *new_id = NULL;
	role_datum_t *role, *base_role, *new_role = NULL;
	link_state_t *state = (link_state_t *) data;

	role = (role_datum_t *) datum;

	base_role = hashtab_search(state->base->p_roles.table, id);
	if (base_role != NULL) {
		/* role already exists.  check that it is what this
		 * module expected.  duplicate declarations (e.g., two
		 * modules both declare role foo_r) is checked during
		 * scope_copy_callback(). */
		if (role->flavor == ROLE_ATTRIB
		    && base_role->flavor != ROLE_ATTRIB) {
			ERR(state->handle,
			    "%s: Expected %s to be a role attribute, but it was already declared as a regular role.",
			    state->cur_mod_name, id);
			return -1;
		} else if (role->flavor != ROLE_ATTRIB
			   && base_role->flavor == ROLE_ATTRIB) {
			ERR(state->handle,
			    "%s: Expected %s to be a regular role, but it was already declared as a role attribute.",
			    state->cur_mod_name, id);
			return -1;
		}
	} else {
		if (state->verbose)
			INFO(state->handle, "copying role %s", id);

		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}

		if ((new_role =
		     (role_datum_t *) malloc(sizeof(*new_role))) == NULL) {
			goto cleanup;
		}
		role_datum_init(new_role);

		/* new_role's dominates, types and roles field will be copied
		 * during role_fix_callback() */
		new_role->flavor = role->flavor;
		new_role->s.value = state->base->p_roles.nprim + 1;

		ret = hashtab_insert(state->base->p_roles.table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_role);
		if (ret) {
			goto cleanup;
		}
		state->base->p_roles.nprim++;
		base_role = new_role;
	}

	if (state->dest_decl) {
		new_id = NULL;
		if ((new_role = malloc(sizeof(*new_role))) == NULL) {
			goto cleanup;
		}
		role_datum_init(new_role);
		new_role->flavor = base_role->flavor;
		new_role->s.value = base_role->s.value;
		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}
		if (hashtab_insert
		    (state->dest_decl->p_roles.table, new_id, new_role)) {
			goto cleanup;
		}
		state->dest_decl->p_roles.nprim++;
	}

	state->cur->map[SYM_ROLES][role->s.value - 1] = base_role->s.value;
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	role_datum_destroy(new_role);
	free(new_id);
	free(new_role);
	return -1;
}

/* Copy types and attributes from a module into the base module. The
 * attributes are copied, but the types that make up this attribute
 * are delayed type_fix_callback(). */
static int type_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id = key, *new_id = NULL;
	type_datum_t *type, *base_type, *new_type = NULL;
	link_state_t *state = (link_state_t *) data;

	type = (type_datum_t *) datum;
	if ((type->flavor == TYPE_TYPE && !type->primary)
	    || type->flavor == TYPE_ALIAS) {
		/* aliases are handled later, in alias_copy_callback() */
		return 0;
	}

	base_type = hashtab_search(state->base->p_types.table, id);
	if (base_type != NULL) {
		/* type already exists.  check that it is what this
		 * module expected.  duplicate declarations (e.g., two
		 * modules both declare type foo_t) is checked during
		 * scope_copy_callback(). */
		if (type->flavor == TYPE_ATTRIB
		    && base_type->flavor != TYPE_ATTRIB) {
			ERR(state->handle,
			    "%s: Expected %s to be an attribute, but it was already declared as a type.",
			    state->cur_mod_name, id);
			return -1;
		} else if (type->flavor != TYPE_ATTRIB
			   && base_type->flavor == TYPE_ATTRIB) {
			ERR(state->handle,
			    "%s: Expected %s to be a type, but it was already declared as an attribute.",
			    state->cur_mod_name, id);
			return -1;
		}

		base_type->flags |= type->flags;
	} else {
		if (state->verbose)
			INFO(state->handle, "copying type %s", id);

		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}

		if ((new_type =
		     (type_datum_t *) calloc(1, sizeof(*new_type))) == NULL) {
			goto cleanup;
		}
		new_type->primary = type->primary;
		new_type->flags = type->flags;
		new_type->flavor = type->flavor;
		/* for attributes, the writing of new_type->types is
		   done in type_fix_callback() */

		new_type->s.value = state->base->p_types.nprim + 1;

		ret = hashtab_insert(state->base->p_types.table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_type);
		if (ret) {
			goto cleanup;
		}
		state->base->p_types.nprim++;
		base_type = new_type;
	}

	if (state->dest_decl) {
		new_id = NULL;
		if ((new_type = calloc(1, sizeof(*new_type))) == NULL) {
			goto cleanup;
		}
		new_type->primary = type->primary;
		new_type->flavor = type->flavor;
		new_type->flags = type->flags;
		new_type->s.value = base_type->s.value;
		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}
		if (hashtab_insert
		    (state->dest_decl->p_types.table, new_id, new_type)) {
			goto cleanup;
		}
		state->dest_decl->p_types.nprim++;
	}

	state->cur->map[SYM_TYPES][type->s.value - 1] = base_type->s.value;
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	free(new_id);
	free(new_type);
	return -1;
}

static int user_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id = key, *new_id = NULL;
	user_datum_t *user, *base_user, *new_user = NULL;
	link_state_t *state = (link_state_t *) data;

	user = (user_datum_t *) datum;

	base_user = hashtab_search(state->base->p_users.table, id);
	if (base_user == NULL) {
		if (state->verbose)
			INFO(state->handle, "copying user %s", id);

		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}

		if ((new_user =
		     (user_datum_t *) malloc(sizeof(*new_user))) == NULL) {
			goto cleanup;
		}
		user_datum_init(new_user);
		/* new_users's roles and MLS fields will be copied during
		   user_fix_callback(). */

		new_user->s.value = state->base->p_users.nprim + 1;

		ret = hashtab_insert(state->base->p_users.table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_user);
		if (ret) {
			goto cleanup;
		}
		state->base->p_users.nprim++;
		base_user = new_user;
	}

	if (state->dest_decl) {
		new_id = NULL;
		if ((new_user = malloc(sizeof(*new_user))) == NULL) {
			goto cleanup;
		}
		user_datum_init(new_user);
		new_user->s.value = base_user->s.value;
		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}
		if (hashtab_insert
		    (state->dest_decl->p_users.table, new_id, new_user)) {
			goto cleanup;
		}
		state->dest_decl->p_users.nprim++;
	}

	state->cur->map[SYM_USERS][user->s.value - 1] = base_user->s.value;
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	user_datum_destroy(new_user);
	free(new_id);
	free(new_user);
	return -1;
}

static int bool_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	int ret;
	char *id = key, *new_id = NULL;
	cond_bool_datum_t *booldatum, *base_bool, *new_bool = NULL;
	link_state_t *state = (link_state_t *) data;
	scope_datum_t *scope;

	booldatum = (cond_bool_datum_t *) datum;

	base_bool = hashtab_search(state->base->p_bools.table, id);
	if (base_bool == NULL) {
		if (state->verbose)
			INFO(state->handle, "copying boolean %s", id);

		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}

		if ((new_bool =
		     (cond_bool_datum_t *) malloc(sizeof(*new_bool))) == NULL) {
			goto cleanup;
		}
		new_bool->s.value = state->base->p_bools.nprim + 1;

		ret = hashtab_insert(state->base->p_bools.table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_bool);
		if (ret) {
			goto cleanup;
		}
		state->base->p_bools.nprim++;
		base_bool = new_bool;
		base_bool->flags = booldatum->flags;
		base_bool->state = booldatum->state;
	} else if ((booldatum->flags & COND_BOOL_FLAGS_TUNABLE) !=
		   (base_bool->flags & COND_BOOL_FLAGS_TUNABLE)) {
			/* A mismatch between boolean/tunable declaration
			 * and usage(for example a boolean used in the
			 * tunable_policy() or vice versa).
			 *
			 * This is not allowed and bail out with errors */
			ERR(state->handle,
			    "%s: Mismatch between boolean/tunable definition "
			    "and usage for %s", state->cur_mod_name, id);
			return -1;
	}

	/* Get the scope info for this boolean to see if this is the declaration, 
 	 * if so set the state */
	scope = hashtab_search(state->cur->policy->p_bools_scope.table, id);
	if (!scope)
		return SEPOL_ERR;
	if (scope->scope == SCOPE_DECL) {
		base_bool->state = booldatum->state;
		/* Only the declaration rather than requirement
		 * decides if it is a boolean or tunable. */
		base_bool->flags = booldatum->flags;
	}
	state->cur->map[SYM_BOOLS][booldatum->s.value - 1] = base_bool->s.value;
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	ksu_cond_destroy_bool(new_id, new_bool, NULL);
	return -1;
}

static int sens_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			      void *data)
{
	char *id = key;
	level_datum_t *level, *base_level;
	link_state_t *state = (link_state_t *) data;
	scope_datum_t *scope;

	level = (level_datum_t *) datum;

	base_level = hashtab_search(state->base->p_levels.table, id);
	if (!base_level) {
		scope =
		    hashtab_search(state->cur->policy->p_sens_scope.table, id);
		if (!scope)
			return SEPOL_ERR;
		if (scope->scope == SCOPE_DECL) {
			/* disallow declarations in modules */
			ERR(state->handle,
			    "%s: Modules may not declare new sensitivities.",
			    state->cur_mod_name);
			return SEPOL_ENOTSUP;
		} else if (scope->scope == SCOPE_REQ) {
			/* unmet requirement */
			ERR(state->handle,
			    "%s: Sensitivity %s not declared by base.",
			    state->cur_mod_name, id);
			return SEPOL_ENOTSUP;
		} else {
			ERR(state->handle,
			    "%s: has an unknown scope: %d",
			    state->cur_mod_name, scope->scope);
			return SEPOL_ENOTSUP;
		}
	}

	state->cur->map[SYM_LEVELS][level->level->sens - 1] =
	    base_level->level->sens;

	return 0;
}

static int cat_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			     void *data)
{
	char *id = key;
	cat_datum_t *cat, *base_cat;
	link_state_t *state = (link_state_t *) data;
	scope_datum_t *scope;

	cat = (cat_datum_t *) datum;

	base_cat = hashtab_search(state->base->p_cats.table, id);
	if (!base_cat) {
		scope = hashtab_search(state->cur->policy->p_cat_scope.table, id);
		if (!scope)
			return SEPOL_ERR;
		if (scope->scope == SCOPE_DECL) {
			/* disallow declarations in modules */
			ERR(state->handle,
			    "%s: Modules may not declare new categories.",
			    state->cur_mod_name);
			return SEPOL_ENOTSUP;
		} else if (scope->scope == SCOPE_REQ) {
			/* unmet requirement */
			ERR(state->handle,
			    "%s: Category %s not declared by base.",
			    state->cur_mod_name, id);
			return SEPOL_ENOTSUP;
		} else {
			/* unknown scope?  malformed policy? */
			ERR(state->handle,
			    "%s: has an unknown scope: %d",
			    state->cur_mod_name, scope->scope);
			return SEPOL_ENOTSUP;
		}
	}

	state->cur->map[SYM_CATS][cat->s.value - 1] = base_cat->s.value;

	return 0;
}

static int (*copy_callback_f[SYM_NUM]) (hashtab_key_t key,
					hashtab_datum_t datum, void *datap) = {
NULL, class_copy_callback, role_copy_callback, type_copy_callback,
	    user_copy_callback, bool_copy_callback, sens_copy_callback,
	    cat_copy_callback};

/*
 * The boundaries have to be copied after the types/roles/users are copied,
 * because it refers hashtab to lookup destinated objects.
 */
static int type_bounds_copy_callback(hashtab_key_t key,
				     hashtab_datum_t datum, void *data)
{
	link_state_t *state = (link_state_t *) data;
	type_datum_t *type = (type_datum_t *) datum;
	type_datum_t *dest;
	uint32_t bounds_val;

	if (!type->bounds)
		return 0;

	bounds_val = state->cur->map[SYM_TYPES][type->bounds - 1];

	dest = hashtab_search(state->base->p_types.table, key);
	if (!dest) {
		ERR(state->handle,
		    "Type lookup failed for %s", (char *)key);
		return -1;
	}
	if (dest->bounds != 0 && dest->bounds != bounds_val) {
		ERR(state->handle,
		    "Inconsistent boundary for %s", (char *)key);
		return -1;
	}
	dest->bounds = bounds_val;

	return 0;
}

static int role_bounds_copy_callback(hashtab_key_t key,
				     hashtab_datum_t datum, void *data)
{
	link_state_t *state = (link_state_t *) data;
	role_datum_t *role = (role_datum_t *) datum;
	role_datum_t *dest;
	uint32_t bounds_val;

	if (!role->bounds)
		return 0;

	bounds_val = state->cur->map[SYM_ROLES][role->bounds - 1];

	dest = hashtab_search(state->base->p_roles.table, key);
	if (!dest) {
		ERR(state->handle,
		    "Role lookup failed for %s", (char *)key);
		return -1;
	}
	if (dest->bounds != 0 && dest->bounds != bounds_val) {
		ERR(state->handle,
		    "Inconsistent boundary for %s", (char *)key);
		return -1;
	}
	dest->bounds = bounds_val;

	return 0;
}

static int user_bounds_copy_callback(hashtab_key_t key,
				     hashtab_datum_t datum, void *data)
{
	link_state_t *state = (link_state_t *) data;
	user_datum_t *user = (user_datum_t *) datum;
	user_datum_t *dest;
	uint32_t bounds_val;

	if (!user->bounds)
		return 0;

	bounds_val = state->cur->map[SYM_USERS][user->bounds - 1];

	dest = hashtab_search(state->base->p_users.table, key);
	if (!dest) {
		ERR(state->handle,
		    "User lookup failed for %s", (char *)key);
		return -1;
	}
	if (dest->bounds != 0 && dest->bounds != bounds_val) {
		ERR(state->handle,
		    "Inconsistent boundary for %s", (char *)key);
		return -1;
	}
	dest->bounds = bounds_val;

	return 0;
}

/* The aliases have to be copied after the types and attributes to be
 * certain that the base symbol table will have the type that the
 * alias refers. Otherwise, we won't be able to find the type value
 * for the alias. We can't depend on the declaration ordering because
 * of the hash table.
 */
static int alias_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			       void *data)
{
	char *id = key, *new_id = NULL, *target_id;
	type_datum_t *type, *base_type, *new_type = NULL, *target_type;
	link_state_t *state = (link_state_t *) data;
	policy_module_t *mod = state->cur;
	int primval;

	type = (type_datum_t *) datum;
	/* there are 2 kinds of aliases. Ones with their own value (TYPE_ALIAS)
	 * and ones with the value of their primary (TYPE_TYPE && type->primary = 0)
	 */
	if (!
	    (type->flavor == TYPE_ALIAS
	     || (type->flavor == TYPE_TYPE && !type->primary))) {
		/* ignore types and attributes -- they were handled in
		 * type_copy_callback() */
		return 0;
	}

	if (type->flavor == TYPE_ALIAS)
		primval = type->primary;
	else
		primval = type->s.value;

	target_id = mod->policy->p_type_val_to_name[primval - 1];
	target_type = hashtab_search(state->base->p_types.table, target_id);
	if (target_type == NULL) {
		ERR(state->handle, "%s: Could not find type %s for alias %s.",
		    state->cur_mod_name, target_id, id);
		return -1;
	}

	if (!strcmp(id, target_id)) {
		ERR(state->handle, "%s: Self aliasing of %s.",
		    state->cur_mod_name, id);
		return -1;
	}

	target_type->flags |= type->flags;

	base_type = hashtab_search(state->base->p_types.table, id);
	if (base_type == NULL) {
		if (state->verbose)
			INFO(state->handle, "copying alias %s", id);

		if ((new_type =
		     (type_datum_t *) calloc(1, sizeof(*new_type))) == NULL) {
			goto cleanup;
		}
		/* the linked copy always has TYPE_ALIAS style aliases */
		new_type->primary = target_type->s.value;
		new_type->flags = target_type->flags;
		new_type->flavor = TYPE_ALIAS;
		new_type->s.value = state->base->p_types.nprim + 1;
		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}
		if (hashtab_insert
		    (state->base->p_types.table, new_id, new_type)) {
			goto cleanup;
		}
		state->base->p_types.nprim++;
		base_type = new_type;
	} else {

		/* if this already exists and isn't an alias it was required by another module (or base)
		 * and inserted into the hashtable as a type, fix it up now */

		if (base_type->flavor == TYPE_ALIAS) {
			/* error checking */
			assert(base_type->primary == target_type->s.value);
			assert(base_type->primary ==
			       mod->map[SYM_TYPES][primval - 1]);
			assert(mod->map[SYM_TYPES][type->s.value - 1] ==
			       base_type->primary);
			return 0;
		}

		if (base_type->flavor == TYPE_ATTRIB) {
			ERR(state->handle,
			    "%s is an alias of an attribute, not allowed", id);
			return -1;
		}

		base_type->flavor = TYPE_ALIAS;
		base_type->primary = target_type->s.value;
		base_type->flags |= target_type->flags;

	}
	/* the aliases map points from its value to its primary so when this module 
	 * references this type the value it gets back from the map is the primary */
	mod->map[SYM_TYPES][type->s.value - 1] = base_type->primary;

	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	free(new_id);
	free(new_type);
	return -1;
}

/*********** callbacks that fix bitmaps ***********/

static int type_set_convert(type_set_t * types, type_set_t * dst,
			    policy_module_t * mod, link_state_t * state
			    __attribute__ ((unused)))
{
	unsigned int i;
	ebitmap_node_t *tnode;
	ebitmap_for_each_positive_bit(&types->types, tnode, i) {
		assert(mod->map[SYM_TYPES][i]);
		if (ksu_ebitmap_set_bit
		    (&dst->types, mod->map[SYM_TYPES][i] - 1, 1)) {
			goto cleanup;
		}
	}
	ebitmap_for_each_positive_bit(&types->negset, tnode, i) {
		assert(mod->map[SYM_TYPES][i]);
		if (ksu_ebitmap_set_bit
		    (&dst->negset, mod->map[SYM_TYPES][i] - 1, 1)) {
			goto cleanup;
		}
	}
	dst->flags = types->flags;
	return 0;

      cleanup:
	return -1;
}

/* OR 2 typemaps together and at the same time map the src types to
 * the correct values in the dst typeset.
 */
static int type_set_or_convert(type_set_t * types, type_set_t * dst,
			       policy_module_t * mod, link_state_t * state)
{
	type_set_t ts_tmp;

	type_set_init(&ts_tmp);
	if (type_set_convert(types, &ts_tmp, mod, state) == -1) {
		goto cleanup;
	}
	if (type_set_or_eq(dst, &ts_tmp)) {
		goto cleanup;
	}
	type_set_destroy(&ts_tmp);
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	type_set_destroy(&ts_tmp);
	return -1;
}

static int role_set_or_convert(role_set_t * roles, role_set_t * dst,
			       policy_module_t * mod, link_state_t * state)
{
	unsigned int i;
	ebitmap_t tmp;
	ebitmap_node_t *rnode;

	ebitmap_init(&tmp);
	ebitmap_for_each_positive_bit(&roles->roles, rnode, i) {
		assert(mod->map[SYM_ROLES][i]);
		if (ksu_ebitmap_set_bit
		    (&tmp, mod->map[SYM_ROLES][i] - 1, 1)) {
			goto cleanup;
		}
	}
	if (ebitmap_union(&dst->roles, &tmp)) {
		goto cleanup;
	}
	dst->flags |= roles->flags;
	ksu_ebitmap_destroy(&tmp);
	return 0;
      cleanup:
	ERR(state->handle, "Out of memory!");
	ksu_ebitmap_destroy(&tmp);
	return -1;
}

static int mls_level_convert(mls_semantic_level_t * src, mls_semantic_level_t * dst,
			     policy_module_t * mod, link_state_t * state)
{
	mls_semantic_cat_t *src_cat, *new_cat;

	if (!mod->policy->mls)
		return 0;

	/* Required not declared. */
	if (!src->sens)
		return 0;

	assert(mod->map[SYM_LEVELS][src->sens - 1]);
	dst->sens = mod->map[SYM_LEVELS][src->sens - 1];

	for (src_cat = src->cat; src_cat; src_cat = src_cat->next) {
		new_cat =
		    (mls_semantic_cat_t *) malloc(sizeof(mls_semantic_cat_t));
		if (!new_cat) {
			ERR(state->handle, "Out of memory");
			return -1;
		}
		mls_semantic_cat_init(new_cat);

		new_cat->next = dst->cat;
		dst->cat = new_cat;

		assert(mod->map[SYM_CATS][src_cat->low - 1]);
		dst->cat->low = mod->map[SYM_CATS][src_cat->low - 1];
		assert(mod->map[SYM_CATS][src_cat->high - 1]);
		dst->cat->high = mod->map[SYM_CATS][src_cat->high - 1];
	}

	return 0;
}

static int mls_range_convert(mls_semantic_range_t * src, mls_semantic_range_t * dst,
			     policy_module_t * mod, link_state_t * state)
{
	int ret;
	ret = mls_level_convert(&src->level[0], &dst->level[0], mod, state);
	if (ret)
		return ret;
	ret = mls_level_convert(&src->level[1], &dst->level[1], mod, state);
	if (ret)
		return ret;
	return 0;
}

static int role_fix_callback(hashtab_key_t key, hashtab_datum_t datum,
			     void *data)
{
	unsigned int i;
	char *id = key;
	role_datum_t *role, *dest_role = NULL;
	link_state_t *state = (link_state_t *) data;
	ebitmap_t e_tmp;
	policy_module_t *mod = state->cur;
	ebitmap_node_t *rnode;
	hashtab_t role_tab;

	role = (role_datum_t *) datum;
	if (state->dest_decl == NULL)
		role_tab = state->base->p_roles.table;
	else
		role_tab = state->dest_decl->p_roles.table;

	dest_role = hashtab_search(role_tab, id);
	assert(dest_role != NULL);

	if (state->verbose) {
		INFO(state->handle, "fixing role %s", id);
	}

	ebitmap_init(&e_tmp);
	ebitmap_for_each_positive_bit(&role->dominates, rnode, i) {
		assert(mod->map[SYM_ROLES][i]);
		if (ksu_ebitmap_set_bit
		    (&e_tmp, mod->map[SYM_ROLES][i] - 1, 1)) {
			goto cleanup;
		}
	}
	if (ebitmap_union(&dest_role->dominates, &e_tmp)) {
		goto cleanup;
	}
	if (type_set_or_convert(&role->types, &dest_role->types, mod, state)) {
		goto cleanup;
	}
	ksu_ebitmap_destroy(&e_tmp);
	
	if (role->flavor == ROLE_ATTRIB) {
		ebitmap_init(&e_tmp);
		ebitmap_for_each_positive_bit(&role->roles, rnode, i) {
			assert(mod->map[SYM_ROLES][i]);
			if (ksu_ebitmap_set_bit
			    (&e_tmp, mod->map[SYM_ROLES][i] - 1, 1)) {
				goto cleanup;
			}
		}
		if (ebitmap_union(&dest_role->roles, &e_tmp)) {
			goto cleanup;
		}
		ksu_ebitmap_destroy(&e_tmp);
	}

	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	ksu_ebitmap_destroy(&e_tmp);
	return -1;
}

static int type_fix_callback(hashtab_key_t key, hashtab_datum_t datum,
			     void *data)
{
	unsigned int i;
	char *id = key;
	type_datum_t *type, *new_type = NULL;
	link_state_t *state = (link_state_t *) data;
	ebitmap_t e_tmp;
	policy_module_t *mod = state->cur;
	ebitmap_node_t *tnode;
	symtab_t *typetab;

	type = (type_datum_t *) datum;

	if (state->dest_decl == NULL)
		typetab = &state->base->p_types;
	else
		typetab = &state->dest_decl->p_types;

	/* only fix attributes */
	if (type->flavor != TYPE_ATTRIB) {
		return 0;
	}

	new_type = hashtab_search(typetab->table, id);
	assert(new_type != NULL && new_type->flavor == TYPE_ATTRIB);

	if (state->verbose) {
		INFO(state->handle, "fixing attribute %s", id);
	}

	ebitmap_init(&e_tmp);
	ebitmap_for_each_positive_bit(&type->types, tnode, i) {
		assert(mod->map[SYM_TYPES][i]);
		if (ksu_ebitmap_set_bit
		    (&e_tmp, mod->map[SYM_TYPES][i] - 1, 1)) {
			goto cleanup;
		}
	}
	if (ebitmap_union(&new_type->types, &e_tmp)) {
		goto cleanup;
	}
	ksu_ebitmap_destroy(&e_tmp);
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	ksu_ebitmap_destroy(&e_tmp);
	return -1;
}

static int user_fix_callback(hashtab_key_t key, hashtab_datum_t datum,
			     void *data)
{
	char *id = key;
	user_datum_t *user, *new_user = NULL;
	link_state_t *state = (link_state_t *) data;
	policy_module_t *mod = state->cur;
	symtab_t *usertab;

	user = (user_datum_t *) datum;

	if (state->dest_decl == NULL)
		usertab = &state->base->p_users;
	else
		usertab = &state->dest_decl->p_users;

	new_user = hashtab_search(usertab->table, id);
	assert(new_user != NULL);

	if (state->verbose) {
		INFO(state->handle, "fixing user %s", id);
	}

	if (role_set_or_convert(&user->roles, &new_user->roles, mod, state)) {
		goto cleanup;
	}

	if (mls_range_convert(&user->range, &new_user->range, mod, state))
		goto cleanup;

	if (mls_level_convert(&user->dfltlevel, &new_user->dfltlevel, mod, state))
		goto cleanup;

	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	return -1;
}

static int (*fix_callback_f[SYM_NUM]) (hashtab_key_t key, hashtab_datum_t datum,
				       void *datap) = {
NULL, NULL, role_fix_callback, type_fix_callback, user_fix_callback,
	    NULL, NULL, NULL};

/*********** functions that copy AV rules ***********/

static int copy_avrule_list(avrule_t * list, avrule_t ** dst,
			    policy_module_t * module, link_state_t * state)
{
	unsigned int i;
	avrule_t *cur, *new_rule = NULL, *tail;
	class_perm_node_t *cur_perm, *new_perm, *tail_perm = NULL;

	tail = *dst;
	while (tail && tail->next) {
		tail = tail->next;
	}

	cur = list;
	while (cur) {
		if ((new_rule = (avrule_t *) malloc(sizeof(avrule_t))) == NULL) {
			goto cleanup;
		}
		avrule_init(new_rule);

		new_rule->specified = cur->specified;
		new_rule->flags = cur->flags;
		if (type_set_convert
		    (&cur->stypes, &new_rule->stypes, module, state) == -1
		    || type_set_convert(&cur->ttypes, &new_rule->ttypes, module,
					state) == -1) {
			goto cleanup;
		}

		cur_perm = cur->perms;
		tail_perm = NULL;
		while (cur_perm) {
			if ((new_perm = (class_perm_node_t *)
			     malloc(sizeof(class_perm_node_t))) == NULL) {
				goto cleanup;
			}
			class_perm_node_init(new_perm);

			new_perm->tclass =
			    module->map[SYM_CLASSES][cur_perm->tclass - 1];
			assert(new_perm->tclass);

			if (new_rule->specified & AVRULE_AV) {
				for (i = 0;
				     i <
				     module->perm_map_len[cur_perm->tclass - 1];
				     i++) {
					if (!(cur_perm->data & (UINT32_C(1) << i)))
						continue;
					new_perm->data |=
					    (UINT32_C(1) <<
					     (module->
					      perm_map[cur_perm->tclass - 1][i] -
					      1));
				}
			} else {
				new_perm->data =
				    module->map[SYM_TYPES][cur_perm->data - 1];
			}

			if (new_rule->perms == NULL) {
				new_rule->perms = new_perm;
			} else {
				assert(tail_perm);
				tail_perm->next = new_perm;
			}
			tail_perm = new_perm;
			cur_perm = cur_perm->next;
		}

		if (cur->xperms) {
			new_rule->xperms = calloc(1, sizeof(*new_rule->xperms));
			if (!new_rule->xperms)
				goto cleanup;
			memcpy(new_rule->xperms, cur->xperms,
			       sizeof(*new_rule->xperms));
		}

		new_rule->line = cur->line;
		new_rule->source_line = cur->source_line;
		if (cur->source_filename) {
			new_rule->source_filename = strdup(cur->source_filename);
			if (!new_rule->source_filename)
				goto cleanup;
		}

		cur = cur->next;

		if (*dst == NULL) {
			*dst = new_rule;
		} else {
			tail->next = new_rule;
		}
		tail = new_rule;
	}

	return 0;
      cleanup:
	ERR(state->handle, "Out of memory!");
	avrule_destroy(new_rule);
	free(new_rule);
	return -1;
}

static int copy_role_trans_list(role_trans_rule_t * list,
				role_trans_rule_t ** dst,
				policy_module_t * module, link_state_t * state)
{
	role_trans_rule_t *cur, *new_rule = NULL, *tail;
	unsigned int i;
	ebitmap_node_t *cnode;

	cur = list;
	tail = *dst;
	while (tail && tail->next) {
		tail = tail->next;
	}
	while (cur) {
		if ((new_rule =
		     (role_trans_rule_t *) malloc(sizeof(role_trans_rule_t))) ==
		    NULL) {
			goto cleanup;
		}
		role_trans_rule_init(new_rule);

		if (role_set_or_convert
		    (&cur->roles, &new_rule->roles, module, state)
		    || type_set_or_convert(&cur->types, &new_rule->types,
					   module, state)) {
			goto cleanup;
		}

		ebitmap_for_each_positive_bit(&cur->classes, cnode, i) {
			assert(module->map[SYM_CLASSES][i]);
			if (ksu_ebitmap_set_bit(&new_rule->classes,
					    module->
					    map[SYM_CLASSES][i] - 1,
					    1)) {
				goto cleanup;
			}
		}

		new_rule->new_role = module->map[SYM_ROLES][cur->new_role - 1];

		if (*dst == NULL) {
			*dst = new_rule;
		} else {
			tail->next = new_rule;
		}
		tail = new_rule;
		cur = cur->next;
	}
	return 0;
      cleanup:
	ERR(state->handle, "Out of memory!");
	role_trans_rule_list_destroy(new_rule);
	return -1;
}

static int copy_role_allow_list(role_allow_rule_t * list,
				role_allow_rule_t ** dst,
				policy_module_t * module, link_state_t * state)
{
	role_allow_rule_t *cur, *new_rule = NULL, *tail;

	cur = list;
	tail = *dst;
	while (tail && tail->next) {
		tail = tail->next;
	}

	while (cur) {
		if ((new_rule =
		     (role_allow_rule_t *) malloc(sizeof(role_allow_rule_t))) ==
		    NULL) {
			goto cleanup;
		}
		role_allow_rule_init(new_rule);

		if (role_set_or_convert
		    (&cur->roles, &new_rule->roles, module, state)
		    || role_set_or_convert(&cur->new_roles,
					   &new_rule->new_roles, module,
					   state)) {
			goto cleanup;
		}
		if (*dst == NULL) {
			*dst = new_rule;
		} else {
			tail->next = new_rule;
		}
		tail = new_rule;
		cur = cur->next;
	}
	return 0;
      cleanup:
	ERR(state->handle, "Out of memory!");
	role_allow_rule_list_destroy(new_rule);
	return -1;
}

static int copy_filename_trans_list(filename_trans_rule_t * list,
				    filename_trans_rule_t ** dst,
				    policy_module_t * module,
				    link_state_t * state)
{
	filename_trans_rule_t *cur, *new_rule, *tail;

	cur = list;
	tail = *dst;
	while (tail && tail->next)
		tail = tail->next;

	while (cur) {
		new_rule = malloc(sizeof(*new_rule));
		if (!new_rule)
			goto err;

		filename_trans_rule_init(new_rule);

		if (*dst == NULL)
			*dst = new_rule;
		else
			tail->next = new_rule;
		tail = new_rule;

		new_rule->name = strdup(cur->name);
		if (!new_rule->name)
			goto err;

		if (type_set_or_convert(&cur->stypes, &new_rule->stypes, module, state) ||
		    type_set_or_convert(&cur->ttypes, &new_rule->ttypes, module, state))
			goto err;

		new_rule->tclass = module->map[SYM_CLASSES][cur->tclass - 1];
		new_rule->otype = module->map[SYM_TYPES][cur->otype - 1];
		new_rule->flags = cur->flags;

		cur = cur->next;
	}
	return 0;
err:
	ERR(state->handle, "Out of memory!");
	return -1;
}

static int copy_range_trans_list(range_trans_rule_t * rules,
				 range_trans_rule_t ** dst,
				 policy_module_t * mod, link_state_t * state)
{
	range_trans_rule_t *rule, *new_rule = NULL;
	unsigned int i;
	ebitmap_node_t *cnode;

	for (rule = rules; rule; rule = rule->next) {
		new_rule =
		    (range_trans_rule_t *) malloc(sizeof(range_trans_rule_t));
		if (!new_rule)
			goto cleanup;

		range_trans_rule_init(new_rule);

		new_rule->next = *dst;
		*dst = new_rule;

		if (type_set_convert(&rule->stypes, &new_rule->stypes,
				     mod, state))
			goto cleanup;

		if (type_set_convert(&rule->ttypes, &new_rule->ttypes,
				     mod, state))
			goto cleanup;

		ebitmap_for_each_positive_bit(&rule->tclasses, cnode, i) {
			assert(mod->map[SYM_CLASSES][i]);
			if (ksu_ebitmap_set_bit
			    (&new_rule->tclasses,
			     mod->map[SYM_CLASSES][i] - 1, 1)) {
				goto cleanup;
			}
		}

		if (mls_range_convert(&rule->trange, &new_rule->trange, mod, state))
			goto cleanup;
	}
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	range_trans_rule_list_destroy(new_rule);
	return -1;
}

static int copy_cond_list(cond_node_t * list, cond_node_t ** dst,
			  policy_module_t * module, link_state_t * state)
{
	unsigned i;
	cond_node_t *cur, *new_node = NULL, *tail;
	cond_expr_t *cur_expr;
	tail = *dst;
	while (tail && tail->next)
		tail = tail->next;

	cur = list;
	while (cur) {
		new_node = (cond_node_t *) malloc(sizeof(cond_node_t));
		if (!new_node) {
			goto cleanup;
		}
		memset(new_node, 0, sizeof(cond_node_t));

		new_node->cur_state = cur->cur_state;
		new_node->expr = cond_copy_expr(cur->expr);
		if (!new_node->expr)
			goto cleanup;
		/* go back through and remap the expression */
		for (cur_expr = new_node->expr; cur_expr != NULL;
		     cur_expr = cur_expr->next) {
			/* expression nodes don't have a bool value of 0 - don't map them */
			if (cur_expr->expr_type != COND_BOOL)
				continue;
			assert(module->map[SYM_BOOLS][cur_expr->bool - 1] != 0);
			cur_expr->bool =
			    module->map[SYM_BOOLS][cur_expr->bool - 1];
		}
		new_node->nbools = cur->nbools;
		/* FIXME should COND_MAX_BOOLS be used here? */
		for (i = 0; i < min(cur->nbools, COND_MAX_BOOLS); i++) {
			uint32_t remapped_id =
			    module->map[SYM_BOOLS][cur->bool_ids[i] - 1];
			assert(remapped_id != 0);
			new_node->bool_ids[i] = remapped_id;
		}
		new_node->expr_pre_comp = cur->expr_pre_comp;

		if (copy_avrule_list
		    (cur->avtrue_list, &new_node->avtrue_list, module, state)
		    || copy_avrule_list(cur->avfalse_list,
					&new_node->avfalse_list, module,
					state)) {
			goto cleanup;
		}

		if (*dst == NULL) {
			*dst = new_node;
		} else {
			tail->next = new_node;
		}
		tail = new_node;
		cur = cur->next;
	}
	return 0;
      cleanup:
	ERR(state->handle, "Out of memory!");
	cond_node_destroy(new_node);
	free(new_node);
	return -1;

}

/*********** functions that copy avrule_decls from module to base ***********/

static int copy_identifiers(link_state_t * state, symtab_t * src_symtab,
			    avrule_decl_t * dest_decl)
{
	int i, ret;

	state->dest_decl = dest_decl;
	for (i = 0; i < SYM_NUM; i++) {
		if (copy_callback_f[i] != NULL) {
			ret =
			    ksu_hashtab_map(src_symtab[i].table, copy_callback_f[i],
					state);
			if (ret) {
				return ret;
			}
		}
	}

	if (ksu_hashtab_map(src_symtab[SYM_TYPES].table,
			type_bounds_copy_callback, state))
		return -1;

	if (ksu_hashtab_map(src_symtab[SYM_TYPES].table,
			alias_copy_callback, state))
		return -1;

	if (ksu_hashtab_map(src_symtab[SYM_ROLES].table,
			role_bounds_copy_callback, state))
		return -1;

	if (ksu_hashtab_map(src_symtab[SYM_USERS].table,
			user_bounds_copy_callback, state))
		return -1;

	/* then fix bitmaps associated with those newly copied identifiers */
	for (i = 0; i < SYM_NUM; i++) {
		if (fix_callback_f[i] != NULL &&
		    ksu_hashtab_map(src_symtab[i].table, fix_callback_f[i],
				state)) {
			return -1;
		}
	}
	return 0;
}

static int copy_scope_index(scope_index_t * src, scope_index_t * dest,
			    policy_module_t * module, link_state_t * state)
{
	unsigned int i, j;
	uint32_t largest_mapped_class_value = 0;
	ebitmap_node_t *node;
	/* copy the scoping information for this avrule decl block */
	for (i = 0; i < SYM_NUM; i++) {
		ebitmap_t *srcmap = src->scope + i;
		ebitmap_t *destmap = dest->scope + i;
		if (copy_callback_f[i] == NULL) {
			continue;
		}
		ebitmap_for_each_positive_bit(srcmap, node, j) {
			assert(module->map[i][j] != 0);
			if (ksu_ebitmap_set_bit
			    (destmap, module->map[i][j] - 1, 1) != 0) {

				goto cleanup;
			}
			if (i == SYM_CLASSES &&
			    largest_mapped_class_value <
			    module->map[SYM_CLASSES][j]) {
				largest_mapped_class_value =
				    module->map[SYM_CLASSES][j];
			}
		}
	}

	/* next copy the enabled permissions data  */
	if ((dest->class_perms_map = calloc(largest_mapped_class_value,
					    sizeof(*dest->class_perms_map))) == NULL) {
		goto cleanup;
	}
	dest->class_perms_len = largest_mapped_class_value;
	for (i = 0; i < src->class_perms_len; i++) {
		ebitmap_t *srcmap = src->class_perms_map + i;
		ebitmap_t *destmap =
		    dest->class_perms_map + module->map[SYM_CLASSES][i] - 1;
		ebitmap_for_each_positive_bit(srcmap, node, j) {
			if (ksu_ebitmap_set_bit(destmap, module->perm_map[i][j] - 1,
					    1)) {
				goto cleanup;
			}
		}
	}

	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	return -1;
}

static int copy_avrule_decl(link_state_t * state, policy_module_t * module,
			    avrule_decl_t * src_decl, avrule_decl_t * dest_decl)
{
	int ret;

	/* copy all of the RBAC and TE rules */
	if (copy_avrule_list
	    (src_decl->avrules, &dest_decl->avrules, module, state) == -1
	    || copy_role_trans_list(src_decl->role_tr_rules,
				    &dest_decl->role_tr_rules, module,
				    state) == -1
	    || copy_role_allow_list(src_decl->role_allow_rules,
				    &dest_decl->role_allow_rules, module,
				    state) == -1
	    || copy_cond_list(src_decl->cond_list, &dest_decl->cond_list,
			      module, state) == -1) {
		return -1;
	}

	if (copy_filename_trans_list(src_decl->filename_trans_rules,
				     &dest_decl->filename_trans_rules,
				     module, state))
		return -1;

	if (copy_range_trans_list(src_decl->range_tr_rules,
				  &dest_decl->range_tr_rules, module, state))
		return -1;

	/* finally copy any identifiers local to this declaration */
	ret = copy_identifiers(state, src_decl->symtab, dest_decl);
	if (ret < 0) {
		return ret;
	}

	/* then copy required and declared scope indices here */
	if (copy_scope_index(&src_decl->required, &dest_decl->required,
			     module, state) == -1 ||
	    copy_scope_index(&src_decl->declared, &dest_decl->declared,
			     module, state) == -1) {
		return -1;
	}

	return 0;
}

static int copy_avrule_block(link_state_t * state, policy_module_t * module,
			     avrule_block_t * block)
{
	avrule_block_t *new_block = avrule_block_create();
	avrule_decl_t *decl, *last_decl = NULL;
	int ret;

	if (new_block == NULL) {
		ERR(state->handle, "Out of memory!");
		ret = -1;
		goto cleanup;
	}

	new_block->flags = block->flags;

	for (decl = block->branch_list; decl != NULL; decl = decl->next) {
		avrule_decl_t *new_decl =
		    avrule_decl_create(state->next_decl_id);
		if (new_decl == NULL) {
			ERR(state->handle, "Out of memory!");
			ret = -1;
			goto cleanup;
		}

		if (module->policy->name != NULL) {
			new_decl->module_name = strdup(module->policy->name);
			if (new_decl->module_name == NULL) {
				ERR(state->handle, "Out of memory");
				avrule_decl_destroy(new_decl);
				ret = -1;
				goto cleanup;
			}
		}

		if (last_decl == NULL) {
			new_block->branch_list = new_decl;
		} else {
			last_decl->next = new_decl;
		}
		last_decl = new_decl;
		state->base->decl_val_to_struct[state->next_decl_id - 1] =
		    new_decl;
		state->decl_to_mod[state->next_decl_id] = module->policy;

		module->avdecl_map[decl->decl_id] = new_decl->decl_id;

		ret = copy_avrule_decl(state, module, decl, new_decl);
		if (ret) {
			avrule_decl_destroy(new_decl);
			goto cleanup;
		}

		state->next_decl_id++;
	}
	state->last_avrule_block->next = new_block;
	state->last_avrule_block = new_block;
	return 0;

      cleanup:
	avrule_block_list_destroy(new_block);
	return ret;
}

static int scope_copy_callback(hashtab_key_t key, hashtab_datum_t datum,
			       void *data)
{
	unsigned int i;
	int ret;
	char *id = key, *new_id = NULL;
	scope_datum_t *scope, *base_scope;
	link_state_t *state = (link_state_t *) data;
	uint32_t symbol_num = state->symbol_num;
	uint32_t *avdecl_map = state->cur->avdecl_map;

	scope = (scope_datum_t *) datum;

	/* check if the base already has a scope entry */
	base_scope = hashtab_search(state->base->scope[symbol_num].table, id);
	if (base_scope == NULL) {
		scope_datum_t *new_scope;
		if ((new_id = strdup(id)) == NULL) {
			goto cleanup;
		}

		if ((new_scope =
		     (scope_datum_t *) calloc(1, sizeof(*new_scope))) == NULL) {
			free(new_id);
			goto cleanup;
		}
		ret = hashtab_insert(state->base->scope[symbol_num].table,
				     (hashtab_key_t) new_id,
				     (hashtab_datum_t) new_scope);
		if (ret) {
			free(new_id);
			free(new_scope);
			goto cleanup;
		}
		new_scope->scope = SCOPE_REQ;	/* this is reset further down */
		base_scope = new_scope;
	}
	if (base_scope->scope == SCOPE_REQ && scope->scope == SCOPE_DECL) {
		/* this module declared symbol, so overwrite the old
		 * list with the new decl ids */
		base_scope->scope = SCOPE_DECL;
		free(base_scope->decl_ids);
		base_scope->decl_ids = NULL;
		base_scope->decl_ids_len = 0;
		for (i = 0; i < scope->decl_ids_len; i++) {
			if (add_i_to_a(avdecl_map[scope->decl_ids[i]],
				       &base_scope->decl_ids_len,
				       &base_scope->decl_ids) == -1) {
				goto cleanup;
			}
		}
	} else if (base_scope->scope == SCOPE_DECL && scope->scope == SCOPE_REQ) {
		/* this module depended on a symbol that now exists,
		 * so don't do anything */
	} else if (base_scope->scope == SCOPE_REQ && scope->scope == SCOPE_REQ) {
		/* symbol is still required, so add to the list */
		for (i = 0; i < scope->decl_ids_len; i++) {
			if (add_i_to_a(avdecl_map[scope->decl_ids[i]],
				       &base_scope->decl_ids_len,
				       &base_scope->decl_ids) == -1) {
				goto cleanup;
			}
		}
	} else {
		/* this module declared a symbol, and it was already
		 * declared.  only roles and users may be multiply
		 * declared; for all others this is an error. */
		if (symbol_num != SYM_ROLES && symbol_num != SYM_USERS) {
			ERR(state->handle,
			    "%s: Duplicate declaration in module: %s %s",
			    state->cur_mod_name,
			    symtab_names[state->symbol_num], id);
			return -1;
		}
		for (i = 0; i < scope->decl_ids_len; i++) {
			if (add_i_to_a(avdecl_map[scope->decl_ids[i]],
				       &base_scope->decl_ids_len,
				       &base_scope->decl_ids) == -1) {
				goto cleanup;
			}
		}
	}
	return 0;

      cleanup:
	ERR(state->handle, "Out of memory!");
	return -1;
}

/* Copy a module over to a base, remapping all values within.  After
 * all identifiers and rules are done, copy the scoping information.
 * This is when it checks for duplicate declarations. */
static int copy_module(link_state_t * state, policy_module_t * module)
{
	int i, ret;
	avrule_block_t *cur;
	state->cur = module;
	state->cur_mod_name = module->policy->name;

	/* first copy all of the identifiers */
	ret = copy_identifiers(state, module->policy->symtab, NULL);
	if (ret) {
		return ret;
	}

	/* next copy all of the avrule blocks */
	for (cur = module->policy->global; cur != NULL; cur = cur->next) {
		ret = copy_avrule_block(state, module, cur);
		if (ret) {
			return ret;
		}
	}

	/* then copy the scoping tables */
	for (i = 0; i < SYM_NUM; i++) {
		state->symbol_num = i;
		if (ksu_hashtab_map
		    (module->policy->scope[i].table, scope_copy_callback,
		     state)) {
			return -1;
		}
	}

	return 0;
}

/***** functions that check requirements and enable blocks in a module ******/

/* borrowed from checkpolicy.c */

struct find_perm_arg {
	unsigned int valuep;
	hashtab_key_t key;
};

static int find_perm(hashtab_key_t key, hashtab_datum_t datum, void *varg)
{

	struct find_perm_arg *arg = varg;

	perm_datum_t *perdatum = (perm_datum_t *) datum;
	if (arg->valuep == perdatum->s.value) {
		arg->key = key;
		return 1;
	}

	return 0;
}

/* Check if the requirements are met for a single declaration.  If all
 * are met return 1.  For the first requirement found to be missing,
 * if 'missing_sym_num' and 'missing_value' are both not NULL then
 * write to them the symbol number and value for the missing
 * declaration.  Then return 0 to indicate a missing declaration.
 * Note that if a declaration had no requirement at all (e.g., an ELSE
 * block) this returns 1. */
static int is_decl_requires_met(link_state_t * state,
				avrule_decl_t * decl,
				struct missing_requirement *req)
{
	/* (This algorithm is very unoptimized.  It performs many
	 * redundant checks.  A very obvious improvement is to cache
	 * which symbols have been verified, so that they do not need
	 * to be re-checked.) */
	unsigned int i, j;
	ebitmap_t *bitmap;
	char *id, *perm_id;
	policydb_t *pol = state->base;
	ebitmap_node_t *node;

	/* check that all symbols have been satisfied */
	for (i = 0; i < SYM_NUM; i++) {
		if (i == SYM_CLASSES) {
			/* classes will be checked during permissions
			 * checking phase below */
			continue;
		}
		bitmap = &decl->required.scope[i];
		ebitmap_for_each_positive_bit(bitmap, node, j) {
			/* check base's scope table */
			id = pol->sym_val_to_name[i][j];
			if (!is_id_enabled(id, state->base, i)) {
				/* this symbol was not found */
				if (req != NULL) {
					req->symbol_type = i;
					req->symbol_value = j + 1;
				}
				return 0;
			}
		}
	}
	/* check that all classes and permissions have been satisfied */
	for (i = 0; i < decl->required.class_perms_len; i++) {

		bitmap = decl->required.class_perms_map + i;
		ebitmap_for_each_positive_bit(bitmap, node, j) {
			struct find_perm_arg fparg;
			class_datum_t *cladatum;
			uint32_t perm_value = j + 1;
			int rc;
			scope_datum_t *scope;

			id = pol->p_class_val_to_name[i];
			cladatum = pol->class_val_to_struct[i];

			scope =
			    hashtab_search(state->base->p_classes_scope.table,
					   id);
			if (scope == NULL) {
				ERR(state->handle,
				    "Could not find scope information for class %s",
				    id);
				return -1;
			}

			fparg.valuep = perm_value;
			fparg.key = NULL;

			(void)ksu_hashtab_map(cladatum->permissions.table, find_perm,
				    &fparg);
			if (fparg.key == NULL && cladatum->comdatum != NULL) {
				rc = ksu_hashtab_map(cladatum->comdatum->permissions.table,
						 find_perm, &fparg);
				assert(rc == 1);
			}
			perm_id = fparg.key;

			assert(perm_id != NULL);
			if (!is_perm_enabled(id, perm_id, state->base)) {
				if (req != NULL) {
					req->symbol_type = SYM_CLASSES;
					req->symbol_value = i + 1;
					req->perm_value = perm_value;
				}
				return 0;
			}
		}
	}

	/* all requirements have been met */
	return 1;
}

static int debug_requirements(link_state_t * state, policydb_t * p)
{
	int ret;
	avrule_block_t *cur;
	missing_requirement_t req;
	memset(&req, 0, sizeof(req));

	for (cur = p->global; cur != NULL; cur = cur->next) {
		if (cur->enabled != NULL)
			continue;

		ret = is_decl_requires_met(state, cur->branch_list, &req);
		if (ret < 0) {
			return ret;
		} else if (ret == 0) {
			const char *mod_name = cur->branch_list->module_name ?
			    cur->branch_list->module_name : "BASE";
			if (req.symbol_type == SYM_CLASSES) {
				struct find_perm_arg fparg;

				class_datum_t *cladatum;
				cladatum = p->class_val_to_struct[req.symbol_value - 1];

				fparg.valuep = req.perm_value;
				fparg.key = NULL;
				(void)ksu_hashtab_map(cladatum->permissions.table,
						  find_perm, &fparg);

				if (cur->flags & AVRULE_OPTIONAL) {
					ERR(state->handle,
					    "%s[%d]'s optional requirements were not met: class %s, permission %s",
					    mod_name, cur->branch_list->decl_id,
					    p->p_class_val_to_name[req.symbol_value - 1],
					    fparg.key);
				} else {
					ERR(state->handle,
					    "%s[%d]'s global requirements were not met: class %s, permission %s",
					    mod_name, cur->branch_list->decl_id,
					    p->p_class_val_to_name[req.symbol_value - 1],
					    fparg.key);
				}
			} else {
				if (cur->flags & AVRULE_OPTIONAL) {
					ERR(state->handle,
					    "%s[%d]'s optional requirements were not met: %s %s",
					    mod_name, cur->branch_list->decl_id,
					    symtab_names[req.symbol_type],
					    p->sym_val_to_name[req.
							       symbol_type][req.
									    symbol_value
									    -
									    1]);
				} else {
					ERR(state->handle,
					    "%s[%d]'s global requirements were not met: %s %s",
					    mod_name, cur->branch_list->decl_id,
					    symtab_names[req.symbol_type],
					    p->sym_val_to_name[req.
							       symbol_type][req.
									    symbol_value
									    -
									    1]);
				}
			}
		}
	}
	return 0;
}

static void print_missing_requirements(link_state_t * state,
				       avrule_block_t * cur,
				       missing_requirement_t * req)
{
	policydb_t *p = state->base;
	const char *mod_name = cur->branch_list->module_name ?
	    cur->branch_list->module_name : "BASE";

	if (req->symbol_type == SYM_CLASSES) {

		struct find_perm_arg fparg;

		class_datum_t *cladatum;
		cladatum = p->class_val_to_struct[req->symbol_value - 1];

		fparg.valuep = req->perm_value;
		fparg.key = NULL;
		(void)ksu_hashtab_map(cladatum->permissions.table, find_perm, &fparg);

		ERR(state->handle,
		    "%s's global requirements were not met: class %s, permission %s",
		    mod_name,
		    p->p_class_val_to_name[req->symbol_value - 1], fparg.key);
	} else {
		ERR(state->handle,
		    "%s's global requirements were not met: %s %s",
		    mod_name,
		    symtab_names[req->symbol_type],
		    p->sym_val_to_name[req->symbol_type][req->symbol_value - 1]);
	}
}

/* Enable all of the avrule_decl blocks for the policy. This simple
 * algorithm is the following:
 *
 * 1) Enable all of the non-else avrule_decls for all blocks.
 * 2) Iterate through the non-else decls looking for decls whose requirements
 *    are not met.
 *    2a) If the decl is non-optional, return immediately with an error.
 *    2b) If the decl is optional, disable the block and mark changed = 1
 * 3) If changed == 1 goto 2.
 * 4) Iterate through all blocks looking for those that have no enabled
 *    decl. If the block has an else decl, enable.
 *
 * This will correctly handle all dependencies, including mutual and
 * circular. The only downside is that it is slow.
 */
static int enable_avrules(link_state_t * state, policydb_t * pol)
{
	int changed = 1;
	avrule_block_t *block;
	avrule_decl_t *decl;
	missing_requirement_t req;
	int ret = 0, rc;

	if (state->verbose) {
		INFO(state->handle, "Determining which avrules to enable.");
	}

	/* 1) enable all of the non-else blocks */
	for (block = pol->global; block != NULL; block = block->next) {
		block->enabled = block->branch_list;
		block->enabled->enabled = 1;
		for (decl = block->branch_list->next; decl != NULL;
		     decl = decl->next)
			decl->enabled = 0;
	}

	/* 2) Iterate */
	while (changed) {
		changed = 0;
		for (block = pol->global; block != NULL; block = block->next) {
			if (block->enabled == NULL) {
				continue;
			}
			decl = block->branch_list;
			if (state->verbose) {
				const char *mod_name = decl->module_name ?
				    decl->module_name : "BASE";
				INFO(state->handle, "check module %s decl %d",
				     mod_name, decl->decl_id);
			}
			rc = is_decl_requires_met(state, decl, &req);
			if (rc < 0) {
				ret = SEPOL_ERR;
				goto out;
			} else if (rc == 0) {
				decl->enabled = 0;
				block->enabled = NULL;
				changed = 1;
				if (!(block->flags & AVRULE_OPTIONAL)) {
					print_missing_requirements(state, block,
								   &req);
					ret = SEPOL_EREQ;
					goto out;
				}
			}
		}
	}

	/* 4) else handling
	 *
	 * Iterate through all of the blocks skipping the first (which is the
	 * global block, is required to be present, and cannot have an else).
	 * If the block is disabled and has an else decl, enable that.
	 *
	 * This code assumes that the second block in the branch list is the else
	 * block. This is currently supported by the compiler.
	 */
	for (block = pol->global->next; block != NULL; block = block->next) {
		if (block->enabled == NULL) {
			if (block->branch_list->next != NULL) {
				block->enabled = block->branch_list->next;
				block->branch_list->next->enabled = 1;
			}
		}
	}

      out:
	if (state->verbose)
		debug_requirements(state, pol);

	return ret;
}

/*********** the main linking functions ***********/

/* Given a module's policy, normalize all conditional expressions
 * within.  Return 0 on success, -1 on error. */
static int cond_normalize(policydb_t * p)
{
	avrule_block_t *block;
	for (block = p->global; block != NULL; block = block->next) {
		avrule_decl_t *decl;
		for (decl = block->branch_list; decl != NULL; decl = decl->next) {
			cond_list_t *cond = decl->cond_list;
			while (cond) {
				if (cond_normalize_expr(p, cond) < 0)
					return -1;
				cond = cond->next;
			}
		}
	}
	return 0;
}

/* Allocate space for the various remapping arrays. */
static int prepare_module(link_state_t * state, policy_module_t * module)
{
	int i;
	uint32_t items, num_decls = 0;
	avrule_block_t *cur;

	/* allocate the maps */
	for (i = 0; i < SYM_NUM; i++) {
		items = module->policy->symtab[i].nprim;
		if ((module->map[i] =
		     (uint32_t *) calloc(items,
					 sizeof(*module->map[i]))) == NULL) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}
	}

	/* allocate the permissions remap here */
	items = module->policy->p_classes.nprim;
	if ((module->perm_map_len =
	     calloc(items, sizeof(*module->perm_map_len))) == NULL) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	if ((module->perm_map =
	     calloc(items, sizeof(*module->perm_map))) == NULL) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	/* allocate a map for avrule_decls */
	for (cur = module->policy->global; cur != NULL; cur = cur->next) {
		avrule_decl_t *decl;
		for (decl = cur->branch_list; decl != NULL; decl = decl->next) {
			if (decl->decl_id > num_decls) {
				num_decls = decl->decl_id;
			}
		}
	}
	num_decls++;
	if ((module->avdecl_map = calloc(num_decls, sizeof(uint32_t))) == NULL) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	module->num_decls = num_decls;

	/* normalize conditionals within */
	if (cond_normalize(module->policy) < 0) {
		ERR(state->handle,
		    "Error while normalizing conditionals within the module %s.",
		    module->policy->name);
		return -1;
	}
	return 0;
}

static int prepare_base(link_state_t * state, uint32_t num_mod_decls)
{
	avrule_block_t *cur = state->base->global;
	assert(cur != NULL);
	state->next_decl_id = 0;

	/* iterate through all of the declarations in the base, to
	   determine what the next decl_id should be */
	while (cur != NULL) {
		avrule_decl_t *decl;
		for (decl = cur->branch_list; decl != NULL; decl = decl->next) {
			if (decl->decl_id > state->next_decl_id) {
				state->next_decl_id = decl->decl_id;
			}
		}
		state->last_avrule_block = cur;
		cur = cur->next;
	}
	state->last_base_avrule_block = state->last_avrule_block;
	state->next_decl_id++;

	/* allocate the table mapping from base's decl_id to its
	 * avrule_decls and set the initial mappings */
	free(state->base->decl_val_to_struct);
	if ((state->base->decl_val_to_struct =
	     calloc(state->next_decl_id + num_mod_decls,
		    sizeof(*(state->base->decl_val_to_struct)))) == NULL) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	/* This allocates the decl block to module mapping used for error reporting */
	if ((state->decl_to_mod = calloc(state->next_decl_id + num_mod_decls,
					 sizeof(*(state->decl_to_mod)))) ==
	    NULL) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}
	cur = state->base->global;
	while (cur != NULL) {
		avrule_decl_t *decl = cur->branch_list;
		while (decl != NULL) {
			state->base->decl_val_to_struct[decl->decl_id - 1] =
			    decl;
			state->decl_to_mod[decl->decl_id] = state->base;
			decl = decl->next;
		}
		cur = cur->next;
	}

	/* normalize conditionals within */
	if (cond_normalize(state->base) < 0) {
		ERR(state->handle,
		    "Error while normalizing conditionals within the base module.");
		return -1;
	}
	return 0;
}

static int expand_role_attributes(hashtab_key_t key, hashtab_datum_t datum,
				  void * data)
{
	char *id;
	role_datum_t *role, *sub_attr;
	link_state_t *state;
	unsigned int i;
	ebitmap_node_t *rnode;

	id = key;
	role = (role_datum_t *)datum;
	state = (link_state_t *)data;

	if (strcmp(id, OBJECT_R) == 0){
		/* object_r is never a role attribute by far */
		return 0;
	}

	if (role->flavor != ROLE_ATTRIB)
		return 0;

	if (state->verbose)
		INFO(state->handle, "expanding role attribute %s", id);

restart:
	ebitmap_for_each_positive_bit(&role->roles, rnode, i) {
		sub_attr = state->base->role_val_to_struct[i];
		if (sub_attr->flavor != ROLE_ATTRIB)
			continue;

		/* remove the sub role attribute from the parent
		 * role attribute's roles ebitmap */
		if (ksu_ebitmap_set_bit(&role->roles, i, 0))
			return -1;

		/* loop dependency of role attributes */
		if (sub_attr->s.value == role->s.value)
			continue;

		/* now go on to expand a sub role attribute
		 * by escalating its roles ebitmap */
		if (ebitmap_union(&role->roles, &sub_attr->roles)) {
			ERR(state->handle, "Out of memory!");
			return -1;
		}

		/* sub_attr->roles may contain other role attributes,
		 * re-scan the parent role attribute's roles ebitmap */
		goto restart;
	}

	return 0;
}

/* For any role attribute in a declaration's local symtab[SYM_ROLES] table,
 * copy its roles ebitmap into its duplicate's in the base->p_roles.table.
 */
static int populate_decl_roleattributes(hashtab_key_t key, 
					hashtab_datum_t datum,
					void *data)
{
	char *id = key;
	role_datum_t *decl_role, *base_role;
	link_state_t *state = (link_state_t *)data;

	decl_role = (role_datum_t *)datum;

	if (strcmp(id, OBJECT_R) == 0) {
		/* object_r is never a role attribute by far */
		return 0;
	}

	if (decl_role->flavor != ROLE_ATTRIB)
		return 0;

	base_role = (role_datum_t *)hashtab_search(state->base->p_roles.table,
						   id);
	assert(base_role != NULL && base_role->flavor == ROLE_ATTRIB);

	if (ebitmap_union(&base_role->roles, &decl_role->roles)) {
		ERR(state->handle, "Out of memory!");
		return -1;
	}

	return 0;
}

static int populate_roleattributes(link_state_t *state, policydb_t *pol)
{
	avrule_block_t *block;
	avrule_decl_t *decl;

	if (state->verbose)
		INFO(state->handle, "Populating role-attribute relationship "
			    "from enabled declarations' local symtab.");

	/* Iterate through all of the blocks skipping the first(which is the
	 * global block, is required to be present and can't have an else).
	 * If the block is disabled or not having an enabled decl, skip it.
	 */
	for (block = pol->global->next; block != NULL; block = block->next)
	{
		decl = block->enabled;
		if (decl == NULL || decl->enabled == 0)
			continue;

		if (ksu_hashtab_map(decl->symtab[SYM_ROLES].table, 
				populate_decl_roleattributes, state))
			return -1;
	}

	return 0;
}

/* Link a set of modules into a base module. This process is somewhat
 * similar to an actual compiler: it requires a set of order dependent
 * steps.  The base and every module must have been indexed prior to
 * calling this function.
 */
int link_modules(sepol_handle_t * handle,
		 policydb_t * b, policydb_t ** mods, int len, int verbose)
{
	int i, ret, retval = -1;
	policy_module_t **modules = NULL;
	link_state_t state;
	uint32_t num_mod_decls = 0;

	memset(&state, 0, sizeof(state));
	state.base = b;
	state.verbose = verbose;
	state.handle = handle;

	if (b->policy_type != POLICY_BASE) {
		ERR(state.handle, "Target of link was not a base policy.");
		return -1;
	}

	/* first allocate some space to hold the maps from module
	 * symbol's value to the destination symbol value; then do
	 * other preparation work */
	if ((modules =
	     (policy_module_t **) calloc(len, sizeof(*modules))) == NULL) {
		ERR(state.handle, "Out of memory!");
		return -1;
	}
	for (i = 0; i < len; i++) {
		if (mods[i]->policy_type != POLICY_MOD) {
			ERR(state.handle,
			    "Tried to link in a policy that was not a module.");
			goto cleanup;
		}

		if (mods[i]->mls != b->mls) {
			if (b->mls)
				ERR(state.handle,
				    "Tried to link in a non-MLS module with an MLS base.");
			else
				ERR(state.handle,
				    "Tried to link in an MLS module with a non-MLS base.");
			goto cleanup;
		}

		if (mods[i]->policyvers > b->policyvers) {
			WARN(state.handle,
			     "Upgrading policy version from %u to %u", b->policyvers, mods[i]->policyvers);
			b->policyvers = mods[i]->policyvers;
		}

		if ((modules[i] =
		     (policy_module_t *) calloc(1,
						sizeof(policy_module_t))) ==
		    NULL) {
			ERR(state.handle, "Out of memory!");
			goto cleanup;
		}
		modules[i]->policy = mods[i];
		if (prepare_module(&state, modules[i]) == -1) {
			goto cleanup;
		}
		num_mod_decls += modules[i]->num_decls;
	}
	if (prepare_base(&state, num_mod_decls) == -1) {
		goto cleanup;
	}

	/* copy and remap the module's data over to base */
	for (i = 0; i < len; i++) {
		state.cur = modules[i];
		ret = copy_module(&state, modules[i]);
		if (ret) {
			retval = ret;
			goto cleanup;
		}
	}

	/* re-index base, for symbols were added to symbol tables  */
	if (policydb_index_classes(state.base)) {
		ERR(state.handle, "Error while indexing classes");
		goto cleanup;
	}
	if (policydb_index_others(state.handle, state.base, 0)) {
		ERR(state.handle, "Error while indexing others");
		goto cleanup;
	}

	if (enable_avrules(&state, state.base)) {
		retval = SEPOL_EREQ;
		goto cleanup;
	}

	/* Now that all role attribute's roles ebitmap have been settled,
	 * escalate sub role attribute's roles ebitmap into that of parent.
	 *
	 * First, since some role-attribute relationships could be recorded
	 * in some decl's local symtab(see get_local_role()), we need to
	 * populate them up to the base.p_roles table. */
	if (populate_roleattributes(&state, state.base)) {
		retval = SEPOL_EREQ;
		goto cleanup;
	}
	
	/* Now do the escalation. */
	if (ksu_hashtab_map(state.base->p_roles.table, expand_role_attributes,
			&state))
		goto cleanup;

	retval = 0;
      cleanup:
	for (i = 0; modules != NULL && i < len; i++) {
		policy_module_destroy(modules[i]);
	}
	free(modules);
	free(state.decl_to_mod);
	return retval;
}

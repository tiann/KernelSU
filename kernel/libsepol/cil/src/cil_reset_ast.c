
#include "cil_internal.h"
#include "cil_log.h"
#include "cil_list.h"
#include "cil_reset_ast.h"
#include "cil_symtab.h"

static inline void cil_reset_classperms_list(struct cil_list *cp_list);
static inline void cil_reset_level(struct cil_level *level);
static inline void cil_reset_levelrange(struct cil_levelrange *levelrange);
static inline void cil_reset_context(struct cil_context *context);


static int __class_reset_perm_values(__attribute__((unused)) hashtab_key_t k, hashtab_datum_t d, void *args)
{
	struct cil_perm *perm = (struct cil_perm *)d;

	perm->value -= *((int *)args);

	return SEPOL_OK;
}

static void cil_reset_class(struct cil_class *class)
{
	if (class->common != NULL) {
		/* Must assume that the common has been destroyed */
		int num_common_perms = class->num_perms - class->perms.nprim;
		cil_symtab_map(&class->perms, __class_reset_perm_values, &num_common_perms);
		/* during a re-resolve, we need to reset the common, so a classcommon
		 * statement isn't seen as a duplicate */
		class->num_perms = class->perms.nprim;
		class->common = NULL; /* Must make this NULL or there will be an error when re-resolving */
	}
	class->ordered = CIL_FALSE;
}

static void cil_reset_perm(struct cil_perm *perm)
{
	cil_list_destroy(&perm->classperms, CIL_FALSE);
}

static inline void cil_reset_classperms(struct cil_classperms *cp)
{
	if (cp == NULL) {
		return;
	}

	cp->class = NULL;
	cil_list_destroy(&cp->perms, CIL_FALSE);
}

static void cil_reset_classpermission(struct cil_classpermission *cp)
{
	if (cp == NULL) {
		return;
	}

	cil_list_destroy(&cp->classperms, CIL_FALSE);
}

static void cil_reset_classperms_set(struct cil_classperms_set *cp_set)
{
	if (cp_set == NULL || cp_set->set == NULL) {
		return;
	}

	if (cp_set->set->datum.name == NULL) {
		cil_reset_classperms_list(cp_set->set->classperms);
	}

	cp_set->set = NULL;
}

static inline void cil_reset_classperms_list(struct cil_list *cp_list)
{
	struct cil_list_item *curr;

	if (cp_list == NULL) {
		return;
	}

	cil_list_for_each(curr, cp_list) {
		if (curr->flavor == CIL_CLASSPERMS) { /* KERNEL or MAP */
			cil_reset_classperms(curr->data);
		} else if (curr->flavor == CIL_CLASSPERMS_SET) { /* SET */
			cil_reset_classperms_set(curr->data);
		}
	}
}

static void cil_reset_classpermissionset(struct cil_classpermissionset *cps)
{
	cil_reset_classperms_list(cps->classperms);
}

static void cil_reset_classmapping(struct cil_classmapping *cm)
{
	cil_reset_classperms_list(cm->classperms);
}

static void cil_reset_alias(struct cil_alias *alias)
{
	/* reset actual to NULL during a re-resolve */
	alias->actual = NULL;
}

static void cil_reset_user(struct cil_user *user)
{
	/* reset the bounds to NULL during a re-resolve */
	user->bounds = NULL;
	user->dftlevel = NULL;
	user->range = NULL;
}

static void cil_reset_userattr(struct cil_userattribute *attr)
{
	struct cil_list_item *expr = NULL;
	struct cil_list_item *next = NULL;

	/* during a re-resolve, we need to reset the lists of expression stacks associated with this attribute from a userattribute statement */
	if (attr->expr_list != NULL) {
		/* we don't want to destroy the expression stacks (cil_list) inside
		 * this list cil_list_destroy destroys sublists, so we need to do it
		 * manually */
		expr = attr->expr_list->head;
		while (expr != NULL) {
			next = expr->next;
			cil_list_item_destroy(&expr, CIL_FALSE);
			expr = next;
		}
		free(attr->expr_list);
		attr->expr_list = NULL;
	}
}

static void cil_reset_userattributeset(struct cil_userattributeset *uas)
{
	cil_list_destroy(&uas->datum_expr, CIL_FALSE);
}

static void cil_reset_selinuxuser(struct cil_selinuxuser *selinuxuser)
{
	selinuxuser->user = NULL;
	if (selinuxuser->range_str == NULL) {
		cil_reset_levelrange(selinuxuser->range);
	} else {
		selinuxuser->range = NULL;
	}
}

static void cil_reset_role(struct cil_role *role)
{
	/* reset the bounds to NULL during a re-resolve */
	role->bounds = NULL;
}

static void cil_reset_roleattr(struct cil_roleattribute *attr)
{
	/* during a re-resolve, we need to reset the lists of expression stacks  associated with this attribute from a attributeroles statement */
	if (attr->expr_list != NULL) {
		/* we don't want to destroy the expression stacks (cil_list) inside
		 * this list cil_list_destroy destroys sublists, so we need to do it
		 * manually */
		struct cil_list_item *expr = attr->expr_list->head;
		while (expr != NULL) {
			struct cil_list_item *next = expr->next;
			cil_list_item_destroy(&expr, CIL_FALSE);
			expr = next;
		}
		free(attr->expr_list);
		attr->expr_list = NULL;
	}
}

static void cil_reset_roleattributeset(struct cil_roleattributeset *ras)
{
	cil_list_destroy(&ras->datum_expr, CIL_FALSE);
}

static void cil_reset_type(struct cil_type *type)
{
	/* reset the bounds to NULL during a re-resolve */
	type->bounds = NULL;
}

static void cil_reset_typeattr(struct cil_typeattribute *attr)
{
	/* during a re-resolve, we need to reset the lists of expression stacks  associated with this attribute from a attributetypes statement */
	if (attr->expr_list != NULL) {
		/* we don't want to destroy the expression stacks (cil_list) inside
		 * this list cil_list_destroy destroys sublists, so we need to do it
		 * manually */
		struct cil_list_item *expr = attr->expr_list->head;
		while (expr != NULL) {
			struct cil_list_item *next = expr->next;
			cil_list_item_destroy(&expr, CIL_FALSE);
			expr = next;
		}
		free(attr->expr_list);
		attr->expr_list = NULL;
	}
	attr->used = CIL_FALSE;
	attr->keep = CIL_FALSE;
}

static void cil_reset_typeattributeset(struct cil_typeattributeset *tas)
{
	cil_list_destroy(&tas->datum_expr, CIL_FALSE);
}

static void cil_reset_expandtypeattribute(struct cil_expandtypeattribute *expandattr)
{
	cil_list_destroy(&expandattr->attr_datums, CIL_FALSE);
}

static void cil_reset_avrule(struct cil_avrule *rule)
{
	cil_reset_classperms_list(rule->perms.classperms);
}

static void cil_reset_rangetransition(struct cil_rangetransition *rangetrans)
{
	if (rangetrans->range_str == NULL) {
		cil_reset_levelrange(rangetrans->range);
	} else {
		rangetrans->range = NULL;
	}
}

static void cil_reset_sens(struct cil_sens *sens)
{
	/* during a re-resolve, we need to reset the categories associated with
	 * this sensitivity from a (sensitivitycategory) statement */
	cil_list_destroy(&sens->cats_list, CIL_FALSE);
	sens->ordered = CIL_FALSE;
}

static void cil_reset_cat(struct cil_cat *cat)
{
	cat->ordered = CIL_FALSE;
}

static inline void cil_reset_cats(struct cil_cats *cats)
{
	if (cats != NULL) {
		cats->evaluated = CIL_FALSE;
		cil_list_destroy(&cats->datum_expr, CIL_FALSE);
	}
}


static void cil_reset_senscat(struct cil_senscat *senscat)
{
	cil_reset_cats(senscat->cats);
}

static void cil_reset_catset(struct cil_catset *catset)
{
	cil_reset_cats(catset->cats);
}

static inline void cil_reset_level(struct cil_level *level)
{
	level->sens = NULL;
	cil_reset_cats(level->cats);
}

static inline void cil_reset_levelrange(struct cil_levelrange *levelrange)
{
	if (levelrange->low_str == NULL) {
		cil_reset_level(levelrange->low);
	} else {
		levelrange->low = NULL;
	}

	if (levelrange->high_str == NULL) {
		cil_reset_level(levelrange->high);
	} else {
		levelrange->high = NULL;
	}
}

static inline void cil_reset_userlevel(struct cil_userlevel *userlevel)
{
	if (userlevel->level_str == NULL) {
		cil_reset_level(userlevel->level);
	} else {
		userlevel->level = NULL;
	}
}

static inline void cil_reset_userrange(struct cil_userrange *userrange)
{
	if (userrange->range_str == NULL) {
		cil_reset_levelrange(userrange->range);
	} else {
		userrange->range = NULL;
	}
}

static inline void cil_reset_context(struct cil_context *context)
{
	if (!context) {
		return;
	}
	if (context->range_str == NULL) {
		cil_reset_levelrange(context->range);
	} else {
		context->range = NULL;
	}
}

static void cil_reset_sidcontext(struct cil_sidcontext *sidcontext)
{
	if (sidcontext->context_str == NULL) {
		cil_reset_context(sidcontext->context);
	} else {
		sidcontext->context = NULL;
	}
}

static void cil_reset_filecon(struct cil_filecon *filecon)
{
	if (filecon->context_str == NULL) {
		cil_reset_context(filecon->context);
	} else {
		filecon->context = NULL;
	}
}

static void cil_reset_ibpkeycon(struct cil_ibpkeycon *ibpkeycon)
{
	if (ibpkeycon->context_str == NULL) {
		cil_reset_context(ibpkeycon->context);
	} else {
		ibpkeycon->context = NULL;
	}
}

static void cil_reset_portcon(struct cil_portcon *portcon)
{
	if (portcon->context_str == NULL) {
		cil_reset_context(portcon->context);
	} else {
		portcon->context = NULL;
	}
}

static void cil_reset_nodecon(struct cil_nodecon *nodecon)
{
	if (nodecon->context_str == NULL) {
		cil_reset_context(nodecon->context);
	} else {
		nodecon->context = NULL;
	}
}

static void cil_reset_genfscon(struct cil_genfscon *genfscon)
{
	if (genfscon->context_str == NULL) {
		cil_reset_context(genfscon->context);
	} else {
		genfscon->context = NULL;
	}
}

static void cil_reset_netifcon(struct cil_netifcon *netifcon)
{
	if (netifcon->if_context_str == NULL) {
		cil_reset_context(netifcon->if_context);
	} else {
		netifcon->if_context = NULL;
	}

	if (netifcon->packet_context_str == NULL) {
		cil_reset_context(netifcon->packet_context);
	} else {
		netifcon->packet_context = NULL;
	}
}

static void cil_reset_ibendportcon(struct cil_ibendportcon *ibendportcon)
{
	if (ibendportcon->context_str == NULL) {
		cil_reset_context(ibendportcon->context);
	} else {
		ibendportcon->context = NULL;
	}
}

static void cil_reset_pirqcon(struct cil_pirqcon *pirqcon)
{
	if (pirqcon->context_str == NULL) {
		cil_reset_context(pirqcon->context);
	} else {
		pirqcon->context = NULL;
	}
}

static void cil_reset_iomemcon(struct cil_iomemcon *iomemcon)
{
	if (iomemcon->context_str == NULL) {
		cil_reset_context(iomemcon->context);
	} else {
		iomemcon->context = NULL;
	}
}

static void cil_reset_ioportcon(struct cil_ioportcon *ioportcon)
{
	if (ioportcon->context_str == NULL) {
		cil_reset_context(ioportcon->context);
	} else {
		ioportcon->context = NULL;
	}
}

static void cil_reset_pcidevicecon(struct cil_pcidevicecon *pcidevicecon)
{
	if (pcidevicecon->context_str == NULL) {
		cil_reset_context(pcidevicecon->context);
	} else {
		pcidevicecon->context = NULL;
	}
}

static void cil_reset_devicetreecon(struct cil_devicetreecon *devicetreecon)
{
	if (devicetreecon->context_str == NULL) {
		cil_reset_context(devicetreecon->context);
	} else {
		devicetreecon->context = NULL;
	}
}

static void cil_reset_fsuse(struct cil_fsuse *fsuse)
{
	if (fsuse->context_str == NULL) {
		cil_reset_context(fsuse->context);
	} else {
		fsuse->context = NULL;
	}
}

static void cil_reset_sid(struct cil_sid *sid)
{
	/* reset the context to NULL during a re-resolve */
	sid->context = NULL;
	sid->ordered = CIL_FALSE;
}

static void cil_reset_constrain(struct cil_constrain *con)
{
	cil_reset_classperms_list(con->classperms);
	cil_list_destroy(&con->datum_expr, CIL_FALSE);
}

static void cil_reset_validatetrans(struct cil_validatetrans *vt)
{
	cil_list_destroy(&vt->datum_expr, CIL_FALSE);
}

static void cil_reset_default(struct cil_default *def)
{
	cil_list_destroy(&def->class_datums, CIL_FALSE);
}

static void cil_reset_defaultrange(struct cil_defaultrange *def)
{
	cil_list_destroy(&def->class_datums, CIL_FALSE);
}

static void cil_reset_booleanif(struct cil_booleanif *bif)
{
	cil_list_destroy(&bif->datum_expr, CIL_FALSE);
}

static int __cil_reset_node(struct cil_tree_node *node,  __attribute__((unused)) uint32_t *finished, __attribute__((unused)) void *extra_args)
{
	switch (node->flavor) {
	case CIL_CLASS:
		cil_reset_class(node->data);
		break;
	case CIL_PERM:
	case CIL_MAP_PERM:
		cil_reset_perm(node->data);
		break;
	case CIL_CLASSPERMISSION:
		cil_reset_classpermission(node->data);
		break;
	case CIL_CLASSPERMISSIONSET:
		cil_reset_classpermissionset(node->data);
		break;
	case CIL_CLASSMAPPING:
		cil_reset_classmapping(node->data);
		break;
	case CIL_TYPEALIAS:
	case CIL_SENSALIAS:
	case CIL_CATALIAS:
		cil_reset_alias(node->data);
		break;
	case CIL_USERRANGE:
		cil_reset_userrange(node->data);
		break;
	case CIL_USERLEVEL:
		cil_reset_userlevel(node->data);
		break;
	case CIL_USER:
		cil_reset_user(node->data);
		break;
	case CIL_USERATTRIBUTE:
		cil_reset_userattr(node->data);
		break;
	case CIL_USERATTRIBUTESET:
		cil_reset_userattributeset(node->data);
		break;
	case CIL_SELINUXUSERDEFAULT:
	case CIL_SELINUXUSER:
		cil_reset_selinuxuser(node->data);
		break;
	case CIL_ROLE:
		cil_reset_role(node->data);
		break;
	case CIL_ROLEATTRIBUTE:
		cil_reset_roleattr(node->data);
		break;
	case CIL_ROLEATTRIBUTESET:
		cil_reset_roleattributeset(node->data);
		break;
	case CIL_TYPE:
		cil_reset_type(node->data);
		break;
	case CIL_TYPEATTRIBUTE:
		cil_reset_typeattr(node->data);
		break;
	case CIL_TYPEATTRIBUTESET:
		cil_reset_typeattributeset(node->data);
		break;
	case CIL_EXPANDTYPEATTRIBUTE:
		cil_reset_expandtypeattribute(node->data);
		break;
	case CIL_RANGETRANSITION:
		cil_reset_rangetransition(node->data);
		break;
	case CIL_AVRULE:
		cil_reset_avrule(node->data);
		break;
	case CIL_SENS:
		cil_reset_sens(node->data);
		break;
	case CIL_CAT:
		cil_reset_cat(node->data);
		break;
	case CIL_SENSCAT:
		cil_reset_senscat(node->data);
		break;
	case CIL_CATSET:
		cil_reset_catset(node->data);
		break;
	case CIL_LEVEL:
		cil_reset_level(node->data);
		break;
	case CIL_LEVELRANGE:
		cil_reset_levelrange(node->data);
		break;
	case CIL_CONTEXT:
		cil_reset_context(node->data);
		break;
	case CIL_SIDCONTEXT:
		cil_reset_sidcontext(node->data);
		break;
	case CIL_FILECON:
		cil_reset_filecon(node->data);
		break;
	case CIL_IBPKEYCON:
		cil_reset_ibpkeycon(node->data);
		break;
	case CIL_IBENDPORTCON:
		cil_reset_ibendportcon(node->data);
		break;
	case CIL_PORTCON:
		cil_reset_portcon(node->data);
		break;
	case CIL_NODECON:
		cil_reset_nodecon(node->data);
		break;
	case CIL_GENFSCON:
		cil_reset_genfscon(node->data);
		break;
	case CIL_NETIFCON:
		cil_reset_netifcon(node->data);
		break;
	case CIL_PIRQCON:
		cil_reset_pirqcon(node->data);
		break;
	case CIL_IOMEMCON:
		cil_reset_iomemcon(node->data);
		break;
	case CIL_IOPORTCON:
		cil_reset_ioportcon(node->data);
		break;
	case CIL_PCIDEVICECON:
		cil_reset_pcidevicecon(node->data);
		break;
	case CIL_DEVICETREECON:
		cil_reset_devicetreecon(node->data);
		break;
	case CIL_FSUSE:
		cil_reset_fsuse(node->data);
		break;
	case CIL_SID:
		cil_reset_sid(node->data);
		break;
	case CIL_CONSTRAIN:
	case CIL_MLSCONSTRAIN:
		cil_reset_constrain(node->data);
		break;
	case CIL_VALIDATETRANS:
	case CIL_MLSVALIDATETRANS:
		cil_reset_validatetrans(node->data);
		break;
	case CIL_DEFAULTUSER:
	case CIL_DEFAULTROLE:
	case CIL_DEFAULTTYPE:
		cil_reset_default(node->data);
		break;
	case CIL_DEFAULTRANGE:
		cil_reset_defaultrange(node->data);
		break;
	case CIL_BOOLEANIF:
		cil_reset_booleanif(node->data);
		break;
	case CIL_TUNABLEIF:
	case CIL_CALL:
		break; /* Not effected by optional block disabling */
	case CIL_MACRO:
	case CIL_SIDORDER:
	case CIL_CLASSORDER:
	case CIL_CATORDER:
	case CIL_SENSITIVITYORDER:
		break; /* Nothing to reset */
	default:
		break;
	}

	return SEPOL_OK;
}

int cil_reset_ast(struct cil_tree_node *current)
{
	int rc = SEPOL_ERR;

	rc = cil_tree_walk(current, __cil_reset_node, NULL, NULL, NULL);
	if (rc != SEPOL_OK) {
		cil_log(CIL_ERR, "Failed to reset AST\n");
		return SEPOL_ERR;
	}

	return SEPOL_OK;
}

#include <linux/gfp.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "sepolicy.h"
#include "../klog.h" // IWYU pragma: keep
#include "ss/symtab.h"
#include "../kernel_compat.h" // Add check Huawei Device

#define KSU_SUPPORT_ADD_TYPE

//////////////////////////////////////////////////////
// Declaration
//////////////////////////////////////////////////////

static struct avtab_node *get_avtab_node(struct policydb *db,
					 struct avtab_key *key,
					 struct avtab_extended_perms *xperms);

static bool add_rule(struct policydb *db, const char *s, const char *t,
		     const char *c, const char *p, int effect, bool invert);

static void add_rule_raw(struct policydb *db, struct type_datum *src,
			 struct type_datum *tgt, struct class_datum *cls,
			 struct perm_datum *perm, int effect, bool invert);

static void add_xperm_rule_raw(struct policydb *db, struct type_datum *src,
			       struct type_datum *tgt, struct class_datum *cls,
			       uint16_t low, uint16_t high, int effect,
			       bool invert);
static bool add_xperm_rule(struct policydb *db, const char *s, const char *t,
			   const char *c, const char *range, int effect,
			   bool invert);

static bool add_type_rule(struct policydb *db, const char *s, const char *t,
			  const char *c, const char *d, int effect);

static bool add_filename_trans(struct policydb *db, const char *s,
			       const char *t, const char *c, const char *d,
			       const char *o);

static bool add_genfscon(struct policydb *db, const char *fs_name,
			 const char *path, const char *context);

static bool add_type(struct policydb *db, const char *type_name, bool attr);

static bool set_type_state(struct policydb *db, const char *type_name,
			   bool permissive);

static void add_typeattribute_raw(struct policydb *db, struct type_datum *type,
				  struct type_datum *attr);

static bool add_typeattribute(struct policydb *db, const char *type,
			      const char *attr);

//////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////

// Invert is adding rules for auditdeny; in other cases, invert is removing
// rules
#define strip_av(effect, invert) ((effect == AVTAB_AUDITDENY) == !invert)

#define ksu_hash_for_each(node_ptr, n_slot, cur)                               \
	int i;                                                                 \
	for (i = 0; i < n_slot; ++i)                                           \
		for (cur = node_ptr[i]; cur; cur = cur->next)

// htable is a struct instead of pointer above 5.8.0:
// https://elixir.bootlin.com/linux/v5.8-rc1/source/security/selinux/ss/symtab.h
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
#define ksu_hashtab_for_each(htab, cur)                                        \
	ksu_hash_for_each(htab.htable, htab.size, cur)
#else
#define ksu_hashtab_for_each(htab, cur)                                        \
	ksu_hash_for_each(htab->htable, htab->size, cur)
#endif

// symtab_search is introduced on 5.9.0:
// https://elixir.bootlin.com/linux/v5.9-rc1/source/security/selinux/ss/symtab.h
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
#define symtab_search(s, name) hashtab_search((s)->table, name)
#define symtab_insert(s, name, datum) hashtab_insert((s)->table, name, datum)
#endif

#define avtab_for_each(avtab, cur)                                             \
	ksu_hash_for_each(avtab.htable, avtab.nslot, cur);

static struct avtab_node *get_avtab_node(struct policydb *db,
					 struct avtab_key *key,
					 struct avtab_extended_perms *xperms)
{
	struct avtab_node *node;

	/* AVTAB_XPERMS entries are not necessarily unique */
	if (key->specified & AVTAB_XPERMS) {
		bool match = false;
		node = avtab_search_node(&db->te_avtab, key);
		while (node) {
			if ((node->datum.u.xperms->specified ==
			     xperms->specified) &&
			    (node->datum.u.xperms->driver == xperms->driver)) {
				match = true;
				break;
			}
			node = avtab_search_node_next(node, key->specified);
		}
		if (!match)
			node = NULL;
	} else {
		node = avtab_search_node(&db->te_avtab, key);
	}

	if (!node) {
		struct avtab_datum avdatum = {};
		/*
     * AUDITDENY, aka DONTAUDIT, are &= assigned, versus |= for
     * others. Initialize the data accordingly.
     */
		if (key->specified & AVTAB_XPERMS) {
			avdatum.u.xperms = xperms;
		} else {
			avdatum.u.data =
				key->specified == AVTAB_AUDITDENY ? ~0U : 0U;
		}
		/* this is used to get the node - insertion is actually unique */
		node = avtab_insert_nonunique(&db->te_avtab, key, &avdatum);

		int grow_size = sizeof(struct avtab_key);
		grow_size += sizeof(struct avtab_datum);
		if (key->specified & AVTAB_XPERMS) {
			grow_size += sizeof(u8);
			grow_size += sizeof(u8);
			grow_size += sizeof(u32) *
				     ARRAY_SIZE(avdatum.u.xperms->perms.p);
		}
		db->len += grow_size;
	}

	return node;
}

static bool add_rule(struct policydb *db, const char *s, const char *t,
		     const char *c, const char *p, int effect, bool invert)
{
	struct type_datum *src = NULL, *tgt = NULL;
	struct class_datum *cls = NULL;
	struct perm_datum *perm = NULL;

	if (s) {
		src = symtab_search(&db->p_types, s);
		if (src == NULL) {
			pr_info("source type %s does not exist\n", s);
			return false;
		}
	}

	if (t) {
		tgt = symtab_search(&db->p_types, t);
		if (tgt == NULL) {
			pr_info("target type %s does not exist\n", t);
			return false;
		}
	}

	if (c) {
		cls = symtab_search(&db->p_classes, c);
		if (cls == NULL) {
			pr_info("class %s does not exist\n", c);
			return false;
		}
	}

	if (p) {
		if (c == NULL) {
			pr_info("No class is specified, cannot add perm [%s] \n",
				p);
			return false;
		}

		perm = symtab_search(&cls->permissions, p);
		if (perm == NULL && cls->comdatum != NULL) {
			perm = symtab_search(&cls->comdatum->permissions, p);
		}
		if (perm == NULL) {
			pr_info("perm %s does not exist in class %s\n", p, c);
			return false;
		}
	}
	add_rule_raw(db, src, tgt, cls, perm, effect, invert);
	return true;
}

static void add_rule_raw(struct policydb *db, struct type_datum *src,
			 struct type_datum *tgt, struct class_datum *cls,
			 struct perm_datum *perm, int effect, bool invert)
{
	if (src == NULL) {
		struct hashtab_node *node;
		if (strip_av(effect, invert)) {
			ksu_hashtab_for_each(db->p_types.table, node)
			{
				add_rule_raw(db,
					     (struct type_datum *)node->datum,
					     tgt, cls, perm, effect, invert);
			};
		} else {
			ksu_hashtab_for_each(db->p_types.table, node)
			{
				struct type_datum *type =
					(struct type_datum *)(node->datum);
				if (type->attribute) {
					add_rule_raw(db, type, tgt, cls, perm,
						     effect, invert);
				}
			};
		}
	} else if (tgt == NULL) {
		struct hashtab_node *node;
		if (strip_av(effect, invert)) {
			ksu_hashtab_for_each(db->p_types.table, node)
			{
				add_rule_raw(db, src,
					     (struct type_datum *)node->datum,
					     cls, perm, effect, invert);
			};
		} else {
			ksu_hashtab_for_each(db->p_types.table, node)
			{
				struct type_datum *type =
					(struct type_datum *)(node->datum);
				if (type->attribute) {
					add_rule_raw(db, src, type, cls, perm,
						     effect, invert);
				}
			};
		}
	} else if (cls == NULL) {
		struct hashtab_node *node;
		ksu_hashtab_for_each(db->p_classes.table, node)
		{
			add_rule_raw(db, src, tgt,
				     (struct class_datum *)node->datum, perm,
				     effect, invert);
		}
	} else {
		struct avtab_key key;
		key.source_type = src->value;
		key.target_type = tgt->value;
		key.target_class = cls->value;
		key.specified = effect;

		struct avtab_node *node = get_avtab_node(db, &key, NULL);
		if (invert) {
			if (perm)
				node->datum.u.data &=
					~(1U << (perm->value - 1));
			else
				node->datum.u.data = 0U;
		} else {
			if (perm)
				node->datum.u.data |= 1U << (perm->value - 1);
			else
				node->datum.u.data = ~0U;
		}
	}
}

#define ioctl_driver(x) (x >> 8 & 0xFF)
#define ioctl_func(x) (x & 0xFF)

#define xperm_test(x, p) (1 & (p[x >> 5] >> (x & 0x1f)))
#define xperm_set(x, p) (p[x >> 5] |= (1 << (x & 0x1f)))
#define xperm_clear(x, p) (p[x >> 5] &= ~(1 << (x & 0x1f)))

static void add_xperm_rule_raw(struct policydb *db, struct type_datum *src,
			       struct type_datum *tgt, struct class_datum *cls,
			       uint16_t low, uint16_t high, int effect,
			       bool invert)
{
	if (src == NULL) {
		struct hashtab_node *node;
		ksu_hashtab_for_each(db->p_types.table, node)
		{
			struct type_datum *type =
				(struct type_datum *)(node->datum);
			if (type->attribute) {
				add_xperm_rule_raw(db, type, tgt, cls, low,
						   high, effect, invert);
			}
		};
	} else if (tgt == NULL) {
		struct hashtab_node *node;
		ksu_hashtab_for_each(db->p_types.table, node)
		{
			struct type_datum *type =
				(struct type_datum *)(node->datum);
			if (type->attribute) {
				add_xperm_rule_raw(db, src, type, cls, low,
						   high, effect, invert);
			}
		};
	} else if (cls == NULL) {
		struct hashtab_node *node;
		ksu_hashtab_for_each(db->p_classes.table, node)
		{
			add_xperm_rule_raw(db, src, tgt,
					   (struct class_datum *)(node->datum),
					   low, high, effect, invert);
		};
	} else {
		struct avtab_key key;
		key.source_type = src->value;
		key.target_type = tgt->value;
		key.target_class = cls->value;
		key.specified = effect;

		struct avtab_datum *datum;
		struct avtab_node *node;
		struct avtab_extended_perms xperms;

		memset(&xperms, 0, sizeof(xperms));
		if (ioctl_driver(low) != ioctl_driver(high)) {
			xperms.specified = AVTAB_XPERMS_IOCTLDRIVER;
			xperms.driver = 0;
		} else {
			xperms.specified = AVTAB_XPERMS_IOCTLFUNCTION;
			xperms.driver = ioctl_driver(low);
		}
		int i;
		if (xperms.specified == AVTAB_XPERMS_IOCTLDRIVER) {
			for (i = ioctl_driver(low); i <= ioctl_driver(high);
			     ++i) {
				if (invert)
					xperm_clear(i, xperms.perms.p);
				else
					xperm_set(i, xperms.perms.p);
			}
		} else {
			for (i = ioctl_func(low); i <= ioctl_func(high); ++i) {
				if (invert)
					xperm_clear(i, xperms.perms.p);
				else
					xperm_set(i, xperms.perms.p);
			}
		}

		node = get_avtab_node(db, &key, &xperms);
		if (!node) {
			pr_warn("add_xperm_rule_raw cannot found node!\n");
			return;
		}
		datum = &node->datum;

		if (datum->u.xperms == NULL) {
			datum->u.xperms =
				(struct avtab_extended_perms *)(kmalloc(
					sizeof(xperms), GFP_KERNEL));
			if (!datum->u.xperms) {
				pr_err("alloc xperms failed\n");
				return;
			}
			memcpy(datum->u.xperms, &xperms, sizeof(xperms));
		}
	}
}

static bool add_xperm_rule(struct policydb *db, const char *s, const char *t,
			   const char *c, const char *range, int effect,
			   bool invert)
{
	struct type_datum *src = NULL, *tgt = NULL;
	struct class_datum *cls = NULL;

	if (s) {
		src = symtab_search(&db->p_types, s);
		if (src == NULL) {
			pr_info("source type %s does not exist\n", s);
			return false;
		}
	}

	if (t) {
		tgt = symtab_search(&db->p_types, t);
		if (tgt == NULL) {
			pr_info("target type %s does not exist\n", t);
			return false;
		}
	}

	if (c) {
		cls = symtab_search(&db->p_classes, c);
		if (cls == NULL) {
			pr_info("class %s does not exist\n", c);
			return false;
		}
	}

	u16 low, high;

	if (range) {
		if (strchr(range, '-')) {
			sscanf(range, "%hx-%hx", &low, &high);
		} else {
			sscanf(range, "%hx", &low);
			high = low;
		}
	} else {
		low = 0;
		high = 0xFFFF;
	}

	add_xperm_rule_raw(db, src, tgt, cls, low, high, effect, invert);
	return true;
}

static bool add_type_rule(struct policydb *db, const char *s, const char *t,
			  const char *c, const char *d, int effect)
{
	struct type_datum *src, *tgt, *def;
	struct class_datum *cls;

	src = symtab_search(&db->p_types, s);
	if (src == NULL) {
		pr_info("source type %s does not exist\n", s);
		return false;
	}
	tgt = symtab_search(&db->p_types, t);
	if (tgt == NULL) {
		pr_info("target type %s does not exist\n", t);
		return false;
	}
	cls = symtab_search(&db->p_classes, c);
	if (cls == NULL) {
		pr_info("class %s does not exist\n", c);
		return false;
	}
	def = symtab_search(&db->p_types, d);
	if (def == NULL) {
		pr_info("default type %s does not exist\n", d);
		return false;
	}

	struct avtab_key key;
	key.source_type = src->value;
	key.target_type = tgt->value;
	key.target_class = cls->value;
	key.specified = effect;

	struct avtab_node *node = get_avtab_node(db, &key, NULL);
	node->datum.u.data = def->value;

	return true;
}

// 5.9.0 : static inline int hashtab_insert(struct hashtab *h, void *key, void
// *datum, struct hashtab_key_params key_params) 5.8.0: int
// hashtab_insert(struct hashtab *h, void *k, void *d);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
static u32 filenametr_hash(const void *k)
{
	const struct filename_trans_key *ft = k;
	unsigned long hash;
	unsigned int byte_num;
	unsigned char focus;

	hash = ft->ttype ^ ft->tclass;

	byte_num = 0;
	while ((focus = ft->name[byte_num++]))
		hash = partial_name_hash(focus, hash);
	return hash;
}

static int filenametr_cmp(const void *k1, const void *k2)
{
	const struct filename_trans_key *ft1 = k1;
	const struct filename_trans_key *ft2 = k2;
	int v;

	v = ft1->ttype - ft2->ttype;
	if (v)
		return v;

	v = ft1->tclass - ft2->tclass;
	if (v)
		return v;

	return strcmp(ft1->name, ft2->name);
}

static const struct hashtab_key_params filenametr_key_params = {
	.hash = filenametr_hash,
	.cmp = filenametr_cmp,
};
#endif

static bool add_filename_trans(struct policydb *db, const char *s,
			       const char *t, const char *c, const char *d,
			       const char *o)
{
	struct type_datum *src, *tgt, *def;
	struct class_datum *cls;

	src = symtab_search(&db->p_types, s);
	if (src == NULL) {
		pr_warn("source type %s does not exist\n", s);
		return false;
	}
	tgt = symtab_search(&db->p_types, t);
	if (tgt == NULL) {
		pr_warn("target type %s does not exist\n", t);
		return false;
	}
	cls = symtab_search(&db->p_classes, c);
	if (cls == NULL) {
		pr_warn("class %s does not exist\n", c);
		return false;
	}
	def = symtab_search(&db->p_types, d);
	if (def == NULL) {
		pr_warn("default type %s does not exist\n", d);
		return false;
	}

	struct filename_trans_key key;
	key.ttype = tgt->value;
	key.tclass = cls->value;
	key.name = (char *)o;

	struct filename_trans_datum *last = NULL;

	struct filename_trans_datum *trans =
		policydb_filenametr_search(db, &key);
	while (trans) {
		if (ebitmap_get_bit(&trans->stypes, src->value - 1)) {
			// Duplicate, overwrite existing data and return
			trans->otype = def->value;
			return true;
		}
		if (trans->otype == def->value)
			break;
		last = trans;
		trans = trans->next;
	}

	if (trans == NULL) {
		trans = (struct filename_trans_datum *)kcalloc(sizeof(*trans),
							       1, GFP_ATOMIC);
		struct filename_trans_key *new_key =
			(struct filename_trans_key *)kmalloc(sizeof(*new_key),
							     GFP_ATOMIC);
		*new_key = key;
		new_key->name = kstrdup(key.name, GFP_ATOMIC);
		trans->next = last;
		trans->otype = def->value;
		hashtab_insert(&db->filename_trans, new_key, trans,
			       filenametr_key_params);
	}

	db->compat_filename_trans_count++;
	return ebitmap_set_bit(&trans->stypes, src->value - 1, 1) == 0;
}

static bool add_genfscon(struct policydb *db, const char *fs_name,
			 const char *path, const char *context)
{
	return false;
}

static void *ksu_realloc(void *old, size_t new_size, size_t old_size)
{
	// we can't use krealloc, because it may be read-only
	void *new = kzalloc(new_size, GFP_ATOMIC);
	if (!new) {
		return NULL;
	}
	if (old_size) {
		memcpy(new, old, old_size);
	}
	// we can't use kfree, because it may be read-only
	// there maybe some leaks, maybe we can check ptr_write, but it's not a big deal
	// kfree(old);
	return new;
}

static bool add_type(struct policydb *db, const char *type_name, bool attr)
{
	struct type_datum *type = symtab_search(&db->p_types, type_name);
	if (type) {
		pr_warn("Type %s already exists\n", type_name);
		return true;
	}

	u32 value = ++db->p_types.nprim;
	type = (struct type_datum *)kzalloc(sizeof(struct type_datum),
					    GFP_ATOMIC);
	if (!type) {
		pr_err("add_type: alloc type_datum failed.\n");
		return false;
	}

	type->primary = 1;
	type->value = value;
	type->attribute = attr;

	char *key = kstrdup(type_name, GFP_ATOMIC);
	if (!key) {
		pr_err("add_type: alloc key failed.\n");
		return false;
	}

	if (symtab_insert(&db->p_types, key, type)) {
		pr_err("add_type: insert symtab failed.\n");
		return false;
	}

	struct ebitmap *new_type_attr_map_array =
		ksu_realloc(db->type_attr_map_array,
			    value * sizeof(struct ebitmap),
			    (value - 1) * sizeof(struct ebitmap));

	if (!new_type_attr_map_array) {
		pr_err("add_type: alloc type_attr_map_array failed\n");
		return false;
	}

	struct type_datum **new_type_val_to_struct =
		ksu_realloc(db->type_val_to_struct,
			    sizeof(*db->type_val_to_struct) * value,
			    sizeof(*db->type_val_to_struct) * (value - 1));

	if (!new_type_val_to_struct) {
		pr_err("add_type: alloc type_val_to_struct failed\n");
		return false;
	}

	char **new_val_to_name_types =
		ksu_realloc(db->sym_val_to_name[SYM_TYPES],
			    sizeof(char *) * value,
			    sizeof(char *) * (value - 1));
	if (!new_val_to_name_types) {
		pr_err("add_type: alloc val_to_name failed\n");
		return false;
	}

	db->type_attr_map_array = new_type_attr_map_array;
	ebitmap_init(&db->type_attr_map_array[value - 1]);
	ebitmap_set_bit(&db->type_attr_map_array[value - 1], value - 1, 1);

	db->type_val_to_struct = new_type_val_to_struct;
	db->type_val_to_struct[value - 1] = type;

	db->sym_val_to_name[SYM_TYPES] = new_val_to_name_types;
	db->sym_val_to_name[SYM_TYPES][value - 1] = key;

	int i;
	for (i = 0; i < db->p_roles.nprim; ++i) {
		ebitmap_set_bit(&db->role_val_to_struct[i]->types, value - 1,
				1);
	}

	return true;
}

static bool set_type_state(struct policydb *db, const char *type_name,
			   bool permissive)
{
	struct type_datum *type;
	if (type_name == NULL) {
		struct hashtab_node *node;
		ksu_hashtab_for_each(db->p_types.table, node)
		{
			type = (struct type_datum *)(node->datum);
			if (ebitmap_set_bit(&db->permissive_map, type->value,
					    permissive))
				pr_info("Could not set bit in permissive map\n");
		};
	} else {
		type = (struct type_datum *)symtab_search(&db->p_types,
							  type_name);
		if (type == NULL) {
			pr_info("type %s does not exist\n", type_name);
			return false;
		}
		if (ebitmap_set_bit(&db->permissive_map, type->value,
				    permissive)) {
			pr_info("Could not set bit in permissive map\n");
			return false;
		}
	}
	return true;
}

static void add_typeattribute_raw(struct policydb *db, struct type_datum *type,
				  struct type_datum *attr)
{
	struct ebitmap *sattr = &db->type_attr_map_array[type->value - 1];
	ebitmap_set_bit(sattr, attr->value - 1, 1);

	struct hashtab_node *node;
	struct constraint_node *n;
	struct constraint_expr *e;
	ksu_hashtab_for_each(db->p_classes.table, node)
	{
		struct class_datum *cls = (struct class_datum *)(node->datum);
		for (n = cls->constraints; n; n = n->next) {
			for (e = n->expr; e; e = e->next) {
				if (e->expr_type == CEXPR_NAMES &&
				    ebitmap_get_bit(&e->type_names->types,
						    attr->value - 1)) {
					ebitmap_set_bit(&e->names,
							type->value - 1, 1);
				}
			}
		}
	};
}

static bool add_typeattribute(struct policydb *db, const char *type,
			      const char *attr)
{
	struct type_datum *type_d = symtab_search(&db->p_types, type);
	if (type_d == NULL) {
		pr_info("type %s does not exist\n", type);
		return false;
	} else if (type_d->attribute) {
		pr_info("type %s is an attribute\n", attr);
		return false;
	}

	struct type_datum *attr_d = symtab_search(&db->p_types, attr);
	if (attr_d == NULL) {
		pr_info("attribute %s does not exist\n", type);
		return false;
	} else if (!attr_d->attribute) {
		pr_info("type %s is not an attribute \n", attr);
		return false;
	}

	add_typeattribute_raw(db, type_d, attr_d);
	return true;
}

//////////////////////////////////////////////////////////////////////////

// Operation on types
bool ksu_type(struct policydb *db, const char *name, const char *attr)
{
	return add_type(db, name, false) && add_typeattribute(db, name, attr);
}

bool ksu_attribute(struct policydb *db, const char *name)
{
	return add_type(db, name, true);
}

bool ksu_permissive(struct policydb *db, const char *type)
{
	return set_type_state(db, type, true);
}

bool ksu_enforce(struct policydb *db, const char *type)
{
	return set_type_state(db, type, false);
}

bool ksu_typeattribute(struct policydb *db, const char *type, const char *attr)
{
	return add_typeattribute(db, type, attr);
}

bool ksu_exists(struct policydb *db, const char *type)
{
	return symtab_search(&db->p_types, type) != NULL;
}

// Access vector rules
bool ksu_allow(struct policydb *db, const char *src, const char *tgt,
	       const char *cls, const char *perm)
{
	return add_rule(db, src, tgt, cls, perm, AVTAB_ALLOWED, false);
}

bool ksu_deny(struct policydb *db, const char *src, const char *tgt,
	      const char *cls, const char *perm)
{
	return add_rule(db, src, tgt, cls, perm, AVTAB_ALLOWED, true);
}

bool ksu_auditallow(struct policydb *db, const char *src, const char *tgt,
		    const char *cls, const char *perm)
{
	return add_rule(db, src, tgt, cls, perm, AVTAB_AUDITALLOW, false);
}
bool ksu_dontaudit(struct policydb *db, const char *src, const char *tgt,
		   const char *cls, const char *perm)
{
	return add_rule(db, src, tgt, cls, perm, AVTAB_AUDITDENY, true);
}

// Extended permissions access vector rules
bool ksu_allowxperm(struct policydb *db, const char *src, const char *tgt,
		    const char *cls, const char *range)
{
	return add_xperm_rule(db, src, tgt, cls, range, AVTAB_XPERMS_ALLOWED,
			      false);
}

bool ksu_auditallowxperm(struct policydb *db, const char *src, const char *tgt,
			 const char *cls, const char *range)
{
	return add_xperm_rule(db, src, tgt, cls, range, AVTAB_XPERMS_AUDITALLOW,
			      false);
}

bool ksu_dontauditxperm(struct policydb *db, const char *src, const char *tgt,
			const char *cls, const char *range)
{
	return add_xperm_rule(db, src, tgt, cls, range, AVTAB_XPERMS_DONTAUDIT,
			      false);
}

// Type rules
bool ksu_type_transition(struct policydb *db, const char *src, const char *tgt,
			 const char *cls, const char *def, const char *obj)
{
	if (obj) {
		return add_filename_trans(db, src, tgt, cls, def, obj);
	} else {
		return add_type_rule(db, src, tgt, cls, def, AVTAB_TRANSITION);
	}
}

bool ksu_type_change(struct policydb *db, const char *src, const char *tgt,
		     const char *cls, const char *def)
{
	return add_type_rule(db, src, tgt, cls, def, AVTAB_CHANGE);
}

bool ksu_type_member(struct policydb *db, const char *src, const char *tgt,
		     const char *cls, const char *def)
{
	return add_type_rule(db, src, tgt, cls, def, AVTAB_MEMBER);
}

// File system labeling
bool ksu_genfscon(struct policydb *db, const char *fs_name, const char *path,
		  const char *ctx)
{
	return add_genfscon(db, fs_name, path, ctx);
}

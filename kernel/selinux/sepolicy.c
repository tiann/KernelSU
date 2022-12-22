#include "sepolicy.h"

// Invert is adding rules for auditdeny; in other cases, invert is removing rules
#define strip_av(effect, invert) ((effect == AVTAB_AUDITDENY) == !invert)

#define hash_for_each(node_ptr, n_slot, cur)                     \
    int i;                                                      \
    for (i = 0; i < n_slot; ++i)                                 \
        for (cur = node_ptr[i]; cur; cur = cur->next)                \

#define hashtab_for_each(htab, cur)                  \
    hash_for_each(htab.htable, htab.size, cur)    \

#define avtab_for_each(avtab, cur)                   \
    hash_for_each(avtab.htable, avtab.nslot, cur); \

static bool is_redundant(struct avtab_node* node) {
    switch (node->key.specified) {
    case AVTAB_AUDITDENY:
        return node->datum.u.data == ~0U;
    case AVTAB_XPERMS:
        return node->datum.u.xperms == NULL;
    default:
        return node->datum.u.data == 0U;
    }
}

struct avtab_node* get_avtab_node(struct policydb* db, struct avtab_key *key, struct avtab_extended_perms *xperms) {
    struct avtab_node* node;

    /* AVTAB_XPERMS entries are not necessarily unique */
    if (key->specified & AVTAB_XPERMS) {
        bool match = false;
        node = avtab_search_node(&db->te_avtab, key);
        while (node) {
            if ((node->datum.u.xperms->specified == xperms->specified) &&
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
        avdatum.u.data = key->specified == AVTAB_AUDITDENY ? ~0U : 0U;
        /* this is used to get the node - insertion is actually unique */
        node = avtab_insert_nonunique(&db->te_avtab, key, &avdatum);
    }

    return node;
}

bool add_rule(struct policydb* db, const char *s, const char *t, const char *c, const char *p, int effect, bool invert) {
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
            pr_info("No class is specified, cannot add perm [%s] \n", p);
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

void add_rule_raw(struct policydb* db, struct type_datum *src, struct type_datum *tgt, struct class_datum *cls, struct perm_datum *perm, int effect, bool invert) {
    if (src == NULL) {
        struct hashtab_node* node;
        if (strip_av(effect, invert)) {
            hashtab_for_each(db->p_types.table, node) {
                add_rule_raw(db, (struct type_datum*)node->datum, tgt, cls, perm, effect, invert);
            };
        } else {
            hashtab_for_each(db->p_types.table, node) {
                struct type_datum* type = (struct type_datum*)(node->datum);
                if (type->attribute) {
                    add_rule_raw(db, type, tgt, cls, perm, effect, invert);
                }
            };
        }
    } else if (tgt == NULL) {
        struct hashtab_node* node;
        if (strip_av(effect, invert)) {
            hashtab_for_each(db->p_types.table, node) {
                add_rule_raw(db, src, (struct type_datum*)node->datum, cls, perm, effect, invert);
            };
        } else {
            hashtab_for_each(db->p_types.table, node) {
                struct type_datum* type = (struct type_datum*)(node->datum);
                if (type->attribute) {
                    add_rule_raw(db, src, type, cls, perm, effect, invert);
                }
            };
        }
    } else if (cls == NULL) {
        struct hashtab_node* node;
        hashtab_for_each(db->p_classes.table, node) {
            add_rule_raw(db, src, tgt, (struct class_datum*)node->datum, perm, effect, invert);
        }
    } else {
        struct avtab_key key;
        key.source_type = src->value;
        key.target_type = tgt->value;
        key.target_class = cls->value;
        key.specified = effect;

        struct avtab_node* node = get_avtab_node(db, &key, NULL);
        if (invert) {
            if (perm)
                node->datum.u.data &= ~(1U << (perm->value - 1));
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

void add_xperm_rule_raw(struct type_datum *src, struct type_datum *tgt,
        struct class_datum *cls, uint16_t low, uint16_t high, int effect, bool invert) {
}
bool add_xperm_rule(const char *s, const char *t, const char *c, const char *range, int effect, bool invert) {
    return false;
}

bool add_type_rule(struct policydb* db, const char *s, const char *t, const char *c, const char *d, int effect) {
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

    struct avtab_node* node = get_avtab_node(db, &key, NULL);
    node->datum.u.data = def->value;

    return true;
}

bool add_filename_trans(const char *s, const char *t, const char *c, const char *d, const char *o) {
    return false;
}

bool add_genfscon(const char *fs_name, const char *path, const char *context) {
    return false;
}

bool add_type(struct policydb* db, const char *type_name, bool attr) {
    return false;
}

bool set_type_state(struct policydb* db, const char *type_name, bool permissive) {
    struct type_datum *type;
    if (type_name == NULL) {
        struct hashtab_node* node;
        hashtab_for_each(db->p_types.table, node) {
            type = (struct type_datum *)(node->datum);
            if (ebitmap_set_bit(&db->permissive_map, type->value, permissive))
                pr_info("Could not set bit in permissive map\n");
        };
    } else {
        type = (struct type_datum *) symtab_search(&db->p_types, type_name);
        if (type == NULL) {
            pr_info("type %s does not exist\n", type_name);
            return false;
        }
        if (ebitmap_set_bit(&db->permissive_map, type->value, permissive)) {
            pr_info("Could not set bit in permissive map\n");
            return false;
        }
    }
    return true;
}

void add_typeattribute_raw(struct policydb* db, struct type_datum *type, struct type_datum *attr) {
    ebitmap_set_bit(&db->type_attr_map_array[type->value - 1], attr->value - 1, 1);

    struct hashtab_node* node;
    struct constraint_node* n;
    struct constraint_expr* e;
    hashtab_for_each(db->p_classes.table, node) {
        struct class_datum* cls = (struct class_datum*)(node->datum);
        for (n = cls->constraints; n ; n = n->next) {
            for (e = n->expr; e; e = e->next) {
                if (e->expr_type == CEXPR_NAMES &&
                    ebitmap_get_bit(&e->type_names->types, attr->value - 1)) {
                    ebitmap_set_bit(&e->names, type->value - 1, 1);
                }
            }
        }
    };
}

bool add_typeattribute(struct policydb* db, const char *type, const char *attr) {
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

// Operation on types
bool type(struct policydb* db, const char* name, const char* attr) {
    return add_type(db, name, false) && add_typeattribute(db, name, attr);
}

bool attribute(struct policydb* db, const char* name) {
    return add_type(db, name, true);
}

bool permissive(struct policydb* db, const char* type) {
    return set_type_state(db, type, true);
}

bool enforce(struct policydb* db, const char* type) {
    return set_type_state(db, type, false);
}

bool typeattribute(struct policydb* db, const char* type, const char* attr) {
    return add_typeattribute(db, type, attr);
}

bool exists(struct policydb* db, const char* type) {
    return symtab_search(&db->p_types, type) != NULL;
}

// Access vector rules
bool allow(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm) {
    return add_rule(db, src, tgt, cls, perm, AVTAB_ALLOWED, false);
}

bool deny(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm) {
    return add_rule(db, src, tgt, cls, perm, AVTAB_ALLOWED, true);
}

bool auditallow(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm) {
    return add_rule(db, src, tgt, cls, perm, AVTAB_AUDITALLOW, false);
}
bool dontaudit(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm) {
    return add_rule(db, src, tgt, cls, perm, AVTAB_AUDITDENY, true);
}

// Extended permissions access vector rules
bool allowxperm(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* range) {
    return add_xperm_rule(src, tgt, cls, range, AVTAB_XPERMS_ALLOWED, false);
}

bool auditallowxperm(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* range) {
    return add_xperm_rule(src, tgt, cls, range, AVTAB_XPERMS_AUDITALLOW, false);
}

bool dontauditxperm(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* range) {
    return add_xperm_rule(src, tgt, cls, range, AVTAB_XPERMS_DONTAUDIT, false);
}

// Type rules
bool type_transition(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* def, const char* obj) {
    return false;
}

bool type_change(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* def) {
    return false;
}
bool type_member(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* def) {
    return false;
}

// File system labeling
bool genfscon(struct policydb* db, const char* fs_name, const char* path, const char* ctx) {
    return false;
}
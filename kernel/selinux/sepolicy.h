#ifndef __KSU_H_SEPOLICY
#define __KSU_H_SEPOLICY

#include <linux/types.h>
#include <ss/sidtab.h>
#include <ss/services.h>
#include <objsec.h>

// Operation on types
bool type(struct policydb* db, const char* name, const char* attr);
bool attribute(struct policydb* db, const char* name);
bool permissive(struct policydb* db, const char* type);
bool enforce(struct policydb* db, const char* type);
bool typeattribute(struct policydb* db, const char* type, const char* attr);
bool exists(struct policydb* db, const char* type);

// Access vector rules
bool allow(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm);
bool deny(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm);
bool auditallow(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm);
bool dontaudit(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* perm);

// Extended permissions access vector rules
bool allowxperm(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* range);
bool auditallowxperm(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* range);
bool dontauditxperm(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* range);

// Type rules
bool type_transition(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* def, const char* obj);
bool type_change(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* def);
bool type_member(struct policydb* db, const char* src, const char* tgt, const char* cls, const char* def);

// File system labeling
bool genfscon(struct policydb* db, const char* fs_name, const char* path, const char* ctx);


//////////////////////////////////////////////////////
// Internal use
//////////////////////////////////////////////////////

struct avtab_node* get_avtab_node(struct policydb* db, struct avtab_key *key, struct avtab_extended_perms *xperms);

bool add_rule(struct policydb* db, const char *s, const char *t, const char *c, const char *p, int effect, bool invert);
void add_rule_raw(struct policydb* db, struct type_datum *src, struct type_datum *tgt, struct class_datum *cls, struct perm_datum *perm, int effect, bool invert);

void add_xperm_rule_raw(struct policydb* db, struct type_datum *src, struct type_datum *tgt,
        struct class_datum *cls, uint16_t low, uint16_t high, int effect, bool invert);
bool add_xperm_rule(struct policydb* db, const char *s, const char *t, const char *c, const char *range, int effect, bool invert);

bool add_type_rule(struct policydb* db, const char *s, const char *t, const char *c, const char *d, int effect);

bool add_filename_trans(const char *s, const char *t, const char *c, const char *d, const char *o);

bool add_genfscon(const char *fs_name, const char *path, const char *context);

bool add_type(struct policydb* db, const char *type_name, bool attr);

bool set_type_state(struct policydb* db, const char *type_name, bool permissive);

void add_typeattribute_raw(struct policydb* db, struct type_datum *type, struct type_datum *attr);

bool add_typeattribute(struct policydb* db, const char *type, const char *attr);

#endif

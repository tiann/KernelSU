#ifndef __KSU_H_ALLOWLIST
#define __KSU_H_ALLOWLIST

#include "linux/types.h"
#include "linux/capability.h"

extern const struct perm_data NO_PERM;

extern const struct perm_data ALL_PERM;
struct perm_data {
	bool allow;
	kernel_cap_t cap;
};

struct perm_uid_data {
	uid_t uid;
	struct perm_data data;
};

void ksu_allowlist_init(void);

void ksu_allowlist_exit(void);

bool ksu_load_allow_list(void);

void ksu_show_allow_list(void);

struct perm_data ksu_get_uid_data(uid_t uid);

bool ksu_set_uid_data(uid_t uid, struct perm_data data, bool persist);

bool ksu_get_uid_data_list(struct perm_uid_data *array, int *length, bool kbuf);

unsigned int ksu_get_uid_data_list_count(void);

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, void *), void *data);

#endif
#ifndef __KSU_H_ALLOWLIST
#define __KSU_H_ALLOWLIST

#include "linux/types.h"
#include "ksu.h"

void ksu_allowlist_init(void);

void ksu_allowlist_exit(void);

bool ksu_load_allow_list(void);

void ksu_show_allow_list(void);

bool ksu_is_allow_uid(uid_t uid);

bool ksu_get_allow_list(int *array, int *length, bool allow);

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, void *), void *data);

bool ksu_get_app_profile(struct app_profile *, bool query_by_uid);
bool ksu_set_app_profile(struct app_profile *, bool persist);

bool ksu_is_uid_should_umount(uid_t uid);
#endif
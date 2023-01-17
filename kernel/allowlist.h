#ifndef __KSU_H_ALLOWLIST
#define __KSU_H_ALLOWLIST

#include <linux/types.h>

bool ksu_allowlist_init();

bool ksu_allowlist_exit();

bool ksu_is_allow_uid(uid_t uid);

bool ksu_allow_uid(uid_t uid, bool allow);

bool ksu_get_allow_list(int *array, int *length, bool allow);

bool ksu_load_allow_list(void);

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, void *), void* data);

#endif
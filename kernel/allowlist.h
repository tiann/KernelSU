#ifndef __KSU_H_ALLOWLIST
#define __KSU_H_ALLOWLIST

bool ksu_allowlist_init();

bool ksu_allowlist_exit();

bool ksu_is_allow_uid(uid_t uid);

bool ksu_allow_uid(uid_t uid, bool allow);

bool ksu_get_allow_list(int *array, int *length, bool allow);

bool ksu_load_allow_list(void);

#endif
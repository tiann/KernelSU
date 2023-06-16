#ifndef __KSU_H_ALLOWLIST
#define __KSU_H_ALLOWLIST

#include "linux/types.h"
#include "ksu.h"

void ksu_allowlist_init(void);

void ksu_allowlist_exit(void);

bool ksu_load_allow_list(void);

void ksu_show_allow_list(void);

bool __ksu_is_allow_uid(uid_t uid);
#define ksu_is_allow_uid(uid) unlikely(__ksu_is_allow_uid(uid))

bool ksu_get_allow_list(int *array, int *length, bool allow);

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, void *), void *data);

bool ksu_get_app_profile(struct app_profile *);
bool ksu_set_app_profile(struct app_profile *, bool persist);

bool ksu_uid_should_umount(uid_t uid);
struct root_profile *ksu_get_root_profile(uid_t uid);
#endif

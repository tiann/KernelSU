#ifndef __KSU_H_ALLOWLIST
#define __KSU_H_ALLOWLIST

#include <linux/types.h>
#include <linux/uidgid.h>
#include "app_profile.h"

#define PER_USER_RANGE 100000
#define FIRST_APPLICATION_UID 10000
#define LAST_APPLICATION_UID 19999
#define FIRST_ISOLATED_UID 99000
#define LAST_ISOLATED_UID 99999

void ksu_allowlist_init(void);

void ksu_allowlist_exit(void);

void ksu_load_allow_list(void);

void ksu_show_allow_list(void);

// Check if the uid is in allow list
bool __ksu_is_allow_uid(uid_t uid);
#define ksu_is_allow_uid(uid) unlikely(__ksu_is_allow_uid(uid))

// Check if the uid is in allow list, or current is ksu domain root
bool __ksu_is_allow_uid_for_current(uid_t uid);
#define ksu_is_allow_uid_for_current(uid)                                      \
    unlikely(__ksu_is_allow_uid_for_current(uid))

bool ksu_get_allow_list(int *array, u16 length, u16 *out_length, u16 *out_total,
                        bool allow);

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, char *, void *),
                         void *data);
void ksu_persistent_allow_list();

bool ksu_get_app_profile(struct app_profile *);
int ksu_set_app_profile(struct app_profile *);

bool ksu_uid_should_umount(uid_t uid);
void ksu_get_root_profile(uid_t uid, struct root_profile *);

static inline bool is_appuid(uid_t uid)
{
    uid_t appid = uid % PER_USER_RANGE;
    return appid >= FIRST_APPLICATION_UID && appid <= LAST_APPLICATION_UID;
}

static inline bool is_isolated_process(uid_t uid)
{
    uid_t appid = uid % PER_USER_RANGE;
    return appid >= FIRST_ISOLATED_UID && appid <= LAST_ISOLATED_UID;
}
#endif

#ifndef __KSU_H_KSU_MANAGER
#define __KSU_H_KSU_MANAGER

#include <linux/cred.h>
#include <linux/types.h>
#include "allowlist.h"

#define KSU_INVALID_APPID -1

extern uid_t ksu_manager_appid; // DO NOT DIRECT USE

static inline bool ksu_is_manager_appid_valid()
{
    return ksu_manager_appid != KSU_INVALID_APPID;
}

static inline bool is_manager()
{
    return unlikely(ksu_manager_appid == current_uid().val % PER_USER_RANGE);
}

static inline bool is_uid_manager(uid_t uid)
{
    return unlikely(ksu_manager_appid == uid % PER_USER_RANGE);
}

static inline uid_t ksu_get_manager_appid()
{
    return ksu_manager_appid;
}

static inline void ksu_set_manager_appid(uid_t appid)
{
    ksu_manager_appid = appid;
}

static inline void ksu_invalidate_manager_uid()
{
    ksu_manager_appid = KSU_INVALID_APPID;
}

int ksu_observer_init(void);
#endif

#ifndef __KSU_H_KSU
#define __KSU_H_KSU

#include "linux/uidgid.h"
#define KERNEL_SU_VERSION 9

#define KERNEL_SU_OPTION 0xDEADBEEF

#define CMD_GRANT_ROOT 0

#define CMD_BECOME_MANAGER 1
#define CMD_GET_VERSION 2
#define CMD_ALLOW_SU 3
#define CMD_DENY_SU 4
#define CMD_GET_ALLOW_LIST 5
#define CMD_GET_DENY_LIST 6

#define INVALID_UID -1

extern uid_t ksu_manager_uid;

static inline bool ksu_is_manager_uid_valid() {
    return ksu_manager_uid != INVALID_UID;
}

static inline uid_t ksu_get_manager_uid() {
    return ksu_manager_uid;
}

static inline void ksu_set_manager_uid(uid_t uid) {
    ksu_manager_uid = uid;
}

static inline void ksu_invalidate_manager_uid() {
    ksu_manager_uid = INVALID_UID;
}

#endif
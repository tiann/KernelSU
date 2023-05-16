//
// Created by weishu on 2022/12/9.
//

#include <sys/prctl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "ksu.h"

#define KERNEL_SU_OPTION 0xDEADBEEF

#define CMD_GRANT_ROOT 0

#define CMD_BECOME_MANAGER 1
#define CMD_GET_VERSION 2
#define CMD_ALLOW_SU 3
#define CMD_DENY_SU 4
#define CMD_GET_SU_LIST 5
#define CMD_GET_DENY_LIST 6
#define CMD_CHECK_SAFEMODE 9

#define CMD_GET_WORK_MODE 10
#define CMD_SET_WORK_MODE 11
#define CMD_IN_ALLOW_LIST 12
#define CMD_IN_DENY_LIST 13
#define CMD_ADD_ALLOW_LIST 14
#define CMD_REMOVE_ALLOW_LIST 15
#define CMD_ADD_DENY_LIST 16
#define CMD_REMOVE_DENY_LIST 17

static bool ksuctl(int cmd, void* arg1, void* arg2) {
    int32_t result = 0;
    prctl(KERNEL_SU_OPTION, cmd, arg1, arg2, &result);
    return result == KERNEL_SU_OPTION;
}

bool become_manager(const char* pkg) {
    char param[128];
    uid_t uid = getuid();
    uint32_t userId = uid / 100000;
    if (userId == 0) {
        sprintf(param, "/data/data/%s", pkg);
    } else {
        snprintf(param, sizeof(param), "/data/user/%d/%s", userId, pkg);
    }

    return ksuctl(CMD_BECOME_MANAGER, param, nullptr);
}

int get_version() {
    int32_t version = -1;
    if (ksuctl(CMD_GET_VERSION, &version, nullptr)) {
        return version;
    }
    return version;
}

bool allow_su(int uid, bool allow) {
    int cmd = allow ? CMD_ALLOW_SU : CMD_DENY_SU;
    return ksuctl(cmd, (void*) uid, nullptr);
}

bool get_allow_list(int *uids, int *size) {
    return ksuctl(CMD_GET_SU_LIST, uids, size);
}

bool get_deny_list(int *uids, int *size) {
    return ksuctl(CMD_GET_DENY_LIST, uids, size);
}

bool is_safe_mode() {
    return ksuctl(CMD_CHECK_SAFEMODE, nullptr, nullptr);
}

bool is_allowlist_mode() {
    int32_t mode = -1;
    ksuctl(CMD_GET_WORK_MODE, &mode, nullptr);
    // for kernel that doesn't support allowlist mode, return -1 and it is always allowlist mode
    return mode <= 0;
}

bool set_allowlist_mode(bool allowlist_mode) {
    int32_t mode = allowlist_mode ? 0 : 1;
    return ksuctl(CMD_SET_WORK_MODE, &mode, nullptr);
}

bool is_in_allow_list(int uid) {
    return ksuctl(CMD_IN_ALLOW_LIST, &uid, nullptr);
}

bool is_in_deny_list(int uid) {
    return ksuctl(CMD_IN_DENY_LIST, &uid, nullptr);
}

bool add_to_allow_list(int uid) {
    return ksuctl(CMD_ADD_ALLOW_LIST, &uid, nullptr);
}

bool remove_from_allow_list(int uid) {
    return ksuctl(CMD_REMOVE_ALLOW_LIST, &uid, nullptr);
}

bool add_to_deny_list(int uid) {
    return ksuctl(CMD_ADD_DENY_LIST, &uid, nullptr);
}

bool remove_from_deny_list(int uid) {
    return ksuctl(CMD_REMOVE_DENY_LIST, &uid, nullptr);
}
//
// Created by weishu on 2022/12/9.
//

#include <sys/prctl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "ksu.h"

#define KERNEL_SU_OPTION 0xDEADBEEF

#define CMD_GRANT_ROOT 0

#define CMD_BECOME_MANAGER 1
#define CMD_GET_VERSION 2
#define CMD_ALLOW_SU 3
#define CMD_DENY_SU 4
#define CMD_GET_ALLOW_LIST 5
#define CMD_GET_DENY_LIST 6
#define CMD_CHECK_SAFEMODE 9

static bool ksuctl(int cmd, void* arg1, void* arg2) {
    int32_t result = 0;
    prctl(KERNEL_SU_OPTION, cmd, arg1, arg2, &result);
    return result == KERNEL_SU_OPTION;
}

bool become_manager(const char* pkg) {
    char param[128];
    sprintf(param, "/data/data/%s", pkg);
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
    return ksuctl(CMD_GET_ALLOW_LIST, uids, size);
}

bool get_deny_list(int *uids, int *size) {
    return ksuctl(CMD_GET_DENY_LIST, uids, size);
}

bool is_safe_mode() {
    return ksuctl(CMD_CHECK_SAFEMODE, nullptr, nullptr);
}
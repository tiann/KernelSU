//
// Created by weishu on 2022/12/9.
//

#include <sys/prctl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

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

#define CMD_GET_APP_PROFILE 10
#define CMD_SET_APP_PROFILE 11

#define CMD_IS_UID_GRANTED_ROOT 12
#define CMD_IS_UID_SHOULD_UMOUNT 13

static bool ksuctl(int cmd, void* arg1, void* arg2) {
    int32_t result = 0;
    prctl(KERNEL_SU_OPTION, cmd, arg1, arg2, &result);
    return result == KERNEL_SU_OPTION;
}
#include <android/log.h>
#define LOG_TAG "KernelSU"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

bool become_manager(const char* pkg) {
    char addr[128];
//    uid_t uid = getuid();
//    uint32_t userId = uid / 100000;
//    if (userId == 0) {
//        sprintf(param, "/data/data/%s", pkg);
//    } else {
//        snprintf(param, sizeof(param), "/data/user/%d/%s", userId, pkg);
//    }

    int page = getpagesize();
//    void *addr = mmap(nullptr, page, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    madvise(addr, page, MADV_PAGEOUT);

    bool result = ksuctl(CMD_BECOME_MANAGER, addr, nullptr);
    bool found = false;
    mincore(addr, page, reinterpret_cast<unsigned char *>(&found));
//    munmap(addr, page);
    LOGD("found: %d\n", found);

    return result;
}

int get_version() {
    int32_t version = -1;
    if (ksuctl(CMD_GET_VERSION, &version, nullptr)) {
        return version;
    }
    return version;
}

bool get_allow_list(int *uids, int *size) {
    return ksuctl(CMD_GET_SU_LIST, uids, size);
}

bool is_safe_mode() {
    return ksuctl(CMD_CHECK_SAFEMODE, nullptr, nullptr);
}

bool uid_should_umount(int uid) {
    bool should;
    return ksuctl(CMD_IS_UID_SHOULD_UMOUNT, reinterpret_cast<void*>(uid), &should) && should;
}

bool set_app_profile(const app_profile *profile) {
    return ksuctl(CMD_SET_APP_PROFILE, (void*) profile, nullptr);
}

bool get_app_profile(p_key_t key, app_profile *profile) {
    return ksuctl(CMD_GET_APP_PROFILE, (void*) profile, nullptr);
}

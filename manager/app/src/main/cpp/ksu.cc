//
// Created by weishu on 2022/12/9.
//

#include <sys/prctl.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "ksu.h"

#define KERNEL_SU_OPTION 0xDEADBEEF

#define CMD_GRANT_ROOT 100
#define CMD_GET_VERSION 101
#define CMD_REPORT_EVENT 102

// require manager
#define CMD_BECOME_MANAGER 200
#define CMD_SET_UID_DATA 201
#define CMD_GET_UID_DATA 202
#define CMD_COUNT_UID_DATA 203
#define CMD_LIST_UID_DATA 204

struct perm_data {
    bool allow;
    uint32_t cap1;
    uint32_t cap2;
};

struct perm_uid_data {
    uint32_t uid;
    bool allow;
    uint32_t cap1;
    uint32_t cap2;
};

const perm_data NO_PERM = {
        .allow = false,
        .cap1 = 0,
        .cap2 = 0,
};

const perm_data ALL_PERM = {
        .allow = true,
        .cap1 = 0xFFFFFFFF,
        .cap2 = 0x1FF,
};

static bool ksuctl(int cmd, void* arg1, void* arg2) {
    int32_t result = 0;
    prctl(KERNEL_SU_OPTION, cmd, arg1, arg2, &result);
    return result == KERNEL_SU_OPTION;
}

bool become_manager(const char* pkg) {
    char param[128];
    snprintf(param,128, "/data/data/%s", pkg);
    return ksuctl(CMD_BECOME_MANAGER, (void*)param, nullptr);
}

int get_version() {
    int32_t version = -1;
    if (ksuctl(CMD_GET_VERSION, &version, nullptr)) {
        return version;
    }
    return version;
}

bool allow_su(int uid, bool allow) {
    return ksuctl(CMD_SET_UID_DATA, (void*)uid, (void*)(allow ? &ALL_PERM : &NO_PERM));
}

bool get_allow_list(int *uids, int *size) {
    perm_uid_data data[128];
    bzero(data, sizeof(data));
    unsigned int len = 128;
    bool ret = ksuctl(CMD_LIST_UID_DATA, data, &len);
    for (int i = 0; i < len; ++i) {
        if(data[i].allow){
            uids[i] = data[i].uid;
        }
    }
    *size = len;
    return ret;
}

bool get_deny_list(int *uids, int *size) {
    perm_uid_data data[128];
    unsigned int len = 128;
    bool ret = ksuctl(CMD_LIST_UID_DATA, data, &len);
    for (int i = 0; i < len; ++i) {
        if(!data[i].allow){
            uids[i] = data[i].uid;
        }
    }
    *size = len;
    return ret;
}
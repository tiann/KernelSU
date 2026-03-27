//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

#include <cstdint>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <utility>

#include "uapi/supercall.h"
#include "uapi/app_profile.h"
#include "uapi/feature.h"
#include "uapi/selinux.h"

uint32_t get_version();

bool uid_should_umount(int uid);

bool is_safe_mode();

bool is_lkm_mode();

bool is_late_load_mode();

bool is_manager();

bool is_pr_build();

using p_key_t = char[KSU_MAX_PACKAGE_NAME];

bool set_app_profile(const app_profile *profile);

int get_app_profile(app_profile *profile);

// Su compat
bool set_su_enabled(bool enabled);

bool is_su_enabled();

// Kernel umount
bool set_kernel_umount_enabled(bool enabled);

bool is_kernel_umount_enabled();

bool get_allow_list(struct ksu_new_get_allow_list_cmd *);

inline std::pair<int, int> legacy_get_info() {
    int32_t version = -1;
    int32_t flags = 0;
    int32_t result = 0;
    prctl(0xDEADBEEF, 2, &version, &flags, &result);
    return {version, flags};
}

#endif //KERNELSU_KSU_H

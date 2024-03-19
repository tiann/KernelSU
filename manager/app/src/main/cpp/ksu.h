//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

#include <linux/capability.h>

bool become_manager(const char *);

int get_version();

bool get_allow_list(int *uids, int *size);

bool uid_should_umount(int uid);

bool is_safe_mode();

bool is_lkm_mode();

#define KSU_APP_PROFILE_VER 2
#define KSU_MAX_PACKAGE_NAME 256
// NGROUPS_MAX for Linux is 65535 generally, but we only supports 32 groups.
#define KSU_MAX_GROUPS 32
#define KSU_SELINUX_DOMAIN 64

using p_key_t = char[KSU_MAX_PACKAGE_NAME];

struct root_profile {
    int32_t uid;
    int32_t gid;

    int32_t groups_count;
    int32_t groups[KSU_MAX_GROUPS];

    // kernel_cap_t is u32[2] for capabilities v3
    struct {
        uint64_t effective;
        uint64_t permitted;
        uint64_t inheritable;
    } capabilities;

    char selinux_domain[KSU_SELINUX_DOMAIN];

    int32_t namespaces;
};

struct non_root_profile {
    bool umount_modules;
};

struct app_profile {
    // It may be utilized for backward compatibility, although we have never explicitly made any promises regarding this.
    uint32_t version;

    // this is usually the package of the app, but can be other value for special apps
    char key[KSU_MAX_PACKAGE_NAME];
    int32_t current_uid;
    bool allow_su;

    union {
        struct {
            bool use_default;
            char template_name[KSU_MAX_PACKAGE_NAME];

            struct root_profile profile;
        } rp_config;

        struct {
            bool use_default;

            struct non_root_profile profile;
        } nrp_config;
    };
};

bool set_app_profile(const app_profile *profile);

bool get_app_profile(p_key_t key, app_profile *profile);

#endif //KERNELSU_KSU_H

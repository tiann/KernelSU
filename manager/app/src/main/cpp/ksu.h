//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

bool become_manager(const char*);

int get_version();

bool allow_su(int uid, bool allow);

bool get_allow_list(int *uids, int *size);

bool get_deny_list(int *uids, int *size);

bool is_safe_mode();

bool is_allowlist_mode();

bool set_allowlist_mode(bool allowlist_mode);

bool is_in_allow_list(int uid);

bool is_in_deny_list(int uid);

bool add_to_allow_list(int uid);

bool remove_from_allow_list(int uid);

bool add_to_deny_list(int uid);

bool remove_from_deny_list(int uid);

// NGROUPS_MAX for Linux is 65535 generally, but we only supports 32 groups.
#define KSU_MAX_GROUPS 32
#define KSU_SELINUX_DOMAIN 64

#define DEFAULT_ROOT_PROFILE_KEY 0
#define DEFAULT_NON_ROOT_PROFILE_KEY 9999 // This UID means NOBODY in Android

struct app_profile {

    int32_t key; // this is usually the uid of the app, but can be other value for special apps

    int32_t uid;
    int32_t gid;

    int32_t groups[KSU_MAX_GROUPS];
    int32_t groups_count;

    // kernel_cap_t is u32[2]
    uint64_t capabilities;
    char selinux_domain[KSU_SELINUX_DOMAIN];

    bool allow_su;
    bool mount_module;
};

bool set_app_profile(const app_profile *profile);

bool get_app_profile(int32_t key, app_profile *profile);

#endif //KERNELSU_KSU_H

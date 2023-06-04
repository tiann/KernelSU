//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

bool become_manager(const char *);

int get_version();

bool get_allow_list(int *uids, int *size);

bool uid_should_umount(int uid);

bool is_safe_mode();

#define KSU_APP_PROFILE_VER 1
#define KSU_MAX_PACKAGE_NAME 256
// NGROUPS_MAX for Linux is 65535 generally, but we only supports 32 groups.
#define KSU_MAX_GROUPS 32
#define KSU_SELINUX_DOMAIN 64

using p_key_t = char[KSU_MAX_PACKAGE_NAME];

struct app_profile {

    int32_t version;

    // this is usually the package of the app, but can be other value for special apps
    p_key_t key;
    int32_t current_uid;
    bool allow_su;

    union {
        struct {
            bool use_default;
            char template_name[KSU_MAX_PACKAGE_NAME];
            int32_t uid;
            int32_t gid;

            int32_t groups[KSU_MAX_GROUPS];
            int32_t groups_count;

            // kernel_cap_t is u32[2]
            int32_t capabilities[2];
            char selinux_domain[KSU_SELINUX_DOMAIN];

            int32_t namespaces;
        } root_profile;

        struct {
            bool use_default;
            bool umount_modules;
        } non_root_profile;
    };
};

bool set_app_profile(const app_profile *profile);

bool get_app_profile(p_key_t key, app_profile *profile);

#endif //KERNELSU_KSU_H

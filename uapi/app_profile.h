#ifndef __KSU_UAPI_APP_PROFILE_H
#define __KSU_UAPI_APP_PROFILE_H

#include <linux/types.h>

#define KSU_APP_PROFILE_VER 3
#define KSU_MAX_PACKAGE_NAME 256
/* NGROUPS_MAX for Linux is 65535 generally, but we only supports 32 groups. */
#define KSU_MAX_GROUPS 32
#define KSU_SELINUX_DOMAIN 64

struct root_profile {
    __s32 uid;
    __s32 gid;

    __u32 groups_count;
    __s32 groups[KSU_MAX_GROUPS];

    /* kernel_cap_t is u32[2] for capabilities v3 */
    struct {
        __u64 effective;
        __u64 permitted;
        __u64 inheritable;
    } capabilities;

    char selinux_domain[KSU_SELINUX_DOMAIN];

    __s32 namespaces;
};

struct non_root_profile {
    bool umount_modules;
};

struct app_profile {
    /*
     * It may be utilized for backward compatibility, although we have never
     * explicitly made any promises regarding this.
     */
    __u32 version;

    /* this is usually the package of the app, but can be other value for special apps */
    char key[KSU_MAX_PACKAGE_NAME];
    __s32 curr_uid;
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

#endif

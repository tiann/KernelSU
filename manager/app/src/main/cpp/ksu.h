//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

#include <stdint.h>
#include <sys/ioctl.h>

uint32_t get_version();

bool uid_should_umount(int uid);

bool is_safe_mode();

bool is_lkm_mode();

bool is_manager();

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

int get_app_profile(app_profile *profile);

// Feature IDs
enum ksu_feature_id {
    KSU_FEATURE_SU_COMPAT = 0,
    KSU_FEATURE_KERNEL_UMOUNT = 1,
};

// Generic feature API
struct ksu_get_feature_cmd {
    uint32_t feature_id; // Input: feature ID
    uint64_t value;      // Output: feature value/state
    uint8_t supported;   // Output: whether the feature is supported
};

struct ksu_set_feature_cmd {
    uint32_t feature_id; // Input: feature ID
    uint64_t value;      // Input: feature value/state to set
};

struct ksu_become_daemon_cmd {
    uint8_t token[65]; // Input: daemon token (null-terminated)
};

struct ksu_get_info_cmd {
    uint32_t version; // Output: KERNEL_SU_VERSION
    uint32_t flags;   // Output: flags (bit 0: MODULE mode)
    uint32_t features; // Output: max feature ID supported (KSU_FEATURE_MAX)
};

struct ksu_report_event_cmd {
    uint32_t event; // Input: EVENT_POST_FS_DATA, EVENT_BOOT_COMPLETED, etc.
};

struct ksu_set_sepolicy_cmd {
    uint64_t cmd; // Input: sepolicy command
    uint64_t arg; // Input: sepolicy argument pointer
};

struct ksu_check_safemode_cmd {
    uint8_t in_safe_mode; // Output: true if in safe mode, false otherwise
};

struct ksu_get_allow_list_cmd {
    uint32_t uids[128]; // Output: array of allowed/denied UIDs
    uint32_t count; // Output: number of UIDs in array
    uint8_t allow; // Input: true for allow list, false for deny list
};

struct ksu_uid_granted_root_cmd {
    uint32_t uid; // Input: target UID to check
    uint8_t granted; // Output: true if granted, false otherwise
};

struct ksu_uid_should_umount_cmd {
    uint32_t uid; // Input: target UID to check
    uint8_t should_umount; // Output: true if should umount, false otherwise
};

struct ksu_get_manager_uid_cmd {
    uint32_t uid; // Output: manager UID
};

struct ksu_set_manager_uid_cmd {
    uint32_t uid; // Input: new manager UID
};

struct ksu_get_app_profile_cmd {
    struct app_profile profile; // Input/Output: app profile structure
};

struct ksu_set_app_profile_cmd {
    struct app_profile profile; // Input: app profile structure
};

// Su compat
bool set_su_enabled(bool enabled);
bool is_su_enabled();

// Kernel umount
bool set_kernel_umount_enabled(bool enabled);
bool is_kernel_umount_enabled();

// IOCTL command definitions
#define KSU_IOCTL_GRANT_ROOT _IO('K', 1)
#define KSU_IOCTL_GET_INFO _IOR('K', 2, struct ksu_get_info_cmd)
#define KSU_IOCTL_REPORT_EVENT _IOW('K', 3, struct ksu_report_event_cmd)
#define KSU_IOCTL_SET_SEPOLICY _IOWR('K', 4, struct ksu_set_sepolicy_cmd)
#define KSU_IOCTL_CHECK_SAFEMODE _IOR('K', 5, struct ksu_check_safemode_cmd)
#define KSU_IOCTL_GET_ALLOW_LIST _IOWR('K', 6, struct ksu_get_allow_list_cmd)
#define KSU_IOCTL_GET_DENY_LIST _IOWR('K', 7, struct ksu_get_allow_list_cmd)
#define KSU_IOCTL_UID_GRANTED_ROOT _IOWR('K', 8, struct ksu_uid_granted_root_cmd)
#define KSU_IOCTL_UID_SHOULD_UMOUNT _IOWR('K', 9, struct ksu_uid_should_umount_cmd)
#define KSU_IOCTL_GET_MANAGER_UID _IOR('K', 10, struct ksu_get_manager_uid_cmd)
#define KSU_IOCTL_GET_APP_PROFILE _IOWR('K', 11, struct ksu_get_app_profile_cmd)
#define KSU_IOCTL_SET_APP_PROFILE _IOW('K', 12, struct ksu_set_app_profile_cmd)
#define KSU_IOCTL_GET_FEATURE _IOWR('K', 13, struct ksu_get_feature_cmd)
#define KSU_IOCTL_SET_FEATURE _IOW('K', 14, struct ksu_set_feature_cmd)

bool get_allow_list(struct ksu_get_allow_list_cmd*);

#endif //KERNELSU_KSU_H

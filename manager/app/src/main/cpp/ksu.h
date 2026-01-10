//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

#include <cstdint>
#include <sys/ioctl.h>
#include <utility>

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

struct ksu_new_get_allow_list_cmd {
    uint16_t count; // Input / Output: number of UIDs in array
    uint16_t total_count; // Output: total number of UIDs in requested list
    uint32_t uids[0]; // Output: array of allowed/denied UIDs
};

struct ksu_uid_granted_root_cmd {
    uint32_t uid; // Input: target UID to check
    uint8_t granted; // Output: true if granted, false otherwise
};

struct ksu_uid_should_umount_cmd {
    uint32_t uid; // Input: target UID to check
    uint8_t should_umount; // Output: true if should umount, false otherwise
};

struct ksu_get_manager_appid_cmd {
    uint32_t appid; // Output: manager app id
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
#define KSU_IOCTL_GRANT_ROOT _IOC(_IOC_NONE, 'K', 1, 0)
#define KSU_IOCTL_GET_INFO _IOC(_IOC_READ, 'K', 2, 0)
#define KSU_IOCTL_REPORT_EVENT _IOC(_IOC_WRITE, 'K', 3, 0)
#define KSU_IOCTL_SET_SEPOLICY _IOC(_IOC_READ|_IOC_WRITE, 'K', 4, 0)
#define KSU_IOCTL_CHECK_SAFEMODE _IOC(_IOC_READ, 'K', 5, 0)
#define KSU_IOCTL_NEW_GET_ALLOW_LIST _IOWR('K', 6, struct ksu_new_get_allow_list_cmd)
#define KSU_IOCTL_NEW_GET_DENY_LIST _IOWR('K', 7, struct ksu_new_get_allow_list_cmd)
#define KSU_IOCTL_UID_GRANTED_ROOT _IOC(_IOC_READ|_IOC_WRITE, 'K', 8, 0)
#define KSU_IOCTL_UID_SHOULD_UMOUNT _IOC(_IOC_READ|_IOC_WRITE, 'K', 9, 0)
#define KSU_IOCTL_GET_MANAGER_APPID _IOC(_IOC_READ, 'K', 10, 0)
#define KSU_IOCTL_GET_APP_PROFILE _IOC(_IOC_READ|_IOC_WRITE, 'K', 11, 0)
#define KSU_IOCTL_SET_APP_PROFILE _IOC(_IOC_WRITE, 'K', 12, 0)
#define KSU_IOCTL_GET_FEATURE _IOC(_IOC_READ|_IOC_WRITE, 'K', 13, 0)
#define KSU_IOCTL_SET_FEATURE _IOC(_IOC_WRITE, 'K', 14, 0)

bool get_allow_list(struct ksu_new_get_allow_list_cmd *);

inline std::pair<int, int> legacy_get_info() {
    int32_t version = -1;
    int32_t flags = 0;
    int32_t result = 0;
    prctl(0xDEADBEEF, 2, &version, &flags, &result);
    return {version, flags};
}

#endif //KERNELSU_KSU_H

#ifndef __KSU_H_SUPERCALLS
#define __KSU_H_SUPERCALLS

#include <linux/types.h>
#include <linux/ioctl.h>
#include "ksu.h"

// Magic numbers for reboot hook to install fd
#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#define KSU_INSTALL_MAGIC2 0xCAFEBABE

// Command structures for ioctl

struct ksu_become_daemon_cmd {
    __u8 token[65]; // Input: daemon token (null-terminated)
};

struct ksu_get_info_cmd {
    __u32 version; // Output: KERNEL_SU_VERSION
    __u32 flags; // Output: flags (bit 0: MODULE mode)
    __u32 features; // Output: max feature ID supported
};

struct ksu_report_event_cmd {
    __u32 event; // Input: EVENT_POST_FS_DATA, EVENT_BOOT_COMPLETED, etc.
};

struct ksu_set_sepolicy_cmd {
    __u64 cmd; // Input: sepolicy command
    __aligned_u64 arg; // Input: sepolicy argument pointer
};

struct ksu_check_safemode_cmd {
    __u8 in_safe_mode; // Output: true if in safe mode, false otherwise
};

struct ksu_get_allow_list_cmd {
    __u32 uids[128]; // Output: array of allowed/denied UIDs
    __u32 count; // Output: number of UIDs in array
    __u8 allow; // Input: true for allow list, false for deny list
};

struct ksu_uid_granted_root_cmd {
    __u32 uid; // Input: target UID to check
    __u8 granted; // Output: true if granted, false otherwise
};

struct ksu_uid_should_umount_cmd {
    __u32 uid; // Input: target UID to check
    __u8 should_umount; // Output: true if should umount, false otherwise
};

struct ksu_get_manager_uid_cmd {
    __u32 uid; // Output: manager UID
};

struct ksu_get_app_profile_cmd {
    struct app_profile profile; // Input/Output: app profile structure
};

struct ksu_set_app_profile_cmd {
    struct app_profile profile; // Input: app profile structure
};

struct ksu_get_feature_cmd {
    __u32 feature_id; // Input: feature ID (enum ksu_feature_id)
    __u64 value; // Output: feature value/state
    __u8 supported; // Output: true if feature is supported, false otherwise
};

struct ksu_set_feature_cmd {
    __u32 feature_id; // Input: feature ID (enum ksu_feature_id)
    __u64 value; // Input: feature value/state to set
};

struct ksu_get_wrapper_fd_cmd {
    __u32 fd; // Input: userspace fd
    __u32 flags; // Input: flags of userspace fd
};

// IOCTL command definitions
#define KSU_IOCTL_GRANT_ROOT _IOC(_IOC_NONE, 'K', 1, 0)
#define KSU_IOCTL_GET_INFO _IOC(_IOC_READ, 'K', 2, 0)
#define KSU_IOCTL_REPORT_EVENT _IOC(_IOC_WRITE, 'K', 3, 0)
#define KSU_IOCTL_SET_SEPOLICY _IOC(_IOC_READ|_IOC_WRITE, 'K', 4, 0)
#define KSU_IOCTL_CHECK_SAFEMODE _IOC(_IOC_READ, 'K', 5, 0)
#define KSU_IOCTL_GET_ALLOW_LIST _IOC(_IOC_READ|_IOC_WRITE, 'K', 6, 0)
#define KSU_IOCTL_GET_DENY_LIST _IOC(_IOC_READ|_IOC_WRITE, 'K', 7, 0)
#define KSU_IOCTL_UID_GRANTED_ROOT _IOC(_IOC_READ|_IOC_WRITE, 'K', 8, 0)
#define KSU_IOCTL_UID_SHOULD_UMOUNT _IOC(_IOC_READ|_IOC_WRITE, 'K', 9, 0)
#define KSU_IOCTL_GET_MANAGER_UID _IOC(_IOC_READ, 'K', 10, 0)
#define KSU_IOCTL_GET_APP_PROFILE _IOC(_IOC_READ|_IOC_WRITE, 'K', 11, 0)
#define KSU_IOCTL_SET_APP_PROFILE _IOC(_IOC_WRITE, 'K', 12, 0)
#define KSU_IOCTL_GET_FEATURE _IOC(_IOC_READ|_IOC_WRITE, 'K', 13, 0)
#define KSU_IOCTL_SET_FEATURE _IOC(_IOC_WRITE, 'K', 14, 0)
#define KSU_IOCTL_GET_WRAPPER_FD _IOC(_IOC_WRITE, 'K', 15, 0)

// IOCTL handler types
typedef int (*ksu_ioctl_handler_t)(void __user *arg);
typedef bool (*ksu_perm_check_t)(void);

// IOCTL command mapping
struct ksu_ioctl_cmd_map {
    unsigned int cmd;
    const char *name;
    ksu_ioctl_handler_t handler;
    ksu_perm_check_t perm_check; // Permission check function
};

// Install KSU fd to current process
int ksu_install_fd(void);

void ksu_supercalls_init();

#endif // __KSU_H_SUPERCALLS

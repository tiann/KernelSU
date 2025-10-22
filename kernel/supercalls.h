#ifndef __KSU_H_SUPERCALLS
#define __KSU_H_SUPERCALLS

#include <linux/types.h>
#include <linux/ioctl.h>
#include "ksu.h"

// Magic numbers for reboot hook to install fd
#define KSU_INSTALL_MAGIC1 0xDEADBEEF
#define KSU_INSTALL_MAGIC2 0xCAFEBABE

// Command structures for ioctl

struct ksu_become_manager_cmd {
	// No fields needed, success is indicated by return value 0
};

struct ksu_become_daemon_cmd {
	char token[65]; // Input: daemon token (null-terminated)
};

struct ksu_grant_root_cmd {
	// No fields needed, success is indicated by return value 0
};

struct ksu_get_version_cmd {
	u32 version; // Output: KERNEL_SU_VERSION
	u32 version_flags; // Output: flags (bit 0: MODULE mode)
};

struct ksu_report_event_cmd {
	u32 event; // Input: EVENT_POST_FS_DATA, EVENT_BOOT_COMPLETED, etc.
};

struct ksu_set_sepolicy_cmd {
	unsigned long cmd; // Input: sepolicy command
	void __user *arg; // Input: sepolicy argument pointer
};

struct ksu_check_safemode_cmd {
	bool in_safe_mode; // Output: true if in safe mode, false otherwise
};

struct ksu_get_allow_list_cmd {
	u32 uids[128]; // Output: array of allowed/denied UIDs
	u32 count; // Output: number of UIDs in array
	bool allow; // Input: true for allow list, false for deny list
};

struct ksu_uid_granted_root_cmd {
	uid_t uid; // Input: target UID to check
	bool granted; // Output: true if granted, false otherwise
};

struct ksu_uid_should_umount_cmd {
	uid_t uid; // Input: target UID to check
	bool should_umount; // Output: true if should umount, false otherwise
};

struct ksu_get_manager_uid_cmd {
	uid_t uid; // Output: manager UID
};

struct ksu_set_manager_uid_cmd {
	uid_t uid; // Input: new manager UID
};

struct ksu_get_app_profile_cmd {
	struct app_profile profile; // Input/Output: app profile structure
};

struct ksu_set_app_profile_cmd {
	struct app_profile profile; // Input: app profile structure
};

struct ksu_is_su_enabled_cmd {
	bool enabled; // Output: true if su compat enabled
};

struct ksu_enable_su_cmd {
	bool enable; // Input: true to enable, false to disable
};

// IOCTL command definitions
#define KSU_IOCTL_BECOME_MANAGER _IOWR('K', 1, struct ksu_become_manager_cmd)
#define KSU_IOCTL_BECOME_DAEMON _IOWR('K', 2, struct ksu_become_daemon_cmd)
#define KSU_IOCTL_GRANT_ROOT _IOWR('K', 3, struct ksu_grant_root_cmd)
#define KSU_IOCTL_GET_VERSION _IOR('K', 4, struct ksu_get_version_cmd)
#define KSU_IOCTL_REPORT_EVENT _IOW('K', 5, struct ksu_report_event_cmd)
#define KSU_IOCTL_SET_SEPOLICY _IOWR('K', 6, struct ksu_set_sepolicy_cmd)
#define KSU_IOCTL_CHECK_SAFEMODE _IOR('K', 7, struct ksu_check_safemode_cmd)
#define KSU_IOCTL_GET_ALLOW_LIST _IOWR('K', 8, struct ksu_get_allow_list_cmd)
#define KSU_IOCTL_GET_DENY_LIST _IOWR('K', 9, struct ksu_get_allow_list_cmd)
#define KSU_IOCTL_UID_GRANTED_ROOT _IOWR('K', 10, struct ksu_uid_granted_root_cmd)
#define KSU_IOCTL_UID_SHOULD_UMOUNT _IOWR('K', 11, struct ksu_uid_should_umount_cmd)
#define KSU_IOCTL_GET_MANAGER_UID _IOR('K', 12, struct ksu_get_manager_uid_cmd)
#define KSU_IOCTL_SET_MANAGER_UID _IOW('K', 13, struct ksu_set_manager_uid_cmd)
#define KSU_IOCTL_GET_APP_PROFILE _IOWR('K', 14, struct ksu_get_app_profile_cmd)
#define KSU_IOCTL_SET_APP_PROFILE _IOW('K', 15, struct ksu_set_app_profile_cmd)
#define KSU_IOCTL_IS_SU_ENABLED _IOR('K', 16, struct ksu_is_su_enabled_cmd)
#define KSU_IOCTL_ENABLE_SU _IOW('K', 17, struct ksu_enable_su_cmd)

// Handler function declarations
int do_become_manager(void __user *arg);
int do_become_daemon(void __user *arg);
int do_grant_root(void __user *arg);
int do_get_version(void __user *arg);
int do_report_event(void __user *arg);
int do_set_sepolicy(void __user *arg);
int do_check_safemode(void __user *arg);
int do_get_allow_list(void __user *arg);
int do_get_deny_list(void __user *arg);
int do_uid_granted_root(void __user *arg);
int do_uid_should_umount(void __user *arg);
int do_get_manager_uid(void __user *arg);
int do_set_manager_uid(void __user *arg);
int do_get_app_profile(void __user *arg);
int do_set_app_profile(void __user *arg);
int do_is_su_enabled(void __user *arg);
int do_enable_su(void __user *arg);

// IOCTL handler types
typedef int (*ksu_ioctl_handler_t)(void __user *arg);
typedef bool (*ksu_perm_check_t)(void);

// Permission check functions
bool perm_check_manager(void);
bool perm_check_root(void);
bool perm_check_daemon(void);
bool perm_check_daemon_or_manager(void);
bool perm_check_basic(void);
bool perm_check_all(void);

// IOCTL command mapping
struct ksu_ioctl_cmd_map {
	unsigned int cmd;
	ksu_ioctl_handler_t handler;
	ksu_perm_check_t perm_check; // Permission check function
};

// Install KSU fd to current process
int ksu_install_fd(void);

#endif // __KSU_H_SUPERCALLS

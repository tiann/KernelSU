#include "supercalls.h"

#include <linux/anon_inodes.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "allowlist.h"
#include "feature.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "ksud.h"
#include "manager.h"
#include "selinux/selinux.h"

// Forward declarations from core_hook.c
extern void escape_to_root(void);
extern void nuke_ext4_sysfs(void);
extern bool ksu_module_mounted;
extern int handle_sepolicy(unsigned long arg3, void __user *arg4);
extern void ksu_sucompat_init(void);
extern void ksu_sucompat_exit(void);

// Permission check functions
bool perm_check_manager(void)
{
    return is_manager();
}

bool perm_check_root(void)
{
    return current_uid().val == 0;
}

bool perm_check_basic(void)
{
    return current_uid().val == 0 || is_manager();
}

bool perm_check_all(void)
{
    return true; // No permission check
}

static int do_grant_root(void __user *arg)
{
    // Check if current UID is allowed
    bool is_allowed = is_manager() || ksu_is_allow_uid(current_uid().val);

    if (!is_allowed) {
        return -EPERM;
    }

    pr_info("allow root for: %d\n", current_uid().val);
    escape_to_root();

    return 0;
}

static int do_get_info(void __user *arg)
{
    struct ksu_get_info_cmd cmd = {.version = KERNEL_SU_VERSION, .flags = 0};

#ifdef MODULE
    cmd.flags |= 0x1;
#endif

    if (is_manager()) {
        cmd.flags |= 0x2;
    }

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("get_version: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_report_event(void __user *arg)
{
    struct ksu_report_event_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        return -EFAULT;
    }

    switch (cmd.event) {
    case EVENT_POST_FS_DATA: {
        static bool post_fs_data_lock = false;
        if (!post_fs_data_lock) {
            post_fs_data_lock = true;
            pr_info("post-fs-data triggered\n");
            on_post_fs_data();
        }
        break;
    }
    case EVENT_BOOT_COMPLETED: {
        static bool boot_complete_lock = false;
        if (!boot_complete_lock) {
            boot_complete_lock = true;
            pr_info("boot_complete triggered\n");
        }
        break;
    }
    case EVENT_MODULE_MOUNTED: {
        ksu_module_mounted = true;
        pr_info("module mounted!\n");
        nuke_ext4_sysfs();
        break;
    }
    default:
        break;
    }

    return 0;
}

static int do_set_sepolicy(void __user *arg)
{
    struct ksu_set_sepolicy_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        return -EFAULT;
    }

    return handle_sepolicy(cmd.cmd, (void __user *)cmd.arg);
}

static int do_check_safemode(void __user *arg)
{
    struct ksu_check_safemode_cmd cmd;

    cmd.in_safe_mode = ksu_is_safe_mode();

    if (cmd.in_safe_mode) {
        pr_warn("safemode enabled!\n");
    }

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("check_safemode: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_get_allow_list(void __user *arg)
{
    struct ksu_get_allow_list_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        return -EFAULT;
    }

    bool success = ksu_get_allow_list((int *)cmd.uids, (int *)&cmd.count, true);

    if (!success) {
        return -EFAULT;
    }

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("get_allow_list: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_get_deny_list(void __user *arg)
{
    struct ksu_get_allow_list_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        return -EFAULT;
    }

    bool success = ksu_get_allow_list((int *)cmd.uids, (int *)&cmd.count, false);

    if (!success) {
        return -EFAULT;
    }

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("get_deny_list: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_uid_granted_root(void __user *arg)
{
    struct ksu_uid_granted_root_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        return -EFAULT;
    }

    cmd.granted = ksu_is_allow_uid(cmd.uid);

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("uid_granted_root: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_uid_should_umount(void __user *arg)
{
    struct ksu_uid_should_umount_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        return -EFAULT;
    }

    cmd.should_umount = ksu_uid_should_umount(cmd.uid);

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("uid_should_umount: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_get_manager_uid(void __user *arg)
{
    struct ksu_get_manager_uid_cmd cmd;

    cmd.uid = ksu_get_manager_uid();

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("get_manager_uid: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_get_app_profile(void __user *arg)
{
    struct ksu_get_app_profile_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        pr_err("get_app_profile: copy_from_user failed\n");
        return -EFAULT;
    }

    if (!ksu_get_app_profile(&cmd.profile)) {
        return -ENOENT;
    }

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("get_app_profile: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_set_app_profile(void __user *arg)
{
    struct ksu_set_app_profile_cmd cmd;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        pr_err("set_app_profile: copy_from_user failed\n");
        return -EFAULT;
    }

    if (!ksu_set_app_profile(&cmd.profile, true)) {
        return -EFAULT;
    }

    return 0;
}

static int do_get_feature(void __user *arg)
{
    struct ksu_get_feature_cmd cmd;
    bool supported;
    int ret;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        pr_err("get_feature: copy_from_user failed\n");
        return -EFAULT;
    }

    ret = ksu_get_feature(cmd.feature_id, &cmd.value, &supported);
    cmd.supported = supported ? 1 : 0;

    if (ret && supported) {
        pr_err("get_feature: failed for feature %llu: %d\n", cmd.feature_id, ret);
        return ret;
    }

    if (copy_to_user(arg, &cmd, sizeof(cmd))) {
        pr_err("get_feature: copy_to_user failed\n");
        return -EFAULT;
    }

    return 0;
}

static int do_set_feature(void __user *arg)
{
    struct ksu_set_feature_cmd cmd;
    int ret;

    if (copy_from_user(&cmd, arg, sizeof(cmd))) {
        pr_err("set_feature: copy_from_user failed\n");
        return -EFAULT;
    }

    ret = ksu_set_feature(cmd.feature_id, cmd.value);
    if (ret) {
        pr_err("set_feature: failed for feature %llu: %d\n", cmd.feature_id, ret);
        return ret;
    }

    return 0;
}

// IOCTL handlers mapping table
static const struct ksu_ioctl_cmd_map ksu_ioctl_handlers[] = {
    { .cmd = KSU_IOCTL_GRANT_ROOT, .handler = do_grant_root, .perm_check = perm_check_basic },
    { .cmd = KSU_IOCTL_GET_INFO, .handler = do_get_info, .perm_check = perm_check_all },
    { .cmd = KSU_IOCTL_REPORT_EVENT, .handler = do_report_event, .perm_check = perm_check_root },
    { .cmd = KSU_IOCTL_SET_SEPOLICY, .handler = do_set_sepolicy, .perm_check = perm_check_root },
    { .cmd = KSU_IOCTL_CHECK_SAFEMODE, .handler = do_check_safemode, .perm_check = perm_check_all },
    { .cmd = KSU_IOCTL_GET_ALLOW_LIST, .handler = do_get_allow_list, .perm_check = perm_check_basic },
    { .cmd = KSU_IOCTL_GET_DENY_LIST, .handler = do_get_deny_list, .perm_check = perm_check_basic },
    { .cmd = KSU_IOCTL_UID_GRANTED_ROOT, .handler = do_uid_granted_root, .perm_check = perm_check_basic },
    { .cmd = KSU_IOCTL_UID_SHOULD_UMOUNT, .handler = do_uid_should_umount, .perm_check = perm_check_basic },
    { .cmd = KSU_IOCTL_GET_MANAGER_UID, .handler = do_get_manager_uid, .perm_check = perm_check_basic },
    { .cmd = KSU_IOCTL_GET_APP_PROFILE, .handler = do_get_app_profile, .perm_check = perm_check_manager },
    { .cmd = KSU_IOCTL_SET_APP_PROFILE, .handler = do_set_app_profile, .perm_check = perm_check_manager },
    { .cmd = KSU_IOCTL_GET_FEATURE, .handler = do_get_feature, .perm_check = perm_check_manager },
    { .cmd = KSU_IOCTL_SET_FEATURE, .handler = do_set_feature, .perm_check = perm_check_manager },
    { .cmd = 0, .handler = NULL, .perm_check = NULL } // Sentinel
};

// IOCTL dispatcher
static long anon_ksu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    void __user *argp = (void __user *)arg;
    int i;

#ifdef CONFIG_KSU_DEBUG
    pr_info("ksu ioctl: cmd=0x%x from uid=%d\n", cmd, current_uid().val);
#endif

    for (i = 0; ksu_ioctl_handlers[i].handler; i++) {
        if (cmd == ksu_ioctl_handlers[i].cmd) {
            // Check permission first
            if (ksu_ioctl_handlers[i].perm_check &&
                !ksu_ioctl_handlers[i].perm_check()) {
                pr_warn("ksu ioctl: permission denied for cmd=0x%x uid=%d\n",
                    cmd, current_uid().val);
                return -EPERM;
            }
            // Execute handler
            return ksu_ioctl_handlers[i].handler(argp);
        }
    }

    pr_warn("ksu ioctl: unsupported command 0x%x\n", cmd);
    return -ENOTTY;
}

// File release handler
static int anon_ksu_release(struct inode *inode, struct file *filp)
{
    pr_info("ksu fd released\n");
    return 0;
}

// File operations structure
static const struct file_operations anon_ksu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = anon_ksu_ioctl,
    .compat_ioctl = anon_ksu_ioctl,
    .release = anon_ksu_release,
};

// Install KSU fd to current process
int ksu_install_fd(void)
{
    struct file *filp;
    int fd;

    // Get unused fd
    fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0) {
        pr_err("ksu_install_fd: failed to get unused fd\n");
        return fd;
    }

    // Create anonymous inode file
    filp = anon_inode_getfile("[ksu_driver]", &anon_ksu_fops, NULL, O_RDWR | O_CLOEXEC);
    if (IS_ERR(filp)) {
        pr_err("ksu_install_fd: failed to create anon inode file\n");
        put_unused_fd(fd);
        return PTR_ERR(filp);
    }

    // Install fd
    fd_install(fd, filp);

    pr_info("ksu fd installed: %d for pid %d\n", fd, current->pid);

    return fd;
}

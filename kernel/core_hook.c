#include <linux/seccomp.h>
#include <linux/bpf.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/init_task.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/version.h>

#include "allowlist.h"
#include "arch.h"
#include "core_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "ksud.h"
#include "manager.h"
#include "selinux/selinux.h"
#include "throne_tracker.h"
#include "kernel_compat.h"
#include "supercalls.h"

bool ksu_module_mounted = false;

extern int handle_sepolicy(unsigned long arg3, void __user *arg4);

extern void ksu_sucompat_init();
extern void ksu_sucompat_exit();

static inline bool is_allow_su()
{
    if (is_manager()) {
        // we are manager, allow!
        return true;
    }
    return ksu_is_allow_uid(current_uid().val);
}

static inline bool is_unsupported_uid(uid_t uid)
{
#define LAST_APPLICATION_UID 19999
    uid_t appid = uid % 100000;
    return appid > LAST_APPLICATION_UID;
}

static struct group_info root_groups = { .usage = ATOMIC_INIT(2) };

static void setup_groups(struct root_profile *profile, struct cred *cred)
{
    if (profile->groups_count > KSU_MAX_GROUPS) {
        pr_warn("Failed to setgroups, too large group: %d!\n",
            profile->uid);
        return;
    }

    if (profile->groups_count == 1 && profile->groups[0] == 0) {
        // setgroup to root and return early.
        if (cred->group_info)
            put_group_info(cred->group_info);
        cred->group_info = get_group_info(&root_groups);
        return;
    }

    u32 ngroups = profile->groups_count;
    struct group_info *group_info = groups_alloc(ngroups);
    if (!group_info) {
        pr_warn("Failed to setgroups, ENOMEM for: %d\n", profile->uid);
        return;
    }

    int i;
    for (i = 0; i < ngroups; i++) {
        gid_t gid = profile->groups[i];
        kgid_t kgid = make_kgid(current_user_ns(), gid);
        if (!gid_valid(kgid)) {
            pr_warn("Failed to setgroups, invalid gid: %d\n", gid);
            put_group_info(group_info);
            return;
        }
        group_info->gid[i] = kgid;
    }

    groups_sort(group_info);
    set_groups(cred, group_info);
    put_group_info(group_info);
}

static void disable_seccomp()
{
    assert_spin_locked(&current->sighand->siglock);
    // disable seccomp
#if defined(CONFIG_GENERIC_ENTRY) &&                                           \
    LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    clear_syscall_work(SECCOMP);
#else
    clear_thread_flag(TIF_SECCOMP);
#endif

#ifdef CONFIG_SECCOMP
    current->seccomp.mode = 0;
    current->seccomp.filter = NULL;
    atomic_set(&current->seccomp.filter_count, 0);
#else
#endif
}

void escape_to_root(void)
{
    struct cred *cred;

    cred = prepare_creds();
    if (!cred) {
        pr_warn("prepare_creds failed!\n");
        return;
    }

    if (cred->euid.val == 0) {
        pr_warn("Already root, don't escape!\n");
        abort_creds(cred);
        return;
    }

    struct root_profile *profile = ksu_get_root_profile(cred->uid.val);

    cred->uid.val = profile->uid;
    cred->suid.val = profile->uid;
    cred->euid.val = profile->uid;
    cred->fsuid.val = profile->uid;

    cred->gid.val = profile->gid;
    cred->fsgid.val = profile->gid;
    cred->sgid.val = profile->gid;
    cred->egid.val = profile->gid;
    cred->securebits = 0;

    BUILD_BUG_ON(sizeof(profile->capabilities.effective) !=
             sizeof(kernel_cap_t));

    // setup capabilities
    // we need CAP_DAC_READ_SEARCH becuase `/data/adb/ksud` is not accessible for non root process
    // we add it here but don't add it to cap_inhertiable, it would be dropped automaticly after exec!
    u64 cap_for_ksud =
        profile->capabilities.effective | CAP_DAC_READ_SEARCH;
    memcpy(&cred->cap_effective, &cap_for_ksud,
           sizeof(cred->cap_effective));
    memcpy(&cred->cap_permitted, &profile->capabilities.effective,
           sizeof(cred->cap_permitted));
    memcpy(&cred->cap_bset, &profile->capabilities.effective,
           sizeof(cred->cap_bset));

    setup_groups(profile, cred);

    commit_creds(cred);

    // Refer to kernel/seccomp.c: seccomp_set_mode_strict
    // When disabling Seccomp, ensure that current->sighand->siglock is held during the operation.
    spin_lock_irq(&current->sighand->siglock);
    disable_seccomp();
    spin_unlock_irq(&current->sighand->siglock);

    setup_selinux(profile->selinux_domain);
}

void nuke_ext4_sysfs(void)
{
    struct path path;
    int err = kern_path("/data/adb/modules", 0, &path);
    if (err) {
        pr_err("nuke path err: %d\n", err);
        return;
    }

    struct super_block *sb = path.dentry->d_inode->i_sb;
    const char *name = sb->s_type->name;
    if (strcmp(name, "ext4") != 0) {
        pr_info("nuke but module aren't mounted\n");
        path_put(&path);
        return;
    }

    ext4_unregister_sysfs(sb);
    path_put(&path);
}

// ksu_handle_prctl removed - now using ioctl via reboot hook

static bool is_appuid(kuid_t uid)
{
#define PER_USER_RANGE 100000
#define FIRST_APPLICATION_UID 10000
#define LAST_APPLICATION_UID 19999

    uid_t appid = uid.val % PER_USER_RANGE;
    return appid >= FIRST_APPLICATION_UID && appid <= LAST_APPLICATION_UID;
}

static bool should_umount(struct path *path)
{
    if (!path) {
        return false;
    }

    if (current->nsproxy->mnt_ns == init_nsproxy.mnt_ns) {
        pr_info("ignore global mnt namespace process: %d\n",
            current_uid().val);
        return false;
    }

    if (path->mnt && path->mnt->mnt_sb && path->mnt->mnt_sb->s_type) {
        const char *fstype = path->mnt->mnt_sb->s_type->name;
        return strcmp(fstype, "overlay") == 0;
    }
    return false;
}

static void ksu_umount_mnt(struct path *path, int flags)
{
    int err = path_umount(path, flags);
    if (err) {
        pr_info("umount %s failed: %d\n", path->dentry->d_iname, err);
    }
}

static void try_umount(const char *mnt, bool check_mnt, int flags)
{
    struct path path;
    int err = kern_path(mnt, 0, &path);
    if (err) {
        return;
    }

    if (path.dentry != path.mnt->mnt_root) {
        // it is not root mountpoint, maybe umounted by others already.
        path_put(&path);
        return;
    }

    // we are only interest in some specific mounts
    if (check_mnt && !should_umount(&path)) {
        path_put(&path);
        return;
    }

    ksu_umount_mnt(&path, flags);
}

int ksu_handle_setuid(struct cred *new, const struct cred *old)
{
    if (!new || !old) {
        return 0;
    }

    kuid_t new_uid = new->uid;
    kuid_t old_uid = old->uid;

    if (0 != old_uid.val) {
        // old process is not root, ignore it.
        return 0;
    }

    if (!is_appuid(new_uid) || is_unsupported_uid(new_uid.val)) {
        // pr_info("handle setuid ignore non application or isolated uid: %d\n", new_uid.val);
        return 0;
    }

    if (ksu_get_manager_uid() == new_uid.val) {
        pr_info("install fd for: %d\n", new_uid.val);
        spin_lock_irq(&current->sighand->siglock);
        ksu_install_fd();
        ksu_seccomp_allow_cache(current->seccomp.filter, __NR_reboot);
        spin_unlock_irq(&current->sighand->siglock);
        return 0;
    }

    // this hook is used for umounting overlayfs for some uid, if there isn't any module mounted, just ignore it!
    if (!ksu_module_mounted) {
        return 0;
    }

    if (!ksu_uid_should_umount(new_uid.val)) {
        return 0;
    } else {
#ifdef CONFIG_KSU_DEBUG
        pr_info("uid: %d should not umount!\n", current_uid().val);
#endif
    }

    // check old process's selinux context, if it is not zygote, ignore it!
    // because some su apps may setuid to untrusted_app but they are in global mount namespace
    // when we umount for such process, that is a disaster!
    bool is_zygote_child = is_zygote(old->security);
    if (!is_zygote_child) {
        pr_info("handle umount ignore non zygote child: %d\n",
            current->pid);
        return 0;
    }
#ifdef CONFIG_KSU_DEBUG
    // umount the target mnt
    pr_info("handle umount for uid: %d, pid: %d\n", new_uid.val,
        current->pid);
#endif

    // fixme: use `collect_mounts` and `iterate_mount` to iterate all mountpoint and
    // filter the mountpoint whose target is `/data/adb`
    try_umount("/odm", true, 0);
    try_umount("/system", true, 0);
    try_umount("/vendor", true, 0);
    try_umount("/product", true, 0);
    try_umount("/system_ext", true, 0);
    try_umount("/data/adb/modules", false, MNT_DETACH);

    return 0;
}

// Init functons - kprobe hooks

// 1. Reboot hook for installing fd
static int reboot_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct pt_regs *real_regs = PT_REAL_REGS(regs);
    int magic1 = (int)PT_REGS_PARM1(real_regs);
    int magic2 = (int)PT_REGS_PARM2(real_regs);
    unsigned long arg4;

    // Check if this is a request to install KSU fd
    if (magic1 == KSU_INSTALL_MAGIC1 && magic2 == KSU_INSTALL_MAGIC2) {
        int fd = ksu_install_fd();
        pr_info("[%d] install ksu fd: %d\n", current->pid, fd);

        arg4 = (unsigned long)PT_REGS_SYSCALL_PARM4(real_regs);
        if (copy_to_user((int *)arg4, &fd, sizeof(fd))) {
            pr_err("install ksu fd reply err\n");
        }
    }

    return 0;
}

static struct kprobe reboot_kp = {
    .symbol_name = REBOOT_SYMBOL,
    .pre_handler = reboot_handler_pre,
};

// 2. security_task_fix_setuid hook for handling setuid
static int security_task_fix_setuid_handler_pre(struct kprobe *p,
                        struct pt_regs *regs)
{
    struct cred *new = (struct cred *)PT_REGS_PARM1(regs);
    const struct cred *old = (const struct cred *)PT_REGS_PARM2(regs);

    ksu_handle_setuid(new, old);

    // Always return 0 to continue the original function
    return 0;
}

static struct kprobe security_task_fix_setuid_kp = {
    .symbol_name = SECURITY_TASK_FIX_SETUID_SYMBOL,
    .pre_handler = security_task_fix_setuid_handler_pre,
};

__maybe_unused int ksu_kprobe_init(void)
{
    int rc = 0;

    // Register reboot kprobe
    rc = register_kprobe(&reboot_kp);
    if (rc) {
        pr_err("reboot kprobe failed: %d\n", rc);
        return rc;
    }
    pr_info("reboot kprobe registered successfully\n");

    // Register security_task_fix_setuid kprobe
    rc = register_kprobe(&security_task_fix_setuid_kp);
    if (rc) {
        pr_err("security_task_fix_setuid kprobe failed: %d\n", rc);
        unregister_kprobe(&reboot_kp);
        return rc;
    }
    pr_info("security_task_fix_setuid kprobe registered successfully\n");

    return 0;
}

__maybe_unused int ksu_kprobe_exit(void)
{
    unregister_kprobe(&reboot_kp);
    unregister_kprobe(&security_task_fix_setuid_kp);
    return 0;
}

void __init ksu_core_init(void)
{
#ifdef CONFIG_KPROBES
    int rc = ksu_kprobe_init();
    if (rc) {
        pr_err("ksu_kprobe_init failed: %d\n", rc);
    }
#endif
}

void ksu_core_exit(void)
{
#ifdef CONFIG_KPROBES
    pr_info("ksu_core_exit\n");
    ksu_kprobe_exit();
#endif
}

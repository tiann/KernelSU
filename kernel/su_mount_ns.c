#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/proc_ns.h>
#include <linux/pid.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/task_work.h>
#include <uapi/linux/mount.h>
#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "su_mount_ns.h"

extern int path_mount(const char *dev_name, struct path *path,
                      const char *type_page, unsigned long flags,
                      void *data_page);

#if defined(__aarch64__)
extern long __arm64_sys_setns(const struct pt_regs *regs);
#elif defined(__x86_64__)
extern long __x64_sys_setns(const struct pt_regs *regs);
#endif

static long ksu_sys_setns(int fd, int flags)
{
    struct pt_regs regs;
    memset(&regs, 0, sizeof(regs));

    PT_REGS_PARM1(&regs) = fd;
    PT_REGS_PARM2(&regs) = flags;

#if defined(__aarch64__)
    return __arm64_sys_setns(&regs);
#elif defined(__x86_64__)
    return __x64_sys_setns(&regs);
#else
#error "Unsupported arch"
#endif
}

static void setup_mount_namespace(int32_t ns_mode)
{
    pr_info("setup mount namespace for pid: %d\n", current->pid);
    // inherit mode
    if (ns_mode == KSU_NS_INHERITED) {
        pr_info("mount namespace mode: inherit\n");
        // do nothing
        return;
    }

    if (ns_mode != KSU_NS_GLOBAL && ns_mode != KSU_NS_INDIVIDUAL) {
        pr_warn("unknown mount namespace mode: %d\n", ns_mode);
        return;
    }

    const struct cred *old_cred = NULL;
    struct cred *new_cred = NULL;
    struct path root_path;
    struct path saved_path;

    // save current working directory
    get_fs_pwd(current->fs, &saved_path);

    if (!(capable(CAP_SYS_ADMIN) && capable(CAP_SYS_CHROOT))) {
        pr_info(
            "process does not have CAP_SYS_ADMIN or CAP_SYS_CHROOT, adding it temporarily.\n");
        new_cred = prepare_creds();
        if (!new_cred) {
            pr_warn("failed to prepare new credentials\n");
            return;
        }
        cap_raise(new_cred->cap_effective, CAP_SYS_ADMIN);
        cap_raise(new_cred->cap_effective, CAP_SYS_CHROOT);
        old_cred = override_creds(new_cred);
    }
    // global mode , need CAP_SYS_ADMIN and CAP_SYS_CHROOT to perform setns
    if (ns_mode == KSU_NS_GLOBAL) {
        pr_info("mount namespace mode: global\n");
        struct file *ns_file;
        struct path ns_path;
        struct task_struct *pid1_task = NULL;
        struct pid *pid_struct = NULL;
        rcu_read_lock();
        // find init
        pid_struct = find_pid_ns(1, &init_pid_ns);
        if (unlikely(!pid_struct)) {
            rcu_read_unlock();
            pr_warn("failed to find pid_struct for PID 1\n");
            goto try_drop_caps;
        }

        pid1_task = get_pid_task(pid_struct, PIDTYPE_PID);
        rcu_read_unlock();
        if (unlikely(!pid1_task)) {
            pr_warn("failed to get task_struct for PID 1\n");
            goto try_drop_caps;
        }
        // maybe you can use &init_task for first stage init?
        long ret = ns_get_path(&ns_path, pid1_task, &mntns_operations);
        put_task_struct(pid1_task);
        if (ret) {
            pr_warn("failed to get path for init's mount namespace: %ld\n",
                    ret);
            goto try_drop_caps;
        }
        ns_file = dentry_open(&ns_path, O_RDONLY, current_cred());

        path_put(&ns_path);
        if (IS_ERR(ns_file)) {
            pr_warn("failed to open file for init's mount namespace: %ld\n",
                    PTR_ERR(ns_file));
            goto try_drop_caps;
        }

        int fd = get_unused_fd_flags(O_CLOEXEC);
        if (fd < 0) {
            pr_warn("failed to get an unused fd: %d\n", fd);
            fput(ns_file);
            goto try_drop_caps;
        }

        fd_install(fd, ns_file);
        pr_info("calling sys_setns with fd : %d\n", fd);

        ret = ksu_sys_setns(fd, CLONE_NEWNS);
        if (ret) {
            pr_warn("sys_setns failed: %ld\n", ret);
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
        ksys_close(fd);
#else
        close_fd(fd);
#endif
    }
    // individual mode , need CAP_SYS_ADMIN to perform unshare
    if (ns_mode == KSU_NS_INDIVIDUAL) {
        long ret;
        pr_info("mount namespace mode: individual\n");

        ret = ksys_unshare(CLONE_NEWNS);
        if (ret) {
            pr_warn("call ksys_unshare failed: %ld\n", ret);
            goto try_drop_caps;
        }

        // Make root mount private
        get_fs_root(current->fs, &root_path);
        int pm_ret;
        pm_ret = path_mount(NULL, &root_path, NULL, MS_PRIVATE | MS_REC, NULL);

        if (pm_ret < 0) {
            pr_err("failed to make root private, err: %d\n", pm_ret);
        }

        path_put(&root_path);
    }
// finally drop capability
try_drop_caps:
    if (old_cred) {
        pr_info("dropping temporarily capability.\n");
        revert_creds(old_cred);
        put_cred(new_cred);
    }
    // Restore working directory
    set_fs_pwd(current->fs, &saved_path);
    return;
}

void ksu_setup_mount_namespace_tw_func(struct callback_head *cb)
{
    struct ksu_mns_tw *tw = container_of(cb, struct ksu_mns_tw, cb);
    setup_mount_namespace(tw->ns_mode);
    kfree(tw);
}

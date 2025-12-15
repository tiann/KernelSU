#include <linux/dcache.h>
#include <linux/errno.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/limits.h>
#include <linux/namei.h>
#include <linux/proc_ns.h>
#include <linux/pid.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/task_work.h>
#include <linux/version.h>
#include <uapi/linux/mount.h>

#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
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

// global mode , need CAP_SYS_ADMIN and CAP_SYS_CHROOT to perform setns
static void ksu_mnt_ns_global(void)
{
    // save current working directory as absolute path before setns
    char *pwd_path = NULL;
    char *pwd_buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!pwd_buf) {
        pr_warn("no mem for pwd buffer, skip restore pwd!!\n");
        goto try_setns;
    }

    struct path saved_pwd;
    get_fs_pwd(current->fs, &saved_pwd);
    pwd_path = d_path(&saved_pwd, pwd_buf, PATH_MAX);
    path_put(&saved_pwd);

    if (IS_ERR(pwd_path)) {
        if (PTR_ERR(pwd_path) == -ENAMETOOLONG) {
            pr_warn("absolute pwd longer than: %d, skip restore pwd!!\n",
                    PATH_MAX);
        } else {
            pr_warn("get absolute pwd failed: %ld\n", PTR_ERR(pwd_path));
        }
        pwd_path = NULL;
    }

try_setns:

    rcu_read_lock();
    // &init_task is not init, but swapper/idle, which forks the init process
    // so we need find init process
    struct pid *pid_struct = find_pid_ns(1, &init_pid_ns);
    if (unlikely(!pid_struct)) {
        rcu_read_unlock();
        pr_warn("failed to find pid_struct for PID 1\n");
        goto out;
    }

    struct task_struct *pid1_task = get_pid_task(pid_struct, PIDTYPE_PID);
    rcu_read_unlock();
    if (unlikely(!pid1_task)) {
        pr_warn("failed to get task_struct for PID 1\n");
        goto out;
    }
    struct path ns_path;
    long ret = ns_get_path(&ns_path, pid1_task, &mntns_operations);
    put_task_struct(pid1_task);
    if (ret) {
        pr_warn("failed get path for init mount namespace: %ld\n", ret);
        goto out;
    }
    struct file *ns_file = dentry_open(&ns_path, O_RDONLY, ksu_cred);

    path_put(&ns_path);
    if (IS_ERR(ns_file)) {
        pr_warn("failed open file for init mount namespace: %ld\n",
                PTR_ERR(ns_file));
        goto out;
    }

    int fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0) {
        pr_warn("failed to get an unused fd: %d\n", fd);
        fput(ns_file);
        goto out;
    }

    fd_install(fd, ns_file);
    ret = ksu_sys_setns(fd, CLONE_NEWNS);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 11, 0)
    ksys_close(fd);
#else
    close_fd(fd);
#endif

    if (ret) {
        pr_warn("call setns failed: %ld\n", ret);
        goto out;
    }
    // try to restore working directory using absolute path after setns
    if (pwd_path) {
        struct path new_pwd;
        int err = kern_path(pwd_path, 0, &new_pwd);
        if (!err) {
            set_fs_pwd(current->fs, &new_pwd);
            path_put(&new_pwd);
        } else {
            pr_warn("restore pwd failed: %d, path: %s\n", err, pwd_path);
        }
    }
out:
    kfree(pwd_buf);
}

// individual mode , need CAP_SYS_ADMIN to perform unshare and remount
static void ksu_mnt_ns_individual(void)
{
    long ret = ksys_unshare(CLONE_NEWNS);
    if (ret) {
        pr_warn("call ksys_unshare failed: %ld\n", ret);
        return;
    }

    // make root mount private
    struct path root_path;
    get_fs_root(current->fs, &root_path);
    int pm_ret = path_mount(NULL, &root_path, NULL, MS_PRIVATE | MS_REC, NULL);
    path_put(&root_path);

    if (pm_ret < 0) {
        pr_err("failed to make root private, err: %d\n", pm_ret);
    }
}

static void ksu_setup_mount_ns_tw_func(struct callback_head *cb)
{
    struct ksu_mns_tw *tw = container_of(cb, struct ksu_mns_tw, cb);
    const struct cred *old_cred = override_creds(ksu_cred);
    if (tw->ns_mode == KSU_NS_GLOBAL) {
        ksu_mnt_ns_global();
    } else {
        ksu_mnt_ns_individual();
    }
    revert_creds(old_cred);
    kfree(tw);
}

void setup_mount_ns(int32_t ns_mode)
{
    // inherit mode
    if (ns_mode == KSU_NS_INHERITED) {
        // do nothing
        return;
    }

    if (ns_mode != KSU_NS_GLOBAL && ns_mode != KSU_NS_INDIVIDUAL) {
        pr_warn("pid: %d ,unknown mount namespace mode: %d\n", current->pid,
                ns_mode);
        return;
    }

    if (!ksu_cred) {
        pr_err("no ksu cred! skip mnt_ns magic for pid: %d.\n", current->pid);
        return;
    }

    struct ksu_mns_tw *tw = kzalloc(sizeof(*tw), GFP_ATOMIC);
    if (!tw) {
        pr_err("no mem for tw! skip mnt_ns magic for pid: %d.\n", current->pid);
        return;
    }
    tw->cb.func = ksu_setup_mount_ns_tw_func;
    tw->ns_mode = ns_mode;
    if (task_work_add(current, &tw->cb, TWA_RESUME)) {
        kfree(tw);
        pr_err("add task work failed! skip mnt_ns magic for pid: %d.\n",
               current->pid);
    }
}

#include <linux/anon_inodes.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/task_work.h>
#include <linux/uaccess.h>
#include <linux/version.h>

#include "uapi/supercall.h"
#include "supercall/internal.h"
#include "arch.h"
#include "hook/syscall_hook_manager.h"
#include "util.h"
#include "klog.h" // IWYU pragma: keep

struct ksu_install_fd_tw {
    struct callback_head cb;
    int __user *outp;
};

static bool can_use_supercall(void)
{
    return manager_or_root() || allowed_for_su();
}

static bool can_install_driver_fd(void)
{
    if (ksu_module_unload_recovery_allowed())
        return only_root();
    if (ksu_module_unload_in_progress())
        return false;
    return can_use_supercall();
}

static int anon_ksu_release(struct inode *inode, struct file *filp)
{
    pr_debug("ksu fd released\n");
    return 0;
}

static long anon_ksu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret;

    if (cmd == KSU_IOCTL_PREPARE_UNLOAD || cmd == KSU_IOCTL_COMMIT_UNLOAD || cmd == KSU_IOCTL_ABORT_UNLOAD) {
        if (!only_root())
            return -EPERM;

        if (cmd == KSU_IOCTL_PREPARE_UNLOAD)
            return ksu_prepare_module_unload();
        if (cmd == KSU_IOCTL_COMMIT_UNLOAD)
            return ksu_commit_module_unload();
        return ksu_abort_module_unload();
    }

    if (!ksu_module_unload_try_enter())
        return -ESHUTDOWN;

    if (!can_use_supercall()) {
        ret = -EPERM;
        goto out;
    }

    ret = ksu_supercall_handle_ioctl(cmd, (void __user *)arg);

out:
    ksu_module_unload_leave();
    return ret;
}

static const struct file_operations anon_ksu_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = anon_ksu_ioctl,
    .compat_ioctl = anon_ksu_ioctl,
    .release = anon_ksu_release,
};

int ksu_install_fd(void)
{
    struct file *filp;
    int fd;

    if (!ksu_module_control_try_enter())
        return -ESHUTDOWN;

    if (ksu_module_unload_recovery_allowed() && !only_root()) {
        fd = -EPERM;
        goto out;
    }

    fd = get_unused_fd_flags(O_CLOEXEC);
    if (fd < 0) {
        pr_err("ksu_install_fd: failed to get unused fd\n");
        goto out;
    }

    filp = anon_inode_getfile("[ksu_driver]", &anon_ksu_fops, NULL, O_RDWR | O_CLOEXEC);
    if (IS_ERR(filp)) {
        pr_err("ksu_install_fd: failed to create anon inode file\n");
        put_unused_fd(fd);
        fd = PTR_ERR(filp);
        goto out;
    }

    fd_install(fd, filp);
    pr_debug("ksu fd installed: %d for pid %d\n", fd, current->pid);

out:
    ksu_module_unload_leave();
    return fd;
}

static void ksu_install_fd_tw_func(struct callback_head *cb)
{
    struct ksu_install_fd_tw *tw = container_of(cb, struct ksu_install_fd_tw, cb);
    int fd = ksu_install_fd();

    pr_debug("[%d] install ksu fd: %d\n", current->pid, fd);
    if (copy_to_user(tw->outp, &fd, sizeof(fd))) {
        pr_err("install ksu fd reply err\n");
        if (fd >= 0)
            ksu_close_fd(fd);
    }

    kfree(tw);
    module_put(THIS_MODULE);
}

static int reboot_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct pt_regs *real_regs = PT_REAL_REGS(regs);
    int magic1 = (int)PT_REGS_PARM1(real_regs);
    int magic2 = (int)PT_REGS_PARM2(real_regs);

    if (magic1 == KSU_INSTALL_MAGIC1 && magic2 == KSU_INSTALL_MAGIC2 && can_install_driver_fd()) {
        struct ksu_install_fd_tw *tw;
        unsigned long arg4 = (unsigned long)PT_REGS_SYSCALL_PARM4(real_regs);

        tw = kzalloc(sizeof(*tw), GFP_ATOMIC);
        if (!tw)
            return 0;

        tw->outp = (int __user *)arg4;
        tw->cb.func = ksu_install_fd_tw_func;

        // task_work stores a callback into module text. Only pin a live module;
        // once delete_module() has moved it to GOING, no new callback may be queued.
        if (!try_module_get(THIS_MODULE)) {
            kfree(tw);
            return 0;
        }

        if (task_work_add(current, &tw->cb, TWA_RESUME)) {
            module_put(THIS_MODULE);
            kfree(tw);
            pr_warn("install fd add task_work failed\n");
        }
    }

    return 0;
}

static struct kprobe reboot_kp = {
    .symbol_name = REBOOT_SYMBOL,
    .pre_handler = reboot_handler_pre,
};

void __init ksu_supercalls_init(void)
{
    int rc;

    ksu_supercall_dump_commands();

    rc = register_kprobe(&reboot_kp);
    if (rc) {
        pr_err("reboot kprobe failed: %d\n", rc);
    } else {
        pr_info("reboot kprobe registered successfully\n");
    }
}

void __exit ksu_supercalls_exit(void)
{
    unregister_kprobe(&reboot_kp);
    ksu_supercall_cleanup_state();
}

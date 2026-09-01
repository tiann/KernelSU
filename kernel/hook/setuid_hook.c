#include <linux/compiler.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/thread_info.h>
#include <linux/seccomp.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/susfs_def.h>
#include "selinux/selinux.h"

#include "policy/allowlist.h"
#include "hook/setuid_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "infra/seccomp_cache.h"
#include "supercall/supercall.h"
#include "feature/kernel_umount.h"

extern u32 susfs_zygote_sid;
extern u32 susfs_zygote_next_sid;
extern void disable_seccomp(void);
extern struct work_struct susfs_extra_works;

static inline void ksu_handle_extra_susfs_work(void)
{
    if (work_pending(&susfs_extra_works))
        return;

    schedule_work(&susfs_extra_works);
}

static int handle_zygote_setresuid(uid_t ruid) {
    // Check if spawned process is isolated service first, and force to do umount if so
    if (is_isolated_process(ruid)) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        goto do_umount;
    }

    // - Since ksu maanger app uid is excluded in allow_list_arr, so ksu_uid_should_umount(manager_uid)
    //   will always return true, that's why we need to explicitly check if new_uid belongs to
    //   ksu manager.
    // - Disable seccomp restriction for KSU manager since running with "su" will disable seccomp anyway
    if (likely(ksu_is_manager_appid_valid()) && unlikely(is_uid_manager(ruid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", ruid);
        ksu_install_fd();
        return 0;
    }

    // - Check if spawned process is normal user app and needs to be umounted
    // - Now app_profile for webview_zygote is available in KernelSU manager
    if (likely(is_appuid(ruid) && ksu_uid_should_umount(ruid))) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        goto do_umount;
    }

    // Disable seccomp restriction for root allowed apps since running with "su" will disable seccomp anyway
    if (ksu_is_allow_uid_for_current(ruid)) {
        disable_seccomp();
        return 0;
    }

    // Process not umounted but also root not allowed
    susfs_set_current_proc_no_su();
    return 0;

do_umount:
    {
        // Handle kernel umount
        ksu_handle_umount(current_uid().val, ruid);

        // Handle extra susfs work
        ksu_handle_extra_susfs_work();
    }

    return 0;
}

static int handle_zygote_next_setresuid(uid_t ruid) {
    // Check if spawned process is isolated service first, and force to do umount if so
    if (is_isolated_process(ruid)) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        susfs_set_current_proc_umounted_for_zygote_next();
        goto do_susfs_work;
    }

    // - Since ksu maanger app uid is excluded in allow_list_arr, so ksu_uid_should_umount(manager_uid)
    //   will always return true, that's why we need to explicitly check if new_uid belongs to
    //   ksu manager.
    // - Disable seccomp restriction for KSU manager since running with "su" will disable seccomp anyway
    if (likely(ksu_is_manager_appid_valid()) && unlikely(is_uid_manager(ruid))) {
        disable_seccomp();
        pr_info("install fd for manager: %d\n", ruid);
        ksu_install_fd();
        return 0;
    }

    // - Check if spawned process is normal user app and needs to be umounted
    // - Now app_profile for webview_zygote is available in KernelSU manager
    if (likely(is_appuid(ruid) && ksu_uid_should_umount(ruid))) {
        susfs_set_current_proc_no_su();
        susfs_set_current_proc_umounted();
        susfs_set_current_proc_umounted_for_zygote_next();
        goto do_susfs_work;
    }

    // Disable seccomp restriction for root allowed apps since running with "su" will disable seccomp anyway
    if (ksu_is_allow_uid_for_current(ruid)) {
        disable_seccomp();
        return 0;
    }

    // Process not umounted but also root not allowed
    susfs_set_current_proc_no_su();
    return 0;

do_susfs_work:
    {
        // Do not umount here as we are in init namespace now

        // Handle extra susfs work
        ksu_handle_extra_susfs_work();
    }

    return 0;
}

int ksu_handle_setresuid(uid_t ruid, uid_t euid, uid_t suid)
{
    uid_t cur_uid = current_uid().val;

    if (cur_uid != 0)
        return 0;

    // We only interest in process spwaned by zygote or zygote_next
    if (susfs_is_sid_equal(current_cred(), susfs_zygote_sid))
        return handle_zygote_setresuid(ruid);

    if (susfs_is_sid_equal(current_cred(), susfs_zygote_next_sid))
        return handle_zygote_next_setresuid(ruid);

    return 0;

}

void __init ksu_setuid_hook_init(void)
{
    ksu_kernel_umount_init();
}

void __exit ksu_setuid_hook_exit(void)
{
    pr_info("ksu_core_exit\n");
    ksu_kernel_umount_exit();
}

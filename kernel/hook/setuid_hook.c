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
#include <asm/unistd.h>

#include "policy/allowlist.h"
#include "hook/setuid_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "infra/seccomp_cache.h"
#include "supercall/supercall.h"
#include "hook/tp_marker.h"
#include "feature/kernel_umount.h"
#include "api/event_registry.h"
#include "manager/throne_tracker.h"
#include "selinux/selinux.h"

int ksu_handle_setresuid(uid_t old_uid, uid_t new_uid)
{
    // we rely on the fact that zygote always call setresuid(3) with same uids

    pr_info("handle_setresuid from %d to %d\n", old_uid, new_uid);

    if (unlikely(is_uid_manager(new_uid))) {
        spin_lock_irq(&current->sighand->siglock);
        ksu_seccomp_allow_cache(current->seccomp.filter, __NR_reboot);
        ksu_set_task_tracepoint_flag(current);
        spin_unlock_irq(&current->sighand->siglock);

        pr_info("install fd for manager: %d\n", new_uid);
        {
            int fd = ksu_install_fd();
            struct ksu_policy_event event = {
                .size = sizeof(event),
                .version = 1,
                .uid = new_uid,
                .result = fd < 0 ? fd : 0,
            };
            ksu_event_emit(KSU_EVENT_MANAGER_READY, &event, event.result);
        }
        {
            struct ksu_uid_event event = {
                .size = sizeof(event),
                .version = 1,
                .syscall_nr = __NR_setresuid,
                .old_uid = old_uid,
                .new_uid = new_uid,
                .pid = current->pid,
                .tgid = current->tgid,
                .old_allowlisted = __ksu_is_allow_uid(old_uid),
                .new_allowlisted = __ksu_is_allow_uid(new_uid),
                .is_zygote_child = is_zygote(current_cred()),
            };
            ksu_event_emit(KSU_EVENT_UID_COMMITTED, &event, 0);
        }
        return 0;
    }

    if (ksu_is_allow_uid_for_current(new_uid)) {
        if (current->seccomp.mode == SECCOMP_MODE_FILTER && current->seccomp.filter) {
            spin_lock_irq(&current->sighand->siglock);
            ksu_seccomp_allow_cache(current->seccomp.filter, __NR_reboot);
            spin_unlock_irq(&current->sighand->siglock);
        }
        ksu_set_task_tracepoint_flag(current);
    } else {
        ksu_clear_task_tracepoint_flag_if_needed(current);
    }

    // Handle kernel umount
    ksu_handle_umount(old_uid, new_uid);

    {
        struct ksu_uid_event event = {
            .size = sizeof(event),
            .version = 1,
            .syscall_nr = __NR_setresuid,
            .old_uid = old_uid,
            .new_uid = new_uid,
            .pid = current->pid,
            .tgid = current->tgid,
            .old_allowlisted = __ksu_is_allow_uid(old_uid),
            .new_allowlisted = __ksu_is_allow_uid(new_uid),
            .is_zygote_child = is_zygote(current_cred()),
        };
        ksu_event_emit(KSU_EVENT_UID_COMMITTED, &event, 0);
    }

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

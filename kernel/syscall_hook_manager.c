#include "linux/compiler.h"
#include "linux/cred.h"
#include "linux/kallsyms.h"
#include "linux/printk.h"
#include "selinux/selinux.h"
#include <linux/spinlock.h>
#include <linux/kprobes.h>
#include <linux/tracepoint.h>
#include <asm/syscall.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <trace/events/syscalls.h>

#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "syscall_hook_manager.h"
#include "tp_marker.h"
#include "sucompat.h"
#include "setuid_hook.h"
#include "selinux/selinux.h"
#include "ksud.h"
#include "hook/syscall_hook.h"

#ifdef CONFIG_KRETPROBES

static struct kretprobe *init_kretprobe(const char *name,
                                        kretprobe_handler_t handler)
{
    struct kretprobe *rp = kzalloc(sizeof(struct kretprobe), GFP_KERNEL);
    if (!rp)
        return NULL;
    rp->kp.symbol_name = name;
    rp->handler = handler;
    rp->data_size = 0;
    rp->maxactive = 0;

    int ret = register_kretprobe(rp);
    pr_info("hook_manager: register_%s kretprobe: %d\n", name, ret);
    if (ret) {
        kfree(rp);
        return NULL;
    }

    return rp;
}

static void destroy_kretprobe(struct kretprobe **rp_ptr)
{
    struct kretprobe *rp = *rp_ptr;
    if (!rp)
        return;
    unregister_kretprobe(rp);
    synchronize_rcu();
    kfree(rp);
    *rp_ptr = NULL;
}

static int syscall_regfunc_handler(struct kretprobe_instance *ri,
                                   struct pt_regs *regs)
{
    unsigned long flags;
    ksu_tp_marker_lock(&flags);
    if (ksu_tp_marker_reg_count() < 1) {
        // while install our tracepoint, mark our processes
        ksu_mark_running_process_locked();
    } else if (ksu_tp_marker_reg_count() == 1) {
        // while other tracepoint first added, mark all processes
        ksu_mark_all_process();
    }
    ksu_tp_marker_inc_reg_count();
    ksu_tp_marker_unlock(&flags);
    return 0;
}

static int syscall_unregfunc_handler(struct kretprobe_instance *ri,
                                     struct pt_regs *regs)
{
    unsigned long flags;
    ksu_tp_marker_lock(&flags);
    ksu_tp_marker_dec_reg_count();
    if (ksu_tp_marker_reg_count() <= 0) {
        // while no tracepoint left, unmark all processes
        ksu_unmark_all_process();
    } else if (ksu_tp_marker_reg_count() == 1) {
        // while just our tracepoint left, unmark disallowed processes
        ksu_mark_running_process_locked();
    }
    ksu_tp_marker_unlock(&flags);
    return 0;
}

static struct kretprobe *syscall_regfunc_rp = NULL;
static struct kretprobe *syscall_unregfunc_rp = NULL;
#endif

static inline bool check_syscall_fastpath(int nr)
{
    switch (nr) {
    case __NR_newfstatat:
    case __NR_faccessat:
    case __NR_execve:
    case __NR_setresuid:
        return true;
    default:
        return false;
    }
}

// Unmark init's child that are not zygote, adbd or ksud
int ksu_handle_init_mark_tracker(const char __user **filename_user)
{
    char path[64];
    unsigned long addr;
    const char __user *fn;

    if (unlikely(!filename_user))
        return 0;

    addr = untagged_addr((unsigned long)*filename_user);
    fn = (const char __user *)addr;

    memset(path, 0, sizeof(path));
    strncpy_from_user(path, fn, sizeof(path));

    if (unlikely(strcmp(path, KSUD_PATH) == 0)) {
        pr_info("hook_manager: escape to root for init executing ksud: %d\n",
                current->pid);
        escape_to_root_for_init();
    } else if (likely(strstr(path, "/app_process") == NULL &&
                      strstr(path, "/adbd") == NULL)) {
        pr_info("hook_manager: unmark %d exec %s\n", current->pid, path);
        ksu_clear_task_tracepoint_flag_if_needed(current);
    }

    return 0;
}

#define MAGIC_VALUE 0xdeadbeef

#define CHECK_SYSCALL                                                          \
    if (regs->unused2 != MAGIC_VALUE)                                          \
        return -ENOSYS;                                                        \
    ((struct pt_regs *)regs)->unused2 = 0;

static long ksu_syscall_newfstatat(const struct pt_regs *regs)
{
    CHECK_SYSCALL

    int *dfd = (int *)&PT_REGS_PARM1(regs);
    const char __user **filename_user =
        (const char __user **)&PT_REGS_PARM2(regs);
    int *flags = (int *)&PT_REGS_SYSCALL_PARM4(regs);
    ksu_handle_stat(dfd, filename_user, flags);

    return ksu_syscall_table[__NR_newfstatat](regs);
}

static long ksu_syscall_faccessat(const struct pt_regs *regs)
{
    CHECK_SYSCALL

    int *dfd = (int *)&PT_REGS_PARM1(regs);
    const char __user **filename_user =
        (const char __user **)&PT_REGS_PARM2(regs);
    int *mode = (int *)&PT_REGS_PARM3(regs);
    ksu_handle_faccessat(dfd, filename_user, mode, NULL);

    return ksu_syscall_table[__NR_faccessat](regs);
}

static long ksu_syscall_execve(const struct pt_regs *regs)
{
    int ret;
    CHECK_SYSCALL

    const char __user **filename_user =
        (const char __user **)&PT_REGS_PARM1(regs);
    if (current->pid != 1 && is_init(get_current_cred())) {
        ksu_handle_init_mark_tracker(filename_user);
    } else {
        ret = ksu_handle_execve_sucompat(filename_user, NULL, NULL, NULL);
        if (ret < 0) {
            return ret;
        }
    }

    return ksu_syscall_table[__NR_execve](regs);
}

static long ksu_syscall_setresuid(const struct pt_regs *regs)
{
    CHECK_SYSCALL

    uid_t old_uid = current_uid().val;

    long ret = ksu_syscall_table[__NR_setresuid](regs);

    if (ret < 0)
        return ret;

    ksu_handle_setresuid(old_uid, current_uid().val);

    return ret;
}

static int nr_for_setresuid = -1;
static int nr_for_execve = -1;
static int nr_for_newfstatat = -1;
static int nr_for_faccessat = -1;

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
// Generic sys_enter handler that dispatches to specific handlers
static void ksu_sys_enter_handler(void *data, struct pt_regs *regs, long id)
{
    if (unlikely(is_compat_task())) {
        return;
    }
    struct pt_regs *current_regs = task_pt_regs(current);
    if (unlikely(check_syscall_fastpath(id))) {
        if (ksu_su_compat_enabled) {
            // Handle newfstatat
            if (id == __NR_newfstatat) {
                if (nr_for_newfstatat < 0)
                    return;
                current_regs->syscallno = nr_for_newfstatat;
                current_regs->unused2 = MAGIC_VALUE;
                return;
            }

            // Handle faccessat
            if (id == __NR_faccessat) {
                if (nr_for_faccessat < 0)
                    return;
                current_regs->syscallno = nr_for_faccessat;
                current_regs->unused2 = MAGIC_VALUE;
                return;
            }

            // Handle execve
            if (id == __NR_execve) {
                if (nr_for_execve < 0)
                    return;
                current_regs->syscallno = nr_for_execve;
                current_regs->unused2 = MAGIC_VALUE;
                return;
            }
        }

        // Handle setresuid
        if (id == __NR_setresuid) {
            if (nr_for_setresuid < 0)
                return;
            current_regs->syscallno = nr_for_setresuid;
            current_regs->unused2 = MAGIC_VALUE;
            return;
        }
    }
}
#endif

#define KSU_MAX_NI_SYSCALL_SLOTS 4

void ksu_syscall_hook_manager_init(void)
{
    int ret, nr_count;
    int slots[KSU_MAX_NI_SYSCALL_SLOTS];
    pr_info("hook_manager: ksu_hook_manager_init called\n");

#ifdef CONFIG_KRETPROBES
    // Register kretprobe for syscall_regfunc
    syscall_regfunc_rp =
        init_kretprobe("syscall_regfunc", syscall_regfunc_handler);
    // Register kretprobe for syscall_unregfunc
    syscall_unregfunc_rp =
        init_kretprobe("syscall_unregfunc", syscall_unregfunc_handler);
#endif

    nr_count = ksu_find_ni_syscall_slots(slots, KSU_MAX_NI_SYSCALL_SLOTS);
    if (nr_count != KSU_MAX_NI_SYSCALL_SLOTS) {
        pr_err("not enough ni_syscall: %d !!!\n", nr_count);
    }

    nr_for_setresuid = (nr_count > 0) ? slots[0] : -1;
    nr_for_execve = (nr_count > 1) ? slots[1] : -1;
    nr_for_newfstatat = (nr_count > 2) ? slots[2] : -1;
    nr_for_faccessat = (nr_count > 3) ? slots[3] : -1;

    ksu_replace_syscall_table(nr_for_setresuid, ksu_syscall_setresuid, NULL);
    ksu_replace_syscall_table(nr_for_execve, ksu_syscall_execve, NULL);
    ksu_replace_syscall_table(nr_for_newfstatat, ksu_syscall_newfstatat, NULL);
    ksu_replace_syscall_table(nr_for_faccessat, ksu_syscall_faccessat, NULL);

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    ret = register_trace_sys_enter(ksu_sys_enter_handler, NULL);
#ifndef CONFIG_KRETPROBES
    ksu_mark_running_process_locked();
#endif
    if (ret) {
        pr_err("hook_manager: failed to register sys_enter tracepoint: %d\n",
               ret);
    } else {
        pr_info("hook_manager: sys_enter tracepoint registered\n");
    }
#endif

    ksu_setuid_hook_init();
    ksu_sucompat_init();
}

void ksu_syscall_hook_manager_exit(void)
{
    pr_info("hook_manager: ksu_hook_manager_exit called\n");
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    unregister_trace_sys_enter(ksu_sys_enter_handler, NULL);
    tracepoint_synchronize_unregister();
    pr_info("hook_manager: sys_enter tracepoint unregistered\n");
#endif

#ifdef CONFIG_KRETPROBES
    destroy_kretprobe(&syscall_regfunc_rp);
    destroy_kretprobe(&syscall_unregfunc_rp);
#endif

    ksu_syscall_hook_exit();

    ksu_sucompat_exit();
    ksu_setuid_hook_exit();
}

#include "linux/printk.h"
#include <linux/cred.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/tracepoint.h>
#include <linux/kprobes.h>
#include <asm/syscall.h>
#include <linux/ptrace.h>
#include <trace/events/syscalls.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 7, 0)
#include <linux/compat.h>
#include <linux/sched/task_stack.h>
#endif

#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "hook/syscall_hook_manager.h"
#include "hook/tp_marker.h"
#include "feature/sucompat.h"
#include "feature/selinux_hide.h"
#include "hook/setuid_hook.h"
#include "hook/syscall_hook.h"
#include "hook/syscall_event_bridge.h"
#include "selinux/selinux.h"

static bool manager_cleanup_done;
static bool sys_enter_registered;

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
/* Conservative until the mutation observer proves KernelSU is the sole owner. */
static bool syscall_tracepoint_shared = true;
#endif

enum ksu_unload_state {
    KSU_UNLOAD_RUNNING,
    KSU_UNLOAD_PREPARING,
    KSU_UNLOAD_PREPARED,
    KSU_UNLOAD_COMMITTED,
    KSU_UNLOAD_FAILED,
};

static enum ksu_unload_state unload_state;
static unsigned int active_runtime_calls;
static DEFINE_MUTEX(unload_state_lock);

#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
static DEFINE_MUTEX(unload_guard_lock);
static bool unload_guard_held;
static bool prepared_guard_needed;
#endif

bool ksu_module_unload_in_progress(void)
{
    return READ_ONCE(unload_state) != KSU_UNLOAD_RUNNING;
}

bool ksu_module_unload_recovery_allowed(void)
{
    enum ksu_unload_state state = READ_ONCE(unload_state);

    return state == KSU_UNLOAD_PREPARED || state == KSU_UNLOAD_COMMITTED;
}

bool ksu_syscall_tracepoint_allows_selective_marks(void)
{
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    return READ_ONCE(sys_enter_registered) && !READ_ONCE(syscall_tracepoint_shared);
#else
    return false;
#endif
}

bool ksu_module_unload_try_enter(void)
{
    bool entered = false;

    mutex_lock(&unload_state_lock);
    if (unload_state == KSU_UNLOAD_RUNNING) {
        active_runtime_calls++;
        entered = true;
    }
    mutex_unlock(&unload_state_lock);
    return entered;
}

bool ksu_module_control_try_enter(void)
{
    bool entered = false;

    mutex_lock(&unload_state_lock);
    if (unload_state == KSU_UNLOAD_RUNNING || unload_state == KSU_UNLOAD_PREPARED ||
        unload_state == KSU_UNLOAD_COMMITTED) {
        active_runtime_calls++;
        entered = true;
    }
    mutex_unlock(&unload_state_lock);
    return entered;
}

void ksu_module_unload_leave(void)
{
    mutex_lock(&unload_state_lock);
    if (active_runtime_calls > 0)
        active_runtime_calls--;
    mutex_unlock(&unload_state_lock);
}

void ksu_syscall_hook_hold_unload_guard(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    mutex_lock(&unload_guard_lock);
    if (!unload_guard_held) {
        __module_get(THIS_MODULE);
        unload_guard_held = true;
        pr_debug("hook_manager: module unload guard acquired\n");
    }
    mutex_unlock(&unload_guard_lock);
#endif
}

static void release_unload_guard(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    mutex_lock(&unload_guard_lock);
    if (unload_guard_held) {
        unload_guard_held = false;
        module_put(THIS_MODULE);
        pr_debug("hook_manager: module unload guard released\n");
    }
    mutex_unlock(&unload_guard_lock);
#endif
}

static bool unload_guard_is_held(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    bool held;

    mutex_lock(&unload_guard_lock);
    held = unload_guard_held;
    mutex_unlock(&unload_guard_lock);
    return held;
#else
    return false;
#endif
}

static int preflight_unload_refs(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    int expected_refs = 1;
    int refs;

    if (active_runtime_calls != 0) {
        pr_debug("hook_manager: unload blocked by %u active runtime calls\n", active_runtime_calls);
        return -EBUSY;
    }

    if (unload_guard_is_held())
        expected_refs++;

    refs = module_refcount(THIS_MODULE);
    if (refs != expected_refs) {
        pr_warn("hook_manager: unload blocked by module refs: have=%d expected=%d\n", refs, expected_refs);
        return -EBUSY;
    }

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
static __always_inline bool ksu_syscall_id_maybe_hooked(long id)
{
    switch (id) {
    case __NR_setresuid:
    case __NR_execve:
    case __NR_execveat:
    case __NR_newfstatat:
    case __NR_faccessat:
        return true;
    default:
        return false;
    }
}

static __always_inline bool ksu_current_task_needs_hook_for_syscall(long id)
{
    if (ksu_current_task_needs_syscall_hook())
        return true;

    /*
     * init forks children before execing adbd, app_process/stub_zygote, and
     * other boot helpers. Those children are no longer PID 1 but still run in
     * the init SELinux domain, and the exec hook intentionally handles them.
     * Keep this exception scoped to exec so unrelated init-domain syscalls do
     * not pay the dispatcher cost.
     */
    return (id == __NR_execve || id == __NR_execveat) && is_init(current_cred());
}

static void ksu_sys_enter_handler(void *data, struct pt_regs *regs, long id)
{
    struct pt_regs *current_regs;

    if (likely(!ksu_syscall_id_maybe_hooked(id)))
        return;

    if (unlikely(ksu_module_unload_in_progress()))
        return;

    /* Never rewrite a syscall unless the dispatcher slot is installed. */
    if (unlikely(READ_ONCE(ksu_dispatcher_nr) < 0))
        return;

    if (!ksu_has_syscall_hook(id))
        return;

#if defined(__x86_64__)
    if (unlikely(in_compat_syscall()))
#elif defined(__aarch64__)
    if (unlikely(is_compat_task()))
#endif
        return;

    if (unlikely(!ksu_current_task_needs_hook_for_syscall(id)))
        return;

    current_regs = task_pt_regs(current);

#if defined(__x86_64__)
    current_regs->ax = id;
    current_regs->orig_ax = ksu_dispatcher_nr;
#elif defined(__aarch64__)
    PT_REGS_ORIG_SYSCALL(current_regs) = id;
    current_regs->syscallno = ksu_dispatcher_nr;
#endif
}

static bool ksu_sys_enter_has_external_consumer(void)
{
    struct tracepoint_func *funcs;
    bool external = false;
    int i;

    rcu_read_lock();
    funcs = rcu_dereference(__tracepoint_sys_enter.funcs);
    if (funcs) {
        for (i = 0; READ_ONCE(funcs[i].func); i++) {
            if (READ_ONCE(funcs[i].func) != (void *)ksu_sys_enter_handler) {
                external = true;
                break;
            }
        }
    }
    rcu_read_unlock();
    return external;
}

static void ksu_refresh_sys_enter_marks(void)
{
    bool shared = ksu_sys_enter_has_external_consumer();

    WRITE_ONCE(syscall_tracepoint_shared, shared);
    if (shared)
        ksu_mark_all_process();
    else
        ksu_mark_running_process_selective();
}

#ifdef CONFIG_KRETPROBES
struct ksu_tracepoint_mutation_ctx {
    bool target;
    bool add;
};

static bool ksu_tracepoint_mutation_is_external(struct pt_regs *regs)
{
    struct tracepoint *tp = (struct tracepoint *)PT_REGS_PARM1(regs);
    struct tracepoint_func *func = (struct tracepoint_func *)PT_REGS_PARM2(regs);

    if (tp != &__tracepoint_sys_enter)
        return false;

    return !func || READ_ONCE(func->func) != (void *)ksu_sys_enter_handler;
}

static int ksu_tracepoint_add_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct ksu_tracepoint_mutation_ctx *ctx = (struct ksu_tracepoint_mutation_ctx *)ri->data;

    ctx->target = ksu_tracepoint_mutation_is_external(regs);
    ctx->add = true;
    if (ctx->target) {
        /*
         * tracepoint_add_func() has not activated the new callback yet. Mark
         * every task now so no external perf/ftrace/eBPF consumer can miss a
         * syscall in the 1->2 transition window.
         */
        WRITE_ONCE(syscall_tracepoint_shared, true);
        ksu_mark_all_process();
    }
    return 0;
}

static int ksu_tracepoint_remove_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct ksu_tracepoint_mutation_ctx *ctx = (struct ksu_tracepoint_mutation_ctx *)ri->data;

    ctx->target = ksu_tracepoint_mutation_is_external(regs);
    ctx->add = false;
    return 0;
}

static int ksu_tracepoint_mutation_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct ksu_tracepoint_mutation_ctx *ctx = (struct ksu_tracepoint_mutation_ctx *)ri->data;
    long ret = (long)PT_REGS_RC(regs);

    if (!ctx->target)
        return 0;

    /*
     * Failed additions need their speculative global marks undone. Successful
     * removals may have left KernelSU as the sole consumer. Re-read the actual
     * callback array instead of inferring consumer count from regfunc calls.
     */
    if (ret || !ctx->add)
        ksu_refresh_sys_enter_marks();
    return 0;
}

static struct kretprobe tracepoint_add_rp = {
    .kp.symbol_name = "tracepoint_add_func",
    .handler = ksu_tracepoint_mutation_ret,
    .entry_handler = ksu_tracepoint_add_entry,
    .data_size = sizeof(struct ksu_tracepoint_mutation_ctx),
    .maxactive = 32,
};

static struct kretprobe tracepoint_remove_rp = {
    .kp.symbol_name = "tracepoint_remove_func",
    .handler = ksu_tracepoint_mutation_ret,
    .entry_handler = ksu_tracepoint_remove_entry,
    .data_size = sizeof(struct ksu_tracepoint_mutation_ctx),
    .maxactive = 32,
};

static bool tracepoint_observer_registered;

static bool ksu_tracepoint_observer_init(void)
{
    int ret;

    ret = register_kretprobe(&tracepoint_add_rp);
    if (ret) {
        pr_warn("hook_manager: tracepoint_add_func observer unavailable: %d\n", ret);
        return false;
    }

    ret = register_kretprobe(&tracepoint_remove_rp);
    if (ret) {
        pr_warn("hook_manager: tracepoint_remove_func observer unavailable: %d\n", ret);
        unregister_kretprobe(&tracepoint_add_rp);
        return false;
    }

    tracepoint_observer_registered = true;
    return true;
}

static void ksu_tracepoint_observer_exit(void)
{
    if (!tracepoint_observer_registered)
        return;

    unregister_kretprobe(&tracepoint_remove_rp);
    unregister_kretprobe(&tracepoint_add_rp);
    tracepoint_observer_registered = false;
}
#else
static bool ksu_tracepoint_observer_init(void)
{
    return false;
}

static void ksu_tracepoint_observer_exit(void)
{
}
#endif
#endif

void __init ksu_syscall_hook_manager_init(void)
{
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    bool observer_ready;
    int ret;
#endif

    manager_cleanup_done = false;
    sys_enter_registered = false;
    active_runtime_calls = 0;
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    prepared_guard_needed = false;
#endif
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    WRITE_ONCE(syscall_tracepoint_shared, true);
#endif
    WRITE_ONCE(unload_state, KSU_UNLOAD_RUNNING);
    pr_info("hook_manager: ksu_hook_manager_init called\n");

    ksu_register_syscall_hook(__NR_setresuid, ksu_hook_setresuid);
    ksu_register_syscall_hook(__NR_execve, ksu_hook_execve);
    ksu_register_syscall_hook(__NR_execveat, ksu_hook_execveat);
    ksu_register_syscall_hook(__NR_newfstatat, ksu_hook_newfstatat);
    ksu_register_syscall_hook(__NR_faccessat, ksu_hook_faccessat);

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    if (READ_ONCE(ksu_dispatcher_nr) < 0) {
        pr_warn("hook_manager: dispatcher unavailable; skipping sys_enter tracepoint\n");
    } else {
        observer_ready = ksu_tracepoint_observer_init();
        ret = register_trace_prio_sys_enter(ksu_sys_enter_handler, NULL, INT_MIN);
        if (ret) {
            pr_err("hook_manager: failed to register sys_enter tracepoint: %d\n", ret);
            ksu_tracepoint_observer_exit();
        } else {
            sys_enter_registered = true;
            if (observer_ready) {
                ksu_refresh_sys_enter_marks();
                pr_info("hook_manager: sys_enter tracepoint registered with coexistence observer\n");
            } else {
                /* Cannot observe later 1->2 transitions, so preserve correctness. */
                WRITE_ONCE(syscall_tracepoint_shared, true);
                ksu_mark_all_process();
                pr_warn("hook_manager: sys_enter observer unavailable; using global tracepoint marks\n");
            }
        }
    }
#endif

    ksu_setuid_hook_init();
    ksu_sucompat_init();
}

static void finish_hook_cleanup(void)
{
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    if (sys_enter_registered) {
        unregister_trace_sys_enter(ksu_sys_enter_handler, NULL);
        tracepoint_synchronize_unregister();
        sys_enter_registered = false;
        pr_info("hook_manager: sys_enter tracepoint unregistered\n");
    }
    ksu_tracepoint_observer_exit();
    WRITE_ONCE(syscall_tracepoint_shared, true);
#endif

    synchronize_rcu_tasks();
    ksu_syscall_hook_finish_exit();
    ksu_sucompat_exit();
    ksu_setuid_hook_exit();
    manager_cleanup_done = true;
}

int ksu_prepare_module_unload(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    int rollback_ret;
    int ret;

    mutex_lock(&unload_state_lock);

    if (unload_state == KSU_UNLOAD_PREPARED) {
        mutex_unlock(&unload_state_lock);
        return 0;
    }
    if (manager_cleanup_done || unload_state != KSU_UNLOAD_RUNNING) {
        mutex_unlock(&unload_state_lock);
        return -EBUSY;
    }

    WRITE_ONCE(unload_state, KSU_UNLOAD_PREPARING);

    ret = preflight_unload_refs();
    if (ret) {
        WRITE_ONCE(unload_state, KSU_UNLOAD_RUNNING);
        mutex_unlock(&unload_state_lock);
        return ret;
    }

    /*
     * A sys_enter callback can have rewritten pt_regs just before PREPARING
     * became visible, then be preempted before the syscall table dispatch.
     * Drain those tasks before restoring the dispatcher slot.
     */
    synchronize_rcu_tasks();

    ret = ksu_syscall_hook_exit();
    if (ret) {
        WRITE_ONCE(unload_state, ret == -EUCLEAN ? KSU_UNLOAD_FAILED : KSU_UNLOAD_RUNNING);
        pr_err("hook_manager: syscall patch restore failed: %d\n", ret);
        mutex_unlock(&unload_state_lock);
        return ret;
    }

    ret = ksu_selinux_hide_prepare_unload();
    if (ret) {
        rollback_ret = ksu_syscall_hook_abort_exit();
        if (rollback_ret)
            pr_err("hook_manager: syscall rollback after SELinux prepare failure failed: %d\n", rollback_ret);
        WRITE_ONCE(unload_state, KSU_UNLOAD_FAILED);
        pr_err("hook_manager: SELinux patch restore failed: %d\n", ret);
        mutex_unlock(&unload_state_lock);
        return rollback_ret ? -EUCLEAN : ret;
    }

    /* Drain module-text callbacks after all persistent entry points are gone. */
    synchronize_rcu_tasks();
    prepared_guard_needed = unload_guard_is_held();
    WRITE_ONCE(unload_state, KSU_UNLOAD_PREPARED);
    mutex_unlock(&unload_state_lock);
    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

int ksu_commit_module_unload(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    int ret;

    mutex_lock(&unload_state_lock);
    if (unload_state != KSU_UNLOAD_PREPARED) {
        mutex_unlock(&unload_state_lock);
        return -EINVAL;
    }

    ret = preflight_unload_refs();
    if (ret) {
        mutex_unlock(&unload_state_lock);
        return ret;
    }

    WRITE_ONCE(unload_state, KSU_UNLOAD_COMMITTED);
    release_unload_guard();
    mutex_unlock(&unload_state_lock);
    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

int ksu_abort_module_unload(void)
{
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    int rollback_ret;
    int ret;

    mutex_lock(&unload_state_lock);
    if (unload_state == KSU_UNLOAD_RUNNING) {
        mutex_unlock(&unload_state_lock);
        return 0;
    }
    if (unload_state != KSU_UNLOAD_PREPARED && unload_state != KSU_UNLOAD_COMMITTED) {
        mutex_unlock(&unload_state_lock);
        return -EINVAL;
    }

    if (unload_state == KSU_UNLOAD_COMMITTED && prepared_guard_needed)
        ksu_syscall_hook_hold_unload_guard();

    ret = ksu_selinux_hide_abort_unload();
    if (ret) {
        WRITE_ONCE(unload_state, KSU_UNLOAD_FAILED);
        pr_err("hook_manager: SELinux patch re-arm failed: %d\n", ret);
        mutex_unlock(&unload_state_lock);
        return ret;
    }

    ret = ksu_syscall_hook_abort_exit();
    if (ret) {
        rollback_ret = ksu_selinux_hide_prepare_unload();
        if (rollback_ret)
            pr_err("hook_manager: SELinux rollback after syscall re-arm failure failed: %d\n", rollback_ret);
        WRITE_ONCE(unload_state, rollback_ret || ret == -EUCLEAN ? KSU_UNLOAD_FAILED : KSU_UNLOAD_PREPARED);
        pr_err("hook_manager: syscall patch re-arm failed: %d\n", ret);
        mutex_unlock(&unload_state_lock);
        return rollback_ret ? -EUCLEAN : ret;
    }

    prepared_guard_needed = false;
    WRITE_ONCE(unload_state, KSU_UNLOAD_RUNNING);
    mutex_unlock(&unload_state_lock);
    return 0;
#else
    return -EOPNOTSUPP;
#endif
}

int ksu_syscall_hook_manager_exit(void)
{
    int ret;

    mutex_lock(&unload_state_lock);
    if (manager_cleanup_done) {
        mutex_unlock(&unload_state_lock);
        return 0;
    }

    WRITE_ONCE(unload_state, KSU_UNLOAD_PREPARING);

    /*
     * module_exit() cannot reject an unload once it is running. The normal
     * PREPARE/COMMIT path prevents us from reaching this point with live
     * persistent patches. If a forced or unexpected unload violates that
     * invariant, continuing after a failed restore would free module text
     * while a syscall/text entry point can still target it. Fail-stop instead.
     */
    ret = ksu_syscall_hook_exit();
    if (ret) {
        WRITE_ONCE(unload_state, KSU_UNLOAD_FAILED);
        pr_emerg("hook_manager: final syscall patch restore failed: %d\n", ret);
        mutex_unlock(&unload_state_lock);
        panic("KernelSU: refusing unsafe module unload with live syscall hooks");
    }

    ret = ksu_selinux_hide_prepare_unload();
    if (ret) {
        WRITE_ONCE(unload_state, KSU_UNLOAD_FAILED);
        pr_emerg("hook_manager: final SELinux patch restore failed: %d\n", ret);
        mutex_unlock(&unload_state_lock);
        panic("KernelSU: refusing unsafe module unload with live SELinux hooks");
    }

    finish_hook_cleanup();
    WRITE_ONCE(unload_state, KSU_UNLOAD_PREPARED);
#if defined(MODULE) && defined(CONFIG_MODULE_UNLOAD)
    prepared_guard_needed = false;
#endif
    release_unload_guard();
    mutex_unlock(&unload_state_lock);
    return 0;
}

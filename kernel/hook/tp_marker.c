#include "hook/tp_marker.h"

#include "linux/cred.h"
#include <linux/version.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>

#include "policy/allowlist.h"
#include "klog.h" // IWYU pragma: keep
#include "selinux/selinux.h"
#include "hook/syscall_hook_manager.h"

static bool ksu_task_needs_syscall_hook(struct task_struct *task, const struct cred *cred, uid_t uid)
{
    return task->pid == 1 || uid == 2000 || is_zygote(cred) || (uid == 0 && is_task_ksu_domain(cred)) ||
           ksu_is_allow_uid(uid);
}

bool ksu_current_task_needs_syscall_hook(void)
{
    /*
     * The syscall tracepoint mark is also exposed through KSU_MARK_MARK.
     * Treat it as authoritative here so explicit marks are not rejected by
     * the automatic policy predicate used when refreshing selective marks.
     */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    return test_task_syscall_work(current, SYSCALL_TRACEPOINT);
#else
    return test_tsk_thread_flag(current, TIF_SYSCALL_TRACEPOINT);
#endif
}

static void handle_process_mark(bool mark_all, bool clear_unneeded)
{
    struct task_struct *p, *t;

    read_lock(&tasklist_lock);
    for_each_process_thread (p, t) {
        const struct cred *cred;
        bool needs_hook = false;
        uid_t uid;

        if (mark_all) {
            ksu_set_task_tracepoint_flag(t);
            continue;
        }

        if (t->pid == 1 || t->mm) {
            uid = task_uid(t).val;
            cred = get_task_cred(t);
            needs_hook = ksu_task_needs_syscall_hook(t, cred, uid);
            put_cred(cred);
        }

        if (needs_hook)
            ksu_set_task_tracepoint_flag(t);
        else if (clear_unneeded)
            ksu_clear_task_tracepoint_flag(t);
    }
    read_unlock(&tasklist_lock);
}

static void handle_process_unmark_all(void)
{
    struct task_struct *p, *t;

    read_lock(&tasklist_lock);
    for_each_process_thread (p, t)
        ksu_clear_task_tracepoint_flag(t);
    read_unlock(&tasklist_lock);
}

void ksu_mark_all_process(void)
{
    handle_process_mark(true, false);
}

void ksu_unmark_all_process(void)
{
    if (!ksu_syscall_tracepoint_allows_selective_marks()) {
        pr_warn("tp_marker: refusing to clear shared syscall tracepoint marks\n");
        return;
    }

    handle_process_unmark_all();
}

void ksu_mark_running_process_selective(void)
{
    handle_process_mark(false, true);
}

void ksu_mark_running_process(void)
{
    /*
     * Policy refreshes may run while perf/ftrace/eBPF also owns sys_enter.
     * Only clear stale marks when the hook manager has proved KernelSU is the
     * sole consumer; otherwise add marks for newly relevant tasks only.
     */
    handle_process_mark(false, ksu_syscall_tracepoint_allows_selective_marks());
}

void ksu_clear_task_tracepoint_flag_if_needed(struct task_struct *t)
{
    if (ksu_syscall_tracepoint_allows_selective_marks())
        ksu_clear_task_tracepoint_flag(t);
}

int ksu_get_task_mark(pid_t pid)
{
    struct task_struct *task;
    int marked = -ESRCH;

    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task) {
        get_task_struct(task);
        rcu_read_unlock();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
        marked = test_task_syscall_work(task, SYSCALL_TRACEPOINT) ? 1 : 0;
#else
        marked = test_tsk_thread_flag(task, TIF_SYSCALL_TRACEPOINT) ? 1 : 0;
#endif
        put_task_struct(task);
    } else {
        rcu_read_unlock();
    }

    return marked;
}

int ksu_set_task_mark(pid_t pid, bool mark)
{
    struct task_struct *task;
    int ret = -ESRCH;

    if (!mark && !ksu_syscall_tracepoint_allows_selective_marks())
        return -EBUSY;

    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task) {
        get_task_struct(task);
        rcu_read_unlock();
        if (mark)
            ksu_set_task_tracepoint_flag(task);
        else
            ksu_clear_task_tracepoint_flag(task);
        put_task_struct(task);
        ret = 0;
    } else {
        rcu_read_unlock();
    }

    return ret;
}

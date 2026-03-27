#include "tp_marker.h"

#include "linux/cred.h"
#include <linux/spinlock.h>
#include <linux/version.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>

#include "allowlist.h"
#include "klog.h" // IWYU pragma: keep
#include "selinux/selinux.h"

// Tracepoint registration count management
// == 1: just us
// >  1: someone else is also using syscall tracepoint e.g. ftrace
static int tracepoint_reg_count = 0;
static DEFINE_SPINLOCK(tracepoint_reg_lock);

int ksu_tp_marker_reg_count(void)
{
    return tracepoint_reg_count;
}

void ksu_tp_marker_lock(unsigned long *flags)
{
    spin_lock_irqsave(&tracepoint_reg_lock, *flags);
}

void ksu_tp_marker_unlock(unsigned long *flags)
{
    spin_unlock_irqrestore(&tracepoint_reg_lock, *flags);
}

void ksu_tp_marker_inc_reg_count(void)
{
    tracepoint_reg_count++;
}

void ksu_tp_marker_dec_reg_count(void)
{
    tracepoint_reg_count--;
}

void ksu_clear_task_tracepoint_flag_if_needed(struct task_struct *t)
{
    unsigned long flags;
    spin_lock_irqsave(&tracepoint_reg_lock, flags);
    if (tracepoint_reg_count <= 1) {
        ksu_clear_task_tracepoint_flag(t);
    }
    spin_unlock_irqrestore(&tracepoint_reg_lock, flags);
}

// Process marking management
static void handle_process_mark(bool mark)
{
    struct task_struct *p, *t;
    read_lock(&tasklist_lock);
    for_each_process_thread (p, t) {
        if (mark)
            ksu_set_task_tracepoint_flag(t);
        else
            ksu_clear_task_tracepoint_flag(t);
    }
    read_unlock(&tasklist_lock);
}

void ksu_mark_all_process(void)
{
    handle_process_mark(true);
    pr_info("tp_marker: mark all user process done!\n");
}

void ksu_unmark_all_process(void)
{
    handle_process_mark(false);
    pr_info("tp_marker: unmark all user process done!\n");
}

void ksu_mark_running_process_locked(void)
{
    struct task_struct *p, *t;
    read_lock(&tasklist_lock);
    for_each_process_thread (p, t) {
        if (t->pid != 1 && !t->mm) {
            // skip kernel threads, but always allow pid 1
            continue;
        }
        int uid = task_uid(t).val;
        const struct cred *cred = get_task_cred(t);
        bool ksu_root_process = uid == 0 && is_task_ksu_domain(cred);
        bool is_zygote_process = is_zygote(cred);
        bool is_shell = uid == 2000;
        // before boot completed, we shall mark init for marking zygote
        bool is_init = t->pid == 1;
        if (ksu_root_process || is_zygote_process || is_shell || is_init || ksu_is_allow_uid(uid)) {
            ksu_set_task_tracepoint_flag(t);
            pr_info("tp_marker: mark process: pid:%d, uid: %d, comm:%s\n", t->pid, uid, t->comm);
        } else {
            ksu_clear_task_tracepoint_flag(t);
            pr_info("tp_marker: unmark process: pid:%d, uid: %d, comm:%s\n", t->pid, uid, t->comm);
        }
        put_cred(cred);
    }
    read_unlock(&tasklist_lock);
}

void ksu_mark_running_process(void)
{
    unsigned long flags;
    spin_lock_irqsave(&tracepoint_reg_lock, flags);
    if (tracepoint_reg_count <= 1) {
        ksu_mark_running_process_locked();
    } else {
        pr_info("tp_marker: not mark running process since syscall tracepoint is in use\n");
    }
    spin_unlock_irqrestore(&tracepoint_reg_lock, flags);
}

// Get task mark status
// Returns: 1 if marked, 0 if not marked, -ESRCH if task not found
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

// Set task mark status
// Returns: 0 on success, -ESRCH if task not found
int ksu_set_task_mark(pid_t pid, bool mark)
{
    struct task_struct *task;
    int ret = -ESRCH;

    rcu_read_lock();
    task = find_task_by_vpid(pid);
    if (task) {
        get_task_struct(task);
        rcu_read_unlock();
        if (mark) {
            ksu_set_task_tracepoint_flag(task);
            pr_info("tp_marker: marked task pid=%d comm=%s\n", pid, task->comm);
        } else {
            ksu_clear_task_tracepoint_flag(task);
            pr_info("tp_marker: unmarked task pid=%d comm=%s\n", pid, task->comm);
        }
        put_task_struct(task);
        ret = 0;
    } else {
        rcu_read_unlock();
    }

    return ret;
}

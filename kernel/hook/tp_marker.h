#ifndef __KSU_H_TP_MARKER
#define __KSU_H_TP_MARKER

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/thread_info.h>

// Process marking for tracepoint
void ksu_mark_all_process(void);
void ksu_unmark_all_process(void);
void ksu_mark_running_process(void);

// Per-task mark operations
int ksu_get_task_mark(pid_t pid);
int ksu_set_task_mark(pid_t pid, bool mark);

static inline void ksu_set_task_tracepoint_flag(struct task_struct *t)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    set_task_syscall_work(t, SYSCALL_TRACEPOINT);
#else
    set_tsk_thread_flag(t, TIF_SYSCALL_TRACEPOINT);
#endif
}

static inline void ksu_clear_task_tracepoint_flag(struct task_struct *t)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    clear_task_syscall_work(t, SYSCALL_TRACEPOINT);
#else
    clear_tsk_thread_flag(t, TIF_SYSCALL_TRACEPOINT);
#endif
}

void ksu_clear_task_tracepoint_flag_if_needed(struct task_struct *t);

// Used by syscall_hook_manager kretprobe handlers
void ksu_mark_running_process_locked(void);

// Tracepoint registration count management (for kretprobe handlers)
int ksu_tp_marker_reg_count(void);
void ksu_tp_marker_lock(unsigned long *flags);
void ksu_tp_marker_unlock(unsigned long *flags);
void ksu_tp_marker_inc_reg_count(void);
void ksu_tp_marker_dec_reg_count(void);

#endif

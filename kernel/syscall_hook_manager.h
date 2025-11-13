#ifndef __KSU_H_HOOK_MANAGER
#define __KSU_H_HOOK_MANAGER

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/thread_info.h>

// Hook manager initialization and cleanup
void ksu_syscall_hook_manager_init(void);
void ksu_syscall_hook_manager_exit(void);

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

#endif

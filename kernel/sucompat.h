#ifndef __KSU_H_SUCOMPAT
#define __KSU_H_SUCOMPAT
#include <linux/sched.h>
#include <linux/thread_info.h>
#include <linux/version.h>

void ksu_mark_running_process(void);

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
#endif
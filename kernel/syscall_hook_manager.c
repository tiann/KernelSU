#include "linux/compiler.h"
#include "linux/cred.h"
#include "linux/printk.h"
#include "selinux/selinux.h"
#include <linux/spinlock.h>
#include <linux/kprobes.h>
#include <linux/tracepoint.h>
#include <asm/syscall.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <trace/events/syscalls.h>

#include "allowlist.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "syscall_hook_manager.h"
#include "sucompat.h"
#include "setuid_hook.h"
#include "selinux/selinux.h"

// Tracepoint registration count management
// == 1: just us
// >  1: someone else is also using syscall tracepoint e.g. ftrace
static int tracepoint_reg_count = 0;
static DEFINE_SPINLOCK(tracepoint_reg_lock);

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
	for_each_process_thread(p, t) {
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
	pr_info("hook_manager: mark all user process done!\n");
}

void ksu_unmark_all_process(void)
{
	handle_process_mark(false);
	pr_info("hook_manager: unmark all user process done!\n");
}

static void ksu_mark_running_process_locked()
{
	struct task_struct *p, *t;
	read_lock(&tasklist_lock);
	for_each_process_thread (p, t) {
		if (!t->mm) { // only user processes
			continue;
		}
		int uid = task_uid(t).val;
        const struct cred *cred = get_task_cred(t);
		bool ksu_root_process =
			uid == 0 && is_task_ksu_domain(cred);
        bool is_zygote_process = is_zygote(cred);
        bool is_shell = uid == 2000;
        // before boot completed, we shall mark init for marking zygote
        bool is_init = t->pid == 1;
		if (ksu_root_process || is_zygote_process  || is_shell || is_init
            || ksu_is_allow_uid(uid)) {
			ksu_set_task_tracepoint_flag(t);
			pr_info("hook_manager: mark process: pid:%d, uid: %d, comm:%s\n",
					t->pid, uid, t->comm);
		} else {
			ksu_clear_task_tracepoint_flag(t);
			pr_info("hook_manager: unmark process: pid:%d, uid: %d, comm:%s\n",
					t->pid, uid, t->comm);
		}
        put_cred(cred);
	}
	read_unlock(&tasklist_lock);
}

void ksu_mark_running_process()
{
	unsigned long flags;
	spin_lock_irqsave(&tracepoint_reg_lock, flags);
	if (tracepoint_reg_count <= 1) {
		ksu_mark_running_process_locked();
	} else {
		pr_info("hook_manager: not mark running process since syscall tracepoint is in use\n");
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
			pr_info("hook_manager: marked task pid=%d comm=%s\n", pid, task->comm);
		} else {
			ksu_clear_task_tracepoint_flag(task);
			pr_info("hook_manager: unmarked task pid=%d comm=%s\n", pid, task->comm);
		}
		put_task_struct(task);
		ret = 0;
	} else {
		rcu_read_unlock();
	}

	return ret;
}

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

static int syscall_regfunc_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&tracepoint_reg_lock, flags);
	if (tracepoint_reg_count < 1) {
		// while install our tracepoint, mark our processes
		ksu_mark_running_process_locked();
	} else if (tracepoint_reg_count == 1) {
		// while other tracepoint first added, mark all processes
		ksu_mark_all_process();
	}
	tracepoint_reg_count++;
	spin_unlock_irqrestore(&tracepoint_reg_lock, flags);
	return 0;
}

static int syscall_unregfunc_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&tracepoint_reg_lock, flags);
	tracepoint_reg_count--;
	if (tracepoint_reg_count <= 0) {
		// while no tracepoint left, unmark all processes
		ksu_unmark_all_process();
	} else if (tracepoint_reg_count == 1) {
		// while just our tracepoint left, unmark disallowed processes
		ksu_mark_running_process_locked();
	}
	spin_unlock_irqrestore(&tracepoint_reg_lock, flags);
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

    if (unlikely(!filename_user))
        return 0;

    memset(path, 0, sizeof(path));
    strncpy_from_user_nofault(path, *filename_user, sizeof(path));

    if (likely(strstr(path, "/app_process") == NULL && strstr(path, "/adbd") == NULL && strstr(path, "/ksud") == NULL)) {
		pr_info("hook_manager: unmark %d exec %s", current->pid, path);
        ksu_clear_task_tracepoint_flag_if_needed(current);
    }

    return 0;
}

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
// Generic sys_enter handler that dispatches to specific handlers
static void ksu_sys_enter_handler(void *data, struct pt_regs *regs, long id)
{
	if (unlikely(check_syscall_fastpath(id))) {
		if (ksu_su_compat_enabled) {
			// Handle newfstatat
			if (id == __NR_newfstatat) {
				int *dfd = (int *)&PT_REGS_PARM1(regs);
				const char __user **filename_user =
					(const char __user **)&PT_REGS_PARM2(regs);
				int *flags = (int *)&PT_REGS_SYSCALL_PARM4(regs);
				ksu_handle_stat(dfd, filename_user, flags);
				return;
			}

			// Handle faccessat
			if (id == __NR_faccessat) {
				int *dfd = (int *)&PT_REGS_PARM1(regs);
				const char __user **filename_user =
					(const char __user **)&PT_REGS_PARM2(regs);
				int *mode = (int *)&PT_REGS_PARM3(regs);
				ksu_handle_faccessat(dfd, filename_user, mode, NULL);
				return;
			}

			// Handle execve
			if (id == __NR_execve) {
				const char __user **filename_user =
					(const char __user **)&PT_REGS_PARM1(regs);
				if (current->pid != 1 && is_init(get_current_cred())) {
					ksu_handle_init_mark_tracker(filename_user);
				} else {
                    ksu_handle_execve_sucompat(filename_user, NULL, NULL, NULL);
                }
				return;
			}
		}

        // Handle setresuid
		if (id == __NR_setresuid) {
			uid_t ruid = (uid_t)PT_REGS_PARM1(regs);
			uid_t euid = (uid_t)PT_REGS_PARM2(regs);
			uid_t suid = (uid_t)PT_REGS_PARM3(regs);
			ksu_handle_setresuid(ruid, euid, suid);
			return;
		}
	}
}
#endif

void ksu_syscall_hook_manager_init(void)
{
	int ret;
	pr_info("hook_manager: ksu_hook_manager_init called\n");

#ifdef CONFIG_KRETPROBES
	// Register kretprobe for syscall_regfunc
	syscall_regfunc_rp = init_kretprobe("syscall_regfunc", syscall_regfunc_handler);
	// Register kretprobe for syscall_unregfunc
	syscall_unregfunc_rp = init_kretprobe("syscall_unregfunc", syscall_unregfunc_handler);
#endif

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
	ret = register_trace_sys_enter(ksu_sys_enter_handler, NULL);
#ifndef CONFIG_KRETPROBES
	ksu_mark_running_process_locked();
#endif
	if (ret) {
		pr_err("hook_manager: failed to register sys_enter tracepoint: %d\n", ret);
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

	ksu_sucompat_exit();
	ksu_setuid_hook_exit();
}

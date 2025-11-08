#include "linux/compiler.h"
#include "linux/printk.h"
#include "selinux/selinux.h"
#include <linux/spinlock.h>
#include <linux/kprobes.h>
#include <linux/tracepoint.h>
#include <asm/syscall.h>
#include <linux/ptrace.h>
#include <trace/events/syscalls.h>

#include "allowlist.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "hook_manager.h"
#include "sucompat.h"
#include "core_hook.h"
#include "selinux/selinux.h"

// Tracepoint 注册计数管理
static int tracepoint_reg_count = 0;
static DEFINE_SPINLOCK(tracepoint_reg_lock);

// 进程标记管理
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

static void mark_all_process(void)
{
	handle_process_mark(true);
	pr_info("hook_manager: mark all user process done!\n");
}

static void unmark_all_process(void)
{
	handle_process_mark(false);
	pr_info("hook_manager: unmark all user process done!\n");
}

void ksu_mark_running_process(void)
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
		if (ksu_root_process || ksu_is_allow_uid(uid) || is_zygote_process) {
			ksu_set_task_tracepoint_flag(t);
			pr_info("hook_manager: mark process: pid:%d, uid: %d, comm:%s\n",
					t->pid, uid, t->comm);
		}
	}
	read_unlock(&tasklist_lock);
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
		unmark_all_process();
		ksu_mark_running_process();
	} else {
		// while installing other tracepoint, mark all processes
		mark_all_process();
	}
	tracepoint_reg_count++;
	spin_unlock_irqrestore(&tracepoint_reg_lock, flags);
	return 0;
}

static int syscall_unregfunc_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	unsigned long flags;
	spin_lock_irqsave(&tracepoint_reg_lock, flags);
	if (tracepoint_reg_count <= 1) {
		// while uninstall our tracepoint, unmark all processes
		unmark_all_process();
	} else {
		// while uninstalling other tracepoint, mark our processes
		unmark_all_process();
		ksu_mark_running_process();
	}
	tracepoint_reg_count--;
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
    case __NR_reboot:
        return true;
    default:
        return false;
    }
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
				ksu_handle_execve_sucompat(AT_FDCWD, filename_user, NULL, NULL,
							   NULL);
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

void ksu_hook_manager_init(void)
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
	unmark_all_process();
	ksu_mark_running_process();
#endif
	if (ret) {
		pr_err("hook_manager: failed to register sys_enter tracepoint: %d\n", ret);
	} else {
		pr_info("hook_manager: sys_enter tracepoint registered\n");
	}
#endif

	ksu_core_init();
	ksu_sucompat_init();
}

void ksu_hook_manager_exit(void)
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
	ksu_core_exit();
}

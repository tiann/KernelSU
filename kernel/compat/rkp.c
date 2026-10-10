#include <linux/jump_label.h>

#include "compat/jailbreak.h"

DEFINE_STATIC_KEY_FALSE(ksu_rkp_key);

#if defined(CONFIG_KRETPROBES) && defined(__aarch64__)

#include <asm/syscall.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/slab.h>
#include <linux/task_work.h>

#include "feature/sucompat.h"
#include "hook/setuid_hook.h"
#include "hook/syscall_event_bridge.h"
#include "hook/syscall_hook.h"
#include "infra/symbol_resolver.h"
#include "klog.h"
#include "policy/allowlist.h"

static bool setresuid_kretprobe_registered;
static bool samsung_sucompat_kprobes_registered;

#define SAMSUNG_SUCOMPAT_BYPASS_NR (-2)

struct ksu_setresuid_task_work {
    struct callback_head callback;
    uid_t old_uid;
    uid_t new_uid;
};

static bool samsung_sucompat_should_redirect(int syscall_nr)
{
    struct pt_regs *syscall_regs = task_pt_regs(current);

    if (unlikely(syscall_regs->syscallno == SAMSUNG_SUCOMPAT_BYPASS_NR)) {
        syscall_regs->syscallno = syscall_nr;
        return false;
    }

    return ksu_su_compat_enabled &&
           ksu_is_allow_uid_for_current(current_uid().val);
}

static long __nocfi samsung_sucompat_execve(const struct pt_regs *regs)
{
    struct pt_regs *syscall_regs = (struct pt_regs *)regs;
    int syscall_nr = syscall_regs->syscallno;
    long ret;

    syscall_regs->syscallno = SAMSUNG_SUCOMPAT_BYPASS_NR;
    ret = ksu_hook_execve(__NR_execve, regs);
    syscall_regs->syscallno = syscall_nr;
    return ret;
}

static long __nocfi samsung_sucompat_newfstatat(const struct pt_regs *regs)
{
    struct pt_regs *syscall_regs = (struct pt_regs *)regs;
    int syscall_nr = syscall_regs->syscallno;
    long ret;

    syscall_regs->syscallno = SAMSUNG_SUCOMPAT_BYPASS_NR;
    ret = ksu_hook_newfstatat(__NR_newfstatat, regs);
    syscall_regs->syscallno = syscall_nr;
    return ret;
}

static long __nocfi samsung_sucompat_faccessat(const struct pt_regs *regs)
{
    struct pt_regs *syscall_regs = (struct pt_regs *)regs;
    int syscall_nr = syscall_regs->syscallno;
    long ret;

    syscall_regs->syscallno = SAMSUNG_SUCOMPAT_BYPASS_NR;
    ret = ksu_hook_faccessat(__NR_faccessat, regs);
    syscall_regs->syscallno = syscall_nr;
    return ret;
}

static long __nocfi samsung_sucompat_statx(const struct pt_regs *regs)
{
    struct pt_regs *syscall_regs = (struct pt_regs *)regs;
    int syscall_nr = syscall_regs->syscallno;
    long ret;

    syscall_regs->syscallno = SAMSUNG_SUCOMPAT_BYPASS_NR;
    ret = ksu_hook_newfstatat(__NR_statx, regs);
    syscall_regs->syscallno = syscall_nr;
    return ret;
}

static long __nocfi samsung_sucompat_faccessat2(const struct pt_regs *regs)
{
    struct pt_regs *syscall_regs = (struct pt_regs *)regs;
    int syscall_nr = syscall_regs->syscallno;
    long ret;

    syscall_regs->syscallno = SAMSUNG_SUCOMPAT_BYPASS_NR;
    ret = ksu_hook_faccessat(__NR_faccessat2, regs);
    syscall_regs->syscallno = syscall_nr;
    return ret;
}

static int samsung_sucompat_execve_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    if (!samsung_sucompat_should_redirect(__NR_execve))
        return 0;
    instruction_pointer_set(regs, (unsigned long)samsung_sucompat_execve);
    return 1;
}

static int samsung_sucompat_newfstatat_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    if (!samsung_sucompat_should_redirect(__NR_newfstatat))
        return 0;
    instruction_pointer_set(regs, (unsigned long)samsung_sucompat_newfstatat);
    return 1;
}

static int samsung_sucompat_faccessat_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    if (!samsung_sucompat_should_redirect(__NR_faccessat))
        return 0;
    instruction_pointer_set(regs, (unsigned long)samsung_sucompat_faccessat);
    return 1;
}

static int samsung_sucompat_statx_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    if (!samsung_sucompat_should_redirect(__NR_statx))
        return 0;
    instruction_pointer_set(regs, (unsigned long)samsung_sucompat_statx);
    return 1;
}

static int samsung_sucompat_faccessat2_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    if (!samsung_sucompat_should_redirect(__NR_faccessat2))
        return 0;
    instruction_pointer_set(regs, (unsigned long)samsung_sucompat_faccessat2);
    return 1;
}

static struct kprobe samsung_sucompat_execve_kprobe = {
    .pre_handler = samsung_sucompat_execve_pre_handler,
};
static struct kprobe samsung_sucompat_newfstatat_kprobe = {
    .pre_handler = samsung_sucompat_newfstatat_pre_handler,
};
static struct kprobe samsung_sucompat_faccessat_kprobe = {
    .pre_handler = samsung_sucompat_faccessat_pre_handler,
};
static struct kprobe samsung_sucompat_statx_kprobe = {
    .pre_handler = samsung_sucompat_statx_pre_handler,
};
static struct kprobe samsung_sucompat_faccessat2_kprobe = {
    .pre_handler = samsung_sucompat_faccessat2_pre_handler,
};

static int __init samsung_sucompat_init(void)
{
    int ret;

    if (!ksu_syscall_table)
        return -ENOENT;

    samsung_sucompat_execve_kprobe.addr =
        (kprobe_opcode_t *)READ_ONCE(ksu_syscall_table[__NR_execve]);
    samsung_sucompat_newfstatat_kprobe.addr =
        (kprobe_opcode_t *)READ_ONCE(ksu_syscall_table[__NR_newfstatat]);
    samsung_sucompat_faccessat_kprobe.addr =
        (kprobe_opcode_t *)READ_ONCE(ksu_syscall_table[__NR_faccessat]);
    samsung_sucompat_statx_kprobe.addr =
        (kprobe_opcode_t *)READ_ONCE(ksu_syscall_table[__NR_statx]);
    samsung_sucompat_faccessat2_kprobe.addr =
        (kprobe_opcode_t *)READ_ONCE(ksu_syscall_table[__NR_faccessat2]);

    ksu_sucompat_init();

    ret = register_kprobe(&samsung_sucompat_execve_kprobe);
    if (ret)
        goto exit_sucompat;
    ret = register_kprobe(&samsung_sucompat_newfstatat_kprobe);
    if (ret)
        goto unregister_execve;
    ret = register_kprobe(&samsung_sucompat_faccessat_kprobe);
    if (ret)
        goto unregister_newfstatat;
    ret = register_kprobe(&samsung_sucompat_statx_kprobe);
    if (ret)
        goto unregister_faccessat;
    ret = register_kprobe(&samsung_sucompat_faccessat2_kprobe);
    if (ret)
        goto unregister_statx;

    samsung_sucompat_kprobes_registered = true;
    pr_info("KSU: RKP sucompat kprobes registered\n");
    return 0;

unregister_statx:
    unregister_kprobe(&samsung_sucompat_statx_kprobe);
unregister_faccessat:
    unregister_kprobe(&samsung_sucompat_faccessat_kprobe);
unregister_newfstatat:
    unregister_kprobe(&samsung_sucompat_newfstatat_kprobe);
unregister_execve:
    unregister_kprobe(&samsung_sucompat_execve_kprobe);
exit_sucompat:
    return ret;
}

static void __exit samsung_sucompat_exit(void)
{
    if (samsung_sucompat_kprobes_registered) {
        unregister_kprobe(&samsung_sucompat_faccessat2_kprobe);
        unregister_kprobe(&samsung_sucompat_statx_kprobe);
        unregister_kprobe(&samsung_sucompat_faccessat_kprobe);
        unregister_kprobe(&samsung_sucompat_newfstatat_kprobe);
        unregister_kprobe(&samsung_sucompat_execve_kprobe);
        samsung_sucompat_kprobes_registered = false;
    }
    ksu_sucompat_exit();
}

static void setresuid_task_work_func(struct callback_head *callback)
{
    struct ksu_setresuid_task_work *work =
        container_of(callback, struct ksu_setresuid_task_work, callback);
    ksu_handle_setresuid(work->old_uid, work->new_uid);
    kfree(work);
}

static int setresuid_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    *(uid_t *)ri->data = current_uid().val;
    return 0;
}

static int setresuid_return_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct ksu_setresuid_task_work *work;
    uid_t old_uid = *(uid_t *)ri->data;
    uid_t new_uid;

    if (regs_return_value(regs) < 0)
        return 0;

    new_uid = current_uid().val;
    if (old_uid == new_uid)
        return 0;

    work = kzalloc(sizeof(*work), GFP_ATOMIC);
    if (!work)
        return 0;

    work->old_uid = old_uid;
    work->new_uid = new_uid;
    work->callback.func = setresuid_task_work_func;

    if (task_work_add(current, &work->callback, TWA_RESUME))
        kfree(work);

    return 0;
}

static struct kretprobe setresuid_kretprobe = {
    .kp.symbol_name = "__arm64_sys_setresuid",
    .entry_handler = setresuid_entry_handler,
    .handler = setresuid_return_handler,
    .data_size = sizeof(uid_t),
};

static int __init samsung_setresuid_init(void)
{
    int ret = register_kretprobe(&setresuid_kretprobe);

    if (ret) {
        pr_err("KSU: RKP setresuid kretprobe failed: %d\n", ret);
        return ret;
    }
    setresuid_kretprobe_registered = true;
    ksu_setuid_hook_init();
    pr_info("KSU: RKP setresuid kretprobe registered\n");
    return 0;
}

static void __exit samsung_setresuid_exit(void)
{
    if (!setresuid_kretprobe_registered)
        return;
    unregister_kretprobe(&setresuid_kretprobe);
    setresuid_kretprobe_registered = false;
    ksu_setuid_hook_exit();
}

int __init ksu_rkp_hooks_init(void)
{
    int ret;

    samsung_setresuid_init();

    ret = samsung_sucompat_init();
    if (ret)
        pr_err("KSU: RKP sucompat init failed: %d\n", ret);

    return 0;
}

void __exit ksu_rkp_hooks_exit(void)
{
    samsung_sucompat_exit();
    samsung_setresuid_exit();
}

#endif /* CONFIG_KRETPROBES && __aarch64__ */

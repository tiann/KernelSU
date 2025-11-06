#include "linux/compiler.h"
#include "linux/sched.h"
#include "selinux/selinux.h"
#include <linux/dcache.h>
#include <linux/security.h>
#include <asm/current.h>
#include <linux/cred.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/tracepoint.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/sched/task_stack.h>
#include <asm/syscall.h>
#include <trace/events/syscalls.h>

#include "objsec.h"
#include "allowlist.h"
#include "arch.h"
#include "feature.h"
#include "klog.h" // IWYU pragma: keep
#include "ksud.h"
#include "kernel_compat.h"

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

extern void escape_to_root();
void ksu_sucompat_enable();
void ksu_sucompat_disable();

bool ksu_su_compat_enabled = true;

static int su_compat_feature_get(u64 *value)
{
    *value = ksu_su_compat_enabled ? 1 : 0;
    return 0;
}

static int su_compat_feature_set(u64 value)
{
    bool enable = value != 0;

    if (enable == ksu_su_compat_enabled) {
        pr_info("su_compat: no need to change\n");
        return 0;
    }

    if (enable) {
        ksu_sucompat_enable();
    } else {
        ksu_sucompat_disable();
    }

    ksu_su_compat_enabled = enable;
    pr_info("su_compat: set to %d\n", enable);

    return 0;
}

static const struct ksu_feature_handler su_compat_handler = {
    .feature_id = KSU_FEATURE_SU_COMPAT,
    .name = "su_compat",
    .get_handler = su_compat_feature_get,
    .set_handler = su_compat_feature_set,
};

static void __user *userspace_stack_buffer(const void *d, size_t len)
{
    /* To avoid having to mmap a page in userspace, just write below the stack
   * pointer. */
    char __user *p = (void __user *)current_user_stack_pointer() - len;

    return copy_to_user(p, d, len) ? NULL : p;
}

static char __user *sh_user_path(void)
{
    static const char sh_path[] = "/system/bin/sh";

    return userspace_stack_buffer(sh_path, sizeof(sh_path));
}

static char __user *ksud_user_path(void)
{
    static const char ksud_path[] = KSUD_PATH;

    return userspace_stack_buffer(ksud_path, sizeof(ksud_path));
}

static int ksu_handle_faccessat(int *dfd, const char __user **filename_user,
                                int *mode, int *__unused_flags)
{
    const char su[] = SU_PATH;

    if (!ksu_is_allow_uid_for_current(current_uid().val)) {
        return 0;
    }

    char path[sizeof(su) + 1];
    memset(path, 0, sizeof(path));
    ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

    if (unlikely(!memcmp(path, su, sizeof(su)))) {
        pr_info("faccessat su->sh!\n");
        *filename_user = sh_user_path();
    }

    return 0;
}

static int ksu_handle_stat(int *dfd, const char __user **filename_user,
                           int *flags)
{
    // const char sh[] = SH_PATH;
    const char su[] = SU_PATH;

    if (!ksu_is_allow_uid_for_current(current_uid().val)) {
        return 0;
    }

    if (unlikely(!filename_user)) {
        return 0;
    }

    char path[sizeof(su) + 1];
    memset(path, 0, sizeof(path));
// Remove this later!! we use syscall hook, so this will never happen!!!!!
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0) && 0
    // it becomes a `struct filename *` after 5.18
    // https://elixir.bootlin.com/linux/v5.18/source/fs/stat.c#L216
    const char sh[] = SH_PATH;
    struct filename *filename = *((struct filename **)filename_user);
    if (IS_ERR(filename)) {
        return 0;
    }
    if (likely(memcmp(filename->name, su, sizeof(su))))
        return 0;
    pr_info("vfs_statx su->sh!\n");
    memcpy((void *)filename->name, sh, sizeof(sh));
#else
    ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

    if (unlikely(!memcmp(path, su, sizeof(su)))) {
        pr_info("newfstatat su->sh!\n");
        *filename_user = sh_user_path();
    }
#endif

    return 0;
}

int ksu_handle_execve_sucompat(int *fd, const char __user **filename_user,
                               void *__never_use_argv, void *__never_use_envp,
                               int *__never_use_flags)
{
    const char su[] = SU_PATH;
    char path[sizeof(su) + 1];

    if (unlikely(!filename_user))
        return 0;

    memset(path, 0, sizeof(path));
    ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

    if (likely(memcmp(path, su, sizeof(su))))
        return 0;

    if (!ksu_is_allow_uid_for_current(current_uid().val))
        return 0;

    pr_info("sys_execve su found\n");
    *filename_user = ksud_user_path();

    escape_to_root();

    return 0;
}

int ksu_handle_devpts(struct inode *inode)
{
    if (!current->mm) {
        return 0;
    }

    uid_t uid = current_uid().val;
    if (uid % 100000 < 10000) {
        // not untrusted_app, ignore it
        return 0;
    }

    if (!ksu_is_allow_uid_for_current(uid))
        return 0;

    if (ksu_devpts_sid) {
        struct inode_security_struct *sec = selinux_inode(inode);
        if (sec) {
            sec->sid = ksu_devpts_sid;
        }
    }

    return 0;
}

#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS

// Tracepoint probe for sys_enter
static void sucompat_sys_enter_handler(void *data, struct pt_regs *regs,
                                       long id)
{
    // Handle newfstatat
    if (unlikely(id == __NR_newfstatat)) {
        int *dfd = (int *)&PT_REGS_PARM1(regs);
        const char __user **filename_user =
            (const char __user **)&PT_REGS_PARM2(regs);
        int *flags = (int *)&PT_REGS_SYSCALL_PARM4(regs);
        ksu_handle_stat(dfd, filename_user, flags);
        return;
    }

    // Handle faccessat
    if (unlikely(id == __NR_faccessat)) {
        int *dfd = (int *)&PT_REGS_PARM1(regs);
        const char __user **filename_user =
            (const char __user **)&PT_REGS_PARM2(regs);
        int *mode = (int *)&PT_REGS_PARM3(regs);
        ksu_handle_faccessat(dfd, filename_user, mode, NULL);
        return;
    }

    // Handle execve
    if (unlikely(id == __NR_execve)) {
        const char __user **filename_user =
            (const char __user **)&PT_REGS_PARM1(regs);
        ksu_handle_execve_sucompat(AT_FDCWD, filename_user, NULL, NULL, NULL);
        return;
    }
}

#endif // CONFIG_HAVE_SYSCALL_TRACEPOINTS

#ifdef CONFIG_KPROBES

static int pts_unix98_lookup_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct inode *inode;
    struct file *file = (struct file *)PT_REGS_PARM2(regs);
    inode = file->f_path.dentry->d_inode;

    return ksu_handle_devpts(inode);
}

static struct kprobe *init_kprobe(const char *name,
                                  kprobe_pre_handler_t handler)
{
    struct kprobe *kp = kzalloc(sizeof(struct kprobe), GFP_KERNEL);
    if (!kp)
        return NULL;
    kp->symbol_name = name;
    kp->pre_handler = handler;

    int ret = register_kprobe(kp);
    pr_info("sucompat: register_%s kprobe: %d\n", name, ret);
    if (ret) {
        kfree(kp);
        return NULL;
    }

    return kp;
}

static void destroy_kprobe(struct kprobe **kp_ptr)
{
    struct kprobe *kp = *kp_ptr;
    if (!kp)
        return;
    unregister_kprobe(kp);
    synchronize_rcu();
    kfree(kp);
    *kp_ptr = NULL;
}

static struct kprobe *pts_kp = NULL;
#endif

void ksu_mark_running_process()
{
    struct task_struct *p, *t;
    read_lock(&tasklist_lock);
    for_each_process_thread (p, t) {
        if (!t->mm) { // only user processes
            continue;
        }
        int uid = task_uid(t).val;
        bool ksu_root_process =
            uid == 0 && is_task_ksu_domain(get_task_cred(t));
        if (ksu_root_process || ksu_is_allow_uid(uid)) {
            set_tsk_thread_flag(t, TIF_SYSCALL_TRACEPOINT);
            pr_info("sucompat: mark process: pid:%d, uid: %d, comm:%s\n",
                    t->pid, uid, t->comm);
        }
    }
    read_unlock(&tasklist_lock);
}

static void unmark_all_process()
{
    struct task_struct *p, *t;
    read_lock(&tasklist_lock);
    for_each_process_thread (p, t) {
        clear_tsk_thread_flag(t, TIF_SYSCALL_TRACEPOINT);
    }
    read_unlock(&tasklist_lock);
    pr_info("sucompat: unmark all user process done!\n");
}

void ksu_sucompat_enable()
{
    int ret;
    pr_info("sucompat: ksu_sucompat_enable called\n");
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    // Register sys_enter tracepoint for syscall interception
    ret = register_trace_sys_enter(sucompat_sys_enter_handler, NULL);
    unmark_all_process();
    ksu_mark_running_process();
    if (ret) {
        pr_err("sucompat: failed to register sys_enter tracepoint: %d\n", ret);
    } else {
        pr_info("sucompat: sys_enter tracepoint registered\n");
    }
#endif

#ifdef CONFIG_KPROBES
    // Register kprobe for pts_unix98_lookup
    pts_kp = init_kprobe("pts_unix98_lookup", pts_unix98_lookup_pre);
#endif
}

void ksu_sucompat_disable()
{
    pr_info("sucompat: ksu_sucompat_disable called\n");
#ifdef CONFIG_HAVE_SYSCALL_TRACEPOINTS
    // Unregister sys_enter tracepoint
    unregister_trace_sys_enter(sucompat_sys_enter_handler, NULL);
    tracepoint_synchronize_unregister();
    pr_info("sucompat: sys_enter tracepoint unregistered\n");
#endif

#ifdef CONFIG_KPROBES
    // Unregister pts_unix98_lookup kprobe
    destroy_kprobe(&pts_kp);
#endif
}

// sucompat: permited process can execute 'su' to gain root access.
void ksu_sucompat_init()
{
    if (ksu_register_feature_handler(&su_compat_handler)) {
        pr_err("Failed to register su_compat feature handler\n");
    }
    if (ksu_su_compat_enabled) {
        ksu_sucompat_enable();
    }
}

void ksu_sucompat_exit()
{
    if (ksu_su_compat_enabled) {
        ksu_sucompat_disable();
    }
    ksu_unregister_feature_handler(KSU_FEATURE_SU_COMPAT);
}

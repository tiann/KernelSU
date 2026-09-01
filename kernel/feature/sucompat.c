#include <linux/compiler_types.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/mm.h>
#include <linux/pgtable.h>
#include <linux/uaccess.h>
#include <asm/current.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>
#include <linux/susfs_def.h>
#include <linux/namei.h>
#include <linux/minmax.h>
#include <linux/fs_struct.h>
#include "selinux/selinux.h"
#include "objsec.h"

#include "arch.h"
#include "policy/allowlist.h"
#include "policy/feature.h"
#include "klog.h" // IWYU pragma: keep
#include "runtime/ksud.h"
#include "feature/sucompat.h"
#include "feature/adb_root.h"
#include "policy/app_profile.h"
#include "hook/syscall_hook.h"
#include "sulog/event.h"
#include "uapi/sulog.h"

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

static const char sh_path[] = SH_PATH;
static const char su_path[] = SU_PATH;
static const char ksud_path[] = KSUD_PATH;

DEFINE_STATIC_KEY_TRUE(ksu_su_compat_enabled);

static int su_compat_feature_get(u64 *value)
{
    if (static_key_enabled(&ksu_su_compat_enabled))
        *value = 1;
    else
        *value = 0;
    return 0;
}

static int su_compat_feature_set(u64 value)
{
    bool enable = value != 0;
    if (enable)
        static_branch_enable(&ksu_su_compat_enabled);
    else
        static_branch_disable(&ksu_su_compat_enabled);
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
    // To avoid having to mmap a page in userspace, just write below the stack
    // pointer.
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

static const char __user *get_user_arg_ptr(struct user_arg_ptr argv, int nr)
{
    const char __user *native;

#ifdef CONFIG_COMPAT
    if (unlikely(argv.is_compat)) {
        compat_uptr_t compat;

        if (get_user(compat, argv.ptr.compat + nr))
            return ERR_PTR(-EFAULT);

        return compat_ptr(compat);
    }
#endif

    if (get_user(native, argv.ptr.native + nr))
        return ERR_PTR(-EFAULT);

    return native;
}

/*
 * return 0 -> No further checks should be required afterwards
 * return non-zero -> Further checks should be continued afterwards
 */
int ksu_handle_execveat_init(struct filename *filename, struct user_arg_ptr *argv_user, struct user_arg_ptr *envp_user) {
    int ret;

    if (current->pid == 1)
        return -EINVAL;

    if (!is_init(get_current_cred()))
        return -EINVAL;

    if (unlikely(!strcmp(filename->name, KSUD_PATH))) {
        const char __user *argv_user_ptr = get_user_arg_ptr(*argv_user, 0);
        struct ksu_sulog_pending_event *pending_sucompat = NULL;

        pr_info("hook_manager: escape to root for init executing ksud: %d\n", current->pid);
        pending_sucompat = ksu_sulog_capture_sucompat(filename->name, argv_user, GFP_KERNEL);
        ret = escape_to_root_for_init();
        if (ret) {
            pr_err("escape_to_root_for_init() failed: %d\n", ret);
            return ret;
        }
        if (!argv_user_ptr || IS_ERR(argv_user_ptr)) {
            pr_err("!argv_user_ptr || IS_ERR(argv_user_ptr)\n");
            return 0;
        }
        ksu_sulog_emit_pending(pending_sucompat, ret, GFP_KERNEL);
        return 0;
    }

    if (likely(!strstr(filename->name, "/app_process") && !strstr(filename->name, "/adbd") && !strstr(filename->name, "/stub_zygote"))) {
        pr_info("susfs: mark no sucompat checks for pid: '%d', exec: '%s'\n", current->pid, filename->name);
        susfs_set_current_proc_no_su();
        // - marking proc umounted here is useless because only zygote spawned processes will umount
        //   the sus mounts, tho other susfs features still rely on proc_umounted check, but
        //   it is fine to not spoof for init spawned processes.
        return 0;
    }

#ifdef CONFIG_COMPAT
    if (unlikely(envp_user->is_compat))
        ret = ksu_adb_root_handle_execveat(filename->name, (void ***)&envp_user->ptr.compat);
    else
        ret = ksu_adb_root_handle_execveat(filename->name, (void ***)&envp_user->ptr.native);
#else
        ret = ksu_adb_root_handle_execveat(filename->name, (void ***)&envp_user->ptr.native);
#endif

    if (ret)
        pr_err("adb root failed: %d\n", ret);

    return ret;
}

// the call from execve_handler_pre won't provided correct value for __never_use_argument, use them after fix execve_handler_pre, keeping them for consistence for manually patched code
int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
                 void *argv_user, void *envp_user,
                 int *__never_use_flags)
{
    struct filename *filename;
    struct ksu_sulog_pending_event *pending_sucompat = NULL;
    int ret;

    if (unlikely(!filename_ptr))
        return 0;

    filename = *filename_ptr;
    if (IS_ERR(filename))
        return 0;

    if (!ksu_handle_execveat_init(filename, (struct user_arg_ptr*)argv_user, (struct user_arg_ptr*)envp_user))
        return 0;

    if (!(__ksu_is_allow_uid_for_current(current_uid().val)))
        return 0;

    if (likely(memcmp(filename->name, su_path, sizeof(su_path))))
        return 0;

    if (current_chrooted())
    {
        pr_err("ksu_handle_execveat_sucompat: su found but NOT allowed! Because current process is running in chrooted environment\n");
        return 0;
    }

    pr_info("ksu_handle_execveat_sucompat: su found\n");

    memcpy((void *)filename->name, ksud_path, sizeof(ksud_path));

    pending_sucompat = ksu_sulog_capture_sucompat(filename->name, (struct user_arg_ptr*)argv_user, GFP_KERNEL);

    ret = escape_with_root_profile();
    if (ret)
        pr_err("escape_with_root_profile() failed: %d\n", ret);

    const char __user *argv_user_ptr = get_user_arg_ptr(*((struct user_arg_ptr*)argv_user), 0);
    if (!argv_user_ptr || IS_ERR(argv_user_ptr)) {
        pr_err("!argv_user_ptr || IS_ERR(argv_user_ptr)\n");
        return 0;
    }

    ksu_sulog_emit_pending(pending_sucompat, ret, GFP_KERNEL);
    return 0;
}

extern struct static_key_true is_first_zygote;

int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
            void *envp, int *flags)
{
    if (static_branch_unlikely(&is_first_zygote))
        (void)ksu_handle_execveat_ksud(fd, filename_ptr, argv, envp, flags);

    return ksu_handle_execveat_sucompat(fd, filename_ptr, argv, envp,
                        flags);
}

int ksu_handle_faccessat(int *dfd, struct filename **filename, int *mode,
             int *__unused_flags)
{
    if (unlikely(IS_ERR(*filename) || (*filename)->name == NULL))
        return 0;

    if (likely(memcmp((*filename)->name, su_path, sizeof(su_path))))
        return 0;

    if (current_chrooted())
    {
        pr_err("ksu_handle_faccessat: su found but NOT allowed! Because current process is running in chrooted environment\n");
        return 0;
    }
    pr_info("ksu_handle_faccessat: su->sh!\n");
    memcpy((void *)((*filename)->name), sh_path, sizeof(sh_path));
    return 0;
}

int ksu_handle_stat(int *dfd, struct filename **filename, int *flags) {
    if (unlikely(IS_ERR(*filename) || (*filename)->name == NULL))
        return 0;

    if (likely(memcmp((*filename)->name, su_path, sizeof(su_path))))
        return 0;

    if (current_chrooted())
    {
        pr_err("ksu_handle_stat: su found but NOT allowed! Because current process is running in chrooted environment\n");
        return 0;
    }
    pr_info("ksu_handle_stat: su->sh!\n");
    memcpy((void *)((*filename)->name), sh_path, sizeof(sh_path));
    return 0;
}

// sucompat: permitted process can execute 'su' to gain root access.
void __init ksu_sucompat_init()
{
    if (ksu_register_feature_handler(&su_compat_handler))
        pr_err("Failed to register su_compat feature handler\n");
}

void __exit ksu_sucompat_exit()
{
    ksu_unregister_feature_handler(KSU_FEATURE_SU_COMPAT);
}

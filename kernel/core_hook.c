#include <linux/compiler.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/thread_info.h>
#include <linux/seccomp.h>
#include <linux/bpf.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/init_task.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/version.h>

#include "allowlist.h"
#include "core_hook.h"
#include "feature.h"
#include "klog.h" // IWYU pragma: keep
#include "manager.h"
#include "selinux/selinux.h"
#include "seccomp_cache.h"
#include "supercalls.h"
#include "sucompat.h"
#include "ksud.h"
#include "hook_manager.h"

static bool ksu_kernel_umount_enabled = true;
static bool ksu_enhanced_security_enabled = false;

static int kernel_umount_feature_get(u64 *value)
{
    *value = ksu_kernel_umount_enabled ? 1 : 0;
    return 0;
}

static int kernel_umount_feature_set(u64 value)
{
    bool enable = value != 0;
    ksu_kernel_umount_enabled = enable;
    pr_info("kernel_umount: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler kernel_umount_handler = {
    .feature_id = KSU_FEATURE_KERNEL_UMOUNT,
    .name = "kernel_umount",
    .get_handler = kernel_umount_feature_get,
    .set_handler = kernel_umount_feature_set,
};

static int enhanced_security_feature_get(u64 *value)
{
    *value = ksu_enhanced_security_enabled ? 1 : 0;
    return 0;
}

static int enhanced_security_feature_set(u64 value)
{
    bool enable = value != 0;
    ksu_enhanced_security_enabled = enable;
    pr_info("enhanced_security: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler enhanced_security_handler = {
    .feature_id = KSU_FEATURE_ENHANCED_SECURITY,
    .name = "enhanced_security",
    .get_handler = enhanced_security_feature_get,
    .set_handler = enhanced_security_feature_set,
};

static inline bool is_allow_su()
{
    if (is_manager()) {
        // we are manager, allow!
        return true;
    }
    return ksu_is_allow_uid_for_current(current_uid().val);
}

static inline bool is_unsupported_uid(uid_t uid)
{
#define LAST_APPLICATION_UID 19999
    uid_t appid = uid % 100000;
    return appid > LAST_APPLICATION_UID;
}

// ksu_handle_prctl removed - now using ioctl via reboot hook

static bool is_appuid(uid_t uid)
{
#define PER_USER_RANGE 100000
#define FIRST_APPLICATION_UID 10000
#define LAST_APPLICATION_UID 19999

    uid_t appid = uid % PER_USER_RANGE;
    return appid >= FIRST_APPLICATION_UID && appid <= LAST_APPLICATION_UID;
}

static bool should_umount(struct path *path)
{
    if (!path) {
        return false;
    }

    if (current->nsproxy->mnt_ns == init_nsproxy.mnt_ns) {
        pr_info("ignore global mnt namespace process: %d\n", current_uid().val);
        return false;
    }

    if (path->mnt && path->mnt->mnt_sb && path->mnt->mnt_sb->s_type) {
        const char *fstype = path->mnt->mnt_sb->s_type->name;
        return strcmp(fstype, "overlay") == 0;
    }
    return false;
}
extern int path_umount(struct path *path, int flags);
static void ksu_umount_mnt(struct path *path, int flags)
{
    int err = path_umount(path, flags);
    if (err) {
        pr_info("umount %s failed: %d\n", path->dentry->d_iname, err);
    }
}

static void try_umount(const char *mnt, bool check_mnt, int flags)
{
    struct path path;
    int err = kern_path(mnt, 0, &path);
    if (err) {
        return;
    }

    if (path.dentry != path.mnt->mnt_root) {
        // it is not root mountpoint, maybe umounted by others already.
        path_put(&path);
        return;
    }

    // we are only interest in some specific mounts
    if (check_mnt && !should_umount(&path)) {
        path_put(&path);
        return;
    }

    ksu_umount_mnt(&path, flags);
}

struct umount_tw {
    struct callback_head cb;
    const struct cred *old_cred;
};

static void umount_tw_func(struct callback_head *cb)
{
    struct umount_tw *tw = container_of(cb, struct umount_tw, cb);
    const struct cred *saved = NULL;
    if (tw->old_cred) {
        saved = override_creds(tw->old_cred);
    }

    // fixme: use `collect_mounts` and `iterate_mount` to iterate all mountpoint and
    // filter the mountpoint whose target is `/data/adb`
    try_umount("/odm", true, 0);
    try_umount("/system", true, 0);
    try_umount("/vendor", true, 0);
    try_umount("/product", true, 0);
    try_umount("/system_ext", true, 0);
    try_umount("/data/adb/modules", false, MNT_DETACH);

    if (saved)
        revert_creds(saved);

    if (tw->old_cred)
        put_cred(tw->old_cred);

    kfree(tw);
}

int ksu_handle_setuid(uid_t ruid, uid_t euid, uid_t suid)
{
    struct umount_tw *tw;
    uid_t new_uid = ruid;
	uid_t old_uid = current_uid().val;
    // pr_info("handle_setuid from %d to %d\n", old_uid, new_uid);

    if (0 != old_uid) {
        // old process is not root, ignore it.
        if (ksu_enhanced_security_enabled) {
            // disallow any non-ksu domain escalation from non-root to root!
            if (unlikely(new_uid) == 0) {
                if (!is_ksu_domain()) {
                    pr_warn("find suspicious EoP: %d %s, from %d to %d\n", 
                        current->pid, current->comm, old_uid, new_uid);
                    force_sig(SIGKILL);
                    return 0;
                }
            }
            // disallow appuid decrease to any other uid if it is allowed to su
            if (is_appuid(old_uid)) {
                if (new_uid < old_uid && !ksu_is_allow_uid_for_current(old_uid)) {
                    pr_warn("find suspicious EoP: %d %s, from %d to %d\n", 
                        current->pid, current->comm, old_uid, new_uid);
                    force_sig(SIGKILL);
                    return 0;
                }
            }
        }
        return 0;
    }

    if (new_uid == 2000) {
        if (ksu_su_compat_enabled) {
            ksu_set_task_tracepoint_flag(current);
        }
    }

    if (!is_appuid(new_uid) || is_unsupported_uid(new_uid)) {
        // pr_info("handle setuid ignore non application or isolated uid: %d\n", new_uid);
        return 0;
    }

    // if on private space, see if its possibly the manager
    if (new_uid > 100000 && new_uid % 100000 == ksu_get_manager_uid()) {
        ksu_set_manager_uid(new_uid);
    }

    if (ksu_get_manager_uid() == new_uid) {
        pr_info("install fd for: %d\n", new_uid);
        ksu_install_fd();
        spin_lock_irq(&current->sighand->siglock);
        ksu_seccomp_allow_cache(current->seccomp.filter, __NR_reboot);
        if (ksu_su_compat_enabled) {
            ksu_set_task_tracepoint_flag(current);
        }
        spin_unlock_irq(&current->sighand->siglock);
        return 0;
    }

    if (ksu_is_allow_uid_for_current(new_uid)) {
        if (current->seccomp.mode == SECCOMP_MODE_FILTER &&
            current->seccomp.filter) {
            spin_lock_irq(&current->sighand->siglock);
            ksu_seccomp_allow_cache(current->seccomp.filter, __NR_reboot);
            spin_unlock_irq(&current->sighand->siglock);
        }
        if (ksu_su_compat_enabled) {
            ksu_set_task_tracepoint_flag(current);
        }
    } else {
        // Disable syscall tracepoint sucompat for non-allowed processes
        if (ksu_su_compat_enabled) {
            ksu_clear_task_tracepoint_flag(current);
        }
    }

    // this hook is used for umounting overlayfs for some uid, if there isn't any module mounted, just ignore it!
    if (!ksu_module_mounted) {
        return 0;
    }

    if (!ksu_kernel_umount_enabled) {
        return 0;
    }

    if (!ksu_uid_should_umount(new_uid)) {
        return 0;
    } else {
#ifdef CONFIG_KSU_DEBUG
        pr_info("uid: %d should not umount!\n", current_uid().val);
#endif
    }

    // check old process's selinux context, if it is not zygote, ignore it!
    // because some su apps may setuid to untrusted_app but they are in global mount namespace
    // when we umount for such process, that is a disaster!
    bool is_zygote_child = is_zygote(get_current_cred());
    if (!is_zygote_child) {
        pr_info("handle umount ignore non zygote child: %d\n", current->pid);
        return 0;
    }
#ifdef CONFIG_KSU_DEBUG
    // umount the target mnt
    pr_info("handle umount for uid: %d, pid: %d\n", new_uid, current->pid);
#endif

    tw = kmalloc(sizeof(*tw), GFP_ATOMIC);
    if (!tw)
        return 0;

    tw->old_cred = get_current_cred();
    tw->cb.func = umount_tw_func;

    int err = task_work_add(current, &tw->cb, TWA_RESUME);
    if (err) {
        if (tw->old_cred) {
            put_cred(tw->old_cred);
        }
        kfree(tw);
        pr_warn("unmount add task_work failed\n");
    }

    return 0;
}

int ksu_handle_reboot(int magic1, int magic2, unsigned int cmd,
		      void __user *arg)
{
	// Check if this is a request to install KSU fd
	if (magic1 == KSU_INSTALL_MAGIC1 && magic2 == KSU_INSTALL_MAGIC2) {
		int fd = ksu_install_fd();
		pr_info("[%d] install ksu fd: %d\n", current->pid, fd);

		if (arg && copy_to_user(arg, &fd, sizeof(fd))) {
			pr_err("install ksu fd reply err\n");
		}
	}

	return 0;
}

void __init ksu_core_init(void)
{
    if (ksu_register_feature_handler(&kernel_umount_handler)) {
        pr_err("Failed to register umount feature handler\n");
    }
    if (ksu_register_feature_handler(&enhanced_security_handler)) {
        pr_err("Failed to register enhanced security feature handler\n");
    }
}

void ksu_core_exit(void)
{
    pr_info("ksu_core_exit\n");
    ksu_unregister_feature_handler(KSU_FEATURE_KERNEL_UMOUNT);
    ksu_unregister_feature_handler(KSU_FEATURE_ENHANCED_SECURITY);
}

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/cred.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/types.h>

#include "feature/kernel_umount.h"
#include "klog.h" // IWYU pragma: keep
#include "policy/allowlist.h"
#include "selinux/selinux.h"
#include "policy/feature.h"
#include "runtime/ksud_boot.h"
#include "ksu.h"

static bool ksu_kernel_umount_enabled = true;
bool ksu_webview_zygote_umount_enabled = false;

static int kernel_umount_feature_get(u64 *value)
{
    *value = READ_ONCE(ksu_kernel_umount_enabled) ? 1 : 0;
    return 0;
}

static int kernel_umount_feature_set(u64 value)
{
    bool enable = value != 0;
    WRITE_ONCE(ksu_kernel_umount_enabled, enable);
    pr_info("kernel_umount: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler kernel_umount_handler = {
    .feature_id = KSU_FEATURE_KERNEL_UMOUNT,
    .name = "kernel_umount",
    .get_handler = kernel_umount_feature_get,
    .set_handler = kernel_umount_feature_set,
};

static int webview_zygote_umount_feature_get(u64 *value)
{
    *value = READ_ONCE(ksu_webview_zygote_umount_enabled) ? 1 : 0;
    return 0;
}

static int webview_zygote_umount_feature_set(u64 value)
{
    bool enable = value != 0;
    WRITE_ONCE(ksu_webview_zygote_umount_enabled, enable);
    pr_info("webview_zygote_umount: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler webview_zygote_umount_handler = {
    .feature_id = KSU_FEATURE_WEBVIEW_ZYGOTE_UMOUNT,
    .name = "webview_zygote_umount",
    .get_handler = webview_zygote_umount_feature_get,
    .set_handler = webview_zygote_umount_feature_set,
};

extern int path_umount(struct path *path, int flags);

static void ksu_umount_mnt(const char *mnt, struct path *path, int flags)
{
    int err = path_umount(path, flags);
    if (err) {
        pr_debug("umount %s failed: %d\n", mnt, err);
    }
}

static void try_umount(const char *mnt, int flags)
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

    ksu_umount_mnt(mnt, &path, flags);
}

struct umount_tw {
    struct callback_head cb;
};

int ksu_handle_umount(uid_t old_uid, uid_t new_uid)
{
    // if there isn't any module mounted, just ignore it!
    if (!READ_ONCE(ksu_module_mounted)) {
        return 0;
    }

    if (!READ_ONCE(ksu_kernel_umount_enabled)) {
        return 0;
    }

    // There are 6 scenarios:
    // 1. Normal app: zygote -> appuid
    // 2. Isolated process forked from zygote: zygote -> isolated_process
    // 3. App zygote forked from zygote: zygote -> appuid
    // 4. Webview zygote forked from zygote: zygote -> webview_zygote (controlled by feature policy)
    // 5. Isolated process forked from app zygote: appuid -> isolated_process (already handled by 3)
    // 6. Isolated process forked from webview zygote (already handled by 4)
    if (!is_appuid(new_uid) && new_uid != WEBVIEW_ZYGOTE_UID && !is_isolated_process(new_uid)) {
        return 0;
    }

    // WebView zygote is handled explicitly because it is not part of the
    // normal app allowlist path; its child processes inherit this isolation.
    if (new_uid == WEBVIEW_ZYGOTE_UID && !READ_ONCE(ksu_webview_zygote_umount_enabled)) {
        return 0;
    }

    if (!ksu_uid_should_umount(new_uid) && !is_isolated_process(new_uid)) {
        return 0;
    }

    // check old process's selinux context, if it is not zygote, ignore it!
    // because some su apps may setuid to untrusted_app but they are in global mount namespace
    // when we umount for such process, that is a disaster!
    // also handle case 4 and 5
    bool is_zygote_child = is_zygote(current_cred());
    if (!is_zygote_child) {
        pr_debug("handle umount ignore non zygote child: %d\n", current->pid);
        return 0;
    }
    // umount the target mnt
    pr_debug("handle umount for uid: %d, pid: %d\n", new_uid, current->pid);

    const struct cred *saved = override_creds(ksu_cred);

    struct mount_entry *entry;
    down_read(&mount_list_lock);
    list_for_each_entry (entry, &mount_list, list) {
        struct mount_entry *other;
        unsigned int flags = 0;
        unsigned int layers = 0;
        unsigned int layer;
        bool seen = false;

        /*
         * A path can have both an unmanaged/manual entry and an auto-managed
         * entry. Treat them as one isolation target: summing layer counts
         * could unmount through the KernelSU stack into the real system mount,
         * while processing only one entry could leave a lower KSU layer visible.
         */
        list_for_each_entry (other, &mount_list, list) {
            if (other == entry)
                break;
            if (!strcmp(other->umountable, entry->umountable)) {
                seen = true;
                break;
            }
        }
        if (seen)
            continue;

        list_for_each_entry (other, &mount_list, list) {
            if (strcmp(other->umountable, entry->umountable))
                continue;
            flags |= other->flags;
            if (other->layers > layers)
                layers = other->layers;
        }

        pr_debug("%s: unmounting: %s flags: 0x%x layers: %u\n", __func__, entry->umountable, flags, layers);
        for (layer = 0; layer < layers; layer++)
            try_umount(entry->umountable, flags);
    }
    up_read(&mount_list_lock);

    revert_creds(saved);

    return 0;
}

void __init ksu_kernel_umount_init(void)
{
    if (ksu_register_feature_handler(&kernel_umount_handler)) {
        pr_err("Failed to register kernel_umount feature handler\n");
    }
    if (ksu_register_feature_handler(&webview_zygote_umount_handler)) {
        pr_err("Failed to register webview_zygote_umount feature handler\n");
    }
}

void ksu_kernel_umount_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_WEBVIEW_ZYGOTE_UMOUNT);
    ksu_unregister_feature_handler(KSU_FEATURE_KERNEL_UMOUNT);
}
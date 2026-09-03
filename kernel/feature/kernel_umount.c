#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <linux/cred.h>
#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/string.h>

#include "feature/kernel_umount.h"
#include "klog.h" // IWYU pragma: keep
#include "policy/allowlist.h"
#include "selinux/selinux.h"
#include "policy/feature.h"
#include "runtime/ksud_boot.h"
#include "ksu.h"
#include "api/event_registry.h"

static bool ksu_kernel_umount_enabled = true;

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

extern int path_umount(struct path *path, int flags);

static int ksu_umount_mnt(const char *mnt, struct path *path, int flags)
{
    int err = path_umount(path, flags);
    if (err) {
        pr_info("umount %s failed: %d\n", mnt, err);
    }
    return err;
}

static int try_umount(const char *mnt, int flags)
{
    struct path path;
    int err = kern_path(mnt, 0, &path);
    if (err) {
        return err;
    }

    if (path.dentry != path.mnt->mnt_root) {
        // it is not root mountpoint, maybe umounted by others already.
        path_put(&path);
        return -EINVAL;
    }

    return ksu_umount_mnt(mnt, &path, flags);
}

struct umount_tw {
    struct callback_head cb;
};

struct ksu_umount_snapshot {
    char target[128];
    int result;
    unsigned int flags;
};

int ksu_handle_umount(uid_t old_uid, uid_t new_uid)
{
    struct ksu_umount_event event = {
        .size = sizeof(event),
        .version = 1,
        .reason = 0,
        .old_uid = old_uid,
        .new_uid = new_uid,
        .pid = current->pid,
        .tgid = current->tgid,
    };
    // if there isn't any module mounted, just ignore it!
    if (!ksu_module_mounted) {
        return 0;
    }

    if (!ksu_kernel_umount_enabled) {
        return 0;
    }

    // There are 6 scenarios:
    // 1. Normal app: zygote -> appuid
    // 2. Isolated process forked from zygote: zygote -> isolated_process
    // 3. App zygote forked from zygote: zygote -> appuid
    // 4. Webview zygote forked from zygote: zygote -> webview_zygote
    // 5. Isolated process forked from app zygote: appuid -> isolated_process (already handled by 3)
    // 6. Isolated process forked from webview zygote (already handled by 4)
    if (!is_appuid(new_uid) && new_uid != WEBVIEW_ZYGOTE_UID && !is_isolated_process(new_uid)) {
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
        pr_info("handle umount ignore non zygote child: %d\n", current->pid);
        return 0;
    }
    // umount the target mnt
    pr_info("handle umount for uid: %d, pid: %d\n", new_uid, current->pid);

    ksu_event_emit(KSU_EVENT_KSU_UMOUNT_PRE, &event, 0);

    const struct cred *saved = override_creds(ksu_cred);

    struct mount_entry *entry;
    struct ksu_umount_snapshot *snapshots = NULL;
    size_t snapshot_count = 0;
    size_t i = 0;

    if (ksu_event_has_handlers(KSU_EVENT_KSU_UMOUNT_ITEM)) {
        down_read(&mount_list_lock);
        list_for_each_entry (entry, &mount_list, list)
            snapshot_count++;
        up_read(&mount_list_lock);
    }

    if (snapshot_count)
        snapshots = kcalloc(snapshot_count, sizeof(*snapshots), GFP_KERNEL);

    down_read(&mount_list_lock);
    list_for_each_entry (entry, &mount_list, list) {
        pr_info("%s: unmounting: %s flags: 0x%x\n", __func__, entry->umountable, entry->flags);
        if (snapshots && i < snapshot_count) {
            strscpy(snapshots[i].target, entry->umountable, sizeof(snapshots[i].target));
            snapshots[i].flags = entry->flags;
            snapshots[i].result = try_umount(entry->umountable, entry->flags);
            i++;
        } else {
            try_umount(entry->umountable, entry->flags);
        }
    }
    up_read(&mount_list_lock);

    revert_creds(saved);

    if (snapshots) {
        size_t filled_count = i;

        for (i = 0; i < filled_count; i++) {
            struct ksu_umount_event item = event;
            item.flags = snapshots[i].flags;
            item.result = snapshots[i].result;
            item.target = snapshots[i].target;
            ksu_event_emit(KSU_EVENT_KSU_UMOUNT_ITEM, &item, item.result);
        }
        kfree(snapshots);
    }

    ksu_event_emit(KSU_EVENT_KSU_UMOUNT_POST, &event, 0);

    return 0;
}

void __init ksu_kernel_umount_init(void)
{
    if (ksu_register_feature_handler(&kernel_umount_handler)) {
        pr_err("Failed to register kernel_umount feature handler\n");
    }
}

void __exit ksu_kernel_umount_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_KERNEL_UMOUNT);
}

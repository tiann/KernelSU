// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/fsnotify_backend.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/rculist.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include "klog.h" // IWYU pragma: keep
#include "manager/throne_tracker.h"

#define MASK_SYSTEM (FS_CREATE | FS_MOVE | FS_EVENT_ON_CHILD)

struct watch_dir {
    const char *path;
    u32 mask;
    struct path kpath;
    struct inode *inode;
    struct fsnotify_mark *mark;
};

static struct fsnotify_group *g;
static DEFINE_MUTEX(observer_lock);
static bool observer_initialized;

static void ksu_track_throne_work(struct work_struct *work)
{
    if (READ_ONCE(observer_initialized))
        track_throne(false);
}

static DECLARE_WORK(track_throne_work, ksu_track_throne_work);

static int ksu_handle_inode_event(struct fsnotify_mark *mark, u32 mask, struct inode *inode, struct inode *dir,
                                  const struct qstr *file_name, u32 cookie)
{
    if (!file_name)
        return 0;
    if (mask & FS_ISDIR)
        return 0;
    if (file_name->len == 13 && !memcmp(file_name->name, "packages.list", 13)) {
        pr_info("packages.list detected: %d\n", mask);
        if (READ_ONCE(observer_initialized))
            schedule_work(&track_throne_work);
    }
    return 0;
}

static const struct fsnotify_ops ksu_ops = {
    .handle_inode_event = ksu_handle_inode_event,
};

static int add_mark_on_inode(struct inode *inode, u32 mask, struct fsnotify_mark **out)
{
    struct fsnotify_mark *m;

    m = kzalloc(sizeof(*m), GFP_KERNEL);
    if (!m)
        return -ENOMEM;

    fsnotify_init_mark(m, g);
    m->mask = mask;

    if (fsnotify_add_inode_mark(m, inode, 0)) {
        fsnotify_put_mark(m);
        return -EINVAL;
    }
    *out = m;
    return 0;
}

static int watch_one_dir(struct watch_dir *wd)
{
    int ret = kern_path(wd->path, LOOKUP_FOLLOW, &wd->kpath);
    if (ret) {
        pr_info("path not ready: %s (%d)\n", wd->path, ret);
        return ret;
    }
    wd->inode = d_inode(wd->kpath.dentry);
    ihold(wd->inode);

    ret = add_mark_on_inode(wd->inode, wd->mask, &wd->mark);
    if (ret) {
        pr_err("Add mark failed for %s (%d)\n", wd->path, ret);
        path_put(&wd->kpath);
        memset(&wd->kpath, 0, sizeof(wd->kpath));
        iput(wd->inode);
        wd->inode = NULL;
        return ret;
    }
    pr_info("watching %s\n", wd->path);
    return 0;
}

static void unwatch_one_dir(struct watch_dir *wd)
{
    if (wd->mark) {
        fsnotify_destroy_mark(wd->mark, g);
        fsnotify_put_mark(wd->mark);
        wd->mark = NULL;
    }
    if (wd->inode) {
        iput(wd->inode);
        wd->inode = NULL;
    }
    if (wd->kpath.dentry) {
        path_put(&wd->kpath);
        memset(&wd->kpath, 0, sizeof(wd->kpath));
    }
}

static struct watch_dir g_watch = { .path = "/data/system", .mask = MASK_SYSTEM };

int ksu_observer_init(void)
{
    int ret;

    mutex_lock(&observer_lock);
    if (READ_ONCE(observer_initialized)) {
        mutex_unlock(&observer_lock);
        return 0;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
    g = fsnotify_alloc_group(&ksu_ops, 0);
#else
    g = fsnotify_alloc_group(&ksu_ops);
#endif
    if (IS_ERR(g)) {
        ret = PTR_ERR(g);
        g = NULL;
        mutex_unlock(&observer_lock);
        return ret;
    }

    ret = watch_one_dir(&g_watch);
    if (ret) {
        fsnotify_put_group(g);
        g = NULL;
        mutex_unlock(&observer_lock);
        return ret;
    }

    WRITE_ONCE(observer_initialized, true);
    mutex_unlock(&observer_lock);
    pr_info("observer init done\n");
    return 0;
}

void __exit ksu_observer_exit(void)
{
    mutex_lock(&observer_lock);
    if (!READ_ONCE(observer_initialized)) {
        mutex_unlock(&observer_lock);
        return;
    }

    WRITE_ONCE(observer_initialized, false);
    unwatch_one_dir(&g_watch);
    fsnotify_put_group(g);
    g = NULL;
    mutex_unlock(&observer_lock);

    cancel_work_sync(&track_throne_work);
    pr_info("observer exit done\n");
}

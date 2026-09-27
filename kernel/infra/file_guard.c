#include "file_guard.h"
#include "hook/patch_memory.h"
#include "linux/anon_inodes.h"
#include "linux/cred.h"
#include "linux/fs.h"
#include "linux/init.h"
#include "linux/kallsyms.h"
// security/selinux/include/security.h
#include <security.h>

#include "linux/list.h"
#include "linux/lockdep.h"
#include "linux/rcupdate.h"
#include "linux/sched/signal.h"
#include "linux/sched/task.h"
#include "linux/security.h"
#include "linux/spinlock_types.h"
#include "linux/task_work.h"
#include "linux/types.h"
#include "klog.h" // IWYU pragma: keep
#include "objsec.h"

static DEFINE_SPINLOCK(ksu_files_lock);
static LIST_HEAD(ksu_files);
static bool exiting __read_mostly = false;

int ksu_file_add(struct file *f, struct ksu_file *kf)
{
    spin_lock(&ksu_files_lock);
    if (exiting) {
        spin_unlock(&ksu_files_lock);
        return -EAGAIN;
    }
    kf->f = f;
    list_add(&kf->list, &ksu_files);
    spin_unlock(&ksu_files_lock);
    return 0;
}

void ksu_file_release(struct ksu_file *f)
{
    unsigned long flags;
    spin_lock_irqsave(&ksu_files_lock, flags);
    // https://cs.android.com/android/kernel/superproject/+/common-android-mainline:common/fs/file_table.c;l=467-473;drc=3be0b283b562eabbc2b1f3bb534dc8903079bbaa
    // f_op->release is called before fops_put(f_op), so we put it manually.
    // TODO: since we're not reference THIS_MODULE, we can remove this
    // fops_put(f->f->f_op);
    // prevent it from being put again
    f->f->f_op = NULL;
    list_del(&f->list);
    spin_unlock_irqrestore(&ksu_files_lock, flags);
}

static const struct file_operations *empty_fops = NULL;

int __init ksu_file_guard_init()
{
    struct file *tmp = filp_open("/", O_PATH | O_NOATIME, 0);
    if (IS_ERR(tmp)) {
        pr_err("open O_PATH failed: %d\n", (int)PTR_ERR(tmp));
    } else {
        empty_fops = tmp->f_op;
        pr_info("got empty_fops: %pSb\n", empty_fops);
        filp_close(tmp, 0);
    }
    // TODO: Do we need to fail if empty_fops not found?
    return 0;
}

void ksu_file_guard_exit()
{
    spin_lock(&ksu_files_lock);
    exiting = true;
    struct ksu_file *pos, *n;
    list_for_each_entry_safe (pos, n, &ksu_files, list) {
        list_del(&pos->list);
        if (pos->cleanup) {
            pos->cleanup(pos->f);
        }
        pos->f->f_op = empty_fops;
        pos->f->private_data = NULL;
    }
    spin_unlock(&ksu_files_lock);
}

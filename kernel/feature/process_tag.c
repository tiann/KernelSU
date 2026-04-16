#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/rculist.h>
#include <linux/atomic.h>
#include <linux/rcupdate.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <trace/events/sched.h>

#include "feature/process_tag.h"
#include "policy/feature.h"
#include "hook/lsm_hook.h"
#include "klog.h" // IWYU pragma: keep

#define PROCESS_TAG_HASH_BITS 10 // 1024 buckets

static DEFINE_HASHTABLE(process_tag_table, PROCESS_TAG_HASH_BITS);
static DEFINE_SPINLOCK(process_tag_lock);
static bool process_tag_enabled __read_mostly = false;
static bool process_tag_initialized __read_mostly = false;
static bool process_tag_lsm_hooked __read_mostly = false;

struct process_tag_entry {
    struct hlist_node hnode;
    struct task_struct *task;
    struct process_tag *tag;
    struct rcu_head rcu;
};

int ksu_process_tag_set(struct task_struct *task, enum process_tag_type type, const char *name)
{
    struct process_tag_entry *entry, *existing = NULL;
    struct process_tag *new_tag;
    unsigned long flags;

    if (!process_tag_enabled) {
        return -ENOSYS;
    }

    if (!name) {
        name = "";
    }

    new_tag = kmalloc(sizeof(struct process_tag), GFP_KERNEL);
    if (!new_tag) {
        pr_err("process_tag: failed to alloc tag for task %d\n", task->pid);
        return -ENOMEM;
    }

    new_tag->type = type;
    strncpy(new_tag->name, name, sizeof(new_tag->name) - 1);
    new_tag->name[sizeof(new_tag->name) - 1] = '\0';
    atomic_set(&new_tag->refcount, 1);

    entry = kmalloc(sizeof(struct process_tag_entry), GFP_KERNEL);
    if (!entry) {
        pr_err("process_tag: failed to alloc entry for pid %d\n", task->pid);
        kfree(new_tag);
        return -ENOMEM;
    }

    entry->task = task;
    entry->tag = new_tag;

    spin_lock_irqsave(&process_tag_lock, flags);

    hash_for_each_possible (process_tag_table, existing, hnode, task) {
        if (existing->task == task) {
            struct process_tag *old_tag =
                rcu_replace_pointer(existing->tag, new_tag, lockdep_is_held(&process_tag_lock));
            spin_unlock_irqrestore(&process_tag_lock, flags);

            if (old_tag && atomic_dec_and_test(&old_tag->refcount)) {
                kfree_rcu(old_tag, rcu);
            }
            kfree(entry);

            pr_debug("process_tag: updated tag for task %d, type=%d, name=%s\n", task->pid, type, name);
            return 0;
        }
    }

    hash_add_rcu(process_tag_table, &entry->hnode, task);
    spin_unlock_irqrestore(&process_tag_lock, flags);

    pr_debug("process_tag: added tag for task %d, type=%d, name=%s\n", task->pid, type, name);
    return 0;
}

struct process_tag *ksu_process_tag_get(struct task_struct *task)
{
    struct process_tag_entry *entry;
    struct process_tag *tag = NULL;

    if (!process_tag_enabled) {
        return NULL;
    }

    rcu_read_lock();
    hash_for_each_possible_rcu (process_tag_table, entry, hnode, task) {
        if (entry->task == task) {
            tag = rcu_dereference(entry->tag);
            if (tag && !atomic_inc_not_zero(&tag->refcount)) {
                tag = NULL;
            }
            break;
        }
    }
    rcu_read_unlock();

    return tag;
}

void ksu_process_tag_put(struct process_tag *tag)
{
    if (!tag) {
        return;
    }

    if (atomic_dec_and_test(&tag->refcount)) {
        kfree_rcu(tag, rcu);
    }
}

void ksu_process_tag_delete(struct task_struct *task)
{
    struct process_tag_entry *entry;
    struct process_tag *tag;
    unsigned long flags;

    spin_lock_irqsave(&process_tag_lock, flags);

    hash_for_each_possible (process_tag_table, entry, hnode, task) {
        if (entry->task == task) {
            tag = rcu_dereference_protected(entry->tag, lockdep_is_held(&process_tag_lock));
            hash_del_rcu(&entry->hnode);
            spin_unlock_irqrestore(&process_tag_lock, flags);

            if (tag && atomic_dec_and_test(&tag->refcount)) {
                kfree_rcu(tag, rcu);
            }

            kfree_rcu(entry, rcu);
            pr_debug("process_tag: deleted tag for task %d\n", task->pid);
            return;
        }
    }

    spin_unlock_irqrestore(&process_tag_lock, flags);
}

void ksu_process_tag_flush(void)
{
    struct process_tag_entry *entry;
    struct process_tag *tag;
    unsigned long flags;
    struct hlist_node *tmp;
    int i;

    spin_lock_irqsave(&process_tag_lock, flags);

    hash_for_each_safe (process_tag_table, i, tmp, entry, hnode) {
        tag = rcu_dereference_protected(entry->tag, lockdep_is_held(&process_tag_lock));
        hash_del_rcu(&entry->hnode);
        if (tag && atomic_dec_and_test(&tag->refcount)) {
            kfree_rcu(tag, rcu);
        }

        kfree_rcu(entry, rcu);
    }

    spin_unlock_irqrestore(&process_tag_lock, flags);

    pr_info("process_tag: flushed all entries\n");
}

bool ksu_process_tag_is_enabled(void)
{
    return process_tag_enabled;
}

// Security hook for task alloc/free
static int process_tag_task_alloc(struct task_struct *task, unsigned long clone_flags)
{
    int err = 0;
    struct process_tag *parent_tag;

    rcu_read_lock();
    parent_tag = ksu_process_tag_get(current);
    rcu_read_unlock();
    if (parent_tag) {
        err = ksu_process_tag_set(task, parent_tag->type, parent_tag->name);
        ksu_process_tag_put(parent_tag);
        if (err) {
            pr_debug("process_tag: child %d parent %d set tag failed: %d\n", task_pid_nr(task), task_pid_nr(current),
                     err);
        } else {
            pr_debug("process_tag: child %d inherited tag from parent %d\n", task_pid_nr(task), task_pid_nr(current));
        }
    }

    return err;
}

static void process_tag_task_free(struct task_struct *task)
{
    ksu_process_tag_delete(task);
}

struct ksu_lsm_hook process_tag_task_alloc_hook =
    KSU_LSM_HOOK_INIT(task_alloc, "selinux_task_alloc", process_tag_task_alloc, 0);
struct ksu_lsm_hook process_tag_task_free_hook =
    KSU_LSM_HOOK_INIT(task_free, "selinux_task_alloc", process_tag_task_free, 1);

// Feature handler
static int process_tag_feature_get(u64 *value)
{
    *value = process_tag_enabled ? 1 : 0;
    return 0;
}

static int process_tag_feature_set(u64 value)
{
    bool enable = value != 0;
    int ret;

    if (process_tag_initialized) {
        pr_info("process_tag: feature already latched to %d, ignore new value %d\n", process_tag_enabled, enable);
        return 0;
    }

    process_tag_initialized = true;
    process_tag_enabled = enable;

    if (!enable) {
        pr_info("process_tag: first feature set is disabled, subsystem remains off until reboot\n");
        return 0;
    }

    ret = ksu_register_lsm_hook(&process_tag_task_alloc_hook);
    if (ret) {
        pr_err("process_tag: failed to register task_alloc hook: %d\n", ret);
        process_tag_initialized = false;
        process_tag_enabled = false;
        return ret;
    }

    ret = ksu_register_lsm_hook(&process_tag_task_free_hook);
    if (ret) {
        pr_err("process_tag: failed to register task_free hook: %d\n", ret);
        ksu_unregister_lsm_hook(&process_tag_task_alloc_hook);
        process_tag_initialized = false;
        process_tag_enabled = false;
        return ret;
    }

    process_tag_lsm_hooked = true;
    pr_info("process_tag: initialized and enabled\n");

    // mark the feature setter as ksud
    ksu_process_tag_set(current, PROCESS_TAG_KSUD, "");

    return 0;
}

static const struct ksu_feature_handler process_tag_handler = {
    .feature_id = KSU_FEATURE_PROCESS_TAG,
    .name = "process_tag",
    .get_handler = process_tag_feature_get,
    .set_handler = process_tag_feature_set,
};

void __init ksu_process_tag_init(void)
{
    int ret;

    hash_init(process_tag_table);

    ret = ksu_register_feature_handler(&process_tag_handler);
    if (ret) {
        pr_err("process_tag: failed to register feature handler\n");
        return;
    }

    pr_info("process_tag: module initialized\n");
}

void __exit ksu_process_tag_exit(void)
{
    process_tag_enabled = false;
    process_tag_initialized = false;

    if (process_tag_lsm_hooked) {
        ksu_unregister_lsm_hook(&process_tag_task_alloc_hook);
        ksu_unregister_lsm_hook(&process_tag_task_free_hook);
        process_tag_lsm_hooked = false;
    }

    ksu_process_tag_flush();
    synchronize_rcu();
    ksu_unregister_feature_handler(KSU_FEATURE_PROCESS_TAG);

    pr_info("process_tag: module exited\n");
}

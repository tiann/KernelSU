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
#include "klog.h" // IWYU pragma: keep

#define PROCESS_TAG_HASH_BITS 10 // 1024 buckets

static DEFINE_HASHTABLE(process_tag_table, PROCESS_TAG_HASH_BITS);
static DEFINE_SPINLOCK(process_tag_lock);
static bool process_tag_enabled __read_mostly = false;
static bool process_tag_initialized __read_mostly = false;
static bool process_tag_tracepoint_registered __read_mostly = false;

struct process_tag_entry {
    struct hlist_node hnode;
    pid_t pid;
    struct process_tag *tag;
    struct rcu_head rcu;
};

static void process_tag_entry_free(struct rcu_head *rcu)
{
    struct process_tag_entry *entry = container_of(rcu, struct process_tag_entry, rcu);
    kfree(entry);
}

static void process_tag_free(struct rcu_head *rcu)
{
    struct process_tag *tag = container_of(rcu, struct process_tag, rcu);
    kfree(tag);
}

int ksu_process_tag_set(pid_t pid, enum process_tag_type type, const char *name)
{
    struct process_tag_entry *entry, *existing = NULL;
    struct process_tag *new_tag;
    unsigned long flags;

    if (!process_tag_enabled) {
        return -ENODEV;
    }

    if (!name) {
        name = "";
    }

    new_tag = kmalloc(sizeof(struct process_tag), GFP_KERNEL);
    if (!new_tag) {
        pr_err("process_tag: failed to alloc tag for pid %d\n", pid);
        return -ENOMEM;
    }

    new_tag->type = type;
    strncpy(new_tag->name, name, sizeof(new_tag->name) - 1);
    new_tag->name[sizeof(new_tag->name) - 1] = '\0';
    atomic_set(&new_tag->refcount, 1);

    entry = kmalloc(sizeof(struct process_tag_entry), GFP_KERNEL);
    if (!entry) {
        pr_err("process_tag: failed to alloc entry for pid %d\n", pid);
        kfree(new_tag);
        return -ENOMEM;
    }

    entry->pid = pid;
    entry->tag = new_tag;

    spin_lock_irqsave(&process_tag_lock, flags);

    hash_for_each_possible (process_tag_table, existing, hnode, pid) {
        if (existing->pid == pid) {
            struct process_tag *old_tag =
                rcu_replace_pointer(existing->tag, new_tag, lockdep_is_held(&process_tag_lock));
            spin_unlock_irqrestore(&process_tag_lock, flags);

            if (old_tag && atomic_dec_and_test(&old_tag->refcount)) {
                call_rcu(&old_tag->rcu, process_tag_free);
            }
            kfree(entry);

            pr_debug("process_tag: updated tag for pid %d, type=%d, name=%s\n", pid, type, name);
            return 0;
        }
    }

    hash_add_rcu(process_tag_table, &entry->hnode, pid);
    spin_unlock_irqrestore(&process_tag_lock, flags);

    pr_debug("process_tag: added tag for pid %d, type=%d, name=%s\n", pid, type, name);
    return 0;
}

struct process_tag *ksu_process_tag_get(pid_t pid)
{
    struct process_tag_entry *entry;
    struct process_tag *tag = NULL;

    if (!process_tag_enabled) {
        return NULL;
    }

    rcu_read_lock();
    hash_for_each_possible_rcu (process_tag_table, entry, hnode, pid) {
        if (entry->pid == pid) {
            tag = rcu_dereference(entry->tag);
            if (tag) {
                atomic_inc(&tag->refcount);
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
        call_rcu(&tag->rcu, process_tag_free);
    }
}

void ksu_process_tag_delete(pid_t pid)
{
    struct process_tag_entry *entry;
    struct process_tag *tag;
    unsigned long flags;

    if (!process_tag_enabled) {
        return;
    }

    spin_lock_irqsave(&process_tag_lock, flags);

    hash_for_each_possible (process_tag_table, entry, hnode, pid) {
        if (entry->pid == pid) {
            tag = rcu_dereference_protected(entry->tag, lockdep_is_held(&process_tag_lock));
            hash_del_rcu(&entry->hnode);
            spin_unlock_irqrestore(&process_tag_lock, flags);

            if (tag && atomic_dec_and_test(&tag->refcount)) {
                call_rcu(&tag->rcu, process_tag_free);
            }

            call_rcu(&entry->rcu, process_tag_entry_free);
            pr_debug("process_tag: deleted tag for pid %d\n", pid);
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
            call_rcu(&tag->rcu, process_tag_free);
        }

        call_rcu(&entry->rcu, process_tag_entry_free);
    }

    spin_unlock_irqrestore(&process_tag_lock, flags);

    pr_info("process_tag: flushed all entries\n");
}

bool ksu_process_tag_is_enabled(void)
{
    return process_tag_enabled;
}

// Tracepoint handlers for fork/exit
static void process_tag_sched_fork_handler(void *data, struct task_struct *parent, struct task_struct *child)
{
    struct process_tag *parent_tag;

    if (!process_tag_enabled) {
        return;
    }

    rcu_read_lock();
    parent_tag = ksu_process_tag_get(task_pid_nr(parent));
    if (parent_tag) {
        ksu_process_tag_set(task_pid_nr(child), parent_tag->type, parent_tag->name);
        ksu_process_tag_put(parent_tag);
        pr_debug("process_tag: child %d inherited tag from parent %d\n", task_pid_nr(child), task_pid_nr(parent));
    }
    rcu_read_unlock();
}

static void process_tag_sched_exit_handler(void *data, struct task_struct *task)
{
    if (!process_tag_enabled) {
        return;
    }

    ksu_process_tag_delete(task_pid_nr(task));
}

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

    ret = register_trace_sched_process_fork(process_tag_sched_fork_handler, NULL);
    if (ret) {
        pr_err("process_tag: failed to register sched_process_fork tracepoint: %d\n", ret);
        process_tag_initialized = false;
        process_tag_enabled = false;
        return ret;
    }

    ret = register_trace_sched_process_exit(process_tag_sched_exit_handler, NULL);
    if (ret) {
        pr_err("process_tag: failed to register sched_process_exit tracepoint: %d\n", ret);
        unregister_trace_sched_process_fork(process_tag_sched_fork_handler, NULL);
        process_tag_initialized = false;
        process_tag_enabled = false;
        return ret;
    }

    process_tag_tracepoint_registered = true;
    pr_info("process_tag: initialized and enabled\n");

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

    if (process_tag_tracepoint_registered) {
        unregister_trace_sched_process_fork(process_tag_sched_fork_handler, NULL);
        unregister_trace_sched_process_exit(process_tag_sched_exit_handler, NULL);
        process_tag_tracepoint_registered = false;
    }

    ksu_process_tag_flush();
    synchronize_rcu();
    ksu_unregister_feature_handler(KSU_FEATURE_PROCESS_TAG);

    pr_info("process_tag: module exited\n");
}

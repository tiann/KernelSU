#include <linux/err.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/wait.h>

#include "infra/event_queue.h"

struct ksu_event_queue_entry {
    struct list_head list;
    struct ksu_event_record_hdr hdr;
    __u32 capacity;
    __u8 payload[];
};

static size_t ksu_event_queue_record_size(__u32 payload_len)
{
    return sizeof(struct ksu_event_record_hdr) + payload_len;
}

static void ksu_event_queue_note_drop_locked(struct ksu_event_queue *queue, __u64 seq)
{
    queue->dropped_total++;
    if (!queue->dropped_pending) {
        queue->dropped_first_seq = seq;
    }
    queue->dropped_pending++;
    queue->dropped_last_seq = seq;
}

static bool ksu_event_queue_has_data_locked(const struct ksu_event_queue *queue)
{
    return queue->dropped_pending || queue->dropped_inflight || !list_empty(&queue->pending);
}

static void ksu_event_queue_mark_closed(struct ksu_event_queue *queue)
{
    unsigned long irq_flags;

    spin_lock_irqsave(&queue->lock, irq_flags);
    queue->closed = true;
    spin_unlock_irqrestore(&queue->lock, irq_flags);
}

void ksu_event_queue_init(struct ksu_event_queue *queue, __u32 max_queued, __u32 max_payload_len)
{
    spin_lock_init(&queue->lock);
    mutex_init(&queue->read_lock);
    INIT_LIST_HEAD(&queue->pending);
    init_waitqueue_head(&queue->read_wait);
    queue->queued = 0;
    queue->max_queued = max_queued;
    queue->max_payload_len = max_payload_len;
    queue->next_seq = 1;
    queue->dropped_total = 0;
    queue->dropped_pending = 0;
    queue->dropped_first_seq = 0;
    queue->dropped_last_seq = 0;
    queue->dropped_inflight = 0;
    queue->dropped_inflight_first_seq = 0;
    queue->dropped_inflight_last_seq = 0;
    queue->closed = false;
}

void ksu_event_queue_destroy(struct ksu_event_queue *queue)
{
    struct ksu_event_queue_entry *entry, *tmp;
    unsigned long irq_flags;

    ksu_event_queue_mark_closed(queue);
    wake_up_interruptible_poll(&queue->read_wait, EPOLLHUP | POLLHUP);

    mutex_lock(&queue->read_lock);
    spin_lock_irqsave(&queue->lock, irq_flags);
    list_for_each_entry_safe (entry, tmp, &queue->pending, list) {
        list_del(&entry->list);
        kfree(entry);
    }
    queue->queued = 0;
    queue->dropped_pending = 0;
    queue->dropped_first_seq = 0;
    queue->dropped_last_seq = 0;
    queue->dropped_inflight = 0;
    queue->dropped_inflight_first_seq = 0;
    queue->dropped_inflight_last_seq = 0;
    spin_unlock_irqrestore(&queue->lock, irq_flags);
    mutex_unlock(&queue->read_lock);

    wake_up_interruptible_poll(&queue->read_wait, EPOLLHUP | POLLHUP);
}

struct ksu_event_queue_entry *ksu_event_queue_entry_alloc(struct ksu_event_queue *queue, __u16 type, __u16 flags,
                                                          __u32 capacity, gfp_t gfp)
{
    struct ksu_event_queue_entry *entry;

    if (capacity > queue->max_payload_len) {
        return ERR_PTR(-EMSGSIZE);
    }

    entry = kmalloc(struct_size(entry, payload, capacity), gfp);
    if (!entry) {
        return ERR_PTR(-ENOMEM);
    }

    INIT_LIST_HEAD(&entry->list);
    entry->hdr.type = type;
    entry->hdr.flags = flags;
    entry->hdr.len = capacity;
    entry->hdr.ts_ns = 0;
    entry->hdr.seq = 0;
    entry->capacity = capacity;
    return entry;
}

void *ksu_event_queue_entry_payload(struct ksu_event_queue_entry *entry)
{
    return entry ? entry->payload : NULL;
}

int ksu_event_queue_entry_set_len(struct ksu_event_queue_entry *entry, __u32 len)
{
    if (!entry || len > entry->capacity) {
        return -EINVAL;
    }
    entry->hdr.len = len;
    return 0;
}

int ksu_event_queue_entry_push(struct ksu_event_queue *queue, struct ksu_event_queue_entry *entry)
{
    unsigned long irq_flags;
    __u64 seq;
    bool wake = false;
    int ret = 0;

    if (!entry) {
        return -EINVAL;
    }
    if (entry->hdr.len > entry->capacity || entry->hdr.len > queue->max_payload_len) {
        return -EMSGSIZE;
    }

    spin_lock_irqsave(&queue->lock, irq_flags);
    if (queue->closed) {
        ret = -EPIPE;
        goto out_unlock;
    }

    seq = queue->next_seq++;
    if (queue->max_queued && queue->queued >= queue->max_queued) {
        ksu_event_queue_note_drop_locked(queue, seq);
        wake = true;
        ret = -ENOSPC;
        goto out_unlock;
    }

    entry->hdr.seq = seq;
    entry->hdr.ts_ns = ktime_get_ns();
    list_add_tail(&entry->list, &queue->pending);
    queue->queued++;
    wake = true;

out_unlock:
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    if (wake) {
        wake_up_interruptible_poll(&queue->read_wait, EPOLLIN | EPOLLRDNORM);
    }

    return ret;
}

void ksu_event_queue_entry_free(struct ksu_event_queue_entry *entry)
{
    kfree(entry);
}

int ksu_event_queue_push(struct ksu_event_queue *queue, __u16 type, __u16 flags, const void *payload, __u32 len,
                         gfp_t gfp)
{
    struct ksu_event_queue_entry *entry;
    int ret;

    if (len && !payload) {
        return -EINVAL;
    }

    entry = ksu_event_queue_entry_alloc(queue, type, flags, len, gfp);
    if (IS_ERR(entry)) {
        ret = PTR_ERR(entry);
        if (ret == -ENOMEM) {
            ksu_event_queue_drop(queue);
        }
        return ret;
    }

    if (len) {
        memcpy(entry->payload, payload, len);
    }

    ret = ksu_event_queue_entry_push(queue, entry);
    if (ret) {
        ksu_event_queue_entry_free(entry);
    }
    return ret;
}

void ksu_event_queue_drop(struct ksu_event_queue *queue)
{
    unsigned long irq_flags;
    __u64 seq;

    spin_lock_irqsave(&queue->lock, irq_flags);
    if (queue->closed) {
        spin_unlock_irqrestore(&queue->lock, irq_flags);
        return;
    }

    seq = queue->next_seq++;
    ksu_event_queue_note_drop_locked(queue, seq);
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    wake_up_interruptible_poll(&queue->read_wait, EPOLLIN | EPOLLRDNORM);
}

static int ksu_event_queue_wait_ready(struct ksu_event_queue *queue, int file_flags)
{
    int ret;

    for (;;) {
        if (ksu_event_queue_has_data(queue)) {
            return 0;
        }

        if (READ_ONCE(queue->closed)) {
            return 0;
        }

        if (file_flags & O_NONBLOCK) {
            return -EAGAIN;
        }

        ret = wait_event_interruptible(queue->read_wait, queue->closed || ksu_event_queue_has_data(queue));
        if (ret) {
            return ret;
        }
    }
}

static ssize_t ksu_event_queue_read_drop(struct ksu_event_queue *queue, char __user *buf, size_t count)
{
    struct ksu_event_record_hdr hdr;
    struct ksu_event_queue_dropped_info info;
    size_t record_size = ksu_event_queue_record_size(sizeof(info));
    unsigned long irq_flags;

    spin_lock_irqsave(&queue->lock, irq_flags);
    if (!queue->dropped_pending) {
        spin_unlock_irqrestore(&queue->lock, irq_flags);
        return 0;
    }
    if (count < record_size) {
        spin_unlock_irqrestore(&queue->lock, irq_flags);
        return -EMSGSIZE;
    }

    hdr.type = KSU_EVENT_QUEUE_TYPE_DROPPED;
    hdr.flags = KSU_EVENT_RECORD_FLAG_INTERNAL;
    hdr.len = sizeof(info);
    hdr.seq = queue->dropped_first_seq;
    hdr.ts_ns = ktime_get_ns();

    info.dropped = queue->dropped_pending;
    info.first_seq = queue->dropped_first_seq;
    info.last_seq = queue->dropped_last_seq;

    queue->dropped_inflight = queue->dropped_pending;
    queue->dropped_inflight_first_seq = queue->dropped_first_seq;
    queue->dropped_inflight_last_seq = queue->dropped_last_seq;
    queue->dropped_pending = 0;
    queue->dropped_first_seq = 0;
    queue->dropped_last_seq = 0;
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    if (copy_to_user(buf, &hdr, sizeof(hdr))) {
        goto out_restore;
    }

    if (copy_to_user(buf + sizeof(hdr), &info, sizeof(info))) {
        goto out_restore;
    }

    spin_lock_irqsave(&queue->lock, irq_flags);
    queue->dropped_inflight = 0;
    queue->dropped_inflight_first_seq = 0;
    queue->dropped_inflight_last_seq = 0;
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    return record_size;

out_restore:
    spin_lock_irqsave(&queue->lock, irq_flags);
    if (!queue->dropped_pending) {
        queue->dropped_pending = queue->dropped_inflight;
        queue->dropped_first_seq = queue->dropped_inflight_first_seq;
        queue->dropped_last_seq = queue->dropped_inflight_last_seq;
    } else {
        queue->dropped_pending += queue->dropped_inflight;
        queue->dropped_first_seq = queue->dropped_inflight_first_seq;
    }
    queue->dropped_inflight = 0;
    queue->dropped_inflight_first_seq = 0;
    queue->dropped_inflight_last_seq = 0;
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    return -EFAULT;
}

static ssize_t ksu_event_queue_read_entry(struct ksu_event_queue *queue, char __user *buf, size_t count)
{
    struct ksu_event_queue_entry *entry;
    struct list_head *first;
    size_t record_size;
    unsigned long irq_flags;

    spin_lock_irqsave(&queue->lock, irq_flags);
    if (list_empty(&queue->pending)) {
        spin_unlock_irqrestore(&queue->lock, irq_flags);
        return 0;
    }

    first = queue->pending.next;
    entry = list_entry(first, struct ksu_event_queue_entry, list);
    record_size = ksu_event_queue_record_size(entry->hdr.len);
    if (count < record_size) {
        spin_unlock_irqrestore(&queue->lock, irq_flags);
        return -EMSGSIZE;
    }
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    if (copy_to_user(buf, &entry->hdr, sizeof(entry->hdr))) {
        return -EFAULT;
    }

    if (entry->hdr.len && copy_to_user(buf + sizeof(entry->hdr), entry->payload, entry->hdr.len)) {
        return -EFAULT;
    }

    spin_lock_irqsave(&queue->lock, irq_flags);
    list_del(first);
    queue->queued--;
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    kfree(entry);
    return record_size;
}

ssize_t ksu_event_queue_read(struct ksu_event_queue *queue, char __user *buf, size_t count, int file_flags)
{
    ssize_t ret;
    ssize_t copied = 0;

    if (!count) {
        return 0;
    }

    ret = mutex_lock_interruptible(&queue->read_lock);
    if (ret) {
        return ret;
    }

    ret = ksu_event_queue_wait_ready(queue, file_flags);
    if (ret) {
        copied = ret;
        goto out_unlock;
    }

    while (count > 0) {
        ret = ksu_event_queue_read_drop(queue, buf, count);
        if (ret < 0) {
            if (!copied) {
                copied = ret;
            }
            break;
        }
        if (ret > 0) {
            copied += ret;
            buf += ret;
            count -= ret;
            continue;
        }

        ret = ksu_event_queue_read_entry(queue, buf, count);
        if (ret < 0) {
            if (!copied) {
                copied = ret;
            }
            break;
        }
        if (ret == 0) {
            break;
        }

        copied += ret;
        buf += ret;
        count -= ret;
    }

    if (!copied && READ_ONCE(queue->closed)) {
        copied = 0;
    }

out_unlock:
    mutex_unlock(&queue->read_lock);
    return copied;
}

__poll_t ksu_event_queue_poll(struct ksu_event_queue *queue, struct file *file, poll_table *wait)
{
    __poll_t mask = 0;
    unsigned long irq_flags;

    poll_wait(file, &queue->read_wait, wait);

    spin_lock_irqsave(&queue->lock, irq_flags);
    if (ksu_event_queue_has_data_locked(queue)) {
        mask |= POLLIN | POLLRDNORM;
    }
    if (queue->closed) {
        mask |= POLLHUP;
    }
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    return mask;
}

void ksu_event_queue_close(struct ksu_event_queue *queue)
{
    ksu_event_queue_mark_closed(queue);
    wake_up_interruptible_poll(&queue->read_wait, EPOLLHUP | POLLHUP);
}

bool ksu_event_queue_has_data(struct ksu_event_queue *queue)
{
    bool has_data;
    unsigned long irq_flags;

    spin_lock_irqsave(&queue->lock, irq_flags);
    has_data = ksu_event_queue_has_data_locked(queue);
    spin_unlock_irqrestore(&queue->lock, irq_flags);

    return has_data;
}

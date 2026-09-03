#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/overflow.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/srcu.h>
#include <linux/string.h>
#include <linux/task_work.h>
#include <linux/wait.h>

#include "api/event_registry.h"
#include "uapi/supercall.h"

/*
 * Handler storage is a per-event, immutable, copy-on-write array that is
 * published with rcu_assign_pointer() and read under SRCU.  Readers never walk
 * a linked list that a concurrent unregister may be unlinking (a reverse walk
 * of an RCU list is not safe because list_del_rcu() poisons ->prev), an
 * emitter with no handlers costs a single pointer load, and set_state() gets a
 * stable snapshot for free.
 *
 * Emitters do not take the registry mutex: they bump an active counter and
 * then check the stopping flag (Dekker pairing with registry_stop()), which
 * keeps ksu_reboot_dispatch() usable from the reboot kprobe handler.
 *
 * Registration does not pin the owner module.  Holding a reference would make
 * the owner impossible to unload (its exit routine is where it unregisters).
 * Safety comes from unregister: it returns only after every SRCU reader and
 * queued reboot task work has stopped using the handler.
 */

struct ksu_event_handler {
    struct rcu_head rcu;
    struct ksu_event_handler_desc desc;
    struct module *owner;
    bool registered;
};

struct ksu_event_vec {
    struct rcu_head rcu;
    size_t count;
    struct ksu_event_handler *handlers[];
};

struct ksu_reboot_handler {
    struct ksu_reboot_handler_desc desc;
    struct module *owner;
    bool registered;
    /* Set by a self-unregister; the last task work then frees the handler. */
    bool deferred_free;
    atomic_t pending;
    wait_queue_head_t wait;
};

struct ksu_reboot_vec {
    struct rcu_head rcu;
    size_t count;
    struct ksu_reboot_handler *handlers[];
};

/* Which handler callbacks are running on which task: lets unregister defer a
 * self-unregister and refuse (-EDEADLK) a cross-unregister from a callback. */
struct ksu_callback_frame {
    struct list_head node;
    struct task_struct *task;
    const void *handler;
};

struct ksu_callback_tracker {
    spinlock_t lock;
    struct list_head frames;
};

static void tracker_init(struct ksu_callback_tracker *t)
{
    spin_lock_init(&t->lock);
    INIT_LIST_HEAD(&t->frames);
}

static void frame_enter(struct ksu_callback_tracker *t, struct ksu_callback_frame *f, const void *handler)
{
    f->task = current;
    f->handler = handler;
    spin_lock(&t->lock);
    list_add(&f->node, &t->frames);
    spin_unlock(&t->lock);
}

static void frame_exit(struct ksu_callback_tracker *t, struct ksu_callback_frame *f)
{
    spin_lock(&t->lock);
    list_del(&f->node);
    spin_unlock(&t->lock);
}

/* handler == NULL: is any callback running on this task? */
static bool frame_active(struct ksu_callback_tracker *t, const void *handler)
{
    struct ksu_callback_frame *f;
    bool active = false;

    spin_lock(&t->lock);
    list_for_each_entry (f, &t->frames, node) {
        if (f->task == current && (!handler || f->handler == handler)) {
            active = true;
            break;
        }
    }
    spin_unlock(&t->lock);
    return active;
}

static struct {
    struct mutex lock;
    struct srcu_struct srcu;
    struct ksu_event_vec __rcu *vecs[KSU_EVENT_MAX];
    unsigned long state;
    atomic_t active_emitters;
    wait_queue_head_t wait;
    bool stopping;
    struct ksu_callback_tracker tracker;
} event_registry;

static struct {
    struct mutex lock;
    struct srcu_struct srcu;
    struct ksu_reboot_vec __rcu *vec;
    atomic_t active_dispatchers;
    wait_queue_head_t wait;
    bool stopping;
    struct ksu_callback_tracker tracker;
} reboot_registry;

static void emitter_exit(atomic_t *active, wait_queue_head_t *wait)
{
    if (atomic_dec_and_test(active))
        wake_up_all(wait);
}

/* Pairs with registry_stop(): either we observe stopping, or the stopper
 * observes our increment and waits for emitter_exit(). */
static bool emitter_enter(atomic_t *active, const bool *stopping, wait_queue_head_t *wait)
{
    atomic_inc(active);
    smp_mb__after_atomic();
    if (READ_ONCE(*stopping)) {
        emitter_exit(active, wait);
        return false;
    }
    return true;
}

static void registry_stop(struct mutex *lock, bool *stopping)
{
    mutex_lock(lock);
    WRITE_ONCE(*stopping, true);
    mutex_unlock(lock);
    smp_mb();
}

static void state_context(enum ksu_event event, struct ksu_state_event *ctx)
{
    ctx->size = sizeof(*ctx);
    ctx->version = 1;
    ctx->event = event;
    ctx->reserved = 0;
}

static bool replayable(enum ksu_event event)
{
    return event == KSU_EVENT_CORE_READY || event == KSU_EVENT_POST_FS_DATA || event == KSU_EVENT_MODULE_MOUNTED ||
           event == KSU_EVENT_BOOT_COMPLETED;
}

static bool reserved_reboot_magic(__u32 magic1, __u32 magic2)
{
    return magic1 == KSU_INSTALL_MAGIC1 && magic2 == KSU_INSTALL_MAGIC2;
}

/* ---- event handlers ---------------------------------------------------- */

static void free_event_vec_rcu(struct rcu_head *rcu)
{
    kfree(container_of(rcu, struct ksu_event_vec, rcu));
}

static void free_event_handler_rcu(struct rcu_head *rcu)
{
    kfree(container_of(rcu, struct ksu_event_handler, rcu));
}

static struct ksu_event_vec *event_vec_alloc(size_t count, gfp_t gfp)
{
    struct ksu_event_vec *vec = kzalloc(struct_size(vec, handlers, count), gfp);

    if (vec)
        vec->count = count;
    return vec;
}

static int invoke_pre(struct ksu_event_handler *handler, const void *ctx)
{
    struct ksu_callback_frame frame;
    int ret;

    frame_enter(&event_registry.tracker, &frame, handler);
    ret = handler->desc.pre(ctx, handler->desc.data);
    frame_exit(&event_registry.tracker, &frame);
    return ret;
}

static void invoke_post(struct ksu_event_handler *handler, const void *ctx, long result)
{
    struct ksu_callback_frame frame;

    frame_enter(&event_registry.tracker, &frame, handler);
    handler->desc.post(ctx, result, handler->desc.data);
    frame_exit(&event_registry.tracker, &frame);
}

/* PRE in ascending priority, POST in descending priority. */
static int run_event_vec(const struct ksu_event_vec *vec, const void *ctx, long result)
{
    struct ksu_event_handler *handler;
    int ret = 0;
    size_t i;

    for (i = 0; i < vec->count; i++) {
        handler = vec->handlers[i];
        if (!handler->desc.pre)
            continue;
        if (invoke_pre(handler, ctx) && (handler->desc.flags & KSU_EVENT_HANDLER_POLICY))
            ret = -EPERM;
    }
    for (i = vec->count; i > 0; i--) {
        handler = vec->handlers[i - 1];
        if (handler->desc.post)
            invoke_post(handler, ctx, result);
    }
    return ret;
}

int ksu_register_event_handler(const struct ksu_event_handler_desc *desc, struct module *owner, void **cookie)
{
    struct ksu_event_handler *handler;
    struct ksu_event_vec *old, *new;
    struct ksu_state_event state;
    bool replay = false;
    size_t count, pos, i;
    int idx = 0;

    if (!desc || !cookie || !owner || desc->event >= KSU_EVENT_MAX || (!desc->pre && !desc->post))
        return -EINVAL;
    /* API v1 has no policy events; refuse the flag so nobody relies on a veto. */
    if (desc->flags & KSU_EVENT_HANDLER_POLICY)
        return -EINVAL;
    if (ksu_event_is_stopping())
        return -ESHUTDOWN;

    handler = kzalloc(sizeof(*handler), GFP_KERNEL);
    if (!handler)
        return -ENOMEM;
    handler->desc = *desc;
    handler->owner = owner;

    mutex_lock(&event_registry.lock);
    if (event_registry.stopping) {
        mutex_unlock(&event_registry.lock);
        kfree(handler);
        return -ESHUTDOWN;
    }
    old = rcu_dereference_protected(event_registry.vecs[desc->event], lockdep_is_held(&event_registry.lock));
    count = old ? old->count : 0;
    for (i = 0; i < count; i++) {
        const struct ksu_event_handler *it = old->handlers[i];

        if (it->desc.pre == desc->pre && it->desc.post == desc->post && it->desc.data == desc->data &&
            it->owner == owner) {
            mutex_unlock(&event_registry.lock);
            kfree(handler);
            return -EEXIST;
        }
    }
    new = event_vec_alloc(count + 1, GFP_KERNEL);
    if (!new) {
        mutex_unlock(&event_registry.lock);
        kfree(handler);
        return -ENOMEM;
    }
    /* Insert after every handler with priority <= ours (stable for ties). */
    pos = count;
    for (i = 0; i < count; i++) {
        if (old->handlers[i]->desc.priority > desc->priority) {
            pos = i;
            break;
        }
    }
    for (i = 0; i < pos; i++)
        new->handlers[i] = old->handlers[i];
    new->handlers[pos] = handler;
    for (i = pos; i < count; i++)
        new->handlers[i + 1] = old->handlers[i];
    handler->registered = true;
    rcu_assign_pointer(event_registry.vecs[desc->event], new);

    replay = (desc->flags & KSU_EVENT_HANDLER_REPLAY) && replayable(desc->event) &&
             test_bit(desc->event, &event_registry.state);
    if (replay) {
        /* Keep the handler alive across the replay even if it unregisters
         * itself from the callback. */
        atomic_inc(&event_registry.active_emitters);
        idx = srcu_read_lock(&event_registry.srcu);
    }
    mutex_unlock(&event_registry.lock);
    if (old)
        call_srcu(&event_registry.srcu, &old->rcu, free_event_vec_rcu);
    *cookie = handler;

    if (replay) {
        state_context(desc->event, &state);
        if (handler->desc.pre)
            invoke_pre(handler, &state);
        if (handler->desc.post)
            invoke_post(handler, &state, 0);
        srcu_read_unlock(&event_registry.srcu, idx);
        emitter_exit(&event_registry.active_emitters, &event_registry.wait);
    }
    return 0;
}

/* Caller holds event_registry.lock. Returns the vec without @handler (NULL if empty). */
static struct ksu_event_vec *event_vec_remove(const struct ksu_event_vec *old, const struct ksu_event_handler *handler)
{
    struct ksu_event_vec *new;
    size_t i, j = 0;

    if (old->count <= 1)
        return NULL;
    /* Unregister must not fail; the array is tiny. */
    new = event_vec_alloc(old->count - 1, GFP_KERNEL | __GFP_NOFAIL);
    for (i = 0; i < old->count; i++) {
        if (old->handlers[i] != handler)
            new->handlers[j++] = old->handlers[i];
    }
    return new;
}

int ksu_unregister_event_handler(void *cookie)
{
    struct ksu_event_handler *handler = cookie;
    struct ksu_event_vec *old;
    bool self;

    if (!handler)
        return -EINVAL;
    self = frame_active(&event_registry.tracker, handler);
    if (!self && frame_active(&event_registry.tracker, NULL))
        return -EDEADLK;

    mutex_lock(&event_registry.lock);
    if (!handler->registered) {
        mutex_unlock(&event_registry.lock);
        return -ENOENT;
    }
    old = rcu_dereference_protected(event_registry.vecs[handler->desc.event], lockdep_is_held(&event_registry.lock));
    rcu_assign_pointer(event_registry.vecs[handler->desc.event], event_vec_remove(old, handler));
    handler->registered = false;
    mutex_unlock(&event_registry.lock);

    if (self) {
        /* Our own callback is still on the stack: free after the readers. */
        call_srcu(&event_registry.srcu, &old->rcu, free_event_vec_rcu);
        call_srcu(&event_registry.srcu, &handler->rcu, free_event_handler_rcu);
        return 0;
    }
    synchronize_srcu(&event_registry.srcu);
    kfree(old);
    kfree(handler);
    return 0;
}

bool ksu_event_has_handlers(enum ksu_event event)
{
    return event < KSU_EVENT_MAX && rcu_access_pointer(event_registry.vecs[event]) != NULL;
}

int ksu_event_emit(enum ksu_event event, const void *ctx, long result)
{
    struct ksu_event_vec *vec;
    int ret = 0;
    int idx;

    if (event >= KSU_EVENT_MAX)
        return -EINVAL;
    /* Fast path for the hot execve/setresuid callers. */
    if (!rcu_access_pointer(event_registry.vecs[event]))
        return 0;
    if (!emitter_enter(&event_registry.active_emitters, &event_registry.stopping, &event_registry.wait))
        return -ESHUTDOWN;

    idx = srcu_read_lock(&event_registry.srcu);
    vec = srcu_dereference(event_registry.vecs[event], &event_registry.srcu);
    if (vec)
        ret = run_event_vec(vec, ctx, result);
    srcu_read_unlock(&event_registry.srcu, idx);

    emitter_exit(&event_registry.active_emitters, &event_registry.wait);
    return ret;
}

static void core_exiting(void)
{
    struct ksu_event_vec *vec = NULL;
    struct ksu_state_event state;
    int idx = 0;

    mutex_lock(&event_registry.lock);
    if (!event_registry.stopping) {
        WRITE_ONCE(event_registry.stopping, true);
        vec = rcu_dereference_protected(event_registry.vecs[KSU_EVENT_CORE_EXITING],
                                        lockdep_is_held(&event_registry.lock));
        if (vec) {
            atomic_inc(&event_registry.active_emitters);
            idx = srcu_read_lock(&event_registry.srcu);
        }
    }
    mutex_unlock(&event_registry.lock);
    smp_mb();
    registry_stop(&reboot_registry.lock, &reboot_registry.stopping);

    /* CORE_EXITING is the one notification delivered after stopping. */
    if (vec) {
        state_context(KSU_EVENT_CORE_EXITING, &state);
        run_event_vec(vec, &state, 0);
        srcu_read_unlock(&event_registry.srcu, idx);
        emitter_exit(&event_registry.active_emitters, &event_registry.wait);
    }
}

void ksu_event_set_state(enum ksu_event event)
{
    struct ksu_event_vec *vec;
    struct ksu_state_event state;
    int idx;

    if (event >= KSU_EVENT_MAX)
        return;
    if (event == KSU_EVENT_CORE_EXITING) {
        core_exiting();
        return;
    }
    if (!replayable(event))
        return;

    mutex_lock(&event_registry.lock);
    if (event_registry.stopping || test_and_set_bit(event, &event_registry.state)) {
        mutex_unlock(&event_registry.lock);
        return;
    }
    vec = rcu_dereference_protected(event_registry.vecs[event], lockdep_is_held(&event_registry.lock));
    if (!vec) {
        mutex_unlock(&event_registry.lock);
        return;
    }
    /* Deliver to the handlers present now.  A handler registered after this
     * point sees the state bit and replays itself, so nobody gets it twice. */
    atomic_inc(&event_registry.active_emitters);
    idx = srcu_read_lock(&event_registry.srcu);
    mutex_unlock(&event_registry.lock);

    state_context(event, &state);
    run_event_vec(vec, &state, 0);
    srcu_read_unlock(&event_registry.srcu, idx);
    emitter_exit(&event_registry.active_emitters, &event_registry.wait);
}

bool ksu_event_is_stopping(void)
{
    return READ_ONCE(event_registry.stopping);
}

/* ---- reboot handlers --------------------------------------------------- */

static void free_reboot_vec_rcu(struct rcu_head *rcu)
{
    kfree(container_of(rcu, struct ksu_reboot_vec, rcu));
}

static struct ksu_reboot_vec *reboot_vec_alloc(size_t count, gfp_t gfp)
{
    struct ksu_reboot_vec *vec = kzalloc(struct_size(vec, handlers, count), gfp);

    if (vec)
        vec->count = count;
    return vec;
}

/* Drop one queued task work.  The pending count and deferred_free are
 * serialised by wait.lock so the waiter in reboot_wait_idle() cannot see
 * pending == 0 while we still touch the handler. */
static void reboot_pending_put(struct ksu_reboot_handler *handler)
{
    unsigned long flags;
    bool free_now = false;

    spin_lock_irqsave(&handler->wait.lock, flags);
    if (atomic_dec_and_test(&handler->pending)) {
        if (handler->deferred_free)
            free_now = true;
        else
            wake_up_all_locked(&handler->wait);
    }
    spin_unlock_irqrestore(&handler->wait.lock, flags);
    if (free_now)
        kfree(handler);
}

static void reboot_wait_idle(struct ksu_reboot_handler *handler)
{
    spin_lock_irq(&handler->wait.lock);
    wait_event_lock_irq(handler->wait, atomic_read(&handler->pending) == 0, handler->wait.lock);
    spin_unlock_irq(&handler->wait.lock);
}

struct ksu_reboot_task_work {
    struct callback_head cb;
    __u32 magic1;
    __u32 magic2;
    unsigned long arg;
    struct ksu_reboot_handler *handler;
};

static void ksu_reboot_task_work_func(struct callback_head *cb)
{
    struct ksu_reboot_task_work *work = container_of(cb, struct ksu_reboot_task_work, cb);
    struct ksu_reboot_handler *handler = work->handler;
    struct ksu_callback_frame frame;

    frame_enter(&reboot_registry.tracker, &frame, handler);
    handler->desc.fn(work->magic1, work->magic2, work->arg, handler->desc.data);
    frame_exit(&reboot_registry.tracker, &frame);
    reboot_pending_put(handler);
    kfree(work);
}

int ksu_register_reboot_handler(const struct ksu_reboot_handler_desc *desc, struct module *owner, void **cookie)
{
    struct ksu_reboot_handler *handler;
    struct ksu_reboot_vec *old, *new;
    size_t count, i;

    if (!desc || !desc->fn || !cookie || !owner)
        return -EINVAL;
    if (reserved_reboot_magic(desc->magic1, desc->magic2))
        return -EEXIST;
    if (READ_ONCE(reboot_registry.stopping))
        return -ESHUTDOWN;

    handler = kzalloc(sizeof(*handler), GFP_KERNEL);
    if (!handler)
        return -ENOMEM;
    handler->desc = *desc;
    handler->owner = owner;
    atomic_set(&handler->pending, 0);
    init_waitqueue_head(&handler->wait);

    mutex_lock(&reboot_registry.lock);
    if (reboot_registry.stopping) {
        mutex_unlock(&reboot_registry.lock);
        kfree(handler);
        return -ESHUTDOWN;
    }
    old = rcu_dereference_protected(reboot_registry.vec, lockdep_is_held(&reboot_registry.lock));
    count = old ? old->count : 0;
    for (i = 0; i < count; i++) {
        if (old->handlers[i]->desc.magic1 == desc->magic1 && old->handlers[i]->desc.magic2 == desc->magic2) {
            mutex_unlock(&reboot_registry.lock);
            kfree(handler);
            return -EEXIST;
        }
    }
    new = reboot_vec_alloc(count + 1, GFP_KERNEL);
    if (!new) {
        mutex_unlock(&reboot_registry.lock);
        kfree(handler);
        return -ENOMEM;
    }
    for (i = 0; i < count; i++)
        new->handlers[i] = old->handlers[i];
    new->handlers[count] = handler;
    handler->registered = true;
    rcu_assign_pointer(reboot_registry.vec, new);
    mutex_unlock(&reboot_registry.lock);
    if (old)
        call_srcu(&reboot_registry.srcu, &old->rcu, free_reboot_vec_rcu);
    *cookie = handler;
    return 0;
}

/* Caller holds reboot_registry.lock. */
static struct ksu_reboot_vec *reboot_vec_remove(const struct ksu_reboot_vec *old,
                                                const struct ksu_reboot_handler *handler)
{
    struct ksu_reboot_vec *new;
    size_t i, j = 0;

    if (old->count <= 1)
        return NULL;
    new = reboot_vec_alloc(old->count - 1, GFP_KERNEL | __GFP_NOFAIL);
    for (i = 0; i < old->count; i++) {
        if (old->handlers[i] != handler)
            new->handlers[j++] = old->handlers[i];
    }
    return new;
}

int ksu_unregister_reboot_handler(void *cookie)
{
    struct ksu_reboot_handler *handler = cookie;
    struct ksu_reboot_vec *old;
    bool self;

    if (!handler)
        return -EINVAL;
    self = frame_active(&reboot_registry.tracker, handler);
    if (!self && frame_active(&reboot_registry.tracker, NULL))
        return -EDEADLK;

    mutex_lock(&reboot_registry.lock);
    if (!handler->registered) {
        mutex_unlock(&reboot_registry.lock);
        return -ENOENT;
    }
    old = rcu_dereference_protected(reboot_registry.vec, lockdep_is_held(&reboot_registry.lock));
    rcu_assign_pointer(reboot_registry.vec, reboot_vec_remove(old, handler));
    handler->registered = false;
    if (self) {
        spin_lock_irq(&handler->wait.lock);
        handler->deferred_free = true;
        spin_unlock_irq(&handler->wait.lock);
    }
    mutex_unlock(&reboot_registry.lock);

    /* No dispatcher can queue new work for it after this. */
    synchronize_srcu(&reboot_registry.srcu);
    kfree(old);
    if (self)
        return 0;
    reboot_wait_idle(handler);
    kfree(handler);
    return 0;
}

/* Runs from the reboot kprobe: atomic context, no sleeping, no mutex. */
int ksu_reboot_dispatch(__u32 magic1, __u32 magic2, unsigned long arg)
{
    struct ksu_reboot_vec *vec;
    int idx, ret = 0;
    size_t i;

    if (!rcu_access_pointer(reboot_registry.vec))
        return 0;
    if (!emitter_enter(&reboot_registry.active_dispatchers, &reboot_registry.stopping, &reboot_registry.wait))
        return -ESHUTDOWN;

    idx = srcu_read_lock(&reboot_registry.srcu);
    vec = srcu_dereference(reboot_registry.vec, &reboot_registry.srcu);
    for (i = 0; vec && i < vec->count; i++) {
        struct ksu_reboot_handler *handler = vec->handlers[i];
        struct ksu_reboot_task_work *work;

        if (handler->desc.magic1 != magic1 || handler->desc.magic2 != magic2)
            continue;
        work = kzalloc(sizeof(*work), GFP_ATOMIC);
        if (!work) {
            ret = -ENOMEM;
            continue;
        }
        work->magic1 = magic1;
        work->magic2 = magic2;
        work->arg = arg;
        work->handler = handler;
        init_task_work(&work->cb, ksu_reboot_task_work_func);
        atomic_inc(&handler->pending);
        if (task_work_add(current, &work->cb, TWA_RESUME)) {
            reboot_pending_put(handler);
            kfree(work);
            ret = -EIO;
        }
    }
    srcu_read_unlock(&reboot_registry.srcu, idx);

    emitter_exit(&reboot_registry.active_dispatchers, &reboot_registry.wait);
    return ret;
}

/* ---- lifecycle --------------------------------------------------------- */

int ksu_event_registry_init(void)
{
    int ret;

    mutex_init(&event_registry.lock);
    memset(event_registry.vecs, 0, sizeof(event_registry.vecs));
    event_registry.state = 0;
    atomic_set(&event_registry.active_emitters, 0);
    init_waitqueue_head(&event_registry.wait);
    event_registry.stopping = false;
    tracker_init(&event_registry.tracker);
    ret = init_srcu_struct(&event_registry.srcu);
    if (ret)
        return ret;

    mutex_init(&reboot_registry.lock);
    reboot_registry.vec = NULL;
    atomic_set(&reboot_registry.active_dispatchers, 0);
    init_waitqueue_head(&reboot_registry.wait);
    reboot_registry.stopping = false;
    tracker_init(&reboot_registry.tracker);
    ret = init_srcu_struct(&reboot_registry.srcu);
    if (ret) {
        cleanup_srcu_struct(&event_registry.srcu);
        return ret;
    }
    return 0;
}

void ksu_event_registry_exit(void)
{
    struct ksu_reboot_vec *rvec;
    struct ksu_event_vec *vecs[KSU_EVENT_MAX];
    size_t i, j;

    /* Stop every ingress, then wait for in-flight emitters and dispatchers. */
    registry_stop(&event_registry.lock, &event_registry.stopping);
    registry_stop(&reboot_registry.lock, &reboot_registry.stopping);
    wait_event(event_registry.wait, atomic_read(&event_registry.active_emitters) == 0);
    wait_event(reboot_registry.wait, atomic_read(&reboot_registry.active_dispatchers) == 0);

    /* Reboot handlers: drain queued task work before freeing anything. */
    mutex_lock(&reboot_registry.lock);
    rvec = rcu_dereference_protected(reboot_registry.vec, lockdep_is_held(&reboot_registry.lock));
    rcu_assign_pointer(reboot_registry.vec, NULL);
    mutex_unlock(&reboot_registry.lock);
    synchronize_srcu(&reboot_registry.srcu);
    if (rvec) {
        for (i = 0; i < rvec->count; i++) {
            struct ksu_reboot_handler *handler = rvec->handlers[i];

            handler->registered = false;
            reboot_wait_idle(handler);
            kfree(handler);
        }
        kfree(rvec);
    }
    srcu_barrier(&reboot_registry.srcu);
    cleanup_srcu_struct(&reboot_registry.srcu);

    /* Event handlers: unpublish everything, one grace period, then free. */
    mutex_lock(&event_registry.lock);
    for (i = 0; i < KSU_EVENT_MAX; i++) {
        vecs[i] = rcu_dereference_protected(event_registry.vecs[i], lockdep_is_held(&event_registry.lock));
        rcu_assign_pointer(event_registry.vecs[i], NULL);
    }
    mutex_unlock(&event_registry.lock);
    synchronize_srcu(&event_registry.srcu);
    srcu_barrier(&event_registry.srcu);
    for (i = 0; i < KSU_EVENT_MAX; i++) {
        if (!vecs[i])
            continue;
        for (j = 0; j < vecs[i]->count; j++) {
            vecs[i]->handlers[j]->registered = false;
            kfree(vecs[i]->handlers[j]);
        }
        kfree(vecs[i]);
    }
    cleanup_srcu_struct(&event_registry.srcu);
}

#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/lsm_hooks.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>

#include "infra/symbol_resolver.h"
#include "hook/lsm_hook.h"
#include "hook/patch_memory.h"
#include "klog.h" // IWYU pragma: keep

struct ksu_lsm_hook_entry {
    struct ksu_lsm_hook *hook;
};

static DEFINE_MUTEX(ksu_lsm_hook_lock);
static struct ksu_lsm_hook_entry ksu_lsm_hook_entries[16];
static int ksu_lsm_hook_count;

static bool ksu_lsm_hook_is_tracked(struct ksu_lsm_hook *hook)
{
    int i;

    for (i = 0; i < ksu_lsm_hook_count; i++) {
        if (ksu_lsm_hook_entries[i].hook == hook)
            return true;
    }

    return false;
}

static int ksu_lsm_hook_track(struct ksu_lsm_hook *hook)
{
    if (ksu_lsm_hook_is_tracked(hook))
        return 0;

    if (ksu_lsm_hook_count >= ARRAY_SIZE(ksu_lsm_hook_entries)) {
        pr_err("lsm_hook: tracking table full, cannot record %s\n", hook->head_name ?: "unknown");
        return -ENOSPC;
    }

    ksu_lsm_hook_entries[ksu_lsm_hook_count++].hook = hook;
    return 0;
}

static void ksu_lsm_hook_untrack(struct ksu_lsm_hook *hook)
{
    int i;

    for (i = 0; i < ksu_lsm_hook_count; i++) {
        if (ksu_lsm_hook_entries[i].hook != hook)
            continue;

        ksu_lsm_hook_entries[i] = ksu_lsm_hook_entries[--ksu_lsm_hook_count];
        return;
    }
}

static const char *ksu_lsm_hook_target_name(struct ksu_lsm_hook *hook, char *buf, size_t buf_size)
{
    int ret;

    if (hook->target_name)
        return hook->target_name;

    ret = scnprintf(buf, buf_size, "bpf_lsm_%s", hook->head_name);
    if (ret <= 0 || ret >= buf_size)
        return NULL;

    return buf;
}

static int ksu_lsm_hook_patch_slot(void **slot, void *value)
{
    void *patched = value;
    int ret;

    ret = ksu_patch_text(slot, &patched, sizeof(patched), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (!ret)
        smp_wmb();

    return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
typedef void (*ksu_static_call_update_t)(struct static_call_key *key, void *tramp, void *func);

static int ksu_lsm_hook_update_scall(struct lsm_static_call *scall, void *value)
{
    static ksu_static_call_update_t update_scall;

    if (!scall)
        return -EINVAL;

    if (!update_scall) {
        update_scall = (ksu_static_call_update_t)ksu_lookup_symbol("__static_call_update");
        if (!update_scall) {
            pr_err("lsm_hook: failed to resolve __static_call_update\n");
            return -ENOENT;
        }
    }

    update_scall(scall->key, scall->trampoline, value);
    smp_wmb();
    return 0;
}
#endif

int ksu_lsm_hook(struct ksu_lsm_hook *hook)
{
    struct security_hook_list *entry;
    void *target;
    const char *target_name;
    char bpf_name[KSYM_NAME_LEN];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    unsigned long scalls_addr;
    struct lsm_static_call *scalls;
    int i;
#else
    unsigned long heads_addr;
    struct hlist_head *head;
#endif

    if (!hook || !hook->replacement)
        return -EINVAL;

    mutex_lock(&ksu_lsm_hook_lock);

    if (hook->entry) {
        mutex_unlock(&ksu_lsm_hook_lock);
        return -EALREADY;
    }

    target_name = ksu_lsm_hook_target_name(hook, bpf_name, sizeof(bpf_name));
    if (!target_name) {
        pr_err("lsm_hook: failed to build target name for %s\n", hook->head_name ?: "unknown");
        mutex_unlock(&ksu_lsm_hook_lock);
        return -EINVAL;
    }

    target = hook->original;
    if (!target)
        target = ksu_lookup_symbol(target_name);
    if (!target) {
        pr_err("lsm_hook: failed to resolve target for %s\n", hook->head_name ?: "unknown");
        mutex_unlock(&ksu_lsm_hook_lock);
        return -ENOENT;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    scalls_addr = kallsyms_lookup_name("static_calls_table");
    if (!scalls_addr) {
        pr_err("lsm_hook: failed to resolve static_calls_table\n");
        mutex_unlock(&ksu_lsm_hook_lock);
        return -ENOENT;
    }

    scalls = (struct lsm_static_call *)(scalls_addr + hook->head_offset);
    for (i = 0; i < MAX_LSM_COUNT; i++) {
        struct lsm_static_call *scall = &scalls[i];
        void **slot;
        void *current_hook;

        entry = READ_ONCE(scall->hl);
        if (!entry)
            continue;

        slot = (void **)((char *)entry + hook->hook_offset);
        current_hook = READ_ONCE(*slot);

        if (current_hook == hook->replacement) {
            mutex_unlock(&ksu_lsm_hook_lock);
            return -EALREADY;
        }
        if (current_hook != target)
            continue;

        if (ksu_lsm_hook_patch_slot(slot, hook->replacement)) {
            pr_err("lsm_hook: failed to patch %s\n", hook->head_name ?: "unknown");
            mutex_unlock(&ksu_lsm_hook_lock);
            return -EFAULT;
        }

        if (ksu_lsm_hook_update_scall(scall, hook->replacement)) {
            if (ksu_lsm_hook_patch_slot(slot, target))
                pr_err("lsm_hook: failed to roll back %s after static call update failure\n",
                       hook->head_name ?: "unknown");
            mutex_unlock(&ksu_lsm_hook_lock);
            return -EFAULT;
        }

        hook->entry = entry;
        hook->scall = scall;
        hook->original = target;
        if (ksu_lsm_hook_track(hook)) {
            if (ksu_lsm_hook_update_scall(scall, target))
                pr_err("lsm_hook: failed to roll back static call for %s after track failure\n",
                       hook->head_name ?: "unknown");
            if (ksu_lsm_hook_patch_slot(slot, target))
                pr_err("lsm_hook: failed to roll back %s after track failure\n", hook->head_name ?: "unknown");
            hook->entry = NULL;
            hook->scall = NULL;
            mutex_unlock(&ksu_lsm_hook_lock);
            return -ENOSPC;
        }
        pr_info("lsm_hook: patched %s hook slot %px from %px to %px\n", hook->head_name ?: "unknown", slot, target,
                hook->replacement);
        mutex_unlock(&ksu_lsm_hook_lock);
        return 0;
    }
#else
    heads_addr = kallsyms_lookup_name("security_hook_heads");
    if (!heads_addr) {
        pr_err("lsm_hook: failed to resolve security_hook_heads\n");
        mutex_unlock(&ksu_lsm_hook_lock);
        return -ENOENT;
    }

    head = (struct hlist_head *)(heads_addr + hook->head_offset);
    hlist_for_each_entry (entry, head, list) {
        void **slot = (void **)((char *)entry + hook->hook_offset);
        void *current_hook = READ_ONCE(*slot);

        if (current_hook == hook->replacement) {
            mutex_unlock(&ksu_lsm_hook_lock);
            return -EALREADY;
        }
        if (current_hook != target)
            continue;

        if (ksu_lsm_hook_patch_slot(slot, hook->replacement)) {
            pr_err("lsm_hook: failed to patch %s\n", hook->head_name ?: "unknown");
            mutex_unlock(&ksu_lsm_hook_lock);
            return -EFAULT;
        }

        hook->entry = entry;
        hook->original = target;
        if (ksu_lsm_hook_track(hook)) {
            if (ksu_lsm_hook_patch_slot(slot, hook->original))
                pr_err("lsm_hook: failed to roll back %s after track failure\n", hook->head_name ?: "unknown");
            hook->entry = NULL;
            mutex_unlock(&ksu_lsm_hook_lock);
            return -ENOSPC;
        }
        pr_info("lsm_hook: patched %s hook slot %px from %px to %px\n", hook->head_name ?: "unknown", slot, target,
                hook->replacement);
        mutex_unlock(&ksu_lsm_hook_lock);
        return 0;
    }
#endif

    pr_err("lsm_hook: target %s not found in head %s\n", target_name, hook->head_name ?: "unknown");
    mutex_unlock(&ksu_lsm_hook_lock);
    return -ENOENT;
}

void ksu_lsm_unhook(struct ksu_lsm_hook *hook)
{
    void **slot;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    if (!hook || !hook->entry || !hook->original || !hook->scall)
        return;
#else
    if (!hook || !hook->entry || !hook->original)
        return;
#endif

    mutex_lock(&ksu_lsm_hook_lock);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    if (!hook->entry || !hook->original || !hook->scall) {
#else
    if (!hook->entry || !hook->original) {
#endif
        mutex_unlock(&ksu_lsm_hook_lock);
        return;
    }

    slot = (void **)((char *)hook->entry + hook->hook_offset);
    if (ksu_lsm_hook_patch_slot(slot, hook->original)) {
        pr_err("lsm_hook: failed to restore %s\n", hook->head_name ?: "unknown");
        mutex_unlock(&ksu_lsm_hook_lock);
        return;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    if (ksu_lsm_hook_update_scall(hook->scall, hook->original)) {
        if (ksu_lsm_hook_patch_slot(slot, hook->replacement))
            pr_err("lsm_hook: failed to reapply %s after static call restore failure\n", hook->head_name ?: "unknown");
        mutex_unlock(&ksu_lsm_hook_lock);
        return;
    }
#endif

    synchronize_rcu();
    pr_info("lsm_hook: restored %s hook slot %px to %px\n", hook->head_name ?: "unknown", slot, hook->original);
    ksu_lsm_hook_untrack(hook);
    hook->entry = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    hook->scall = NULL;
#endif
    mutex_unlock(&ksu_lsm_hook_lock);
}

int ksu_register_lsm_hook(struct ksu_lsm_hook *hook)
{
    return ksu_lsm_hook(hook);
}

void ksu_unregister_lsm_hook(struct ksu_lsm_hook *hook)
{
    ksu_lsm_unhook(hook);
}

void ksu_lsm_hook_init(void)
{
    pr_info("lsm_hook: init, tracked hooks=%d\n", READ_ONCE(ksu_lsm_hook_count));
}

void ksu_lsm_hook_exit(void)
{
    struct ksu_lsm_hook *hooks[ARRAY_SIZE(ksu_lsm_hook_entries)];
    int count;
    int i;

    mutex_lock(&ksu_lsm_hook_lock);
    count = ksu_lsm_hook_count;
    for (i = 0; i < count; i++)
        hooks[i] = ksu_lsm_hook_entries[i].hook;
    mutex_unlock(&ksu_lsm_hook_lock);

    for (i = count - 1; i >= 0; i--)
        ksu_lsm_unhook(hooks[i]);
}

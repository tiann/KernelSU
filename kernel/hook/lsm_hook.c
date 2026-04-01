#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/lsm_hooks.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/string.h>

#include "infra/symbol_resolver.h"
#include "hook/lsm_hook.h"
#include "hook/patch_memory.h"
#include "klog.h" // IWYU pragma: keep
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#include "linux/static_call.h"
#endif
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

static bool ksu_lsm_hook_is_bpf_target(struct ksu_lsm_hook *hook)
{
    return hook && !hook->target_name;
}

static bool ksu_lsm_hook_matches_entry(struct ksu_lsm_hook *hook, struct security_hook_list *entry, void *current_hook,
                                       void *target)
{
    char sym[KSYM_NAME_LEN];

    (void)hook;
    (void)entry;

    if (target)
        return current_hook == target;

    if (!hook || !hook->target_name || !current_hook)
        return false;

    if (!sprint_symbol_no_offset(sym, (unsigned long)current_hook))
        return false;

    return !strcmp(sym, hook->target_name);
}

static void ksu_lsm_hook_log_entry(struct ksu_lsm_hook *hook, struct security_hook_list *entry, void *current_hook)
{
    (void)entry;

    pr_info("lsm_hook: candidate head=%s hook=%px [%pSb]\n", hook->head_name ?: "unknown", current_hook, current_hook);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
typedef void (*ksu_static_call_update_t)(struct static_call_key *key, void *tramp, void *func);

static size_t ksu_lsm_hook_scall_count(void)
{
    return sizeof(struct lsm_static_calls_table) / sizeof(struct lsm_static_call);
}

static int ksu_lsm_hook_update_scall(struct lsm_static_call *scall, void *value)
{
    __static_call_update(scall->key, scall->trampoline, value);
    smp_wmb();
    return 0;
}
#endif

int ksu_lsm_hook(struct ksu_lsm_hook *hook)
{
    int ret = 0;
    struct security_hook_list *entry;
    void *target;
    const char *target_name;
    char bpf_name[KSYM_NAME_LEN];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    unsigned long scalls_addr;
    struct lsm_static_call *scalls;
    size_t scalls_count;
    struct security_hook_list *selected_entry = NULL;
    struct lsm_static_call *selected_scall = NULL;
    void **selected_slot = NULL;
    void *selected_hook = NULL;
    size_t i;
#else
    unsigned long heads_addr;
    struct hlist_head *head;
    struct security_hook_list *selected_entry = NULL;
    void **selected_slot = NULL;
    void *selected_hook = NULL;
    struct security_hook_list *last_entry = NULL;
    void **last_slot = NULL;
    void *last_hook = NULL;
#endif

    if (!hook || !hook->replacement)
        return -EINVAL;

    mutex_lock(&ksu_lsm_hook_lock);

    if (hook->entry) {
        ret = -EALREADY;
        goto out_unlock;
    }

    target_name = ksu_lsm_hook_target_name(hook, bpf_name, sizeof(bpf_name));
    if (!target_name) {
        pr_err("lsm_hook: failed to build target name for %s\n", hook->head_name ?: "unknown");
        ret = -EINVAL;
        goto out_unlock;
    }

    target = hook->original;
    if (!target)
        target = ksu_lookup_symbol(target_name);
    if (!target && !ksu_lsm_hook_is_bpf_target(hook)) {
        pr_err("lsm_hook: failed to resolve target for %s\n", hook->head_name ?: "unknown");
        ret = -ENOENT;
        goto out_unlock;
    }
    if (!target && ksu_lsm_hook_is_bpf_target(hook)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
        pr_info("lsm_hook: %s symbol missing for %s, registration will be rejected\n", target_name,
                hook->head_name ?: "unknown");
#elif IS_ENABLED(CONFIG_BPF_LSM)
        pr_info("lsm_hook: %s symbol missing for %s, registration will be rejected\n", target_name,
                hook->head_name ?: "unknown");
#else
        pr_info("lsm_hook: %s symbol missing for %s, best-effort fallback is enabled\n", target_name,
                hook->head_name ?: "unknown");
#endif
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    scalls_addr = kallsyms_lookup_name("static_calls_table");
    if (!scalls_addr) {
        pr_err("lsm_hook: failed to resolve static_calls_table\n");
        ret = -ENOENT;
        goto out_unlock;
    }

    scalls = (struct lsm_static_call *)scalls_addr;
    scalls_count = ksu_lsm_hook_scall_count();
    for (i = 0; i < scalls_count; i++) {
        struct lsm_static_call *scall = &scalls[i];
        void **slot;
        void *current_hook;

        entry = READ_ONCE(scall->hl);
        if (!entry)
            continue;

        slot = (void **)((char *)entry + hook->hook_offset);
        current_hook = READ_ONCE(*slot);

        if (current_hook == hook->replacement) {
            ret = -EALREADY;
            goto out_unlock;
        }
        if (!ksu_lsm_hook_is_bpf_target(hook)) {
            pr_debug("finding %zu: 0x%lx [%pSb]\n", i, (unsigned long)current_hook, current_hook);
        }
        if (!ksu_lsm_hook_matches_entry(hook, entry, current_hook, target))
            continue;

        if (ksu_lsm_hook_is_bpf_target(hook))
            ksu_lsm_hook_log_entry(hook, entry, current_hook);

        selected_entry = entry;
        selected_scall = scall;
        selected_slot = slot;
        selected_hook = current_hook;
        break;
    }

    if (!selected_entry) {
        pr_err("lsm_hook: target %s not found in head %s\n", target_name, hook->head_name ?: "unknown");
        ret = -ENOENT;
        goto out_unlock;
    }

    ret = ksu_lsm_hook_track(hook);
    if (ret) {
        pr_err("lsm_hook: too many hooks to track: %d\n", ret);
        goto out_unlock;
    }

    if (ksu_lsm_hook_patch_slot(selected_slot, hook->replacement)) {
        pr_err("lsm_hook: failed to patch %s\n", hook->head_name ?: "unknown");
        ret = -EFAULT;
        goto out_untrack;
    }

    if (ksu_lsm_hook_update_scall(selected_scall, hook->replacement)) {
        if (ksu_lsm_hook_patch_slot(selected_slot, selected_hook)) {
            pr_err("lsm_hook: failed to roll back %s after static call update failure\n", hook->head_name ?: "unknown");
        }
        ret = -EFAULT;
        goto out_untrack;
    }

    hook->entry = selected_entry;
    hook->scall = selected_scall;
    hook->original = selected_hook;
    pr_info("lsm_hook: patched %s hook slot %px from %px to %px\n", hook->head_name ?: "unknown", selected_slot,
            selected_hook, hook->replacement);
#else
    heads_addr = kallsyms_lookup_name("security_hook_heads");
    if (!heads_addr) {
        pr_err("lsm_hook: failed to resolve security_hook_heads\n");
        ret = -ENOENT;
        goto out_unlock;
    }

    head = (struct hlist_head *)(heads_addr + hook->head_offset);
    hlist_for_each_entry (entry, head, list) {
        void **slot = (void **)((char *)entry + hook->hook_offset);
        void *current_hook = READ_ONCE(*slot);

        if (current_hook == hook->replacement) {
            ret = -EALREADY;
            goto out_unlock;
        }
        if (ksu_lsm_hook_is_bpf_target(hook)) {
            ksu_lsm_hook_log_entry(hook, entry, current_hook);
            last_entry = entry;
            last_slot = slot;
            last_hook = current_hook;
        }
        if (!ksu_lsm_hook_matches_entry(hook, entry, current_hook, target))
            continue;

        selected_entry = entry;
        selected_slot = slot;
        selected_hook = current_hook;
        break;
    }

#if !IS_ENABLED(CONFIG_BPF_LSM)
    if (!selected_entry && !target && ksu_lsm_hook_is_bpf_target(hook) && last_entry) {
        selected_entry = last_entry;
        selected_slot = last_slot;
        selected_hook = last_hook;
        pr_info("lsm_hook: %s unresolved and CONFIG_BPF_LSM is off, fallback to last candidate %px [%pSb]\n",
                target_name, selected_hook, selected_hook);
    }
#endif

    if (!selected_entry) {
        pr_err("lsm_hook: target %s not found in head %s\n", target_name, hook->head_name ?: "unknown");
        ret = -ENOENT;
        goto out_unlock;
    }

    ret = ksu_lsm_hook_track(hook);
    if (ret) {
        pr_err("lsm_hook: too many hooks to track: %d\n", ret);
        goto out_unlock;
    }

    if (ksu_lsm_hook_patch_slot(selected_slot, hook->replacement)) {
        pr_err("lsm_hook: failed to patch %s\n", hook->head_name ?: "unknown");
        ret = -EFAULT;
        goto out_untrack;
    }

    hook->entry = selected_entry;
    hook->original = selected_hook;
    pr_info("lsm_hook: patched %s hook slot %px from %px to %px\n", hook->head_name ?: "unknown", selected_slot,
            selected_hook, hook->replacement);
#endif
    goto out_unlock;
out_untrack:
    ksu_lsm_hook_untrack(hook);

out_unlock:
    mutex_unlock(&ksu_lsm_hook_lock);
    return ret;
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

void __init ksu_lsm_hook_init(void)
{
    pr_info("lsm_hook: init, tracked hooks=%d\n", READ_ONCE(ksu_lsm_hook_count));
}

void __exit ksu_lsm_hook_exit(void)
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

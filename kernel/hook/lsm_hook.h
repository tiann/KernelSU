#ifndef __KSU_H_LSM_HOOK
#define __KSU_H_LSM_HOOK

#include <linux/lsm_hooks.h>
#include <linux/stddef.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
#define KSU_LSM_HOOK_HEADS_TYPE struct lsm_static_calls_table
#else
#define KSU_LSM_HOOK_HEADS_TYPE struct security_hook_heads
#endif

struct ksu_lsm_hook {
    const char *head_name;
    const char *target_name;
    size_t head_offset;
    size_t hook_offset;
    void *replacement;
    void *original;
    struct security_hook_list *entry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    struct lsm_static_call *scall;
#endif
};

// clang-format off
#define KSU_LSM_HOOK_INIT(head_member, hook_member, target_symbol, replacement_fn)                                     \
    {                                                                                                                  \
        .head_name = #head_member,                                                                                     \
        .target_name = target_symbol,                                                                                  \
        .head_offset = offsetof(KSU_LSM_HOOK_HEADS_TYPE, head_member),                                                 \
        .hook_offset = offsetof(struct security_hook_list, hook.hook_member),                                          \
        .replacement = (void *)(replacement_fn),                                                                       \
    }
// clang-format on

#define KSU_LSM_HOOK_BPF_INIT(head_member, hook_member, replacement_fn)                                                \
    KSU_LSM_HOOK_INIT(head_member, hook_member, NULL, replacement_fn)

// This API implements runtime patching of existing LSM hook slots. It is a
// workaround for out-of-tree modules, not the normal LSM registration path via
// security_add_hooks(). The security framework does not expose a dedicated
// runtime lock for security_hook_heads mutations; callers rely on KernelSU's
// internal serialization plus text patching / RCU synchronization in the
// implementation to keep replacement and restoration coherent.

// --- Direct LSM hook patching API (hook/unhook) ---
// Replace the hook function in security_hook_heads[@head_name] that currently
// points to @target_name with @replacement.
// Saves the original handler into hook->original and records the patched entry
// for restoration by ksu_lsm_unhook() or the global ksu_lsm_hook_exit().
int ksu_lsm_hook(struct ksu_lsm_hook *hook);

// Restore the LSM hook entry previously patched by ksu_lsm_hook() back to the
// original handler saved in hook->original, and remove it from global tracking.
void ksu_lsm_unhook(struct ksu_lsm_hook *hook);

// --- BPF LSM chaining API (register/unregister) ---
// Register a handler by replacing the BPF LSM implementation for this hook.
// If hook->target_name is NULL, the target symbol defaults to "bpf_lsm_<hook>".
// The replacement can call hook->original to run the original BPF LSM handler
// after KernelSU-specific logic.
int ksu_register_lsm_hook(struct ksu_lsm_hook *hook);

// Undo ksu_register_lsm_hook() and restore the original BPF LSM handler.
void ksu_unregister_lsm_hook(struct ksu_lsm_hook *hook);

// --- Global lifecycle for tracked hooks ---
// Initialize the internal tracking state used by hook/unhook and
// register/unregister. Safe to call more than once.
void ksu_lsm_hook_init(void);

// Restore all currently tracked LSM hooks in reverse order and clear the
// internal registry. Call this from module exit to avoid leaving patched LSM
// hook slots behind.
void ksu_lsm_hook_exit(void);

#endif

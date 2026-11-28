/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef KSU_CALLSITE_HOOK_H
#define KSU_CALLSITE_HOOK_H

#include <linux/types.h>

struct ksu_callsite_hook {
    void *site;
    void *trampoline;
    void *original;
    size_t call_size;
    u8 saved_call[8];
    u8 patched_call[8];
    bool installed;
};

/* Find the zero-based Nth direct call to target and require exactly N + 1 matches */
int ksu_callsite_find(void *start, size_t size, void *target, unsigned int index, void **result);

/*
 * Call once for a zero-initialized hook
 * The trampoline stays allocated after restore
 * Replacement code must stay live while a call may still use it
 * Replacement must match callee's signature, receive the original arguments, and return to the caller
 * Calls through original are indirect, so replacement needs __nocfi
 */
int ksu_callsite_hook_prepare(struct ksu_callsite_hook *hook, void *site, void *callee, void *replacement,
                              void **original);
int ksu_callsite_hook_enable(struct ksu_callsite_hook *hook);

/* The caller must serialize prepare, enable, restore, and detach for each hook */
int ksu_callsite_hook_restore(struct ksu_callsite_hook *hook);

/*
 * Use only for final shutdown, with no later enable calls
 * If restore fails, route the trampoline to original and leave it allocated
 * After success, wait for running calls before freeing replacement code
 * A task grace period is needed if calls can run outside RCU read sections
 */
int ksu_callsite_hook_detach(struct ksu_callsite_hook *hook);

#endif

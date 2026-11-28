/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/errno.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/vmalloc.h>

#include "callsite_hook.h"
#include "callsite_hook_arch.h"
#include "patch_memory.h"

int ksu_callsite_find(void *start, size_t size, void *target, unsigned int index, void **result)
{
    unsigned long base = (unsigned long)start;
    size_t offset = 0, instruction_size;
    unsigned int i;
    void *site = NULL;

    if (!start || !target || !result)
        return -EINVAL;

    for (i = 0; i <= index; i++) {
        if (offset >= size)
            return -ENOENT;
        site = scan_call_to((void *)(base + offset), size - offset, target);
        if (!site)
            return -ENOENT;
        instruction_size = ksu_callsite_arch_instruction_size(site);
        if (!instruction_size)
            return -EOPNOTSUPP;
        offset = (unsigned long)site - base + instruction_size;
        if (offset > size)
            return -EOPNOTSUPP;
    }

    if (scan_call_to((void *)(base + offset), size - offset, target))
        return -EOPNOTSUPP;
    *result = site;
    return 0;
}

int ksu_callsite_hook_prepare(struct ksu_callsite_hook *hook, void *site, void *callee, void *replacement,
                              void **original)
{
    unsigned long pc, start, end, trampoline_size;
    size_t length, trampoline_offset;
    u8 trampoline_code[KSU_CALLSITE_TRAMPOLINE_SIZE] = {};
    void *trampoline;
    int ret;

    if (!hook || !site || !callee || !replacement || !original)
        return -EINVAL;
    if (hook->trampoline || hook->installed)
        return -EBUSY;
    *original = NULL;

    length = ksu_callsite_arch_instruction_size(site);
    if (!length || length > sizeof(hook->saved_call))
        return -EOPNOTSUPP;
    pc = (unsigned long)site;
    ret = ksu_callsite_arch_trampoline_bounds(site, callee, length, &trampoline_offset, &start, &end);
    if (ret)
        return ret;
    if (start >= end)
        return -ERANGE;

    trampoline_size = PAGE_ALIGN(trampoline_offset + sizeof(trampoline_code));
    trampoline =
        __vmalloc_node_range(trampoline_size, PAGE_SIZE, start, end, GFP_KERNEL | __GFP_NOWARN,
                             ksu_callsite_arch_trampoline_pgprot(), 0, NUMA_NO_NODE, __builtin_return_address(0));
    if (!trampoline)
        return -ENOMEM;
    trampoline = kasan_reset_tag(trampoline);

    ret = ksu_callsite_arch_encode_trampoline(trampoline_code, (unsigned long)trampoline + trampoline_offset,
                                              (unsigned long)callee, (unsigned long)replacement);
    if (ret)
        goto free_trampoline;
    ret = ksu_patch_text((u8 *)trampoline + trampoline_offset, trampoline_code, sizeof(trampoline_code),
                         KSU_PATCH_TEXT_FLUSH_ICACHE);
    if (ret)
        goto free_trampoline;
    ret = ksu_callsite_arch_encode_call(hook->patched_call, pc, (unsigned long)trampoline + trampoline_offset, length);
    if (ret)
        goto free_trampoline;

    memcpy(hook->saved_call, site, length);
    hook->site = site;
    hook->trampoline = trampoline;
    hook->original = (u8 *)trampoline + trampoline_offset + KSU_CALLSITE_ORIGINAL_OFFSET;
    hook->call_size = length;
    *original = hook->original;
    return 0;

free_trampoline:
    vfree(trampoline);
    return ret;
}

int ksu_callsite_hook_enable(struct ksu_callsite_hook *hook)
{
    int ret;

    if (!hook || !hook->site || !hook->trampoline || !hook->call_size)
        return -EINVAL;
    if (hook->installed)
        return -EALREADY;
    if (memcmp(hook->site, hook->saved_call, hook->call_size))
        return -EBUSY;

    ret = ksu_patch_text(hook->site, hook->patched_call, hook->call_size, KSU_PATCH_TEXT_FLUSH_ICACHE);
    if (!ret)
        hook->installed = true;
    return ret;
}

int ksu_callsite_hook_restore(struct ksu_callsite_hook *hook)
{
    int ret;

    if (!hook)
        return -EINVAL;
    if (!hook->installed)
        return 0;
    if (memcmp(hook->site, hook->patched_call, hook->call_size))
        return -EBUSY;

    ret = ksu_patch_text(hook->site, hook->saved_call, hook->call_size, KSU_PATCH_TEXT_FLUSH_ICACHE);
    if (!ret)
        hook->installed = false;
    return ret;
}

int ksu_callsite_hook_detach(struct ksu_callsite_hook *hook)
{
    int ret = ksu_callsite_hook_restore(hook);

    if (!ret || !hook)
        return ret;

    return ksu_callsite_arch_set_replacement((u8 *)hook->original - KSU_CALLSITE_ORIGINAL_OFFSET,
                                             (unsigned long)hook->original);
}

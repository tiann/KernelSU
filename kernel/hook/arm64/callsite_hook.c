/* SPDX-License-Identifier: GPL-2.0-only */
#ifdef __aarch64__

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sizes.h>
#include <linux/string.h>

#include "../callsite_hook_arch.h"
#include "../patch_memory.h"

#define KSU_CALLSITE_REPLACEMENT_OFFSET 8

static void put_insn(void *dst, u32 insn)
{
    __le32 value = cpu_to_le32(insn);

    memcpy(dst, &value, sizeof(value));
}

size_t ksu_callsite_arch_instruction_size(void *site)
{
    return site ? sizeof(u32) : 0;
}

int ksu_callsite_arch_trampoline_bounds(void *site, void *callee, size_t call_size, size_t *trampoline_offset,
                                        unsigned long *start, unsigned long *end)
{
    unsigned long pc = (unsigned long)site;
    unsigned long range = SZ_128M;

    if (!site || !callee || !trampoline_offset || !start || !end)
        return -EINVAL;
    if (offset_in_page(pc) + call_size > PAGE_SIZE)
        return -EOPNOTSUPP;

    *trampoline_offset = 0;
    *start = max_t(unsigned long, pc - range, MODULES_VADDR);
    *end = min_t(unsigned long, pc + range, VMALLOC_END);
    *start = max_t(unsigned long, *start, (unsigned long)callee - range);
    *end = min_t(unsigned long, *end, (unsigned long)callee + range);
    return 0;
}

pgprot_t ksu_callsite_arch_trampoline_pgprot(void)
{
    return PAGE_KERNEL_ROX;
}

int ksu_callsite_arch_encode_call(u8 *code, unsigned long pc, unsigned long target, size_t size)
{
    long offset = target - pc;

    if (size != sizeof(u32) || (offset & 3) || offset < -(long)SZ_128M || offset >= SZ_128M)
        return -ERANGE;
    put_insn(code, 0x94000000 | ((offset >> 2) & 0x03ffffff));
    return 0;
}

int ksu_callsite_arch_encode_trampoline(u8 *code, unsigned long pc, unsigned long callee, unsigned long replacement)
{
    long offset = callee - (pc + KSU_CALLSITE_ORIGINAL_OFFSET + sizeof(u32));

    if ((offset & 3) || offset < -(long)SZ_128M || offset >= SZ_128M)
        return -ERANGE;

    /* LDR x16, literal; BR x16; replacement address */
    put_insn(code, 0x58000050);
    put_insn(code + 4, 0xd61f0200);
    memcpy(code + KSU_CALLSITE_REPLACEMENT_OFFSET, &replacement, sizeof(replacement));
    /* BTI c; B callee, even when the native entry has no BTI instruction */
    put_insn(code + KSU_CALLSITE_ORIGINAL_OFFSET, 0xd503245f);
    put_insn(code + KSU_CALLSITE_ORIGINAL_OFFSET + 4, 0x14000000 | ((offset >> 2) & 0x03ffffff));
    return 0;
}

int ksu_callsite_arch_set_replacement(void *trampoline_code, unsigned long replacement)
{
    return ksu_patch_text((u8 *)trampoline_code + KSU_CALLSITE_REPLACEMENT_OFFSET, &replacement, sizeof(replacement),
                          KSU_PATCH_TEXT_FLUSH_DCACHE);
}

#endif

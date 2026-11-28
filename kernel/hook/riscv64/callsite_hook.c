/* SPDX-License-Identifier: GPL-2.0-only */
#ifdef __riscv

#include <linux/errno.h>
#include <linux/mm.h>

#include "../callsite_hook_arch.h"

/* TODO: Add RISC-V callsite hooks */
size_t ksu_callsite_arch_instruction_size(void *site)
{
    return 0;
}

int ksu_callsite_arch_trampoline_bounds(void *site, void *callee, size_t call_size, size_t *trampoline_offset,
                                        unsigned long *start, unsigned long *end)
{
    return -EOPNOTSUPP;
}

pgprot_t ksu_callsite_arch_trampoline_pgprot(void)
{
    return PAGE_KERNEL;
}

int ksu_callsite_arch_encode_call(u8 *code, unsigned long pc, unsigned long target, size_t size)
{
    return -EOPNOTSUPP;
}

int ksu_callsite_arch_encode_trampoline(u8 *code, unsigned long pc, unsigned long callee, unsigned long replacement)
{
    return -EOPNOTSUPP;
}

int ksu_callsite_arch_set_replacement(void *trampoline_code, unsigned long replacement)
{
    return -EOPNOTSUPP;
}

#endif

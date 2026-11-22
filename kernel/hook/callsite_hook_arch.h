/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef KSU_CALLSITE_HOOK_ARCH_H
#define KSU_CALLSITE_HOOK_ARCH_H

#include <linux/types.h>
#include <asm/pgtable.h>

#define KSU_CALLSITE_ORIGINAL_OFFSET 32
#define KSU_CALLSITE_TRAMPOLINE_SIZE 48

size_t ksu_callsite_arch_instruction_size(void *site);
int ksu_callsite_arch_trampoline_bounds(void *site, void *callee, size_t call_size, size_t *trampoline_offset,
                                        unsigned long *start, unsigned long *end);
pgprot_t ksu_callsite_arch_trampoline_pgprot(void);
int ksu_callsite_arch_encode_call(u8 *code, unsigned long pc, unsigned long target, size_t size);
int ksu_callsite_arch_encode_trampoline(u8 *code, unsigned long pc, unsigned long callee, unsigned long replacement);
int ksu_callsite_arch_set_replacement(void *trampoline_code, unsigned long replacement);

#endif

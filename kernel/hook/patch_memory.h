/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifndef __KSU_PATCH_MEMORY_H
#define __KSU_PATCH_MEMORY_H

#include <linux/types.h>
#include "linux/version.h"

#ifdef __aarch64__
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
#include "asm/text-patching.h" // IWYU pragma: keep
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
#include "asm/patching.h" // IWYU pragma: keep
#else
#include "asm/insn.h" // IWYU pragma: keep
#endif
#elif defined(__x86_64__) || defined(__riscv)
#include <asm/ptrace.h>
#else
#error "Unsupported arch"
#endif

#define KSU_PATCH_TEXT_FLUSH_DCACHE 1
#define KSU_PATCH_TEXT_FLUSH_ICACHE 2

unsigned long phys_from_virt(unsigned long addr, int *err);
/* Low-level fixmap writer. Callers should normally use ksu_patch_memory(). */
int ksu_patch_text(void *dst, void *src, size_t len, int flags);
#ifdef CONFIG_ARM64
/* ARM64 uses private replacement pages for RO/NX kernel mappings, and the
 * direct writer for RX/RW mappings. May sleep; patches and restores serialize.
 * RO mappings larger than 2 MiB return -E2BIG; contiguous groups are replaced
 * as a whole. A replacement is freed only after its entire contents match the
 * original page and the original descriptors have been restored on all CPUs.
 */
int ksu_patch_memory(void *dst, void *src, size_t len, int flags);
#else
static inline int ksu_patch_memory(void *dst, void *src, size_t len, int flags)
{
    return ksu_patch_text(dst, src, len, flags);
}
#endif
void *scan_call_to(void *start, size_t size, void *target);

#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifndef __KSU_H_HOOK_
#define __KSU_H_HOOK_
#include "linux/types.h" // IWYU pragma: keep
#include "linux/version.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 14, 0)
#include "asm/patching.h" // IWYU pragma: keep
#else
#include "asm/insn.h" // IWYU pragma: keep
#endif

#define KSU_PATCH_TEXT_FLUSH_DCACHE 1
#define KSU_PATCH_TEXT_FLUSH_ICACHE 2

int ksu_patch_text(void *dst, void *src, size_t len, int flags);

#endif

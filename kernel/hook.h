/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */

#ifndef __KSU_H_HOOK_
#define __KSU_H_HOOK_
#include "linux/types.h" // IWYU pragma: keep

// https://github.com/bmax121/KernelPatch/blob/94e5be9cc3f8a6fbd9574155e3e9753200ab9bfb/kernel/include/hook.h#L54

#define HOOK_INTO_BRANCH_FUNC

typedef enum {
    HOOK_NO_ERR = 0,
    HOOK_BAD_ADDRESS = 4095,
    HOOK_DUPLICATED = 4094,
    HOOK_NO_MEM = 4093,
    HOOK_BAD_RELO = 4092,
    HOOK_TRANSIT_NO_MEM = 4091,
    HOOK_CHAIN_FULL = 4090,
} hook_err_t;

enum hook_type {
    NONE = 0,
    INLINE,
    INLINE_CHAIN,
    FUNCTION_POINTER_CHAIN,
};

typedef int8_t chain_item_state;

#define CHAIN_ITEM_STATE_EMPTY 0
#define CHAIN_ITEM_STATE_READY 1
#define CHAIN_ITEM_STATE_BUSY 2

#define local_offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)
#define local_container_of(ptr, type, member)                                  \
    ({ (type *)((char *)(ptr) - local_offsetof(type, member)); })

#define HOOK_MEM_REGION_NUM 4
#define TRAMPOLINE_MAX_NUM 6
#define RELOCATE_INST_NUM (4 * 8 + 8 - 4)

#define HOOK_CHAIN_NUM 0x10
#define TRANSIT_INST_NUM 0x60

#define FP_HOOK_CHAIN_NUM 0x20

#define ARM64_NOP 0xd503201f
#define ARM64_BTI_C 0xd503245f
#define ARM64_BTI_J 0xd503249f
#define ARM64_BTI_JC 0xd50324df
#define ARM64_PACIASP 0xd503233f
#define ARM64_PACIBSP 0xd503237f

typedef struct {
    // in
    uint64_t func_addr;
    uint64_t origin_addr;
    uint64_t replace_addr;
    uint64_t relo_addr;
    // out
    int32_t tramp_insts_num;
    int32_t relo_insts_num;
    uint32_t origin_insts[TRAMPOLINE_MAX_NUM] __attribute__((aligned(8)));
    uint32_t tramp_insts[TRAMPOLINE_MAX_NUM] __attribute__((aligned(8)));
    uint32_t relo_insts[RELOCATE_INST_NUM] __attribute__((aligned(8)));
} hook_t __attribute__((aligned(8)));

static inline int is_bad_address(void *addr)
{
    return ((uint64_t)addr & 0x8000000000000000) != 0x8000000000000000;
}

int32_t branch_from_to(uint32_t *tramp_buf, uint64_t src_addr,
                       uint64_t dst_addr);
int32_t branch_relative(uint32_t *buf, uint64_t src_addr, uint64_t dst_addr);
int32_t branch_absolute(uint32_t *buf, uint64_t addr);
int32_t ret_absolute(uint32_t *buf, uint64_t addr);

hook_err_t hook_prepare(hook_t *hook);
void hook_install(hook_t *hook);
void hook_uninstall(hook_t *hook);

/**
 * @brief Inline-hook function which address is @param func with function @param replace, 
 * after hook, original @param func is backuped in @param backup.
 * 
 * @note If multiple modules hook this function simultaneously, 
 * it will cause abnormality when unload the modules. Please use hook_wrap instead
 * 
 * @see hook_wrap
 * 
 * @param func 
 * @param replace 
 * @param backup 
 * @return hook_err_t 
 */
hook_err_t hook(void *func, void *replace, void **backup);

#endif

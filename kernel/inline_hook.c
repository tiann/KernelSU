/* SPDX-License-Identifier: GPL-2.0-or-later */
/* 
 * Copyright (C) 2023 bmax121. All Rights Reserved.
 */
#include "hook.h"
#include "klog.h" // IWYU pragma: keep
#include "linux/vmalloc.h"
#include "asm/patching.h"

// https://github.com/bmax121/KernelPatch/blob/94e5be9cc3f8a6fbd9574155e3e9753200ab9bfb/kernel/base/hook.c#L562-L603

#define bits32(n, high, low)                                                   \
    ((uint32_t)((n) << (31u - (high))) >> (31u - (high) + (low)))
#define bit(n, st) (((n) >> (st)) & 1)
#define sign64_extend(n, len)                                                  \
    (((uint64_t)((n) << (63u - (len - 1))) >> 63u) ?                           \
         ((n) | (0xFFFFFFFFFFFFFFFF << (len))) :                               \
         n)
#define align_ceil(x, align)                                                   \
    (((u64)(x) + (u64)(align) - 1) & ~((u64)(align) - 1))

typedef uint32_t inst_type_t;
typedef uint32_t inst_mask_t;

#define INST_B 0x14000000
#define INST_BC 0x54000000
#define INST_BL 0x94000000
#define INST_ADR 0x10000000
#define INST_ADRP 0x90000000
#define INST_LDR_32 0x18000000
#define INST_LDR_64 0x58000000
#define INST_LDRSW_LIT 0x98000000
#define INST_PRFM_LIT 0xD8000000
#define INST_LDR_SIMD_32 0x1C000000
#define INST_LDR_SIMD_64 0x5C000000
#define INST_LDR_SIMD_128 0x9C000000
#define INST_CBZ 0x34000000
#define INST_CBNZ 0x35000000
#define INST_TBZ 0x36000000
#define INST_TBNZ 0x37000000
#define INST_HINT 0xD503201F
#define INST_IGNORE 0x0

#define MASK_B 0xFC000000
#define MASK_BC 0xFF000010
#define MASK_BL 0xFC000000
#define MASK_ADR 0x9F000000
#define MASK_ADRP 0x9F000000
#define MASK_LDR_32 0xFF000000
#define MASK_LDR_64 0xFF000000
#define MASK_LDRSW_LIT 0xFF000000
#define MASK_PRFM_LIT 0xFF000000
#define MASK_LDR_SIMD_32 0xFF000000
#define MASK_LDR_SIMD_64 0xFF000000
#define MASK_LDR_SIMD_128 0xFF000000
#define MASK_CBZ 0x7F000000u
#define MASK_CBNZ 0x7F000000u
#define MASK_TBZ 0x7F000000u
#define MASK_TBNZ 0x7F000000u
#define MASK_HINT 0xFFFFF01F
#define MASK_IGNORE 0x0

static inst_mask_t masks[] = {
    MASK_B,        MASK_BC,          MASK_BL,          MASK_ADR,
    MASK_ADRP,     MASK_LDR_32,      MASK_LDR_64,      MASK_LDRSW_LIT,
    MASK_PRFM_LIT, MASK_LDR_SIMD_32, MASK_LDR_SIMD_64, MASK_LDR_SIMD_128,
    MASK_CBZ,      MASK_CBNZ,        MASK_TBZ,         MASK_TBNZ,
    MASK_IGNORE,
};
static inst_type_t types[] = {
    INST_B,        INST_BC,          INST_BL,          INST_ADR,
    INST_ADRP,     INST_LDR_32,      INST_LDR_64,      INST_LDRSW_LIT,
    INST_PRFM_LIT, INST_LDR_SIMD_32, INST_LDR_SIMD_64, INST_LDR_SIMD_128,
    INST_CBZ,      INST_CBNZ,        INST_TBZ,         INST_TBNZ,
    INST_IGNORE,
};

static int32_t relo_len[] = {
    6, 8, 8, 4, 4, 6, 6, 6, 8, 8, 8, 8, 6, 6, 6, 6, 2
};

// static uint64_t sign_extend(uint64_t x, uint32_t len)
// {
//     char sign_bit = bit(x, len - 1);
//     unsigned long sign_mask = 0 - sign_bit;
//     x |= ((sign_mask >> len) << len);
//     return x;
// }

static int is_in_tramp(hook_t *hook, uint64_t addr)
{
    uint64_t tramp_start = hook->origin_addr;
    uint64_t tramp_end = tramp_start + hook->tramp_insts_num * 4;
    if (addr >= tramp_start && addr < tramp_end) {
        return 1;
    }
    return 0;
}

static uint64_t relo_in_tramp(hook_t *hook, uint64_t addr)
{
    uint64_t tramp_start = hook->origin_addr;
    uint64_t tramp_end = tramp_start + hook->tramp_insts_num * 4;
    if (!(addr >= tramp_start && addr < tramp_end))
        return addr;
    uint32_t addr_inst_index = (addr - tramp_start) / 4;
    uint64_t fix_addr = hook->relo_addr;
    for (int i = 0; i < addr_inst_index; i++) {
        inst_type_t inst = hook->origin_insts[i];
        for (int j = 0; j < sizeof(relo_len) / sizeof(relo_len[0]); j++) {
            if ((inst & masks[j]) == types[j]) {
                fix_addr += relo_len[j] * 4;
                break;
            }
        }
    }
    return fix_addr;
}

#ifdef HOOK_INTO_BRANCH_FUNC

#endif

static hook_err_t relo_b(hook_t *hook, uint64_t inst_addr, uint32_t inst,
                         inst_type_t type)
{
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;
    uint64_t imm64;
    if (type == INST_BC) {
        uint64_t imm19 = bits32(inst, 23, 5);
        imm64 = sign64_extend(imm19 << 2u, 21u);
    } else {
        uint64_t imm26 = bits32(inst, 25, 0);
        imm64 = sign64_extend(imm26 << 2u, 28u);
    }
    uint64_t addr = inst_addr + imm64;
    addr = relo_in_tramp(hook, addr);

    uint32_t idx = 0;
    if (type == INST_BC) {
        buf[idx++] = (inst & 0xFF00001F) | 0x40u; // B.<cond> #8
        buf[idx++] = 0x14000006; // B #24
    }
    buf[idx++] = 0x58000051; // LDR X17, #8
    buf[idx++] = 0x14000003; // B #12
    buf[idx++] = addr & 0xFFFFFFFF;
    buf[idx++] = addr >> 32u;
    if (type == INST_BL) {
        buf[idx++] = 0x1000001E; // ADR X30, .
        buf[idx++] = 0x910033DE; // ADD X30, X30, #12
        buf[idx++] = 0xD65F0220; // RET X17
    } else {
        buf[idx++] = 0xD65F0220; // RET X17
    }
    buf[idx++] = ARM64_NOP;
    return HOOK_NO_ERR;
}

static hook_err_t relo_adr(hook_t *hook, uint64_t inst_addr, uint32_t inst,
                           inst_type_t type)
{
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;

    uint32_t xd = bits32(inst, 4, 0);
    uint64_t immlo = bits32(inst, 30, 29);
    uint64_t immhi = bits32(inst, 23, 5);
    uint64_t addr;

    if (type == INST_ADR) {
        addr = inst_addr + sign64_extend((immhi << 2u) | immlo, 21u);
    } else {
        addr =
            (inst_addr + sign64_extend((immhi << 14u) | (immlo << 12u), 33u)) &
            0xFFFFFFFFFFFFF000;
        if (is_in_tramp(hook, addr))
            return -HOOK_BAD_RELO;
    }
    buf[0] = 0x58000040u | xd; // LDR Xd, #8
    buf[1] = 0x14000003; // B #12
    buf[2] = addr & 0xFFFFFFFF;
    buf[3] = addr >> 32u;
    return HOOK_NO_ERR;
}

static hook_err_t relo_ldr(hook_t *hook, uint64_t inst_addr, uint32_t inst,
                           inst_type_t type)
{
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;

    uint32_t rt = bits32(inst, 4, 0);
    uint64_t imm19 = bits32(inst, 23, 5);
    uint64_t offset = sign64_extend((imm19 << 2u), 21u);
    uint64_t addr = inst_addr + offset;

    if (is_in_tramp(hook, addr) && type != INST_PRFM_LIT)
        return -HOOK_BAD_RELO;

    addr = relo_in_tramp(hook, addr);

    if (type == INST_LDR_32 || type == INST_LDR_64 || type == INST_LDRSW_LIT) {
        buf[0] = 0x58000060u | rt; // LDR Xt, #12
        if (type == INST_LDR_32) {
            buf[1] = 0xB9400000 | rt | (rt << 5u); // LDR Wt, [Xt]
        } else if (type == INST_LDR_64) {
            buf[1] = 0xF9400000 | rt | (rt << 5u); // LDR Xt, [Xt]
        } else {
            // LDRSW_LIT
            buf[1] = 0xB9800000 | rt | (rt << 5u); // LDRSW Xt, [Xt]
        }
        buf[2] = 0x14000004; // B #16
        buf[3] = ARM64_NOP;
        buf[4] = addr & 0xFFFFFFFF;
        buf[5] = addr >> 32u;
    } else {
        buf[0] = 0xA93F47F0; // STP X16, X17, [SP, -0x10]
        buf[1] = 0x58000091; // LDR X17, #16
        if (type == INST_PRFM_LIT) {
            buf[2] = 0xF9800220 | rt; // PRFM Rt, [X17]
        } else if (type == INST_LDR_SIMD_32) {
            buf[2] = 0xBD400220 | rt; // LDR St, [X17]
        } else if (type == INST_LDR_SIMD_64) {
            buf[2] = 0xFD400220 | rt; // LDR Dt, [X17]
        } else {
            // LDR_SIMD_128
            buf[2] = 0x3DC00220u | rt; // LDR Qt, [X17]
        }
        buf[3] = 0xF85F83F1; // LDR X17, [SP, -0x8]
        buf[4] = 0x14000004; // B #16
        buf[5] = ARM64_NOP;
        buf[6] = addr & 0xFFFFFFFF;
        buf[7] = addr >> 32u;
    }
    return HOOK_NO_ERR;
}

static hook_err_t relo_cb(hook_t *hook, uint64_t inst_addr, uint32_t inst,
                          inst_type_t type)
{
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;

    uint64_t imm19 = bits32(inst, 23, 5);
    uint64_t offset = sign64_extend((imm19 << 2u), 21u);
    uint64_t addr = inst_addr + offset;
    addr = relo_in_tramp(hook, addr);

    buf[0] = (inst & 0xFF00001F) | 0x40u; // CB(N)Z Rt, #8
    buf[1] = 0x14000005; // B #20
    buf[2] = 0x58000051; // LDR X17, #8
    buf[3] = 0xD65F0220; // RET X17
    buf[4] = addr & 0xFFFFFFFF;
    buf[5] = addr >> 32u;
    return HOOK_NO_ERR;
}

static hook_err_t relo_tb(hook_t *hook, uint64_t inst_addr, uint32_t inst,
                          inst_type_t type)
{
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;

    uint64_t imm14 = bits32(inst, 18, 5);
    uint64_t offset = sign64_extend((imm14 << 2u), 16u);
    uint64_t addr = inst_addr + offset;
    addr = relo_in_tramp(hook, addr);

    buf[0] = (inst & 0xFFF8001F) | 0x40u; // TB(N)Z Rt, #<imm>, #8
    buf[1] = 0x14000005; // B #20
    buf[2] = 0x58000051; // LDR X17, #8
    buf[3] = 0xd61f0220; // RET X17
    buf[4] = addr & 0xFFFFFFFF;
    buf[5] = addr >> 32u;
    return HOOK_NO_ERR;
}

static hook_err_t relo_ignore(hook_t *hook, uint64_t inst_addr, uint32_t inst,
                              inst_type_t type)
{
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;
    buf[0] = inst;
    buf[1] = ARM64_NOP;
    return HOOK_NO_ERR;
}

static uint32_t can_b_rel(uint64_t src_addr, uint64_t dst_addr)
{
#define B_REL_RANGE ((1 << 25) << 2)
    return ((dst_addr >= src_addr) & (dst_addr - src_addr <= B_REL_RANGE)) ||
           ((src_addr >= dst_addr) & (src_addr - dst_addr <= B_REL_RANGE));
}

int32_t branch_relative(uint32_t *buf, uint64_t src_addr, uint64_t dst_addr)
{
    if (can_b_rel(src_addr, dst_addr)) {
        buf[0] = 0x14000000u |
                 (((dst_addr - src_addr) & 0x0FFFFFFFu) >> 2u); // B <label>
        buf[1] = ARM64_NOP;
        return 2;
    }
    return 0;
}

int32_t branch_absolute(uint32_t *buf, uint64_t addr)
{
    buf[0] = 0x58000051; // LDR X17, #8
    buf[1] = 0xd61f0220; // BR X17
    buf[2] = addr & 0xFFFFFFFF;
    buf[3] = addr >> 32u;
    return 4;
}

int32_t ret_absolute(uint32_t *buf, uint64_t addr)
{
    buf[0] = 0x58000051; // LDR X17, #8
    buf[1] = 0xD65F0220; // RET X17
    buf[2] = addr & 0xFFFFFFFF;
    buf[3] = addr >> 32u;
    return 4;
}

int32_t branch_from_to(uint32_t *tramp_buf, uint64_t src_addr,
                       uint64_t dst_addr)
{
#if 0
    uint32_t len = branch_relative(tramp_buf, src_addr, dst_addr);
    if (len) return len;
#else
#if 0
    return branch_absolute(tramp_buf, dst_addr);
#else
    return ret_absolute(tramp_buf, dst_addr);
#endif
#endif
}

static hook_err_t relocate_inst(hook_t *hook, uint64_t inst_addr, uint32_t inst)
{
    hook_err_t rc = HOOK_NO_ERR;
    inst_type_t it = INST_IGNORE;
    int len = 1;

    for (int j = 0; j < sizeof(relo_len) / sizeof(relo_len[0]); j++) {
        if ((inst & masks[j]) == types[j]) {
            it = types[j];
            len = relo_len[j];
            break;
        }
    }

    switch (it) {
    case INST_B:
    case INST_BC:
    case INST_BL:
        rc = relo_b(hook, inst_addr, inst, it);
        break;
    case INST_ADR:
    case INST_ADRP:
        rc = relo_adr(hook, inst_addr, inst, it);
        break;
    case INST_LDR_32:
    case INST_LDR_64:
    case INST_LDRSW_LIT:
    case INST_PRFM_LIT:
    case INST_LDR_SIMD_32:
    case INST_LDR_SIMD_64:
    case INST_LDR_SIMD_128:
        rc = relo_ldr(hook, inst_addr, inst, it);
        break;
    case INST_CBZ:
    case INST_CBNZ:
        rc = relo_cb(hook, inst_addr, inst, it);
        break;
    case INST_TBZ:
    case INST_TBNZ:
        rc = relo_tb(hook, inst_addr, inst, it);
        break;
    case INST_IGNORE:
    default:
        rc = relo_ignore(hook, inst_addr, inst, it);
        break;
    }

    hook->relo_insts_num += len;

    return rc;
}

hook_err_t hook_prepare(hook_t *hook)
{
    if (is_bad_address((void *)hook->func_addr))
        return -HOOK_BAD_ADDRESS;
    if (is_bad_address((void *)hook->origin_addr))
        return -HOOK_BAD_ADDRESS;
    if (is_bad_address((void *)hook->replace_addr))
        return -HOOK_BAD_ADDRESS;
    if (is_bad_address((void *)hook->relo_addr))
        return -HOOK_BAD_ADDRESS;

    // backup origin instruction
    for (int i = 0; i < TRAMPOLINE_MAX_NUM; i++) {
        hook->origin_insts[i] = *((uint32_t *)hook->origin_addr + i);
    }
    // trampline to replace_addr
    if (hook->origin_insts[0] == ARM64_PACIASP ||
        hook->origin_insts[0] == ARM64_PACIBSP) {
        hook->tramp_insts_num = branch_from_to(
            &hook->tramp_insts[1], hook->origin_addr, hook->replace_addr);
        hook->tramp_insts[0] = ARM64_BTI_JC;
        hook->tramp_insts_num++;
    } else {
        hook->tramp_insts_num = branch_from_to(
            hook->tramp_insts, hook->origin_addr, hook->replace_addr);
    }

    // relocate
    for (int i = 0; i < sizeof(hook->relo_insts) / sizeof(hook->relo_insts[0]);
         i++) {
        hook->relo_insts[i] = ARM64_NOP;
    }

    for (int i = 0; i < hook->tramp_insts_num; i++) {
        uint64_t inst_addr = hook->origin_addr + i * 4;
        uint32_t inst = hook->origin_insts[i];
        hook_err_t relo_res = relocate_inst(hook, inst_addr, inst);
        if (relo_res) {
            return -HOOK_BAD_RELO;
        }
    }

    // jump back
    uint64_t back_src_addr = hook->relo_addr + hook->relo_insts_num * 4;
    uint64_t back_dst_addr = hook->origin_addr + hook->tramp_insts_num * 4;
    uint32_t *buf = hook->relo_insts + hook->relo_insts_num;
    hook->relo_insts_num += branch_from_to(buf, back_src_addr, back_dst_addr);
    return HOOK_NO_ERR;
}

void hook_install(hook_t *hook)
{
    void *addrs[TRAMPOLINE_MAX_NUM];
    for (int32_t i = 0; i < hook->tramp_insts_num; ++i) {
        addrs[i] = (uint32_t *)hook->origin_addr + i;
    }
    aarch64_insn_patch_text(addrs, hook->tramp_insts, hook->tramp_insts_num);
}

hook_err_t hook(void *func, void *replace, void **backup)
{
    hook_err_t err = HOOK_NO_ERR;
    if (!func || !replace || !backup) {
        return -HOOK_BAD_ADDRESS;
    }
    uint64_t origin_addr = func; // branch_func_addr((uintptr_t)func);
    hook_t *hook = (hook_t *)__vmalloc_node_range(
        PAGE_SIZE, 1, VMALLOC_START, VMALLOC_END, GFP_KERNEL, PAGE_KERNEL_EXEC,
        0, NUMA_NO_NODE, __builtin_return_address(0));
    if (!hook) {
        pr_err("could not vmap for hook\n");
        return -HOOK_NO_MEM;
    }
    pr_info("hook addr 0x%lx\n", (unsigned long)hook);
    hook->func_addr = (uint64_t)func;
    hook->origin_addr = origin_addr;
    hook->replace_addr = (uint64_t)replace;
    hook->relo_addr = (uint64_t)hook->relo_insts;
    *backup = (void *)hook->relo_addr;
    pr_info(
        "Hook func: %llx, origin: %llx, replace: %llx, relocate: %llx, chain: %llx\n",
        hook->func_addr, hook->origin_addr, hook->replace_addr, hook->relo_addr,
        (unsigned long long)hook);
    err = hook_prepare(hook);
    if (err)
        goto out;
    hook_install(hook);
    pr_info("Hook func: %llx successed\n", hook->func_addr);
    return HOOK_NO_ERR;
out:
    vunmap(hook);
    pr_err("Hook func: %llx failed, err: %d\n", hook->func_addr, err);
    return err;
}

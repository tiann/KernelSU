/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __KSU_RISCV64_INSN_H
#define __KSU_RISCV64_INSN_H

#include <linux/types.h>

static inline unsigned int ksu_riscv_insn_length(u16 insn)
{
    if ((insn & 3) != 3)
        return 2;
    if ((insn & 0x1f) != 0x1f)
        return 4;
    /* Do not interpret the payload of an unsupported long instruction. */
    return 0;
}

static inline bool ksu_riscv_jal_target(u32 insn, unsigned long pc, unsigned long *target)
{
    u32 imm;
    s32 offset;

    /* JAL ra, offset. JAL x0 is a jump, not a call. */
    if ((insn & 0xfff) != 0xef)
        return false;
    imm = ((insn >> 31) << 20) | (((insn >> 12) & 0xff) << 12) | (((insn >> 20) & 1) << 11) |
          (((insn >> 21) & 0x3ff) << 1);
    offset = (s32)(imm << 11) >> 11;
    *target = pc + offset;
    return true;
}

static inline bool ksu_riscv_auipc_call_target(u32 upper, u32 next, unsigned int next_len, unsigned long pc,
                                               unsigned long *target)
{
    unsigned int reg = (upper >> 7) & 0x1f;
    s32 lower;

    if ((upper & 0x7f) != 0x17 || !reg)
        return false;
    if (next_len == 4) {
        /* AUIPC rd, imm20; JALR ra, rd, imm12. */
        if ((next & 0x707f) != 0x67 || ((next >> 7) & 0x1f) != 1 || ((next >> 15) & 0x1f) != reg)
            return false;
        lower = (s32)next >> 20;
    } else if (next_len == 2) {
        /* C.JALR implicitly writes ra. RV64 does not have C.JAL. */
        if ((next & 0xf07f) != 0x9002 || ((next >> 7) & 0x1f) != reg)
            return false;
        lower = 0;
    } else {
        return false;
    }
    *target = (pc + (s32)(upper & 0xfffff000) + lower) & ~1UL;
    return true;
}

#endif

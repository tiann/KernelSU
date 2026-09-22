/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Select only the architecture macros consumed by arch.h, after host libc
 * headers. The generated machine code still uses the selected compiler target.
 */
#undef __aarch64__
#undef __x86_64__
#undef __riscv
#undef __riscv_xlen
#if defined(KSU_TEST_ARM64)
#define __aarch64__ 1
#elif defined(KSU_TEST_X86_64)
#define __x86_64__ 1
#else
#define __riscv 1
#define __riscv_xlen 64
#endif

#include <asm/ptrace.h>
#include "arch.h"
#include "hook/riscv64/insn.h"
#include "hook/riscv64/syscall_regs.h"

static u32 jal(int offset)
{
    u32 n = offset;
    return 0xef | (((n >> 20) & 1) << 31) | (((n >> 1) & 0x3ff) << 21) |
           (((n >> 11) & 1) << 20) | (((n >> 12) & 0xff) << 12);
}

static void instruction_tests(void)
{
    unsigned long target = 0;
    const unsigned long pc = 0xffffffff80200000UL;
    const int offsets[] = { 0, 2, 2046, 1048574, -2, -2048, -1048576 };
    unsigned int i;

    assert(ksu_riscv_insn_length(0x0001) == 2);
    assert(ksu_riscv_insn_length(0x006f) == 4);
    assert(ksu_riscv_insn_length(0xffff) == 0);
    for (i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        assert(ksu_riscv_jal_target(jal(offsets[i]), pc, &target));
        assert(target == pc + offsets[i]);
    }
    assert(!ksu_riscv_jal_target(jal(16) & ~(31U << 7), pc, &target));
    assert(ksu_riscv_auipc_call_target(0x00001097, 0xffe080e7, 4, pc, &target));
    assert(target == pc + 4094);
    assert(ksu_riscv_auipc_call_target(0xfffff097, 0x7ff080e7, 4, pc, &target));
    assert(target == pc - 2050);
    assert(ksu_riscv_auipc_call_target(0x00001097, 0x9082, 2, pc, &target));
    assert(target == pc + 4096);
    assert(!ksu_riscv_auipc_call_target(0x00001097, 0x8082, 2, pc, &target));
    assert(!ksu_riscv_auipc_call_target(0x00001097, 0x9102, 2, pc, &target));
    assert(!ksu_riscv_auipc_call_target(0x00001097, 0x00008067, 4, pc, &target));
    assert(!ksu_riscv_auipc_call_target(0x00001097, 0x000100e7, 4, pc, &target));
}

static void register_tests(void)
{
    struct pt_regs frame = { .a0 = (unsigned long)-ENOSYS };
    struct pt_regs caller = { 0 };
    struct pt_regs before;
    const long invalid[] = { -ENOSYS, -1, 512, 511, 0x100000000L };
    unsigned int i;

    PT_REGS_PARM1(&caller) = (unsigned long)&frame;
    assert(PT_REAL_REGS(&caller) == &frame);
    PT_REGS_SYSCALL_PARM1(&frame) = 101;
    PT_REGS_PARM2(&frame) = 102;
    PT_REGS_PARM3(&frame) = 103;
    PT_REGS_SYSCALL_PARM4(&frame) = 104;
    PT_REGS_PARM5(&frame) = 105;
    PT_REGS_PARM6(&frame) = 106;
#if defined(__riscv)
    assert(frame.orig_a0 == 101 && (long)frame.a0 == -ENOSYS);
#elif defined(__aarch64__)
    assert(frame.regs[0] == 101 && frame.regs[3] == 104);
#else
    assert(frame.di == 101 && frame.r10 == 104 && frame.cx == 0);
#endif

    frame.a0 = -ENOSYS;
    frame.a7 = 221;
    frame.orig_a0 = 0xfeedf00ddeadbeefUL;
    before = frame;
    ksu_riscv_redirect_syscall(&frame, 221, 511);
    assert(frame.a7 == 511 && frame.a0 == 221);
    assert(frame.orig_a0 == before.orig_a0);
    assert(ksu_riscv_restore_syscall(&frame, 511, 512) == 221);
    assert(memcmp(&frame, &before, sizeof(frame)) == 0);
    assert(ksu_riscv_restore_syscall(&frame, 511, 512) == -ENOSYS);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        frame.a7 = 511;
        frame.a0 = invalid[i];
        before = frame;
        assert(ksu_riscv_restore_syscall(&frame, 511, 512) == -ENOSYS);
        assert(memcmp(&frame, &before, sizeof(frame)) == 0);
    }
}

int main(void)
{
    _Static_assert(sizeof(unsigned long) == 8, "64-bit test target required");
    instruction_tests();
    register_tests();
    puts("KernelSU register and RISC-V call decoder tests passed");
    return 0;
}

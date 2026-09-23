/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __KSU_RISCV64_SYSCALL_REGS_H
#define __KSU_RISCV64_SYSCALL_REGS_H

#include <linux/errno.h>
#include <asm/ptrace.h>

static inline void ksu_riscv_redirect_syscall(struct pt_regs *regs, long nr, int dispatcher)
{
    regs->a0 = nr;
    regs->a7 = dispatcher;
}

static inline int ksu_riscv_restore_syscall(struct pt_regs *regs, int dispatcher, unsigned int count)
{
    long nr = (long)regs->a0;

    if (dispatcher < 0 || (unsigned int)dispatcher >= count || regs->a7 != (unsigned long)dispatcher || nr < 0 ||
        nr >= count || nr == dispatcher)
        return -ENOSYS;
    regs->a7 = nr;
    regs->a0 = -ENOSYS;
    return (int)nr;
}

#endif

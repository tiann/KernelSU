#ifndef __KSU_H_SYSCALL_EVENT_BRIDGE
#define __KSU_H_SYSCALL_EVENT_BRIDGE

#include <asm/ptrace.h>

long ksu_hook_newfstatat(int orig_nr, const struct pt_regs *regs);
long ksu_hook_faccessat(int orig_nr, const struct pt_regs *regs);
long ksu_hook_execve(int orig_nr, const struct pt_regs *regs);
long ksu_hook_setresuid(int orig_nr, const struct pt_regs *regs);

void ksu_stop_ksud_execve_hook(void);

#endif // __KSU_H_SYSCALL_EVENT_BRIDGE

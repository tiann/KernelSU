#ifndef __KSU_H_ADB_ROOT
#define __KSU_H_ADB_ROOT
#include <asm/ptrace.h>

long ksu_adb_root_handle_execve(struct pt_regs *regs);

void ksu_adb_root_init(void);

void ksu_adb_root_exit(void);

#endif

#ifndef __KSU_H_HOOK_MANAGER
#define __KSU_H_HOOK_MANAGER

#include <asm/ptrace.h>

// Hook manager initialization and cleanup
void ksu_syscall_hook_manager_init(void);
void ksu_syscall_hook_manager_exit(void);

#endif

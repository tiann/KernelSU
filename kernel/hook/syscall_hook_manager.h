#ifndef __KSU_H_HOOK_MANAGER
#define __KSU_H_HOOK_MANAGER

#include <asm/ptrace.h>

void ksu_syscall_hook_manager_init(void);
int ksu_syscall_hook_manager_exit(void);
int ksu_prepare_module_unload(void);
int ksu_commit_module_unload(void);
int ksu_abort_module_unload(void);
bool ksu_module_unload_in_progress(void);
bool ksu_module_unload_recovery_allowed(void);
bool ksu_module_unload_try_enter(void);
bool ksu_module_control_try_enter(void);
bool ksu_syscall_tracepoint_allows_selective_marks(void);
void ksu_module_unload_leave(void);

#endif

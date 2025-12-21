#ifndef __KSU_H_KSU_SYSCALL_HOOK
#define __KSU_H_KSU_SYSCALL_HOOK
#include <asm/syscall.h>

extern syscall_fn_t *ksu_syscall_table;

void ksu_replace_syscall_table(int nr, syscall_fn_t fn, syscall_fn_t *old);

void ksu_syscall_hook_init();

#endif

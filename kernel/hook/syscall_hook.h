#ifndef __KSU_H_KSU_SYSCALL_HOOK
#define __KSU_H_KSU_SYSCALL_HOOK
#include <asm/syscall.h>

extern syscall_fn_t *ksu_syscall_table;

// Dispatcher slot number in syscall table
extern int ksu_dispatcher_nr;

// Syscall hook handler type.
// orig_nr: the original syscall number before redirection
// regs: the original pt_regs from userspace
// Handler is responsible for calling ksu_syscall_table[orig_nr](regs) if needed.
typedef long (*ksu_syscall_hook_fn)(int orig_nr, const struct pt_regs *regs);

// Register a hook for syscall @nr. Returns 0 on success.
int ksu_register_syscall_hook(int nr, ksu_syscall_hook_fn fn);

// Unregister the hook for syscall @nr.
void ksu_unregister_syscall_hook(int nr);

// Check if a hook is registered for syscall @nr.
bool ksu_has_syscall_hook(int nr);

void ksu_replace_syscall_table(int nr, syscall_fn_t fn, syscall_fn_t *old);

void ksu_syscall_hook_init(void);
void ksu_syscall_hook_exit(void);

#endif

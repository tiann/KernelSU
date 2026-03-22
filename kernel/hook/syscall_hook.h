#ifndef __KSU_H_KSU_SYSCALL_HOOK
#define __KSU_H_KSU_SYSCALL_HOOK
#include <asm/syscall.h>

extern syscall_fn_t *ksu_syscall_table;

void ksu_replace_syscall_table(int nr, syscall_fn_t fn, syscall_fn_t *old);

// Find available ni_syscall slots, returns count found.
// Results stored in out_slots[], up to max_slots entries.
int ksu_find_ni_syscall_slots(int *out_slots, int max_slots);

void ksu_syscall_hook_init(void);
void ksu_syscall_hook_exit(void);

#endif

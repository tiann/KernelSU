#include "syscall_hook.h"

#include <asm/cacheflush.h>
#include "patch_text.h"

syscall_fn_t *ksu_syscall_table = NULL;

void ksu_replace_syscall_table(int nr, syscall_fn_t fn, syscall_fn_t *old)
{
    if (nr < 0 || nr >= __NR_syscalls) {
        pr_info("invalid nr: %d\n", nr);
        return;
    }
    pr_info("syscall 0x%lx ", (uintptr_t)&ksu_syscall_table[nr]);
    syscall_fn_t *orig_p = &ksu_syscall_table[nr], orig = READ_ONCE(*orig_p);
    if (old) {
        *old = orig;
    }

    pr_info("Before hook syscall %d, ptr=0x%lx, *ptr=0x%lx -> 0x%lx", nr,
            (unsigned long)orig_p, (unsigned long)orig, (uintptr_t)fn);

    if (ksu_patch_text(&ksu_syscall_table[nr], &fn, sizeof(fn),
                       KSU_PATCH_TEXT_FLUSH_DCACHE)) {
        pr_err("patch syscall %d failed", nr);
    }

    pr_info("After hook syscall %d, ptr=0x%lx, *ptr=0x%lx", nr,
            (unsigned long)orig_p,
            (unsigned long)READ_ONCE(ksu_syscall_table[nr]));
}

void ksu_syscall_hook_init()
{
    ksu_syscall_table = kallsyms_lookup_name("sys_call_table");
}

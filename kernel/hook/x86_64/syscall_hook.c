#ifdef __x86_64__

#include "../syscall_hook.h"

#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <linux/nospec.h>
#include <asm/cacheflush.h>
#include "infra/symbol_resolver.h"
#include "../patch_memory.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep

sys_call_ptr_t *ksu_syscall_table = NULL;
int ksu_dispatcher_nr = -1;

#ifndef __NR_syscalls
#define __NR_syscalls (__NR_syscall_max + 1)
#endif

static ksu_syscall_hook_fn syscall_hooks[__NR_syscalls];

struct syscall_hook_entry {
    int nr;
    sys_call_ptr_t orig;
    sys_call_ptr_t hook;
    bool restore_on_abort;
};

static DEFINE_MUTEX(hooked_entries_lock);
static struct syscall_hook_entry hooked_entries[16];
static int hooked_count;
static bool direct_hooks_prepared;

static int patch_syscall_table(int nr, sys_call_ptr_t fn)
{
    if (ksu_syscall_table == NULL)
        return -ENOENT;
    if (nr < 0 || nr >= __NR_syscalls)
        return -EINVAL;

    pr_info("patch syscall %d, 0x%lx -> 0x%lx\n", nr, (unsigned long)READ_ONCE(ksu_syscall_table[nr]),
            (unsigned long)fn);

    if (ksu_patch_text(&ksu_syscall_table[nr], &fn, sizeof(fn), KSU_PATCH_TEXT_FLUSH_DCACHE)) {
        pr_err("patch syscall %d failed\n", nr);
        return -EIO;
    }

    return 0;
}

int ksu_syscall_table_hook(int nr, sys_call_ptr_t fn, sys_call_ptr_t *old)
{
    bool added = false;
    int entry_idx = -1;
    sys_call_ptr_t current_fn;
    int i;
    int ret;

    if (ksu_syscall_table == NULL)
        return -ENOENT;
    if (nr < 0 || nr >= __NR_syscalls) {
        pr_info("invalid nr: %d\n", nr);
        return -EINVAL;
    }

    mutex_lock(&hooked_entries_lock);
    if (direct_hooks_prepared) {
        ret = -ESHUTDOWN;
        goto out_unlock;
    }

    current_fn = READ_ONCE(ksu_syscall_table[nr]);
    if (old)
        *old = current_fn;

    for (i = 0; i < hooked_count; i++) {
        if (hooked_entries[i].nr == nr) {
            entry_idx = i;
            break;
        }
    }

    if (entry_idx < 0) {
        if (hooked_count >= ARRAY_SIZE(hooked_entries)) {
            pr_err("hooked_entries full, refusing to patch syscall %d\n", nr);
            ret = -ENOSPC;
            goto out_unlock;
        }
        entry_idx = hooked_count++;
        hooked_entries[entry_idx].nr = nr;
        hooked_entries[entry_idx].orig = current_fn;
        hooked_entries[entry_idx].restore_on_abort = false;
        added = true;
    } else if (current_fn != hooked_entries[entry_idx].hook && current_fn != hooked_entries[entry_idx].orig) {
        pr_err("syscall %d ownership lost: current=0x%lx hook=0x%lx orig=0x%lx\n", nr, (unsigned long)current_fn,
               (unsigned long)hooked_entries[entry_idx].hook, (unsigned long)hooked_entries[entry_idx].orig);
        ret = -EBUSY;
        goto out_unlock;
    }

    ret = patch_syscall_table(nr, fn);
    if (ret) {
        if (added)
            --hooked_count;
    } else {
        hooked_entries[entry_idx].hook = fn;
        hooked_entries[entry_idx].restore_on_abort = false;
        ksu_syscall_hook_hold_unload_guard();
    }

out_unlock:
    mutex_unlock(&hooked_entries_lock);
    return ret;
}

int ksu_syscall_table_unhook(int nr)
{
    int i;
    int ret = -ENOENT;

    if (ksu_syscall_table == NULL)
        return -ENOENT;
    if (nr < 0 || nr >= __NR_syscalls)
        return -EINVAL;

    mutex_lock(&hooked_entries_lock);
    if (direct_hooks_prepared) {
        ret = -ESHUTDOWN;
        goto out_unlock;
    }

    for (i = 0; i < hooked_count; i++) {
        sys_call_ptr_t current_fn;

        if (hooked_entries[i].nr != nr)
            continue;

        current_fn = READ_ONCE(ksu_syscall_table[nr]);
        if (current_fn == hooked_entries[i].hook) {
            ret = patch_syscall_table(nr, hooked_entries[i].orig);
            if (ret) {
                pr_err("failed to unhook syscall %d: %d\n", nr, ret);
                break;
            }
        } else if (current_fn == hooked_entries[i].orig) {
            ret = 0;
        } else {
            pr_err("refusing to unhook syscall %d after ownership loss: current=0x%lx\n", nr,
                   (unsigned long)current_fn);
            ret = -EBUSY;
            break;
        }

        hooked_entries[i] = hooked_entries[--hooked_count];
        pr_info("unhooked syscall %d\n", nr);
        break;
    }

out_unlock:
    mutex_unlock(&hooked_entries_lock);
    return ret;
}

static int ksu_find_ni_syscall_slots(int *out_slots, int max_slots)
{
    unsigned long ni_syscall;
    int i, count = 0;

    if (!ksu_syscall_table || max_slots <= 0)
        return 0;

    ni_syscall = (unsigned long)ksu_resolve_symbol_for_functable_hook("__x64_sys_ni_syscall");

    pr_info("sys_ni_syscall: 0x%lx\n", ni_syscall);

    if (!ni_syscall)
        return 0;

    for (i = 0; i < __NR_syscalls && count < max_slots; i++) {
        if ((unsigned long)ksu_syscall_table[i] == ni_syscall) {
            out_slots[count++] = i;
            pr_info("ni_syscall %d: %d\n", count, i);
        }
    }

    return count;
}

static long __nocfi ksu_syscall_dispatcher(const struct pt_regs *regs)
{
    if (regs->orig_ax != ksu_dispatcher_nr)
        return -ENOSYS;

    int orig_nr = (int)regs->ax;

    if (regs->orig_ax == orig_nr)
        return -ENOSYS;

    ((struct pt_regs *)regs)->orig_ax = orig_nr;

    if (likely(orig_nr >= 0 && orig_nr < __NR_syscalls)) {
        ksu_syscall_hook_fn fn = READ_ONCE(syscall_hooks[orig_nr]);
        if (likely(fn))
            return fn(orig_nr, regs);
    }

    return -ENOSYS;
}

int ksu_register_syscall_hook(int nr, ksu_syscall_hook_fn fn)
{
    if (nr < 0 || nr >= __NR_syscalls)
        return -EINVAL;
    if (READ_ONCE(syscall_hooks[nr])) {
        pr_warn("syscall hook for nr=%d already registered, skip\n", nr);
        return -EEXIST;
    }
    WRITE_ONCE(syscall_hooks[nr], fn);
    pr_info("registered syscall hook for nr=%d\n", nr);
    return 0;
}

void ksu_unregister_syscall_hook(int nr)
{
    if (nr < 0 || nr >= __NR_syscalls)
        return;
    WRITE_ONCE(syscall_hooks[nr], NULL);
    pr_info("unregistered syscall hook for nr=%d\n", nr);
}

bool ksu_has_syscall_hook(int nr)
{
    if (nr < 0 || nr >= __NR_syscalls)
        return false;
    return READ_ONCE(syscall_hooks[nr]) != NULL;
}

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
static void *x64_sys_call_patch_addr;
static char x64_sys_call_patch_orig_insn[14];
static char x64_sys_call_patch_insn[14];
static bool x64_restore_on_abort;

static long my_x64_sys_call(const struct pt_regs *regs, unsigned int nr)
{
    return ksu_syscall_table[nr](regs);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
static void *do_syscall_64_patch_addr;
static char do_syscall_64_orig_insn[14];
static char do_syscall_64_patch_insn[14];
static bool do_syscall_restore_on_abort;

static long (*syscall_enter_from_user_mode_fn)(struct pt_regs *regs, long syscall);
static void (*syscall_exit_to_user_mode_fn)(struct pt_regs *regs);

static __always_inline bool my_do_syscall_x64(struct pt_regs *regs, int nr)
{
    unsigned int unr = nr;

    if (likely(unr < NR_syscalls)) {
        unr = array_index_nospec(unr, NR_syscalls);
        regs->ax = ksu_syscall_table[unr](regs);
        return true;
    }
    return false;
}

static void __nocfi my_do_syscall_64(struct pt_regs *regs, int nr)
{
    nr = syscall_enter_from_user_mode_fn(regs, nr);
    nr = syscall_get_nr(current, regs);

    if (!my_do_syscall_x64(regs, nr) && nr != -1)
        regs->ax = -ENOSYS;

    syscall_exit_to_user_mode_fn(regs);
}
#endif
#endif

static void patch_abs_jump(const char *sym, void **patch_addr, void *target, char backup[14], char patch[14])
{
    static const char endbr64_insn[] = {
        // clang-format off
        0xf3, 0x0f, 0x1e, 0xfa
        // clang-format on
    };
    char buf[] = {
        // clang-format off
        0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // clang-format on
    };
    int ret;

    *patch_addr = (void *)find_kernel_symbol_exact(sym);
    pr_info("%s=0x%lx\n", sym, (unsigned long)*patch_addr);
    if (!*patch_addr)
        return;

    if (memcmp(*patch_addr, endbr64_insn, sizeof(endbr64_insn)) == 0)
        *patch_addr = (void *)((char *)(*patch_addr) + 4);

    *((void **)(buf + 6)) = target;
    memcpy(backup, *patch_addr, sizeof(buf));
    memcpy(patch, buf, sizeof(buf));
    ret = ksu_patch_text(*patch_addr, buf, sizeof(buf), KSU_PATCH_TEXT_FLUSH_ICACHE);
    if (ret) {
        pr_err("patch %s err: %d\n", sym, ret);
        *patch_addr = NULL;
        return;
    }

    ksu_syscall_hook_hold_unload_guard();
}

void __init __nocfi ksu_syscall_hook_init(void)
{
    int ni_slot;
    int ret;

    memset(syscall_hooks, 0, sizeof(syscall_hooks));
    direct_hooks_prepared = false;
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    x64_restore_on_abort = false;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    do_syscall_restore_on_abort = false;
#endif
#endif

    ksu_syscall_table = (sys_call_ptr_t *)ksu_resolve_symbol_for_functable_hook("sys_call_table");
    pr_info("sys_call_table=0x%lx\n", (unsigned long)ksu_syscall_table);

    if (!ksu_syscall_table)
        return;

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    patch_abs_jump("x64_sys_call", &x64_sys_call_patch_addr, my_x64_sys_call, x64_sys_call_patch_orig_insn,
                   x64_sys_call_patch_insn);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    syscall_enter_from_user_mode_fn = find_kernel_symbol_exact("syscall_enter_from_user_mode");
    syscall_exit_to_user_mode_fn = find_kernel_symbol_exact("syscall_exit_to_user_mode");
    pr_info("syscall_enter_from_user_mode: 0x%lx, syscall_exit_to_user_mode: 0x%lx\n",
            (unsigned long)syscall_enter_from_user_mode_fn, (unsigned long)syscall_exit_to_user_mode_fn);
    if (syscall_enter_from_user_mode_fn && syscall_exit_to_user_mode_fn) {
        patch_abs_jump("do_syscall_64", &do_syscall_64_patch_addr, my_do_syscall_64, do_syscall_64_orig_insn,
                       do_syscall_64_patch_insn);
    }
#endif
#endif

    if (ksu_find_ni_syscall_slots(&ni_slot, 1) < 1) {
        pr_err("failed to find ni_syscall slot for dispatcher\n");
        return;
    }

    ksu_dispatcher_nr = ni_slot;
    ret = ksu_syscall_table_hook(ksu_dispatcher_nr, (sys_call_ptr_t)ksu_syscall_dispatcher, NULL);
    if (ret) {
        pr_err("failed to install dispatcher at slot %d: %d\n", ksu_dispatcher_nr, ret);
        ksu_dispatcher_nr = -1;
        return;
    }
    pr_info("dispatcher installed at slot %d\n", ksu_dispatcher_nr);
}

static int validate_table_ownership_locked(void)
{
    int i;

    for (i = 0; i < hooked_count; i++) {
        sys_call_ptr_t current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);

        if (current_fn == hooked_entries[i].hook || current_fn == hooked_entries[i].orig)
            continue;

        pr_err("syscall %d ownership lost: current=0x%lx hook=0x%lx orig=0x%lx\n", hooked_entries[i].nr,
               (unsigned long)current_fn, (unsigned long)hooked_entries[i].hook, (unsigned long)hooked_entries[i].orig);
        return -EBUSY;
    }

    return 0;
}

static int rollback_prepared_table_locked(void)
{
    int rollback_ret = 0;
    int i;

    for (i = hooked_count - 1; i >= 0; i--) {
        sys_call_ptr_t current_fn;
        int ret;

        if (!hooked_entries[i].restore_on_abort)
            continue;

        current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);
        if (current_fn == hooked_entries[i].hook) {
            hooked_entries[i].restore_on_abort = false;
            continue;
        }
        if (current_fn != hooked_entries[i].orig) {
            pr_err("cannot rollback syscall %d after ownership loss\n", hooked_entries[i].nr);
            rollback_ret = -EUCLEAN;
            continue;
        }

        ret = patch_syscall_table(hooked_entries[i].nr, hooked_entries[i].hook);
        if (ret) {
            rollback_ret = ret;
            continue;
        }
        hooked_entries[i].restore_on_abort = false;
    }

    return rollback_ret;
}

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
static int inspect_text_patch(void *addr, const char *patch, const char *orig, size_t len, const char *name,
                              bool *owned)
{
    if (!addr) {
        *owned = false;
        return 0;
    }
    if (!memcmp(addr, patch, len)) {
        *owned = true;
        return 0;
    }
    if (!memcmp(addr, orig, len)) {
        *owned = false;
        return 0;
    }

    pr_err("%s ownership lost; refusing to overwrite foreign text\n", name);
    return -EBUSY;
}
#endif

int ksu_syscall_hook_exit(void)
{
    int rollback_ret;
    int ret;
    int i;
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    bool owned;
#endif

    mutex_lock(&hooked_entries_lock);
    if (direct_hooks_prepared) {
        mutex_unlock(&hooked_entries_lock);
        return 0;
    }

    if (ksu_syscall_table) {
        ret = validate_table_ownership_locked();
        if (ret)
            goto out_fail;
    }

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    ret = inspect_text_patch(x64_sys_call_patch_addr, x64_sys_call_patch_insn, x64_sys_call_patch_orig_insn,
                             sizeof(x64_sys_call_patch_insn), "x64_sys_call", &owned);
    if (ret)
        goto out_fail;
    x64_restore_on_abort = owned;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    ret = inspect_text_patch(do_syscall_64_patch_addr, do_syscall_64_patch_insn, do_syscall_64_orig_insn,
                             sizeof(do_syscall_64_patch_insn), "do_syscall_64", &owned);
    if (ret)
        goto out_fail;
    do_syscall_restore_on_abort = owned;
#endif
#endif

    if (ksu_syscall_table) {
        for (i = 0; i < hooked_count; i++) {
            sys_call_ptr_t current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);

            hooked_entries[i].restore_on_abort = false;
            if (current_fn == hooked_entries[i].orig)
                continue;

            ret = patch_syscall_table(hooked_entries[i].nr, hooked_entries[i].orig);
            if (ret) {
                rollback_ret = rollback_prepared_table_locked();
                if (rollback_ret)
                    ret = -EUCLEAN;
                goto out_fail;
            }
            hooked_entries[i].restore_on_abort = true;
        }
    }

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    if (x64_restore_on_abort) {
        ret = ksu_patch_text(x64_sys_call_patch_addr, x64_sys_call_patch_orig_insn,
                             sizeof(x64_sys_call_patch_orig_insn), KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret) {
            rollback_ret = rollback_prepared_table_locked();
            if (rollback_ret)
                ret = -EUCLEAN;
            goto out_fail;
        }
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    if (do_syscall_restore_on_abort) {
        ret = ksu_patch_text(do_syscall_64_patch_addr, do_syscall_64_orig_insn, sizeof(do_syscall_64_orig_insn),
                             KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret) {
            int rollback = 0;

            if (x64_restore_on_abort &&
                !memcmp(x64_sys_call_patch_addr, x64_sys_call_patch_orig_insn, sizeof(x64_sys_call_patch_orig_insn)))
                rollback = ksu_patch_text(x64_sys_call_patch_addr, x64_sys_call_patch_insn,
                                          sizeof(x64_sys_call_patch_insn), KSU_PATCH_TEXT_FLUSH_ICACHE);
            rollback_ret = rollback_prepared_table_locked();
            if (rollback || rollback_ret)
                ret = -EUCLEAN;
            goto out_fail;
        }
    }
#endif
#endif

    direct_hooks_prepared = true;
    mutex_unlock(&hooked_entries_lock);
    pr_info("all direct syscall hooks prepared for unload\n");
    return 0;

out_fail:
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    x64_restore_on_abort = false;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    do_syscall_restore_on_abort = false;
#endif
#endif
    mutex_unlock(&hooked_entries_lock);
    return ret;
}

int ksu_syscall_hook_abort_exit(void)
{
    bool table_applied[ARRAY_SIZE(hooked_entries)] = { false };
    int rollback_ret = 0;
    int ret = 0;
    int i;
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    bool x64_applied = false;
#endif

    mutex_lock(&hooked_entries_lock);
    if (!direct_hooks_prepared) {
        mutex_unlock(&hooked_entries_lock);
        return 0;
    }

    if (ksu_syscall_table) {
        for (i = 0; i < hooked_count; i++) {
            sys_call_ptr_t current_fn;

            if (!hooked_entries[i].restore_on_abort)
                continue;
            current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);
            if (current_fn != hooked_entries[i].orig && current_fn != hooked_entries[i].hook) {
                pr_err("cannot re-arm syscall %d after ownership loss\n", hooked_entries[i].nr);
                ret = -EBUSY;
                goto out_fail;
            }
        }
    }

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    if (x64_restore_on_abort &&
        memcmp(x64_sys_call_patch_addr, x64_sys_call_patch_orig_insn, sizeof(x64_sys_call_patch_orig_insn)) &&
        memcmp(x64_sys_call_patch_addr, x64_sys_call_patch_insn, sizeof(x64_sys_call_patch_insn))) {
        pr_err("x64_sys_call ownership lost during unload abort\n");
        ret = -EBUSY;
        goto out_fail;
    }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    if (do_syscall_restore_on_abort &&
        memcmp(do_syscall_64_patch_addr, do_syscall_64_orig_insn, sizeof(do_syscall_64_orig_insn)) &&
        memcmp(do_syscall_64_patch_addr, do_syscall_64_patch_insn, sizeof(do_syscall_64_patch_insn))) {
        pr_err("do_syscall_64 ownership lost during unload abort\n");
        ret = -EBUSY;
        goto out_fail;
    }
#endif
#endif

    if (ksu_syscall_table) {
        for (i = 0; i < hooked_count; i++) {
            sys_call_ptr_t current_fn;

            if (!hooked_entries[i].restore_on_abort)
                continue;
            current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);
            if (current_fn == hooked_entries[i].hook)
                continue;
            ret = patch_syscall_table(hooked_entries[i].nr, hooked_entries[i].hook);
            if (ret)
                goto rollback;
            table_applied[i] = true;
        }
    }

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    if (x64_restore_on_abort &&
        !memcmp(x64_sys_call_patch_addr, x64_sys_call_patch_orig_insn, sizeof(x64_sys_call_patch_orig_insn))) {
        ret = ksu_patch_text(x64_sys_call_patch_addr, x64_sys_call_patch_insn, sizeof(x64_sys_call_patch_insn),
                             KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret)
            goto rollback;
        x64_applied = true;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    if (do_syscall_restore_on_abort &&
        !memcmp(do_syscall_64_patch_addr, do_syscall_64_orig_insn, sizeof(do_syscall_64_orig_insn))) {
        ret = ksu_patch_text(do_syscall_64_patch_addr, do_syscall_64_patch_insn, sizeof(do_syscall_64_patch_insn),
                             KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret)
            goto rollback;
    }
#endif
#endif

    for (i = 0; i < hooked_count; i++)
        hooked_entries[i].restore_on_abort = false;
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    x64_restore_on_abort = false;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    do_syscall_restore_on_abort = false;
#endif
#endif
    direct_hooks_prepared = false;
    mutex_unlock(&hooked_entries_lock);
    pr_info("all direct syscall hooks re-armed\n");
    return 0;

rollback:
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    if (do_syscall_restore_on_abort &&
        !memcmp(do_syscall_64_patch_addr, do_syscall_64_patch_insn, sizeof(do_syscall_64_patch_insn))) {
        int rollback = ksu_patch_text(do_syscall_64_patch_addr, do_syscall_64_orig_insn,
                                      sizeof(do_syscall_64_orig_insn), KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (rollback)
            rollback_ret = rollback;
    }
#endif
    if (x64_applied) {
        int rollback = ksu_patch_text(x64_sys_call_patch_addr, x64_sys_call_patch_orig_insn,
                                      sizeof(x64_sys_call_patch_orig_insn), KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (rollback)
            rollback_ret = rollback;
    }
#endif
    for (i = hooked_count - 1; i >= 0; i--) {
        if (table_applied[i]) {
            int rollback = patch_syscall_table(hooked_entries[i].nr, hooked_entries[i].orig);
            if (rollback)
                rollback_ret = rollback;
        }
    }
    if (rollback_ret)
        ret = -EUCLEAN;

out_fail:
    mutex_unlock(&hooked_entries_lock);
    return ret;
}

void ksu_syscall_hook_finish_exit(void)
{
    mutex_lock(&hooked_entries_lock);
    hooked_count = 0;
    direct_hooks_prepared = false;
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    x64_restore_on_abort = false;
    x64_sys_call_patch_addr = NULL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    do_syscall_restore_on_abort = false;
    do_syscall_64_patch_addr = NULL;
#endif
#endif
    mutex_unlock(&hooked_entries_lock);

    memset(syscall_hooks, 0, sizeof(syscall_hooks));
    ksu_dispatcher_nr = -1;
}

#endif /* __x86_64__ */

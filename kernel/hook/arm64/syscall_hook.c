#ifdef __aarch64__

#include "../syscall_hook.h"

#include <linux/kallsyms.h>
#include <linux/mutex.h>
#include <asm/cacheflush.h>
#include "infra/symbol_resolver.h"
#include "../patch_memory.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep

syscall_fn_t *ksu_syscall_table = NULL;
int ksu_dispatcher_nr = -1;

static ksu_syscall_hook_fn syscall_hooks[__NR_syscalls];

struct syscall_hook_entry {
    int nr;
    syscall_fn_t orig;
    syscall_fn_t hook;
    bool restore_on_abort;
};

static DEFINE_MUTEX(hooked_entries_lock);
static struct syscall_hook_entry hooked_entries[16];
static int hooked_count;
static bool direct_hooks_prepared;

static int patch_syscall_table(int nr, syscall_fn_t fn)
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

int ksu_syscall_table_hook(int nr, syscall_fn_t fn, syscall_fn_t *old)
{
    bool added = false;
    int entry_idx = -1;
    syscall_fn_t current_fn;
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
        syscall_fn_t current_fn;

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

static int __init ksu_find_ni_syscall_slots(int *out_slots, int max_slots)
{
    unsigned long ni_syscall;
    int i, count = 0;

    if (!ksu_syscall_table || max_slots <= 0)
        return 0;

    ni_syscall = (unsigned long)ksu_resolve_symbol_for_functable_hook("__arm64_sys_ni_syscall");

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
    if (regs->syscallno != ksu_dispatcher_nr)
        return -ENOSYS;

    int orig_nr = (int)PT_REGS_ORIG_SYSCALL(regs);

    if (regs->syscallno == orig_nr)
        return -ENOSYS;

    ((struct pt_regs *)regs)->syscallno = orig_nr;
    PT_REGS_ORIG_SYSCALL((struct pt_regs *)regs) = orig_nr;

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

void __init ksu_syscall_hook_init(void)
{
    int ni_slot;
    int ret;

    memset(syscall_hooks, 0, sizeof(syscall_hooks));
    direct_hooks_prepared = false;

    ksu_syscall_table = (syscall_fn_t *)ksu_resolve_symbol_for_functable_hook("sys_call_table");
    pr_info("sys_call_table=0x%lx", (unsigned long)ksu_syscall_table);

    if (!ksu_syscall_table)
        return;

    if (ksu_find_ni_syscall_slots(&ni_slot, 1) < 1) {
        pr_err("failed to find ni_syscall slot for dispatcher\n");
        return;
    }

    ksu_dispatcher_nr = ni_slot;
    ret = ksu_syscall_table_hook(ksu_dispatcher_nr, (syscall_fn_t)ksu_syscall_dispatcher, NULL);
    if (ret) {
        pr_err("failed to install dispatcher at slot %d: %d\n", ksu_dispatcher_nr, ret);
        ksu_dispatcher_nr = -1;
        return;
    }
    pr_info("dispatcher installed at slot %d\n", ksu_dispatcher_nr);
}

static int validate_hook_ownership_locked(void)
{
    int i;

    for (i = 0; i < hooked_count; i++) {
        syscall_fn_t current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);

        if (current_fn == hooked_entries[i].hook || current_fn == hooked_entries[i].orig)
            continue;

        pr_err("syscall %d ownership lost: current=0x%lx hook=0x%lx orig=0x%lx\n", hooked_entries[i].nr,
               (unsigned long)current_fn, (unsigned long)hooked_entries[i].hook, (unsigned long)hooked_entries[i].orig);
        return -EBUSY;
    }

    return 0;
}

static int rollback_prepared_hooks_locked(void)
{
    int rollback_ret = 0;
    int i;

    for (i = hooked_count - 1; i >= 0; i--) {
        syscall_fn_t current_fn;
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
            pr_err("rollback syscall %d to hook failed: %d\n", hooked_entries[i].nr, ret);
            continue;
        }
        hooked_entries[i].restore_on_abort = false;
    }

    return rollback_ret;
}

int ksu_syscall_hook_exit(void)
{
    int rollback_ret;
    int ret;
    int i;

    mutex_lock(&hooked_entries_lock);
    if (direct_hooks_prepared) {
        mutex_unlock(&hooked_entries_lock);
        return 0;
    }

    if (!ksu_syscall_table) {
        direct_hooks_prepared = true;
        mutex_unlock(&hooked_entries_lock);
        return 0;
    }

    ret = validate_hook_ownership_locked();
    if (ret) {
        mutex_unlock(&hooked_entries_lock);
        return ret;
    }

    for (i = 0; i < hooked_count; i++) {
        syscall_fn_t current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);

        hooked_entries[i].restore_on_abort = false;
        if (current_fn == hooked_entries[i].orig)
            continue;

        pr_info("restore syscall %d to 0x%lx\n", hooked_entries[i].nr, (unsigned long)hooked_entries[i].orig);
        ret = patch_syscall_table(hooked_entries[i].nr, hooked_entries[i].orig);
        if (ret) {
            pr_err("restore syscall %d failed: %d\n", hooked_entries[i].nr, ret);
            rollback_ret = rollback_prepared_hooks_locked();
            mutex_unlock(&hooked_entries_lock);
            return rollback_ret ? -EUCLEAN : ret;
        }
        hooked_entries[i].restore_on_abort = true;
    }

    direct_hooks_prepared = true;
    mutex_unlock(&hooked_entries_lock);
    pr_info("all direct syscall hooks prepared for unload\n");
    return 0;
}

int ksu_syscall_hook_abort_exit(void)
{
    bool applied[ARRAY_SIZE(hooked_entries)] = { false };
    int rollback_ret = 0;
    int ret;
    int i;

    mutex_lock(&hooked_entries_lock);
    if (!direct_hooks_prepared) {
        mutex_unlock(&hooked_entries_lock);
        return 0;
    }

    if (!ksu_syscall_table) {
        direct_hooks_prepared = false;
        mutex_unlock(&hooked_entries_lock);
        return 0;
    }

    for (i = 0; i < hooked_count; i++) {
        syscall_fn_t current_fn;

        if (!hooked_entries[i].restore_on_abort)
            continue;
        current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);
        if (current_fn != hooked_entries[i].orig && current_fn != hooked_entries[i].hook) {
            pr_err("cannot re-arm syscall %d after ownership loss\n", hooked_entries[i].nr);
            mutex_unlock(&hooked_entries_lock);
            return -EBUSY;
        }
    }

    for (i = 0; i < hooked_count; i++) {
        syscall_fn_t current_fn;

        if (!hooked_entries[i].restore_on_abort)
            continue;

        current_fn = READ_ONCE(ksu_syscall_table[hooked_entries[i].nr]);
        if (current_fn == hooked_entries[i].hook)
            continue;

        ret = patch_syscall_table(hooked_entries[i].nr, hooked_entries[i].hook);
        if (ret) {
            int j;

            pr_err("re-arm syscall %d failed: %d\n", hooked_entries[i].nr, ret);
            for (j = i - 1; j >= 0; j--) {
                if (applied[j]) {
                    int rollback = patch_syscall_table(hooked_entries[j].nr, hooked_entries[j].orig);
                    if (rollback)
                        rollback_ret = rollback;
                }
            }
            mutex_unlock(&hooked_entries_lock);
            return rollback_ret ? -EUCLEAN : ret;
        }
        applied[i] = true;
    }

    for (i = 0; i < hooked_count; i++)
        hooked_entries[i].restore_on_abort = false;
    direct_hooks_prepared = false;
    mutex_unlock(&hooked_entries_lock);
    pr_info("all direct syscall hooks re-armed\n");
    return 0;
}

void ksu_syscall_hook_finish_exit(void)
{
    mutex_lock(&hooked_entries_lock);
    hooked_count = 0;
    direct_hooks_prepared = false;
    mutex_unlock(&hooked_entries_lock);

    memset(syscall_hooks, 0, sizeof(syscall_hooks));
    ksu_dispatcher_nr = -1;
}

#endif /* __aarch64__ */

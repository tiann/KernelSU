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

// Hook registration table — read with READ_ONCE from tracepoint/dispatcher
// context, written with WRITE_ONCE from init/exit context.
static ksu_syscall_hook_fn syscall_hooks[__NR_syscalls];

// Track all hooked syscall entries for restoration.
// Protected by hooked_entries_lock.
struct syscall_hook_entry {
    int nr;
    sys_call_ptr_t orig;
};

static DEFINE_MUTEX(hooked_entries_lock);
static struct syscall_hook_entry hooked_entries[16];
static int hooked_count = 0;

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

// Direct syscall table patching: overwrite syscall_table[nr] with fn,
// save original to *old, and record for restoration at module exit.
void ksu_syscall_table_hook(int nr, sys_call_ptr_t fn, sys_call_ptr_t *old)
{
    if (ksu_syscall_table == NULL)
        return;
    if (nr < 0 || nr >= __NR_syscalls) {
        pr_info("invalid nr: %d\n", nr);
        return;
    }

    mutex_lock(&hooked_entries_lock);

    sys_call_ptr_t orig = READ_ONCE(ksu_syscall_table[nr]);
    if (old)
        *old = orig;

    // Record for later restoration
    int i;
    bool found = false;
    for (i = 0; i < hooked_count; i++) {
        if (hooked_entries[i].nr == nr) {
            found = true;
            break;
        }
    }
    if (!found) {
        if (hooked_count < ARRAY_SIZE(hooked_entries)) {
            hooked_entries[hooked_count].nr = nr;
            hooked_entries[hooked_count].orig = orig;
            hooked_count++;
        } else {
            pr_warn("hooked_entries full, cannot track syscall %d for restoration\n", nr);
        }
    }

    patch_syscall_table(nr, fn);

    mutex_unlock(&hooked_entries_lock);
}

// Restore syscall_table[nr] to its original value and remove from tracking list.
void ksu_syscall_table_unhook(int nr)
{
    int i;

    if (ksu_syscall_table == NULL)
        return;
    if (nr < 0 || nr >= __NR_syscalls)
        return;

    mutex_lock(&hooked_entries_lock);

    for (i = 0; i < hooked_count; i++) {
        if (hooked_entries[i].nr == nr) {
            patch_syscall_table(nr, hooked_entries[i].orig);
            // Remove entry by swapping with last
            hooked_entries[i] = hooked_entries[--hooked_count];
            mutex_unlock(&hooked_entries_lock);
            pr_info("unhooked syscall %d\n", nr);
            return;
        }
    }

    mutex_unlock(&hooked_entries_lock);
    pr_warn("syscall %d not found in hooked entries\n", nr);
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

// Unified dispatcher: reads original NR from orig_ax, dispatches to handler.
// Validates that orig_ax matches our dispatcher slot (i.e. we redirected it),
// otherwise it's a spurious call — return -ENOSYS.
static long __nocfi ksu_syscall_dispatcher(const struct pt_regs *regs)
{
    if (regs->orig_ax != ksu_dispatcher_nr)
        return -ENOSYS;

    // On x86_64, orig_ax was overwritten by our tracepoint to route here.
    // The original syscall number passed by userspace is still sitting untouched in ax.
    int orig_nr = (int)regs->ax;

    if (regs->orig_ax == orig_nr)
        return -ENOSYS;

    // Restore registers to original state before dispatching
    ((struct pt_regs *)regs)->orig_ax = orig_nr;

    if (likely(orig_nr >= 0 && orig_nr < __NR_syscalls)) {
        ksu_syscall_hook_fn fn = READ_ONCE(syscall_hooks[orig_nr]);
        if (likely(fn))
            return fn(orig_nr, regs);
    }

    return -ENOSYS;
}

// Register a handler into the dispatcher's routing table.
// Does not modify the syscall table — the dispatcher slot is shared by all hooks.
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

// Remove a handler from the dispatcher's routing table.
// The syscall table is not touched — only the dispatcher stops routing this nr.
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

// https://github.com/torvalds/linux/commit/1e3ad78334a69b36e107232e337f9d693dcc9df2
// harden syscall table was introduced in 6.9, but it was backported to almost
// all of GKI kernel except 5.10
#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
static void *x64_sys_call_patch_addr;
static char x64_sys_call_patch_orig_insn[14];

static long my_x64_sys_call(const struct pt_regs *regs, unsigned int nr)
{
    return ksu_syscall_table[nr](regs);
}

// avd 13-5.15 missing this commit:
// https://github.com/torvalds/linux/commit/fb13b11d53875e28e7fbf0c26b288e4ea676aa9f
// we need to patch the whole do_syscall_64
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
static void *do_syscall_64_patch_addr;
static char do_syscall_64_orig_insn[14];

static long (*syscall_enter_from_user_mode_fn)(struct pt_regs *regs, long syscall);
static void (*syscall_exit_to_user_mode_fn)(struct pt_regs *regs);

static __always_inline bool my_do_syscall_x64(struct pt_regs *regs, int nr)
{
    /*
	 * Convert negative numbers to very high and thus out of range
	 * numbers for comparisons.
	 */
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

    // AVD doesn't have x32 support after A13, we can ignore do_syscall_x32

    if (!my_do_syscall_x64(regs, nr) && nr != -1) {
        /* Invalid system call, but still a system call. */
        regs->ax = -ENOSYS;
    }

    syscall_exit_to_user_mode_fn(regs);
}
#endif
#endif

static void patch_abs_jump(const char *sym, void **patch_addr, void *target, char backup[14])
{
    *patch_addr = (void *)find_kernel_symbol_exact(sym);
    pr_info("%s=0x%lx\n", sym, (unsigned long)*patch_addr);
    if (*patch_addr) {
        pr_info("patching %s\n", sym);
        // skip endbr64
        static const char endbr64_insn[] = {
            // clang-format off
            0xf3, 0x0f, 0x1e, 0xfa
            // clang-format on
        };
        if (memcmp((void *)*patch_addr, endbr64_insn, sizeof(endbr64_insn)) == 0) {
            pr_info("%s: skip endbr64\n", sym);
            *patch_addr = (void *)((char *)(*patch_addr) + 4);
        }
        // clang-format off
        char buf[] = {
            // jmp *(%rip) = addr
            0xff, 0x25, 0x00, 0x00, 0x00, 0x00,
            // addr: .quad 0
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        };
        // clang-format on
        *((void **)(buf + 6)) = target;
        memcpy(backup, *patch_addr, sizeof(buf));
        int ret = ksu_patch_text(*patch_addr, buf, sizeof(buf), KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret) {
            pr_err("patch %s err: %d\n", sym, ret);
            *patch_addr = NULL;
        }
    }
}

void __init __nocfi ksu_syscall_hook_init(void)
{
    int ni_slot;

    memset(syscall_hooks, 0, sizeof(syscall_hooks));

    ksu_syscall_table = (sys_call_ptr_t *)ksu_resolve_symbol_for_functable_hook("sys_call_table");
    pr_info("sys_call_table=0x%lx\n", (unsigned long)ksu_syscall_table);

    if (!ksu_syscall_table)
        return;

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    patch_abs_jump("x64_sys_call", &x64_sys_call_patch_addr, my_x64_sys_call, x64_sys_call_patch_orig_insn);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    syscall_enter_from_user_mode_fn = find_kernel_symbol_exact("syscall_enter_from_user_mode");
    syscall_exit_to_user_mode_fn = find_kernel_symbol_exact("syscall_exit_to_user_mode");
    pr_info("syscall_enter_from_user_mode: 0x%lx, syscall_exit_to_user_mode: 0x%lx\n",
            (unsigned long)syscall_enter_from_user_mode_fn, (unsigned long)syscall_exit_to_user_mode_fn);
    if (syscall_enter_from_user_mode_fn && syscall_exit_to_user_mode_fn) {
        patch_abs_jump("do_syscall_64", &do_syscall_64_patch_addr, my_do_syscall_64, do_syscall_64_orig_insn);
    }
#endif
#endif

    // Find one ni_syscall slot for the dispatcher
    if (ksu_find_ni_syscall_slots(&ni_slot, 1) < 1) {
        pr_err("failed to find ni_syscall slot for dispatcher\n");
        return;
    }

    ksu_dispatcher_nr = ni_slot;
    ksu_syscall_table_hook(ksu_dispatcher_nr, (sys_call_ptr_t)ksu_syscall_dispatcher, NULL);
    pr_info("dispatcher installed at slot %d\n", ksu_dispatcher_nr);
}

void __exit ksu_syscall_hook_exit(void)
{
    int i;

#ifdef CONFIG_KSU_X86_PATCH_SYSCALL_DISPATCHER
    int ret;
    if (x64_sys_call_patch_addr) {
        ret = ksu_patch_text((void *)x64_sys_call_patch_addr, x64_sys_call_patch_orig_insn,
                             sizeof(x64_sys_call_patch_orig_insn), KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret) {
            pr_err("restore x64_sys_call err: %d\n", ret);
        }
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 16, 0)
    if (do_syscall_64_patch_addr) {
        ret = ksu_patch_text((void *)do_syscall_64_patch_addr, do_syscall_64_orig_insn, sizeof(do_syscall_64_orig_insn),
                             KSU_PATCH_TEXT_FLUSH_ICACHE);
        if (ret) {
            pr_err("restore x64_sys_call err: %d\n", ret);
        }
    }
#endif
#endif

    if (!ksu_syscall_table)
        goto clear_state;

    // First, restore all patched syscall table entries while the dispatcher
    // and hook table are still intact, so in-flight syscalls see valid state.
    mutex_lock(&hooked_entries_lock);
    for (i = 0; i < hooked_count; i++) {
        int nr = hooked_entries[i].nr;
        sys_call_ptr_t orig = hooked_entries[i].orig;

        pr_info("restore syscall %d to 0x%lx\n", nr, (unsigned long)orig);
        if (ksu_patch_text(&ksu_syscall_table[nr], &orig, sizeof(orig), KSU_PATCH_TEXT_FLUSH_DCACHE)) {
            pr_err("restore syscall %d failed\n", nr);
        }
    }
    hooked_count = 0;
    mutex_unlock(&hooked_entries_lock);

clear_state:
    // Now that the syscall table is restored, clear internal state.
    // At this point the tracepoint is already unregistered and synchronized
    // (done by ksu_syscall_hook_manager_exit before calling us), so no new
    // dispatches will occur.
    memset(syscall_hooks, 0, sizeof(syscall_hooks));
    ksu_dispatcher_nr = -1;

    pr_info("all syscall hooks restored\n");
}

#endif /* __x86_64__ */

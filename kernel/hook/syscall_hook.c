#include "syscall_hook.h"

#include <linux/kallsyms.h>
#include <asm/cacheflush.h>
#include "patch_memory.h"
#include "../arch.h"
#include "../klog.h" // IWYU pragma: keep

syscall_fn_t *ksu_syscall_table = NULL;
int ksu_dispatcher_nr = -1;

// Hook registration table
static ksu_syscall_hook_fn syscall_hooks[__NR_syscalls];

// Track all hooked syscall entries for restoration
struct syscall_hook_entry {
	int nr;
	syscall_fn_t orig;
};

static struct syscall_hook_entry hooked_entries[16];
static int hooked_count = 0;

void ksu_replace_syscall_table(int nr, syscall_fn_t fn, syscall_fn_t *old)
{
	if (ksu_syscall_table == NULL)
		return;
	if (nr < 0 || nr >= __NR_syscalls) {
		pr_info("invalid nr: %d\n", nr);
		return;
	}
	pr_info("syscall 0x%lx ", (uintptr_t)&ksu_syscall_table[nr]);
	syscall_fn_t *orig_p = &ksu_syscall_table[nr], orig = READ_ONCE(*orig_p);
	if (old) {
		*old = orig;
	}

	// Record for later restoration
	int i;
	bool found = false;
	for (i = 0; i < hooked_count; i++) {
		if (hooked_entries[i].nr == nr) {
			found = true;
			break;
		}
	}
	if (!found && hooked_count < ARRAY_SIZE(hooked_entries)) {
		hooked_entries[hooked_count].nr = nr;
		hooked_entries[hooked_count].orig = orig;
		hooked_count++;
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

static int ksu_find_ni_syscall_slots(int *out_slots, int max_slots)
{
	unsigned long ni_syscall;
	int i, count = 0;

	if (!ksu_syscall_table || max_slots <= 0)
		return 0;

	ni_syscall = kallsyms_lookup_name("__arm64_sys_ni_syscall.cfi_jt");
	if (!ni_syscall)
		ni_syscall = kallsyms_lookup_name("__arm64_sys_ni_syscall");

	pr_info("__arm64_sys_ni_syscall: 0x%lx\n", ni_syscall);

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

// Unified dispatcher: reads original NR from x8/orig_ax, dispatches to handler
static long __nocfi ksu_syscall_dispatcher(const struct pt_regs *regs)
{
	int orig_nr = (int)PT_REGS_ORIG_SYSCALL(regs);

	if (likely(orig_nr >= 0 && orig_nr < __NR_syscalls)) {
		ksu_syscall_hook_fn fn = syscall_hooks[orig_nr];
		if (likely(fn))
			return fn(orig_nr, regs);
	}

	return -ENOSYS;
}

int ksu_register_syscall_hook(int nr, ksu_syscall_hook_fn fn)
{
	if (nr < 0 || nr >= __NR_syscalls)
		return -EINVAL;
	if (syscall_hooks[nr]) {
		pr_warn("syscall hook for nr=%d already registered, skip\n",
			nr);
		return -EEXIST;
	}
	syscall_hooks[nr] = fn;
	pr_info("registered syscall hook for nr=%d\n", nr);
	return 0;
}

void ksu_unregister_syscall_hook(int nr)
{
	if (nr < 0 || nr >= __NR_syscalls)
		return;
	syscall_hooks[nr] = NULL;
	pr_info("unregistered syscall hook for nr=%d\n", nr);
}

bool ksu_has_syscall_hook(int nr)
{
	if (nr < 0 || nr >= __NR_syscalls)
		return false;
	return syscall_hooks[nr] != NULL;
}

void ksu_syscall_hook_init(void)
{
	int ni_slot;

	memset(syscall_hooks, 0, sizeof(syscall_hooks));

	ksu_syscall_table = kallsyms_lookup_name("sys_call_table");
	pr_info("sys_call_table=0x%lx", (unsigned long)ksu_syscall_table);

	if (!ksu_syscall_table)
		return;

	// Find one ni_syscall slot for the dispatcher
	if (ksu_find_ni_syscall_slots(&ni_slot, 1) < 1) {
		pr_err("failed to find ni_syscall slot for dispatcher\n");
		return;
	}

	ksu_dispatcher_nr = ni_slot;
	ksu_replace_syscall_table(ksu_dispatcher_nr,
				  (syscall_fn_t)ksu_syscall_dispatcher, NULL);
	pr_info("dispatcher installed at slot %d\n", ksu_dispatcher_nr);
}

void ksu_syscall_hook_exit(void)
{
	int i;

	// Clear all registered hooks
	memset(syscall_hooks, 0, sizeof(syscall_hooks));
	ksu_dispatcher_nr = -1;

	if (!ksu_syscall_table)
		return;

	for (i = 0; i < hooked_count; i++) {
		int nr = hooked_entries[i].nr;
		syscall_fn_t orig = hooked_entries[i].orig;

		pr_info("restore syscall %d to 0x%lx\n", nr,
			(unsigned long)orig);
		if (ksu_patch_text(&ksu_syscall_table[nr], &orig, sizeof(orig),
				   KSU_PATCH_TEXT_FLUSH_DCACHE)) {
			pr_err("restore syscall %d failed\n", nr);
		}
	}

	hooked_count = 0;
	pr_info("all syscall hooks restored\n");
}

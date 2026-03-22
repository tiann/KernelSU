#include "syscall_hook.h"

#include <linux/kallsyms.h>
#include <asm/cacheflush.h>
#include "patch_memory.h"
#include "../klog.h" // IWYU pragma: keep

syscall_fn_t *ksu_syscall_table = NULL;

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

int ksu_find_ni_syscall_slots(int *out_slots, int max_slots)
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

void ksu_syscall_hook_init(void)
{
	ksu_syscall_table = kallsyms_lookup_name("sys_call_table");
	pr_info("sys_call_table=0x%lx", (unsigned long)ksu_syscall_table);
}

void ksu_syscall_hook_exit(void)
{
	int i;

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

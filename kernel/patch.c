#include "asm/processor.h"
#include "linux/jump_label.h"
#include "linux/kallsyms.h"
#include "asm/patching.h"

#include "klog.h" // IWYU pragma: keep
#include "hook.h"
#include "syscall_hook_manager.h"

extern void ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline();
extern void
ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline_to_original();

void *find_hook_point()
{
    unsigned long addr, size, p;
    struct jump_entry *entry;
    addr = kallsyms_lookup_name("syscall_trace_enter");
    if (!addr) {
        pr_err("syscall_trace_enter addr not found\n");
        return ERR_PTR(-ENOENT);
    }
    pr_info("syscall_trace_enter 0x%lx\n", addr);
    size = 0;
    if (!kallsyms_lookup_size_offset(addr, &size, NULL) || size == 0) {
        pr_err("syscall_trace_enter size not found or 0!\n");
        return ERR_PTR(-ENOENT);
    }
    pr_info("syscall_trace_enter size %ld\n", size);
    p = 0;
    for (entry = __start___jump_table; entry != __stop___jump_table; entry++) {
        p = (unsigned long)&entry->code + entry->code;
        if (p >= addr && p < addr + size) {
            break;
        }
    }
    if (p == 0) {
        pr_err("no jump table found!\n");
        return ERR_PTR(-ENOENT);
    }
    p = (unsigned long)&entry->target + entry->target;
    pr_info(
        "found jump table hook point: 0x%lx, jump table addr 0x%lx (idx=%ld)\n",
        p, (unsigned long)entry, entry - __start___jump_table);
    return p;
}

int hook_trace_sys_enter()
{
    void *hook_point = find_hook_point();
    if (IS_ERR(hook_point)) {
        pr_err("hook_trace_sys_enter: could not found hook point\n");
        return PTR_ERR(hook_point);
    }
    void *backup;
    if (hook(hook_point,
             ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline,
             &backup)) {
        pr_err("hook_trace_sys_enter: hook failed\n");
        return -EINVAL;
    }
    uint32_t buf[4];
    ret_absolute(buf, backup);
    void *addrs[4];
    for (int i = 0; i < 4; i++) {
        addrs[i] =
            (void *)
                ksu_syscall_trace_enter_call_trace_sys_enter_hook_trampoline_to_original +
            i * 4;
    }
    aarch64_insn_patch_text(addrs, buf, 4);
    return 0;
}

void ksu_trace_sys_enter()
{
    ksu_sys_enter_handler(task_pt_regs(current));
}

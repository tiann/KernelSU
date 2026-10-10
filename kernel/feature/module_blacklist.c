#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "hook/syscall_hook.h"
#include "feature/module_blacklist.h"
#include "infra/symbol_resolver.h"
#include "klog.h" // IWYU pragma: keep

static char *blacklist = NULL;
static char *orig_blacklist = NULL;
static char **module_blacklist = NULL;

// workaround https://cs.android.com/android/platform/superproject/+/android-latest-release:system/core/libmodprobe/utils.cpp;l=73
// userspace will throw error when ret != 0
// Android using __NR_finit_module, so we should hook this syscall
//
// https://github.com/tiann/KernelSU/blob/8948025de525cbf1ee03a895505a0a79ec283a7f/userspace/ksuinit/src/lib.rs#L365
// https://github.com/bytecodealliance/rustix/blob/287214b889865d8e1406a0ee71cc409b6f6191c8/src/system.rs#L265
// https://github.com/bytecodealliance/rustix/blob/1e2954a46ecffa6b30426ad7b1833b226530dcf4/src/backend/libc/system/syscalls.rs#L125
// ksuinit using __NR_init_module, so we also need hook this syscall

static long (*orig_sys_init_module)(const struct pt_regs *regs);
static long ksu_sys_init_module(const struct pt_regs *regs)
{
    int ret = orig_sys_init_module(regs);

    if (ret == -EPERM)
        ret = 0;

    return ret;
}

static long (*orig_sys_finit_module)(const struct pt_regs *regs);
static long ksu_sys_finit_module(const struct pt_regs *regs)
{
    int ret = orig_sys_finit_module(regs);

    if (ret == -EPERM)
        ret = 0;

    return ret;
}

void __init ksu_module_blacklist_init(const char *modules)
{
    const char *existing;
    const char *extra = "";
    char *name;
    size_t existing_len;

#ifndef MODULE
    extra = modules[0] ? ",kernelsu" : "kernelsu";
#endif
    if (!modules[0] && !extra[0])
        return;

    module_blacklist = (char **)find_kernel_symbol_exact("module_blacklist");
    if (!module_blacklist) {
        pr_warn("module_blacklist: kernel symbol not found, skipping blocked modules\n");
        return;
    }

    existing = *module_blacklist;
    existing_len = existing ? strlen(existing) : 0;
    blacklist = kasprintf(GFP_KERNEL, "%s%s%s%s", existing_len ? existing : "",
                          existing_len ? "," : "", modules, extra);
    if (!blacklist) {
        pr_err("module_blacklist: cannot allocate blacklist\n");
        return;
    }

    *module_blacklist = blacklist;
    pr_info("module_blacklist: blocked modules: %s\n", blacklist);

    ksu_syscall_table_hook(__NR_init_module, ksu_sys_init_module, &orig_sys_init_module);
    ksu_syscall_table_hook(__NR_finit_module, ksu_sys_finit_module, &orig_sys_finit_module);
    pr_info("module_blacklist: syscall hooked\n");
}

void __exit ksu_module_blacklist_exit()
{
    const char *existing;

    if (module_blacklist == NULL) {
        // that's mean no modules got block
        return;
    }

    ksu_syscall_table_unhook(__NR_init_module);
    ksu_syscall_table_unhook(__NR_finit_module);

    pr_info("module_blacklist: syscall unhooked!\n");

    if (blacklist == NULL) {
        pr_warn("module_blacklist: blacklist ptr not init, skip kfree and revert changes!\n");
        return;
    }

    existing = *module_blacklist;

    if (existing != blacklist) {
        pr_warn("module_blacklist: looks module_blacklist changed by other, skip kfree and revert changes!\n");
        return;
    }

    *module_blacklist = orig_blacklist;
    kfree(blacklist); // kasprintf alloc

    pr_info("module_blacklist: blacklist reverted and memory freed!\n");
}

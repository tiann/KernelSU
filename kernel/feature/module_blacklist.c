#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "feature/module_blacklist.h"
#include "infra/symbol_resolver.h"
#include "klog.h" // IWYU pragma: keep

// Block kernel modules from loading by reusing the kernel's native
// `module_blacklist` (consulted by blacklisted() before a module is laid out),
// instead of hooking init_module/finit_module. It is the standard mechanism and
// needs nothing maintained on our side.
//
// The list is supplied via the `block_modules=name1,name2,...` module param,
// which flows in through ksu_config (boot-patch / boot-patch-v2) or the kernel
// cmdline (built-in). Names use the kernel form (underscores, e.g.
// `oplus_secure_guard`).

static char block_modules[256];

// The param may be parsed before the slab allocator is up (built-in), so copy
// into a static buffer instead of allocating. Truncate rather than fail: a
// param error would abort the whole KernelSU load.
static int block_modules_param_set(const char *val, const struct kernel_param *kp)
{
    if (!val || !val[0])
        return 0;

    if (strscpy(block_modules, val, sizeof(block_modules)) < 0)
        pr_warn("module_blacklist: block_modules too long, truncated\n");

    return 0;
}

static const struct kernel_param_ops block_modules_param_ops = {
    // A bare `block_modules` would otherwise fail parse_args() and the load
    .flags = KERNEL_PARAM_OPS_FL_NOARG,
    .set = block_modules_param_set,
};
module_param_cb(block_modules, &block_modules_param_ops, NULL, 0);

// Address of the kernel's `char *module_blacklist`, the value it held before we
// overwrote it, and the string we installed.
static char **native_blacklist;
static char *orig_blacklist;
static char *our_blacklist;

void ksu_module_blacklist_init(void)
{
    char *merged;
    size_t orig_len;

    if (!block_modules[0])
        return;

    native_blacklist = (char **)find_kernel_symbol_exact("module_blacklist");
    if (!native_blacklist) {
        pr_warn("module_blacklist: symbol not found, cannot block modules\n");
        return;
    }

    orig_blacklist = *native_blacklist;
    orig_len = orig_blacklist ? strlen(orig_blacklist) : 0;

    // Merge with any existing blacklist (e.g. one set on the kernel cmdline).
    merged = kasprintf(GFP_KERNEL, "%s%s%s", orig_len ? orig_blacklist : "", orig_len ? "," : "", block_modules);
    if (!merged) {
        pr_err("module_blacklist: cannot allocate blacklist\n");
        native_blacklist = NULL;
        return;
    }

    our_blacklist = merged;
    WRITE_ONCE(*native_blacklist, merged);
    pr_info("module_blacklist: blocking modules: %s\n", merged);
}

void ksu_module_blacklist_exit(void)
{
    if (!native_blacklist || !our_blacklist)
        return;

    // Only revert if nothing else swapped it in the meantime.
    if (READ_ONCE(*native_blacklist) == our_blacklist)
        WRITE_ONCE(*native_blacklist, orig_blacklist);

    // Deliberately leak `our_blacklist`: the module loader may still be reading
    // it via blacklisted(), so freeing risks a use-after-free. It is a one-time,
    // bounded allocation.
    native_blacklist = NULL;
    our_blacklist = NULL;
    orig_blacklist = NULL;
    pr_info("module_blacklist: restored\n");
}

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "feature/module_blacklist.h"
#include "infra/symbol_resolver.h"
#include "klog.h" // IWYU pragma: keep

void __init ksu_module_blacklist_init(const char *modules)
{
    char **module_blacklist;
    const char *existing;
    char *blacklist, *name;
    size_t existing_len;

    if (!modules[0])
        return;

    module_blacklist = (char **)find_kernel_symbol_exact("module_blacklist");
    if (!module_blacklist) {
        pr_warn("module_blacklist: kernel symbol not found, skipping blocked modules\n");
        return;
    }

    existing = *module_blacklist;
    existing_len = existing ? strlen(existing) : 0;
    blacklist = kasprintf(GFP_KERNEL, "%s%s%s", existing_len ? existing : "", existing_len ? "," : "", modules);
    if (!blacklist) {
        pr_err("module_blacklist: cannot allocate blacklist\n");
        return;
    }

    for (name = blacklist + existing_len; *name; name++) {
        if (*name == '-')
            *name = '_';
    }

    *module_blacklist = blacklist;
    pr_info("module_blacklist: blocked modules: %s\n", blacklist);
}

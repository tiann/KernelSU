/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>

#include "hook/callsite_hook.h"
#include "infra/symbol_resolver.h"
#include "klog.h" // IWYU pragma: keep

static struct ksu_callsite_hook type_lookup_hook;

int ksu_selinux_hide_hook_parser(void *lookup_replacement, void **lookup_original)
{
    const char *names[] = { "string_to_context_struct", "symtab_search" };
    unsigned long addrs[ARRAY_SIZE(names)];
    unsigned long parser_size, parser_offset;
    void *lookup_site;
    int ret;

    if (!lookup_replacement || !lookup_original)
        return -EINVAL;
#if defined(MODULE) && !defined(CONFIG_TASKS_RCU)
    /* An unloadable replacement needs a task grace period before its text can be freed */
    *lookup_original = NULL;
    return -EOPNOTSUPP;
#endif
    if (type_lookup_hook.installed) {
        *lookup_original = type_lookup_hook.original;
        return 0;
    }
    if (type_lookup_hook.trampoline) {
        *lookup_original = type_lookup_hook.original;
        return ksu_callsite_hook_enable(&type_lookup_hook);
    }
    *lookup_original = NULL;

    ret = ksu_lookup_kernel_symbols(names, addrs, ARRAY_SIZE(names));
    if (ret < 0)
        return ret;
    if (!addrs[0] || !addrs[1])
        return -ENOENT;
    if (!kallsyms_lookup_size_offset(addrs[0], &parser_size, &parser_offset) || !parser_size || parser_offset)
        return -ENOENT;

    // The type lookup is expected to be the third direct symtab_search call
    ret = ksu_callsite_find((void *)addrs[0], parser_size, (void *)addrs[1], 2, &lookup_site);
    if (ret)
        return ret;

    ret = ksu_callsite_hook_prepare(&type_lookup_hook, lookup_site, (void *)addrs[1], lookup_replacement,
                                    lookup_original);
    if (ret)
        return ret;
    return ksu_callsite_hook_enable(&type_lookup_hook);
}

int ksu_selinux_hide_unhook_parser(void)
{
    /* Keep the trampoline because callers may run outside an RCU read section */
    return ksu_callsite_hook_restore(&type_lookup_hook);
}

void __exit ksu_selinux_hide_exit_parser(void)
{
    int ret;

    /* Module exit cannot fail, so keep its code live until the trampoline is safe */
    while ((ret = ksu_callsite_hook_detach(&type_lookup_hook))) {
        pr_err_ratelimited("selinux_hide: detach context parser failed: %d, retrying\n", ret);
        msleep(20);
    }
}

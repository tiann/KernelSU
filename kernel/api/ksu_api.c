#include <linux/errno.h>
#include <linux/export.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/unistd.h>

#include "api/ksu_api.h"
#include "hook/lsm_hook.h"
#include "hook/syscall_hook.h"
#include "manager/manager_identity.h"
#include "policy/allowlist.h"
#include "policy/app_profile.h"
#include "runtime/ksud_boot.h"
#include "ksu.h"

/* Plugins that hold the API, so ksud can unload them before the core. */
struct ksu_plugin {
    struct list_head node;
    struct module *owner;
    unsigned int refs;
};

static LIST_HEAD(ksu_plugins);
static DEFINE_MUTEX(ksu_plugins_lock);

static int ksu_plugin_get(struct module *owner)
{
    struct ksu_plugin *p;

    if (!owner)
        return 0;
    mutex_lock(&ksu_plugins_lock);
    list_for_each_entry (p, &ksu_plugins, node) {
        if (p->owner == owner) {
            p->refs++;
            mutex_unlock(&ksu_plugins_lock);
            return 0;
        }
    }
    p = kzalloc(sizeof(*p), GFP_KERNEL);
    if (!p) {
        mutex_unlock(&ksu_plugins_lock);
        return -ENOMEM;
    }
    p->owner = owner;
    p->refs = 1;
    list_add_tail(&p->node, &ksu_plugins);
    mutex_unlock(&ksu_plugins_lock);
    return 0;
}

static void ksu_plugin_put(struct module *owner)
{
    struct ksu_plugin *p;

    if (!owner)
        return;
    mutex_lock(&ksu_plugins_lock);
    list_for_each_entry (p, &ksu_plugins, node) {
        if (p->owner != owner)
            continue;
        if (!--p->refs) {
            list_del(&p->node);
            kfree(p);
        }
        mutex_unlock(&ksu_plugins_lock);
        return;
    }
    mutex_unlock(&ksu_plugins_lock);
    pr_warn("ksu_put_api: unknown owner %s\n", module_name(owner));
}

size_t ksu_api_list_plugins(char *names, size_t name_len, size_t max)
{
    struct ksu_plugin *p;
    size_t count = 0;

    mutex_lock(&ksu_plugins_lock);
    list_for_each_entry (p, &ksu_plugins, node) {
        if (count < max)
            strscpy(names + count * name_len, module_name(p->owner), name_len);
        count++;
    }
    mutex_unlock(&ksu_plugins_lock);
    return count;
}

static u64 ksu_api_get_runtime_flags(void)
{
    u64 flags = 0;

    if (READ_ONCE(ksu_late_loaded))
        flags |= KSU_RUNTIME_FLAG_LATE_LOADED;
    if (READ_ONCE(ksu_boot_completed))
        flags |= KSU_RUNTIME_FLAG_BOOT_COMPLETED;
    if (READ_ONCE(ksu_module_mounted))
        flags |= KSU_RUNTIME_FLAG_MODULE_MOUNTED;
    return flags;
}

/*
 * Every table entry points at a wrapper defined here with the exact prototype
 * of the table field.  The table is an ABI checked by kCFI on each indirect
 * call, so an internal helper with a different (or K&R-style, e.g. "uid_t
 * f()") signature must never be stored in it directly.
 */
static bool ksu_api_is_allow_uid(uid_t uid)
{
    return __ksu_is_allow_uid(uid);
}

static bool ksu_api_is_allow_uid_for_current(uid_t uid)
{
    return __ksu_is_allow_uid_for_current(uid);
}

static bool ksu_api_uid_should_umount(uid_t uid)
{
    return ksu_uid_should_umount(uid);
}

static bool ksu_api_is_uid_manager(uid_t uid)
{
    return is_uid_manager(uid);
}

static uid_t ksu_api_get_manager_appid(void)
{
    return ksu_get_manager_appid();
}

static int ksu_api_grant_root_current(void)
{
    return escape_with_root_profile();
}

static bool ksu_api_has_syscall_hook(int nr)
{
    return ksu_has_syscall_hook(nr);
}

static int ksu_api_lsm_hook(struct ksu_lsm_hook *hook)
{
    return ksu_lsm_hook(hook);
}

static void ksu_api_lsm_unhook(struct ksu_lsm_hook *hook)
{
    ksu_lsm_unhook(hook);
}

static int ksu_api_register_lsm_hook(struct ksu_lsm_hook *hook)
{
    return ksu_register_lsm_hook(hook);
}

static void ksu_api_unregister_lsm_hook(struct ksu_lsm_hook *hook)
{
    ksu_unregister_lsm_hook(hook);
}

static bool ksu_api_core_syscall(int nr)
{
    return nr == __NR_setresuid || nr == __NR_execve || nr == __NR_execveat || nr == __NR_newfstatat ||
           nr == __NR_faccessat;
}

static int ksu_api_register_syscall_hook(int nr, ksu_syscall_hook_fn fn)
{
    if (ksu_api_core_syscall(nr))
        return -EPERM;
    return ksu_register_syscall_hook(nr, fn);
}

static void ksu_api_unregister_syscall_hook(int nr)
{
    if (!ksu_api_core_syscall(nr))
        ksu_unregister_syscall_hook(nr);
}

static void ksu_api_syscall_table_hook(int nr, syscall_fn_t fn, syscall_fn_t *old)
{
    if (!ksu_api_core_syscall(nr))
        ksu_syscall_table_hook(nr, fn, old);
}

static void ksu_api_syscall_table_unhook(int nr)
{
    if (!ksu_api_core_syscall(nr))
        ksu_syscall_table_unhook(nr);
}

static const struct ksu_api ksu_api_table = {
    .api_version = KSU_KERNEL_API_VERSION,
    .table_size = sizeof(struct ksu_api),
    .kernelsu_version = KERNEL_SU_VERSION,
    .reserved0 = 0,
    .capabilities =
        KSU_API_CAP_RUNTIME_FLAGS | KSU_API_CAP_EVENTS | KSU_API_CAP_REBOOT_HANDLERS | KSU_API_CAP_LEGACY_HOOKS,
    .get_runtime_flags = ksu_api_get_runtime_flags,
    .is_allow_uid = ksu_api_is_allow_uid,
    .is_allow_uid_for_current = ksu_api_is_allow_uid_for_current,
    .uid_should_umount = ksu_api_uid_should_umount,
    .is_uid_manager = ksu_api_is_uid_manager,
    .get_manager_appid = ksu_api_get_manager_appid,
    .grant_root_current = ksu_api_grant_root_current,
    .register_event_handler = ksu_register_event_handler,
    .unregister_event_handler = ksu_unregister_event_handler,
    .register_reboot_handler = ksu_register_reboot_handler,
    .unregister_reboot_handler = ksu_unregister_reboot_handler,
    .register_syscall_hook = ksu_api_register_syscall_hook,
    .unregister_syscall_hook = ksu_api_unregister_syscall_hook,
    .has_syscall_hook = ksu_api_has_syscall_hook,
    .syscall_table_hook = ksu_api_syscall_table_hook,
    .syscall_table_unhook = ksu_api_syscall_table_unhook,
    .lsm_hook = ksu_api_lsm_hook,
    .lsm_unhook = ksu_api_lsm_unhook,
    .register_lsm_hook = ksu_api_register_lsm_hook,
    .unregister_lsm_hook = ksu_api_unregister_lsm_hook,
    .release = ksu_put_api,
};

int ksu_get_api(struct module *owner, __u32 version, void *table, size_t table_size)
{
    size_t copy_size;
    int ret;

    if (!table || table_size < offsetof(struct ksu_api, get_runtime_flags))
        return -EINVAL;
    /* Older callers can consume the stable prefix of a newer table. */
    if (version == 0 || version > KSU_KERNEL_API_VERSION)
        return -EOPNOTSUPP;
#ifdef MODULE
    if (!try_module_get(THIS_MODULE))
        return -ENODEV;
#endif
    ret = ksu_plugin_get(owner);
    if (ret) {
#ifdef MODULE
        module_put(THIS_MODULE);
#endif
        return ret;
    }

    /* Zero the caller's whole table so fields beyond our prefix read as absent. */
    memset(table, 0, table_size);
    copy_size = min_t(size_t, table_size, sizeof(ksu_api_table));
    memcpy(table, &ksu_api_table, copy_size);
    return KSU_KERNEL_API_VERSION;
}
EXPORT_SYMBOL_GPL(ksu_get_api);

void ksu_put_api(struct module *owner)
{
    ksu_plugin_put(owner);
#ifdef MODULE
    module_put(THIS_MODULE);
#endif
}

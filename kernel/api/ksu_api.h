#ifndef __KSU_KERNEL_API_H
#define __KSU_KERNEL_API_H

#include <linux/module.h>
#include <linux/types.h>
#include <linux/uidgid.h>

#include "api/event_registry.h"
#include "hook/lsm_hook.h"
#include "hook/syscall_hook.h"

#define KSU_KERNEL_API_VERSION 1U

#define KSU_API_CAP_RUNTIME_FLAGS (1ULL << 0)
#define KSU_API_CAP_EVENTS (1ULL << 1)
#define KSU_API_CAP_REBOOT_HANDLERS (1ULL << 2)
#define KSU_API_CAP_LEGACY_HOOKS (1ULL << 3)

#define KSU_RUNTIME_FLAG_LATE_LOADED (1ULL << 0)
#define KSU_RUNTIME_FLAG_BOOT_COMPLETED (1ULL << 1)
#define KSU_RUNTIME_FLAG_MODULE_MOUNTED (1ULL << 2)

struct ksu_api {
    __u16 api_version;
    __u16 table_size;
    __u32 kernelsu_version;
    __u32 reserved0;

    __u64 (*get_runtime_flags)(void);
    bool (*is_allow_uid)(uid_t uid);
    bool (*is_allow_uid_for_current)(uid_t uid);
    bool (*uid_should_umount)(uid_t uid);
    bool (*is_uid_manager)(uid_t uid);
    uid_t (*get_manager_appid)(void);
    int (*grant_root_current)(void);

    int (*register_syscall_hook)(int nr, ksu_syscall_hook_fn fn);
    void (*unregister_syscall_hook)(int nr);
    bool (*has_syscall_hook)(int nr);
    void (*syscall_table_hook)(int nr, syscall_fn_t fn, syscall_fn_t *old);
    void (*syscall_table_unhook)(int nr);
    int (*lsm_hook)(struct ksu_lsm_hook *hook);
    void (*lsm_unhook)(struct ksu_lsm_hook *hook);
    int (*register_lsm_hook)(struct ksu_lsm_hook *hook);
    void (*unregister_lsm_hook)(struct ksu_lsm_hook *hook);

    __u64 capabilities;
    int (*register_event_handler)(const struct ksu_event_handler_desc *desc, struct module *owner, void **cookie);
    int (*unregister_event_handler)(void *cookie);
    int (*register_reboot_handler)(const struct ksu_reboot_handler_desc *desc, struct module *owner, void **cookie);
    int (*unregister_reboot_handler)(void *cookie);

    /* Balances one successful ksu_get_api() call of the same owner. */
    void (*release)(struct module *owner);
};

/*
 * @owner is the plugin module (THIS_MODULE); it is recorded so userspace can
 * unload plugins before the core.  The full @table_size is zeroed and the
 * stable prefix that fits is copied.  Returns the API version on success.
 */
int ksu_get_api(struct module *owner, __u32 version, void *table, size_t table_size);
void ksu_put_api(struct module *owner);

/* Copy up to @max NUL-terminated module names (stride @name_len) of the
 * plugins that currently hold the API; returns the total count. */
size_t ksu_api_list_plugins(char *names, size_t name_len, size_t max);

#endif

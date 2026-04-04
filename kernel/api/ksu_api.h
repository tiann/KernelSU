#ifndef __KSU_API_H
#define __KSU_API_H

#include <linux/types.h>
#include <linux/uidgid.h>
#include "hook/lsm_hook.h"
#include "hook/syscall_hook.h"

#define KSU_API_VERSION_V1 1U

#define KSU_RUNTIME_FLAG_LATE_LOADED (1ULL << 0)
#define KSU_RUNTIME_FLAG_BOOT_COMPLETED (1ULL << 1)
#define KSU_RUNTIME_FLAG_MODULE_MOUNTED (1ULL << 2)

struct ksu_api {
    u16 api_version;
    u16 table_size;
    u32 kernelsu_version;
    u32 reserved0;

    u64 (*get_runtime_flags)(void);

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
};

int ksu_get_api(u32 api_version, void *table, size_t table_size);

#endif // __KSU_API_H

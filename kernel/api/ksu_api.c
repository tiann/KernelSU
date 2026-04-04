#include <linux/compiler.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/string.h>

#include "api/ksu_api.h"
#include "ksu.h"
#include "hook/lsm_hook.h"
#include "hook/syscall_hook.h"
#include "manager/manager_identity.h"
#include "policy/allowlist.h"
#include "policy/app_profile.h"
#include "runtime/ksud_boot.h"

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

static const struct ksu_api ksu_api_table = {
    .api_version = KSU_API_VERSION_V1,
    .table_size = sizeof(struct ksu_api),
    .kernelsu_version = KERNEL_SU_VERSION,
    .reserved0 = 0,
    .get_runtime_flags = ksu_api_get_runtime_flags,
    .is_allow_uid = __ksu_is_allow_uid,
    .is_allow_uid_for_current = __ksu_is_allow_uid_for_current,
    .uid_should_umount = ksu_uid_should_umount,
    .is_uid_manager = is_uid_manager,
    .get_manager_appid = ksu_get_manager_appid,
    .grant_root_current = escape_with_root_profile,
    .register_syscall_hook = ksu_register_syscall_hook,
    .unregister_syscall_hook = ksu_unregister_syscall_hook,
    .has_syscall_hook = ksu_has_syscall_hook,
    .syscall_table_hook = ksu_syscall_table_hook,
    .syscall_table_unhook = ksu_syscall_table_unhook,
    .lsm_hook = ksu_lsm_hook,
    .lsm_unhook = ksu_lsm_unhook,
    .register_lsm_hook = ksu_register_lsm_hook,
    .unregister_lsm_hook = ksu_unregister_lsm_hook,
};

int ksu_get_api(u32 api_version, void *table, size_t table_size)
{
    if (!table)
        return -EINVAL;

    if (api_version > KSU_API_VERSION_V1)
        return -EOPNOTSUPP;

    if (table_size < sizeof(struct ksu_api))
        return -EINVAL;

    memcpy(table, &ksu_api_table, sizeof(ksu_api_table));
    return ksu_api_table.api_version;
}
EXPORT_SYMBOL_GPL(ksu_get_api);

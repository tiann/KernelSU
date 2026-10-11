#include <linux/cred.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/static_key.h>
#include <linux/types.h>

#include "feature/shell_su_adb_only.h"
#include "policy/feature.h"
#include "selinux/selinux.h"

#include "klog.h" // IWYU pragma: keep

// Apps that manage to run code as the shell uid (e.g. through a system service
// bug) would otherwise inherit the root grant of com.android.shell. When this
// is enabled, a granted shell may only get root if it was spawned by adbd, i.e.
// it is a real `adb shell` session, or by a KernelSU root process (e.g. adb
// daemons provided by modules such as altdb). The latter is no escalation, as
// only processes already granted root by KernelSU run in the ksu domain.
static DEFINE_STATIC_KEY_FALSE(ksu_shell_su_adb_only);

// Deny instead of walking an unbounded chain of ancestors
#define MAX_ANCESTOR_DEPTH 128

static bool has_trusted_ancestor(void)
{
    struct task_struct *t;
    int depth = 0;
    bool found = false;

    rcu_read_lock();
    // Stop at init: orphans reparented to it are not attached to a session any more
    for (t = current; t->pid > 1 && depth < MAX_ANCESTOR_DEPTH; t = rcu_dereference(t->real_parent), depth++) {
        const struct cred *cred = __task_cred(t);
        if (is_adbd(cred) || is_task_ksu_domain(cred)) {
            found = true;
            break;
        }
    }
    rcu_read_unlock();

    return found;
}

bool ksu_shell_su_allowed_for_current(void)
{
    if (!static_branch_unlikely(&ksu_shell_su_adb_only)) {
        return true;
    }

    return has_trusted_ancestor();
}

static int shell_su_adb_only_feature_get(u64 *value)
{
    *value = static_key_enabled(&ksu_shell_su_adb_only) ? 1 : 0;
    return 0;
}

static int shell_su_adb_only_feature_set(u64 value)
{
    bool enable = value != 0;
    if (enable) {
        static_key_enable(&ksu_shell_su_adb_only.key);
    } else {
        static_key_disable(&ksu_shell_su_adb_only.key);
    }
    pr_info("shell_su_adb_only: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler shell_su_adb_only_handler = {
    .feature_id = KSU_FEATURE_SHELL_SU_ADB_ONLY,
    .name = "shell_su_adb_only",
    .get_handler = shell_su_adb_only_feature_get,
    .set_handler = shell_su_adb_only_feature_set,
};

void __init ksu_shell_su_adb_only_init(void)
{
    if (ksu_register_feature_handler(&shell_su_adb_only_handler)) {
        pr_err("Failed to register shell_su_adb_only feature handler\n");
    }
}

void __exit ksu_shell_su_adb_only_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_SHELL_SU_ADB_ONLY);
}

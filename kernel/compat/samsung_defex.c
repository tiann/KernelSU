#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/kprobes.h>
#include <linux/sched.h>

#include "compat/samsung_defex.h"
#include "infra/symbol_resolver.h"
#include "klog.h"
#include "selinux/selinux.h"

#ifdef CONFIG_KSU_SAMSUNG_DEFEX
typedef void (*defex_get_task_creds_t)(struct task_struct *task, unsigned int *uid, unsigned int *fsuid,
                                       unsigned int *egid, unsigned short *cred_flags);
typedef int (*defex_set_task_creds_t)(struct task_struct *task, unsigned int uid, unsigned int fsuid, unsigned int egid,
                                      unsigned short cred_flags);

static defex_get_task_creds_t defex_get_task_creds;
static defex_set_task_creds_t defex_set_task_creds;
static bool defex_enforce_hooked;

static int ksu_samsung_defex_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    struct task_struct *task = (struct task_struct *)regs->regs[0];

    (void)probe;
    if (task == current && current_uid().val == 0 && is_ksu_domain())
        regs->regs[0] = 0;

    return 0;
}

static struct kprobe defex_enforce_kprobe = {
    .symbol_name = "task_defex_enforce",
    .pre_handler = ksu_samsung_defex_pre_handler,
};
#endif

int ksu_samsung_defex_init(void)
{
#ifdef CONFIG_KSU_SAMSUNG_DEFEX
    int ret;

    defex_get_task_creds = (defex_get_task_creds_t)ksu_resolve_symbol_for_functable_hook("get_task_creds");
    defex_set_task_creds = (defex_set_task_creds_t)ksu_resolve_symbol_for_functable_hook("set_task_creds");
    if (!defex_get_task_creds || !defex_set_task_creds) {
        pr_err("Samsung DEFEX credential functions unavailable\n");
        return -ENOENT;
    }

    ret = register_kprobe(&defex_enforce_kprobe);
    if (ret) {
        pr_err("Samsung DEFEX enforce hook unavailable: %d\n", ret);
        return ret;
    }
    defex_enforce_hooked = true;

    pr_info("Samsung DEFEX credential synchronization and KSU-task bypass enabled\n");
#endif
    return 0;
}

void ksu_samsung_defex_exit(void)
{
#ifdef CONFIG_KSU_SAMSUNG_DEFEX
    if (defex_enforce_hooked) {
        unregister_kprobe(&defex_enforce_kprobe);
        defex_enforce_hooked = false;
    }
#endif
}

void ksu_samsung_defex_sync_current(void)
{
#ifdef CONFIG_KSU_SAMSUNG_DEFEX
    const struct cred *cred = current_cred();
    unsigned int stored_uid;
    unsigned int stored_fsuid;
    unsigned int stored_egid;
    unsigned short cred_flags;
    int ret;

    defex_get_task_creds(current, &stored_uid, &stored_fsuid, &stored_egid, &cred_flags);

    if (__kuid_val(cred->euid) == 0 && __kuid_val(cred->fsuid) == 0 && __kgid_val(cred->egid) == 0) {
        stored_uid = 1;
        stored_fsuid = 1;
        stored_egid = 1;
    } else {
        stored_uid = __kuid_val(cred->euid);
        stored_fsuid = __kuid_val(cred->fsuid);
        stored_egid = __kgid_val(cred->egid);
    }

    ret = defex_set_task_creds(current, stored_uid, stored_fsuid, stored_egid, cred_flags);
    if (ret)
        pr_err("Samsung DEFEX credential synchronization failed: %d\n", ret);
#endif
}

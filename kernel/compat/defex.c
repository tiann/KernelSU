#include <linux/cred.h>
#include <linux/jump_label.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/sched.h>

#include "compat/jailbreak.h"
#include "infra/symbol_resolver.h"
#include "klog.h"
#include "selinux/selinux.h"

DEFINE_STATIC_KEY_FALSE(ksu_defex_key);

typedef void (*defex_get_task_creds_t)(struct task_struct *, unsigned int *, unsigned int *,
                                       unsigned int *, unsigned short *);
typedef int (*defex_set_task_creds_t)(struct task_struct *, unsigned int, unsigned int,
                                      unsigned int, unsigned short);

static defex_get_task_creds_t defex_get_task_creds_fn;
static defex_set_task_creds_t defex_set_task_creds_fn;
static bool defex_kprobe_registered;

static int defex_enforce_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    struct task_struct *task = (struct task_struct *)regs->regs[0];

    (void)probe;
    if (task == current && current_uid().val == 0 && is_ksu_domain())
        regs->regs[0] = 0;
    return 0;
}

static struct kprobe defex_enforce_kprobe = {
    .symbol_name = "task_defex_enforce",
    .pre_handler = defex_enforce_pre_handler,
};

void ksu_defex_sync_current(void)
{
    const struct cred *cred;
    unsigned int stored_uid, stored_fsuid, stored_egid;
    unsigned short cred_flags;
    int ret;

    if (!static_branch_unlikely(&ksu_defex_key))
        return;

    cred = current_cred();
    defex_get_task_creds_fn(current, &stored_uid, &stored_fsuid, &stored_egid, &cred_flags);

    if (__kuid_val(cred->euid) == 0 && __kuid_val(cred->fsuid) == 0 &&
        __kgid_val(cred->egid) == 0) {
        stored_uid = 1;
        stored_fsuid = 1;
        stored_egid = 1;
    } else {
        stored_uid = __kuid_val(cred->euid);
        stored_fsuid = __kuid_val(cred->fsuid);
        stored_egid = __kgid_val(cred->egid);
    }

    ret = defex_set_task_creds_fn(current, stored_uid, stored_fsuid, stored_egid, cred_flags);
    if (ret)
        pr_err("KSU: DEFEX cred sync failed: %d\n", ret);
}

void defex_jb_init(void)
{
    int ret;

    defex_get_task_creds_fn =
        (defex_get_task_creds_t)ksu_resolve_symbol_for_functable_hook("get_task_creds");
    defex_set_task_creds_fn =
        (defex_set_task_creds_t)ksu_resolve_symbol_for_functable_hook("set_task_creds");
    if (!defex_get_task_creds_fn || !defex_set_task_creds_fn)
        return;

    ret = register_kprobe(&defex_enforce_kprobe);
    if (ret) {
        pr_warn("KSU: DEFEX kprobe failed: %d\n", ret);
        return;
    }
    defex_kprobe_registered = true;
    static_branch_enable(&ksu_defex_key);
    pr_info("KSU: Samsung DEFEX detected\n");
}

void defex_jb_exit(void)
{
    if (defex_kprobe_registered) {
        unregister_kprobe(&defex_enforce_kprobe);
        defex_kprobe_registered = false;
    }
    static_branch_disable(&ksu_defex_key);
}

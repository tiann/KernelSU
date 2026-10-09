#include <linux/completion.h>
#include <linux/cred.h>
#include <linux/jump_label.h>
#include <linux/pid.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/user.h>
#include <linux/user_namespace.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include "compat/jailbreak.h"
#include "infra/symbol_resolver.h"
#include "klog.h"

DEFINE_STATIC_KEY_FALSE(ksu_kdp_key);

typedef struct cred *(*prepare_ro_creds_t)(struct cred *, int, u64);
typedef void (*kdp_assign_pgd_t)(struct task_struct *);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
typedef unsigned int (*kdp_usecount_sub_and_test_t)(int nr, struct cred *);
#else
typedef unsigned int (*kdp_usecount_dec_and_test_t)(struct cred *);
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
typedef long (*inc_rlimit_ucounts_t)(struct ucounts *ucounts, unsigned int type, long value);
typedef bool (*dec_rlimit_ucounts_t)(struct ucounts *ucounts, unsigned int type, long value);
#endif

static prepare_ro_creds_t prepare_ro_creds_fn;
static kdp_assign_pgd_t kdp_assign_pgd_fn;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static kdp_usecount_sub_and_test_t kdp_usecount_sub_and_test_fn;
#else
static kdp_usecount_dec_and_test_t kdp_usecount_dec_and_test_fn;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
static inc_rlimit_ucounts_t inc_rlimit_ucounts_fn;
static dec_rlimit_ucounts_t dec_rlimit_ucounts_fn;
#endif

enum kdp_cred_cmd {
    KDP_COPY_CREDS = 0,
};

struct kdp_commit_work {
    struct work_struct work;
    struct completion completion;
    struct task_struct *target;
    const struct cred *old_cred;
    struct cred *rw_cred;
    int result;
};

static void __nocfi kdp_commit_worker(struct work_struct *work)
{
    struct kdp_commit_work *cw = container_of(work, struct kdp_commit_work, work);
    const struct cred *old_cred = cw->old_cred;
    const struct cred *target_cred;
    const struct cred *target_real_cred;
    struct cred *ro_cred;
    bool user_changed;

    if (!uid_eq(current_euid(), GLOBAL_ROOT_UID)) {
        cw->result = -EPERM;
        goto out;
    }

    target_cred = rcu_access_pointer(cw->target->cred);
    target_real_cred = rcu_access_pointer(cw->target->real_cred);
    if (target_cred != old_cred || target_real_cred != old_cred) {
        cw->result = -EBUSY;
        goto out;
    }

    ro_cred = prepare_ro_creds_fn(cw->rw_cred, KDP_COPY_CREDS, (u64)cw->target);
    if (!ro_cred) {
        cw->result = -EIO;
        goto out;
    }

    user_changed = ro_cred->user != old_cred->user;
    if (user_changed) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
        inc_rlimit_ucounts_fn(ro_cred->ucounts, UCOUNT_RLIMIT_NPROC, 1);
#else
        atomic_inc(&ro_cred->user->processes);
#endif
    }

    rcu_assign_pointer(cw->target->real_cred, ro_cred);
    rcu_assign_pointer(cw->target->cred, ro_cred);
    kdp_assign_pgd_fn(cw->target);

    if (user_changed) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
        dec_rlimit_ucounts_fn(old_cred->ucounts, UCOUNT_RLIMIT_NPROC, 1);
#else
        atomic_dec(&old_cred->user->processes);
#endif
    }

    abort_creds(cw->rw_cred);
    cw->rw_cred = NULL;
    ksu_put_cred(old_cred);
    ksu_put_cred(old_cred);
    cw->result = 0;

    pr_info("KSU: KDP cred install pid=%d uid=%u euid=%u\n",
            task_pid_nr(cw->target), __kuid_val(ro_cred->uid), __kuid_val(ro_cred->euid));
out:
    complete(&cw->completion);
}

void __nocfi ksu_put_cred(const struct cred *cred)
{
    if (static_branch_unlikely(&ksu_kdp_key)) {
        struct cred *m = (struct cred *)cred;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
        if (m && kdp_usecount_sub_and_test_fn(1, m))
#else
        if (m && kdp_usecount_dec_and_test_fn(m))
#endif
            __put_cred(m);
    } else {
        put_cred(cred);
    }
}

int ksu_commit_creds(struct cred *cred)
{
    struct kdp_commit_work cw;
    bool queued;

    if (!static_branch_unlikely(&ksu_kdp_key))
        return commit_creds(cred);

    if (!cred)
        return -EINVAL;

    INIT_WORK(&cw.work, kdp_commit_worker);
    init_completion(&cw.completion);
    cw.target = current;
    cw.old_cred = current_real_cred();
    cw.rw_cred = cred;
    cw.result = -EIO;

    get_task_struct(cw.target);
    queued = schedule_work(&cw.work);
    if (!queued) {
        put_task_struct(cw.target);
        return -EBUSY;
    }

    wait_for_completion(&cw.completion);
    put_task_struct(cw.target);
    return cw.result;
}

void kdp_jb_init(void)
{
    prepare_ro_creds_fn = (prepare_ro_creds_t)ksu_resolve_symbol_for_functable_hook("prepare_ro_creds");
    kdp_assign_pgd_fn = (kdp_assign_pgd_t)ksu_resolve_symbol_for_functable_hook("kdp_assign_pgd");
    if (!prepare_ro_creds_fn || !kdp_assign_pgd_fn)
        return;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    kdp_usecount_sub_and_test_fn =
        (kdp_usecount_sub_and_test_t)ksu_resolve_symbol_for_functable_hook("kdp_usecount_sub_and_test");
    if (!kdp_usecount_sub_and_test_fn)
        return;
#else
    kdp_usecount_dec_and_test_fn =
        (kdp_usecount_dec_and_test_t)ksu_resolve_symbol_for_functable_hook("kdp_usecount_dec_and_test");
    if (!kdp_usecount_dec_and_test_fn)
        return;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    inc_rlimit_ucounts_fn =
        (inc_rlimit_ucounts_t)ksu_resolve_symbol_for_functable_hook("inc_rlimit_ucounts");
    dec_rlimit_ucounts_fn =
        (dec_rlimit_ucounts_t)ksu_resolve_symbol_for_functable_hook("dec_rlimit_ucounts");
    if (!inc_rlimit_ucounts_fn || !dec_rlimit_ucounts_fn)
        return;
#endif
    static_branch_enable(&ksu_kdp_key);
    pr_info("KSU: Samsung KDP detected\n");
}

void kdp_jb_exit(void)
{
    static_branch_disable(&ksu_kdp_key);
}

#include <linux/completion.h>
#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/user.h>
#include <linux/user_namespace.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include "infra/symbol_resolver.h"
#include "ksu_samsung_kdp.h"
#include "klog.h"

#ifdef CONFIG_KSU_SAMSUNG_KDP
enum samsung_kdp_cred_command {
    SAMSUNG_KDP_COPY_CREDS = 0,
};

typedef struct cred *(*prepare_ro_creds_t)(struct cred *cred, int command, u64 task);
typedef void (*kdp_assign_pgd_t)(struct task_struct *task);
typedef unsigned int (*kdp_usecount_dec_and_test_t)(struct cred *cred);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
typedef long (*inc_rlimit_ucounts_t)(struct ucounts *ucounts, enum rlimit_type type, long value);
typedef bool (*dec_rlimit_ucounts_t)(struct ucounts *ucounts, enum rlimit_type type, long value);
#endif

struct samsung_kdp_commit_work {
    struct work_struct work;
    struct completion completion;
    struct task_struct *target;
    const struct cred *old_cred;
    struct cred *rw_cred;
    int result;
};

static prepare_ro_creds_t prepare_ro_creds_fn;
static kdp_assign_pgd_t kdp_assign_pgd_fn;
static kdp_usecount_dec_and_test_t kdp_usecount_dec_and_test_fn;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
static inc_rlimit_ucounts_t inc_rlimit_ucounts_fn;
static dec_rlimit_ucounts_t dec_rlimit_ucounts_fn;
#endif

static void samsung_kdp_commit_worker(struct work_struct *work)
{
    struct samsung_kdp_commit_work *commit_work = container_of(work, struct samsung_kdp_commit_work, work);
    struct task_struct *target = commit_work->target;
    const struct cred *old_cred = commit_work->old_cred;
    const struct cred *target_cred;
    const struct cred *target_real_cred;
    struct cred *ro_cred;
    bool user_changed;

    if (!uid_eq(current_euid(), GLOBAL_ROOT_UID)) {
        commit_work->result = -EPERM;
        goto out;
    }

    target_cred = rcu_access_pointer(target->cred);
    target_real_cred = rcu_access_pointer(target->real_cred);
    if (target_cred != old_cred || target_real_cred != old_cred) {
        commit_work->result = -EBUSY;
        goto out;
    }

    ro_cred = prepare_ro_creds_fn(commit_work->rw_cred, SAMSUNG_KDP_COPY_CREDS, (u64)target);
    if (!ro_cred) {
        commit_work->result = -EIO;
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

    rcu_assign_pointer(target->real_cred, ro_cred);
    rcu_assign_pointer(target->cred, ro_cred);
    kdp_assign_pgd_fn(target);

    if (user_changed) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
        dec_rlimit_ucounts_fn(old_cred->ucounts, UCOUNT_RLIMIT_NPROC, 1);
#else
        atomic_dec(&old_cred->user->processes);
#endif
    }

    abort_creds(commit_work->rw_cred);
    commit_work->rw_cred = NULL;
    ksu_put_cred(old_cred);
    ksu_put_cred(old_cred);
    commit_work->result = 0;

    pr_info("Samsung KDP task-scoped credential install pid=%d uid=%u euid=%u\n", task_pid_nr(target),
            __kuid_val(ro_cred->uid), __kuid_val(ro_cred->euid));
out:
    complete(&commit_work->completion);
}
#endif

void ksu_samsung_kdp_put_cred(const struct cred *cred)
{
#ifdef CONFIG_KSU_SAMSUNG_KDP
    struct cred *mutable_cred = (struct cred *)cred;

    if (mutable_cred && kdp_usecount_dec_and_test_fn(mutable_cred))
        __put_cred(mutable_cred);
#else
    put_cred(cred);
#endif
}

int ksu_samsung_kdp_init(void)
{
#ifdef CONFIG_KSU_SAMSUNG_KDP
    prepare_ro_creds_fn = (prepare_ro_creds_t)ksu_resolve_symbol_for_functable_hook("prepare_ro_creds");
    kdp_assign_pgd_fn = (kdp_assign_pgd_t)ksu_resolve_symbol_for_functable_hook("kdp_assign_pgd");
    kdp_usecount_dec_and_test_fn = (kdp_usecount_dec_and_test_t)ksu_resolve_symbol_for_functable_hook(
        "kdp_usecount_dec_and_test");
    if (!prepare_ro_creds_fn || !kdp_assign_pgd_fn || !kdp_usecount_dec_and_test_fn) {
        pr_err("Samsung KDP credential functions unavailable\n");
        return -ENOENT;
    }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
    inc_rlimit_ucounts_fn = (inc_rlimit_ucounts_t)ksu_resolve_symbol_for_functable_hook("inc_rlimit_ucounts");
    dec_rlimit_ucounts_fn = (dec_rlimit_ucounts_t)ksu_resolve_symbol_for_functable_hook("dec_rlimit_ucounts");
    if (!inc_rlimit_ucounts_fn || !dec_rlimit_ucounts_fn) {
        pr_err("Samsung KDP ucounts functions unavailable\n");
        return -ENOENT;
    }
#endif

    pr_info("Samsung KDP task-scoped credential and native PGD path enabled\n");
#endif
    return 0;
}

void ksu_samsung_kdp_exit(void)
{
}

int ksu_samsung_kdp_commit_creds(struct cred *cred)
{
#ifdef CONFIG_KSU_SAMSUNG_KDP
    struct samsung_kdp_commit_work commit_work;
    bool queued;

    if (!cred)
        return -EINVAL;

    INIT_WORK(&commit_work.work, samsung_kdp_commit_worker);
    init_completion(&commit_work.completion);
    commit_work.target = current;
    commit_work.old_cred = current_real_cred();
    commit_work.rw_cred = cred;
    commit_work.result = -EIO;

    get_task_struct(commit_work.target);
    queued = schedule_work(&commit_work.work);
    if (!queued) {
        put_task_struct(commit_work.target);
        return -EBUSY;
    }

    wait_for_completion(&commit_work.completion);
    put_task_struct(commit_work.target);
    return commit_work.result;
#else
    return commit_creds(cred);
#endif
}

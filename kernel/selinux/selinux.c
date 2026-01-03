#include "selinux.h"
#include "linux/cred.h"
#include "linux/sched.h"
#include "objsec.h"
#include "linux/version.h"
#include "../klog.h" // IWYU pragma: keep
#include "../ksu.h"

static int transive_to_domain(const char *domain, struct cred *cred)
{
    u32 sid;
    int error;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
    struct task_security_struct *tsec;
#else
    struct cred_security_struct *tsec;
#endif

    tsec = selinux_cred(cred);
    if (!tsec) {
        pr_err("tsec == NULL!\n");
        return -1;
    }

    error = security_secctx_to_secid(domain, strlen(domain), &sid);
    if (error) {
        pr_info("security_secctx_to_secid %s -> sid: %d, error: %d\n", domain,
                sid, error);
    }

    if (!error) {
        tsec->sid = sid;
        tsec->create_sid = 0;
        tsec->keycreate_sid = 0;
        tsec->sockcreate_sid = 0;
    }
    return error;
}

void setup_selinux(const char *domain)
{
    if (transive_to_domain(domain, (struct cred *)__task_cred(current))) {
        pr_err("transive domain failed.\n");
        return;
    }
}

void setup_ksu_cred(void)  // FIX: proper void parameter
{
    if (ksu_cred && transive_to_domain(KERNEL_SU_CONTEXT, ksu_cred)) {
        pr_err("setup ksu cred failed.\n");
    }
}

void setenforce(bool enforce)
{
#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
    selinux_state.enforcing = enforce;
#endif
}

bool getenforce(void)  // FIX: proper void parameter
{
#ifdef CONFIG_SECURITY_SELINUX_DISABLE
    if (selinux_state.disabled) {
        return false;
    }
#endif
#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
    return selinux_state.enforcing;
#else
    return true;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 14, 0)
struct lsm_context {
    char *context;
    u32 len;
};

static int __security_secid_to_secctx(u32 secid, struct lsm_context *cp)
{
    return security_secid_to_secctx(secid, &cp->context, &cp->len);
}

static void __security_release_secctx(struct lsm_context *cp)
{
    security_release_secctx(cp->context, cp->len);  // FIX: void function, no return
}
#else
#define __security_secid_to_secctx security_secid_to_secctx
#define __security_release_secctx security_release_secctx
#endif

// FIX: Helper for correct context comparison
static bool context_equals(const char *expected, const struct lsm_context *ctx)
{
    size_t expected_len = strlen(expected);
    if (expected_len != ctx->len)
        return false;
    return memcmp(expected, ctx->context, ctx->len) == 0;
}

bool is_task_ksu_domain(const struct cred *cred)
{
    struct lsm_context ctx;
    bool result;
    int err;  // FIX: declare before code (C89 compat)

    if (!cred) {
        return false;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
    const struct task_security_struct *tsec = selinux_cred(cred);
#else
    const struct cred_security_struct *tsec = selinux_cred(cred);
#endif

    if (!tsec) {
        return false;
    }

    err = __security_secid_to_secctx(tsec->sid, &ctx);
    if (err) {
        return false;
    }

    result = context_equals(KERNEL_SU_CONTEXT, &ctx);  // FIX: proper comparison
    __security_release_secctx(&ctx);
    return result;
}

bool is_ksu_domain(void)  // FIX: proper void parameter
{
    // FIX: removed unused current_sid() call
    return is_task_ksu_domain(current_cred());
}

bool is_context(const struct cred *cred, const char *context)
{
    struct lsm_context ctx;
    bool result;
    int err;  // FIX: declare before code

    if (!cred) {
        return false;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
    const struct task_security_struct *tsec = selinux_cred(cred);
#else
    const struct cred_security_struct *tsec = selinux_cred(cred);
#endif

    if (!tsec) {
        return false;
    }

    err = __security_secid_to_secctx(tsec->sid, &ctx);
    if (err) {
        return false;
    }

    result = context_equals(context, &ctx);  // FIX: proper comparison
    __security_release_secctx(&ctx);
    return result;
}

bool is_zygote(const struct cred *cred)
{
    return is_context(cred, "u:r:zygote:s0");
}

bool is_init(const struct cred *cred)
{
    return is_context(cred, "u:r:init:s0");
}

u32 ksu_get_ksu_file_sid(void)  // FIX: proper void parameter
{
    u32 ksu_file_sid = 0;
    int err = security_secctx_to_secid(KSU_FILE_CONTEXT,
                                       strlen(KSU_FILE_CONTEXT), &ksu_file_sid);
    if (err) {
        pr_info("get ksufile sid err %d\n", err);
    }
    return ksu_file_sid;
}

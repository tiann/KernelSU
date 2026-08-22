#include "selinux_hide.h"
#include "infra/symbol_resolver.h"
#include "linux/jump_label.h"
#include "selinux/sepolicy.h"
#include <linux/cred.h>
#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <asm-generic/errno-base.h>
#include <net/genetlink.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
// security/selinux/include/security.h
#include <security.h>
#include <ss/context.h>
#include <ss/services.h>
#include <ss/mls.h>
#include <ss/conditional.h>
#include "avc.h"
#include "klog.h" // IWYU pragma: keep
#include "linux/kallsyms.h"
#include "objsec.h"
#include "hook/patch_memory.h"
#include "ksu.h"
#include "policy/feature.h"
#include "hook/lsm_hook.h"
#include "hook/syscall_hook.h"

static DEFINE_MUTEX(selinux_hide_mutex);
static bool ksu_selinux_hide_enabled __read_mostly = false;
static bool ksu_selinux_hide_running __read_mostly = false;
static bool selinux_hide_unload_prepared;
static bool selinux_hide_resume_running;
static bool selinux_hide_resume_status_hook;

enum sel_inos {
    SEL_ROOT_INO = 2,
    SEL_LOAD,
    SEL_ENFORCE,
    SEL_CONTEXT,
    SEL_ACCESS,
    SEL_CREATE,
    SEL_RELABEL,
    SEL_USER,
    SEL_POLICYVERS,
    SEL_COMMIT_BOOLS,
    SEL_MLS,
    SEL_DISABLE,
    SEL_MEMBER,
    SEL_CHECKREQPROT,
    SEL_COMPAT_NET,
    SEL_REJECT_UNKNOWN,
    SEL_DENY_UNKNOWN,
    SEL_STATUS,
    SEL_POLICY,
    SEL_VALIDATE_TRANS,
    SEL_INO_NEXT,
};

typedef ssize_t (*write_op_fn)(struct file *, char *, size_t);
static write_op_fn *selinux_write_op;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int security_context_to_sid_with_policy(struct selinux_policy *policy, const char *scontext, u32 scontext_len,
                                               u32 *sid, u32 def_sid, gfp_t gfp_flags);
static int security_sid_to_context_with_policy(struct selinux_policy *policy, u32 sid, char **scontext,
                                               u32 *scontext_len);
static void security_compute_av_user_with_policy(struct selinux_policy *policy, u32 ssid, u32 tsid, u16 tclass,
                                                 struct av_decision *avd);
static void (*security_dump_masked_av_fn)(struct policydb *policydb, struct context *scontext, struct context *tcontext,
                                          u16 tclass, u32 permissions, const char *reason) = NULL;
static void (*context_struct_compute_av_fn)(struct policydb *policydb, struct context *scontext,
                                            struct context *tcontext, u16 tclass, struct av_decision *avd,
                                            struct extended_perms *xperms) = NULL;
#else
static struct selinux_state fake_state;
#endif

static write_op_fn *context_write, *access_write;
static write_op_fn orig_context_write, orig_access_write;

static ssize_t my_write_context(struct file *file, char *buf, size_t size)
{
    if (likely(current_uid().val < 10000))
        return orig_context_write(file, buf, size);

    char *canon = NULL;
    u32 sid, len;
    ssize_t length;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    length = avc_has_perm(current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY, SECURITY__CHECK_CONTEXT, NULL);
    if (length)
        goto out;
    length = security_context_to_sid_with_policy(backup_sepolicy, buf, size, &sid, SECSID_NULL, GFP_KERNEL);
    if (length)
        goto out;
    length = security_sid_to_context_with_policy(backup_sepolicy, sid, &canon, &len);
    if (length)
        goto out;
    length = -ERANGE;
    if (len > SIMPLE_TRANSACTION_LIMIT) {
        pr_err("SELinux: %s:  context size (%u) exceeds payload max\n", __func__, len);
        goto out;
    }
#else
    length = avc_has_perm(&selinux_state, current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY,
                          SECURITY__CHECK_CONTEXT, NULL);
    if (length)
        goto out;
    length = security_context_to_sid(&fake_state, buf, size, &sid, GFP_KERNEL);
    if (length)
        goto out;
    length = security_sid_to_context(&fake_state, sid, &canon, &len);
    if (length)
        goto out;
#endif
    memcpy(buf, canon, len);
    length = len;
out:
    kfree(canon);
    return length;
}

static ssize_t my_write_access(struct file *file, char *buf, size_t size)
{
    if (likely(current_uid().val < 10000))
        return orig_access_write(file, buf, size);

    char *scon = NULL, *tcon = NULL;
    u32 ssid, tsid;
    u16 tclass;
    struct av_decision avd;
    ssize_t length;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    length = avc_has_perm(current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY, SECURITY__COMPUTE_AV, NULL);
#else
    length =
        avc_has_perm(&selinux_state, current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY, SECURITY__COMPUTE_AV, NULL);
#endif
    if (length)
        goto out;
    length = -ENOMEM;
    scon = kzalloc(size + 1, GFP_KERNEL);
    if (!scon)
        goto out;
    tcon = kzalloc(size + 1, GFP_KERNEL);
    if (!tcon)
        goto out;
    length = -EINVAL;
    if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
        goto out;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    length = security_context_to_sid_with_policy(backup_sepolicy, scon, strlen(scon), &ssid, SECSID_NULL, GFP_KERNEL);
    if (length)
        goto out;
    length = security_context_to_sid_with_policy(backup_sepolicy, tcon, strlen(tcon), &tsid, SECSID_NULL, GFP_KERNEL);
    if (length)
        goto out;
    security_compute_av_user_with_policy(backup_sepolicy, ssid, tsid, tclass, &avd);
#else
    length = security_context_str_to_sid(&fake_state, scon, &ssid, GFP_KERNEL);
    if (length)
        goto out;
    length = security_context_str_to_sid(&fake_state, tcon, &tsid, GFP_KERNEL);
    if (length)
        goto out;
    security_compute_av_user(&fake_state, ssid, tsid, tclass, &avd);
#endif
    length = scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%x %x %x %x %u %x", avd.allowed, 0xffffffff, avd.auditallow,
                       avd.auditdeny, avd.seqno, avd.flags);
out:
    kfree(tcon);
    kfree(scon);
    return length;
}

static int my_setprocattr(const char *name, void *value, size_t size);
struct ksu_lsm_hook selinux_setprocattr_hook = KSU_LSM_HOOK_INIT(setprocattr, "selinux_setprocattr", my_setprocattr, 0);

typedef int (*setprocattr_fn)(const char *name, void *value, size_t size);
static int __nocfi my_setprocattr(const char *name, void *value, size_t size)
{
    int error;
    u32 mysid, sid;
    char *str = value;

    if (likely(current_uid().val < 10000) || strcmp(name, "current"))
        goto call_orig;

    mysid = current_sid();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    error = avc_has_perm(mysid, mysid, SECCLASS_PROCESS, PROCESS__SETCURRENT, NULL);
#else
    error = avc_has_perm(&selinux_state, mysid, mysid, SECCLASS_PROCESS, PROCESS__SETCURRENT, NULL);
#endif
    if (error)
        return error;
    if (size && str[0] && str[0] != '\n') {
        if (str[size - 1] == '\n') {
            str[size - 1] = 0;
            size--;
        }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
        error = security_context_to_sid_with_policy(backup_sepolicy, str, size, &sid, SECSID_NULL, GFP_KERNEL);
#else
        error = security_context_to_sid(&fake_state, str, size, &sid, GFP_KERNEL);
#endif
        if (error)
            return error;
    }
call_orig:
    return ((setprocattr_fn)selinux_setprocattr_hook.original)(name, value, size);
}

static DEFINE_STATIC_KEY_FALSE(fake_status_initialize_key);
static struct page *fake_status;
static struct file_operations fake_status_fops;
static int (*orig_sel_status_release)(struct inode *inode, struct file *filp);

static void initialize_fake_status(void)
{
    struct selinux_kernel_status *new_status;
    struct selinux_kernel_status *status;
    struct page *new_page;

    mutex_lock(&selinux_state.status_lock);
    if (fake_status)
        goto out;
    if (!selinux_state.status_page) {
        pr_warn("initialize_fake_status: status_page not exist\n");
        goto out;
    }
    status = page_address(selinux_state.status_page);
    if (!status->enforcing && !ksu_late_loaded) {
        pr_warn("initialize_fake_status: skip not enforcing\n");
        goto out;
    }
    new_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
    if (!new_page) {
        pr_err("initialize_fake_status: failed to allocate page\n");
        goto out;
    }
    new_status = page_address(new_page);
    memcpy(new_status, status, sizeof(*status));
    if (ksu_late_loaded && !new_status->enforcing) {
        new_status->enforcing = 1;
        new_status->sequence = new_status->policyload ? 4 : 0;
    }
    fake_status = new_page;
    pr_info("initialize_fake_status initialized: sequence=%d, policyload=%d, enforcing=%d\n", new_status->sequence,
            new_status->policyload, new_status->enforcing);
out:
    mutex_unlock(&selinux_state.status_lock);
}

typedef int (*sel_open_handle_status_fn)(struct inode *inode, struct file *filp);
static sel_open_handle_status_fn orig_sel_open_handle_status, *sel_open_handle_status_slot;

static int my_sel_release_handle_status(struct inode *inode, struct file *filp)
{
    struct page *page = filp->private_data;
    int ret = 0;

    if (orig_sel_status_release)
        ret = orig_sel_status_release(inode, filp);
    if (page)
        put_page(page);
    return ret;
}

static int my_sel_open_handle_status(struct inode *inode, struct file *filp)
{
    if (likely(current_uid().val >= 10000 && READ_ONCE(ksu_selinux_hide_enabled))) {
        struct page *page;
        const struct file_operations *fops;

        mutex_lock(&selinux_state.status_lock);
        page = fake_status;
        if (page)
            get_page(page);
        mutex_unlock(&selinux_state.status_lock);

        if (page) {
            fops = fops_get(&fake_status_fops);
            if (!fops) {
                put_page(page);
                return -ENODEV;
            }
            filp->private_data = page;
            replace_fops(filp, fops);
            return 0;
        }
    }

    int ret = orig_sel_open_handle_status(inode, filp);
    if (static_branch_unlikely(&fake_status_initialize_key) && !ret && !fake_status)
        initialize_fake_status();
    return ret;
}

static int hook_selinux_status_open(void);
static int ksu_selinux_hide_unhook(void);

static int ksu_selinux_hide_enable(void)
{
    write_op_fn my;
    int cleanup_ret;
    int ret;

    pr_info("selinux_hide: init selinux hide\n");
    if (!backup_sepolicy)
        return -EAGAIN;
    selinux_write_op = find_kernel_symbol_exact("write_op");
    if (!selinux_write_op)
        return -ENOSYS;
    hook_selinux_status_open();
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    security_dump_masked_av_fn = find_kernel_symbol_exact("security_dump_masked_av");
    context_struct_compute_av_fn = find_kernel_symbol_exact("context_struct_compute_av");
#else
    fake_state.initialized = true;
    fake_state.policy = backup_sepolicy;
#endif
    context_write = &selinux_write_op[SEL_CONTEXT];
    my = my_write_context;
    if (READ_ONCE(*context_write) != my) {
        orig_context_write = READ_ONCE(*context_write);
        ret = ksu_patch_text(context_write, &my, sizeof(my), KSU_PATCH_TEXT_FLUSH_DCACHE);
        if (ret)
            goto unhook;
    }
    access_write = &selinux_write_op[SEL_ACCESS];
    my = my_write_access;
    if (READ_ONCE(*access_write) != my) {
        orig_access_write = READ_ONCE(*access_write);
        ret = ksu_patch_text(access_write, &my, sizeof(my), KSU_PATCH_TEXT_FLUSH_DCACHE);
        if (ret)
            goto unhook;
    }
    ret = ksu_lsm_hook(&selinux_setprocattr_hook);
    if (ret && ret != -EALREADY)
        goto unhook;
    ksu_syscall_hook_hold_unload_guard();
    return 0;
unhook:
    cleanup_ret = ksu_selinux_hide_unhook();
    return cleanup_ret ? -EUCLEAN : ret;
}

static int validate_write_hook_ownership(write_op_fn *slot, write_op_fn mine, write_op_fn orig, const char *name)
{
    write_op_fn current_fn;

    if (!slot)
        return 0;
    current_fn = READ_ONCE(*slot);
    if (current_fn == mine || current_fn == orig)
        return 0;

    pr_err("selinux_hide: %s ownership lost: current=0x%lx mine=0x%lx orig=0x%lx\n", name, (unsigned long)current_fn,
           (unsigned long)mine, (unsigned long)orig);
    return -EBUSY;
}

static int validate_status_hook_ownership(void)
{
    sel_open_handle_status_fn current_fn;

    if (!sel_open_handle_status_slot)
        return 0;
    current_fn = READ_ONCE(*sel_open_handle_status_slot);
    if (current_fn == my_sel_open_handle_status || current_fn == orig_sel_open_handle_status)
        return 0;

    pr_err("selinux_hide: status open ownership lost: current=0x%lx mine=0x%lx orig=0x%lx\n", (unsigned long)current_fn,
           (unsigned long)my_sel_open_handle_status, (unsigned long)orig_sel_open_handle_status);
    return -EBUSY;
}

static int ksu_selinux_hide_validate_ownership(void)
{
    int ret;

    ret = validate_write_hook_ownership(context_write, my_write_context, orig_context_write, "context write");
    if (ret)
        return ret;
    ret = validate_write_hook_ownership(access_write, my_write_access, orig_access_write, "access write");
    if (ret)
        return ret;
    return validate_status_hook_ownership();
}

static int ksu_selinux_hide_unhook(void)
{
    write_op_fn my;
    int first_err = 0;
    int ret;

    ret = ksu_selinux_hide_validate_ownership();
    if (ret)
        return ret;

    my = my_write_context;
    if (context_write && READ_ONCE(*context_write) == my) {
        write_op_fn orig = orig_context_write;
        ret = ksu_patch_text(context_write, &orig, sizeof(orig), KSU_PATCH_TEXT_FLUSH_DCACHE);
        if (ret)
            first_err = ret;
    }
    my = my_write_access;
    if (access_write && READ_ONCE(*access_write) == my) {
        write_op_fn orig = orig_access_write;
        ret = ksu_patch_text(access_write, &orig, sizeof(orig), KSU_PATCH_TEXT_FLUSH_DCACHE);
        if (ret && !first_err)
            first_err = ret;
    }
    if (sel_open_handle_status_slot && READ_ONCE(*sel_open_handle_status_slot) == my_sel_open_handle_status) {
        sel_open_handle_status_fn orig = orig_sel_open_handle_status;
        ret = ksu_patch_text(sel_open_handle_status_slot, &orig, sizeof(orig), KSU_PATCH_TEXT_FLUSH_DCACHE);
        if (ret && !first_err)
            first_err = ret;
    }
    if (selinux_setprocattr_hook.entry) {
        ksu_lsm_unhook(&selinux_setprocattr_hook);
        if (selinux_setprocattr_hook.entry && !first_err)
            first_err = -EIO;
    }
    return first_err;
}

static int ksu_selinux_hide_disable(void)
{
    return ksu_selinux_hide_unhook();
}

static int selinux_hide_feature_get(u64 *value)
{
    *value = READ_ONCE(ksu_selinux_hide_enabled) ? 1 : 0;
    return 0;
}

static int selinux_hide_feature_set(u64 value)
{
    bool enable = value != 0;
    int ret = 0;

    mutex_lock(&selinux_hide_mutex);
    if (enable) {
        if (!ksu_selinux_hide_running) {
            ret = ksu_selinux_hide_enable();
            if (!ret)
                ksu_selinux_hide_running = true;
        }
        WRITE_ONCE(ksu_selinux_hide_enabled, ret == 0);
    } else {
        if (ksu_selinux_hide_running) {
            ret = ksu_selinux_hide_disable();
            if (!ret)
                ksu_selinux_hide_running = false;
        }
        if (!ret)
            WRITE_ONCE(ksu_selinux_hide_enabled, false);
    }
    mutex_unlock(&selinux_hide_mutex);
    return ret;
}

static const struct ksu_feature_handler selinux_hide_handler = {
    .feature_id = KSU_FEATURE_SELINUX_HIDE,
    .name = "selinux_hide",
    .get_handler = selinux_hide_feature_get,
    .set_handler = selinux_hide_feature_set,
};

void ksu_selinux_hide_handle_second_stage(void)
{
    initialize_fake_status();
    if (fake_status)
        static_key_disable(&fake_status_initialize_key.key);
    else
        pr_warn("selinux_hide: fake status need late initialization\n");
}

void ksu_selinux_hide_handle_post_fs_data(void)
{
    static_key_disable(&fake_status_initialize_key.key);
    if (!fake_status)
        pr_err("selinux_hide: fake status is not initialized after post-fs-data!\n");
}

static int hook_selinux_status_open(void)
{
    struct file_operations *ops;
    sel_open_handle_status_fn new_fn = my_sel_open_handle_status;
    int ret;

    if (sel_open_handle_status_slot && READ_ONCE(*sel_open_handle_status_slot) == new_fn)
        return 0;
    ops = find_kernel_symbol_exact("sel_handle_status_ops");
    if (!ops)
        return -ENOENT;
    sel_open_handle_status_slot = &ops->open;
    orig_sel_open_handle_status = READ_ONCE(*sel_open_handle_status_slot);
    orig_sel_status_release = ops->release;
    fake_status_fops = *ops;
    fake_status_fops.owner = THIS_MODULE;
    fake_status_fops.release = my_sel_release_handle_status;
    ret = ksu_patch_text(sel_open_handle_status_slot, &new_fn, sizeof(new_fn), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret)
        return ret;
    ksu_syscall_hook_hold_unload_guard();
    return 0;
}

int ksu_selinux_hide_prepare_unload(void)
{
    int ret;

    mutex_lock(&selinux_hide_mutex);
    if (selinux_hide_unload_prepared) {
        mutex_unlock(&selinux_hide_mutex);
        return 0;
    }
    selinux_hide_resume_running = ksu_selinux_hide_running;
    selinux_hide_resume_status_hook =
        sel_open_handle_status_slot && READ_ONCE(*sel_open_handle_status_slot) == my_sel_open_handle_status;
    ret = ksu_selinux_hide_unhook();
    if (ret) {
        mutex_unlock(&selinux_hide_mutex);
        return -EUCLEAN;
    }
    selinux_hide_unload_prepared = true;
    mutex_unlock(&selinux_hide_mutex);
    return 0;
}

int ksu_selinux_hide_abort_unload(void)
{
    int cleanup_ret;
    int ret = 0;

    mutex_lock(&selinux_hide_mutex);
    if (!selinux_hide_unload_prepared) {
        mutex_unlock(&selinux_hide_mutex);
        return 0;
    }

    /*
     * Do not rebuild our layer on top of an unknown hook that appeared while
     * unload was prepared. Abort must restore exactly the state we removed.
     */
    if (selinux_hide_resume_status_hook) {
        ret = validate_status_hook_ownership();
        if (ret)
            goto rollback;
    }
    if (selinux_hide_resume_running) {
        ret = validate_write_hook_ownership(context_write, my_write_context, orig_context_write, "context write");
        if (ret)
            goto rollback;
        ret = validate_write_hook_ownership(access_write, my_write_access, orig_access_write, "access write");
        if (ret)
            goto rollback;
    }

    if (selinux_hide_resume_status_hook) {
        ret = hook_selinux_status_open();
        if (ret)
            goto rollback;
    }
    if (selinux_hide_resume_running) {
        ret = ksu_selinux_hide_enable();
        if (ret)
            goto rollback;
    }
    selinux_hide_unload_prepared = false;
    selinux_hide_resume_running = false;
    selinux_hide_resume_status_hook = false;
    mutex_unlock(&selinux_hide_mutex);
    return 0;
rollback:
    cleanup_ret = ksu_selinux_hide_unhook();
    mutex_unlock(&selinux_hide_mutex);
    return cleanup_ret ? -EUCLEAN : ret;
}

void __init ksu_selinux_hide_init(void)
{
    selinux_hide_unload_prepared = false;
    selinux_hide_resume_running = false;
    selinux_hide_resume_status_hook = false;
    if (ksu_register_feature_handler(&selinux_hide_handler))
        pr_err("Failed to register selinux_hide feature handler\n");
    if (ksu_late_loaded)
        initialize_fake_status();
    else
        static_key_enable(&fake_status_initialize_key.key);
    hook_selinux_status_open();
}

void __exit ksu_selinux_hide_exit(void)
{
    int ret = 0;

    mutex_lock(&selinux_hide_mutex);
    if (!selinux_hide_unload_prepared)
        ret = ksu_selinux_hide_unhook();
    if (ret)
        pr_err("selinux_hide: final unhook failed: %d\n", ret);
    ksu_selinux_hide_running = false;
    WRITE_ONCE(ksu_selinux_hide_enabled, false);
    selinux_hide_unload_prepared = false;
    selinux_hide_resume_running = false;
    selinux_hide_resume_status_hook = false;
    mutex_unlock(&selinux_hide_mutex);

    ksu_unregister_feature_handler(KSU_FEATURE_SELINUX_HIDE);
    mutex_lock(&selinux_state.status_lock);
    if (fake_status)
        __free_page(fake_status);
    fake_status = NULL;
    mutex_unlock(&selinux_state.status_lock);
}

void ksu_selinux_hide_drop_backup_if_unused(void)
{
    mutex_lock(&selinux_hide_mutex);
    if (!ksu_selinux_hide_running && backup_sepolicy) {
        sidtab_destroy(backup_sepolicy->sidtab);
        kfree(backup_sepolicy->sidtab);
        ksu_destroy_sepolicy(backup_sepolicy);
        backup_sepolicy = NULL;
    }
    mutex_unlock(&selinux_hide_mutex);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int string_to_context_struct(struct policydb *pol, struct sidtab *sidtabp, char *scontext, struct context *ctx,
                                    u32 def_sid)
{
    struct role_datum *role;
    struct type_datum *typdatum;
    struct user_datum *usrdatum;
    char *scontextp, *p, oldc;
    int rc = 0;
    context_init(ctx);
    rc = -EINVAL;
    scontextp = scontext;
    p = scontextp;
    while (*p && *p != ':')
        p++;
    if (*p == 0)
        goto out;
    *p++ = 0;
    usrdatum = symtab_search(&pol->p_users, scontextp);
    if (!usrdatum)
        goto out;
    ctx->user = usrdatum->value;
    scontextp = p;
    while (*p && *p != ':')
        p++;
    if (*p == 0)
        goto out;
    *p++ = 0;
    role = symtab_search(&pol->p_roles, scontextp);
    if (!role)
        goto out;
    ctx->role = role->value;
    scontextp = p;
    while (*p && *p != ':')
        p++;
    oldc = *p;
    *p++ = 0;
    typdatum = symtab_search(&pol->p_types, scontextp);
    if (!typdatum || typdatum->attribute)
        goto out;
    ctx->type = typdatum->value;
    rc = mls_context_to_sid(pol, oldc, p, ctx, sidtabp, def_sid);
    if (rc)
        goto out;
    rc = -EINVAL;
    if (!policydb_context_isvalid(pol, ctx))
        goto out;
    rc = 0;
out:
    if (rc)
        context_destroy(ctx);
    return rc;
}

static int security_context_to_sid_with_policy(struct selinux_policy *policy, const char *scontext, u32 scontext_len,
                                               u32 *sid, u32 def_sid, gfp_t gfp_flags)
{
    struct policydb *policydb;
    struct sidtab *sidtab;
    char *scontext2, *str = NULL;
    struct context context;
    int rc = 0;
    if (!scontext_len)
        return -EINVAL;
    scontext2 = kmemdup_nul(scontext, scontext_len, gfp_flags);
    if (!scontext2)
        return -ENOMEM;
    *sid = SECSID_NULL;
    policydb = &policy->policydb;
    sidtab = policy->sidtab;
    rc = string_to_context_struct(policydb, sidtab, scontext2, &context, def_sid);
    if (rc)
        goto out;
    rc = sidtab_context_to_sid(sidtab, &context, sid);
    if (rc)
        goto out;
    context_destroy(&context);
out:
    kfree(scontext2);
    kfree(str);
    return rc;
}

static int context_struct_to_string(struct policydb *p, struct context *context, char **scontext, u32 *scontext_len)
{
    char *scontextp;
    if (scontext)
        *scontext = NULL;
    *scontext_len = 0;
    if (context->len) {
        *scontext_len = context->len;
        if (scontext) {
            *scontext = kstrdup(context->str, GFP_ATOMIC);
            if (!(*scontext))
                return -ENOMEM;
        }
        return 0;
    }
    *scontext_len += strlen(sym_name(p, SYM_USERS, context->user - 1)) + 1;
    *scontext_len += strlen(sym_name(p, SYM_ROLES, context->role - 1)) + 1;
    *scontext_len += strlen(sym_name(p, SYM_TYPES, context->type - 1)) + 1;
    *scontext_len += mls_compute_context_len(p, context);
    if (!scontext)
        return 0;
    scontextp = kmalloc(*scontext_len, GFP_ATOMIC);
    if (!scontextp)
        return -ENOMEM;
    *scontext = scontextp;
    scontextp += sprintf(scontextp, "%s:%s:%s", sym_name(p, SYM_USERS, context->user - 1),
                         sym_name(p, SYM_ROLES, context->role - 1), sym_name(p, SYM_TYPES, context->type - 1));
    mls_sid_to_context(p, context, &scontextp);
    *scontextp = 0;
    return 0;
}

static int sidtab_entry_to_string(struct policydb *p, struct sidtab *sidtab, struct sidtab_entry *entry,
                                  char **scontext, u32 *scontext_len)
{
    int rc = sidtab_sid2str_get(sidtab, entry, scontext, scontext_len);
    if (rc != -ENOENT)
        return rc;
    rc = context_struct_to_string(p, &entry->context, scontext, scontext_len);
    if (!rc && scontext)
        sidtab_sid2str_put(sidtab, entry, *scontext, *scontext_len);
    return rc;
}

static int security_sid_to_context_with_policy(struct selinux_policy *policy, u32 sid, char **scontext,
                                               u32 *scontext_len)
{
    struct policydb *policydb;
    struct sidtab *sidtab;
    struct sidtab_entry *entry;
    int rc = 0;
    if (scontext)
        *scontext = NULL;
    *scontext_len = 0;
    policydb = &policy->policydb;
    sidtab = policy->sidtab;
    entry = sidtab_search_entry(sidtab, sid);
    if (!entry) {
        pr_err("SELinux: %s:  unrecognized SID %d\n", __func__, sid);
        return -EINVAL;
    }
    rc = sidtab_entry_to_string(policydb, sidtab, entry, scontext, scontext_len);
    return rc;
}

static void avd_init(struct selinux_policy *policy, struct av_decision *avd)
{
    avd->allowed = 0;
    avd->auditallow = 0;
    avd->auditdeny = 0xffffffff;
    avd->seqno = policy ? policy->latest_granting : 0;
    avd->flags = 0;
}

static void context_struct_compute_av(struct policydb *policydb, struct context *scontext, struct context *tcontext,
                                      u16 tclass, struct av_decision *avd, struct extended_perms *xperms);

static void __nocfi type_attribute_bounds_av(struct policydb *policydb, struct context *scontext,
                                             struct context *tcontext, u16 tclass, struct av_decision *avd)
{
    struct context lo_scontext;
    struct context lo_tcontext, *tcontextp = tcontext;
    struct av_decision lo_avd;
    struct type_datum *source;
    struct type_datum *target;
    u32 masked = 0;
    source = policydb->type_val_to_struct[scontext->type - 1];
    BUG_ON(!source);
    if (!source->bounds)
        return;
    target = policydb->type_val_to_struct[tcontext->type - 1];
    BUG_ON(!target);
    memset(&lo_avd, 0, sizeof(lo_avd));
    memcpy(&lo_scontext, scontext, sizeof(lo_scontext));
    lo_scontext.type = source->bounds;
    if (target->bounds) {
        memcpy(&lo_tcontext, tcontext, sizeof(lo_tcontext));
        lo_tcontext.type = target->bounds;
        tcontextp = &lo_tcontext;
    }
    context_struct_compute_av(policydb, &lo_scontext, tcontextp, tclass, &lo_avd, NULL);
    masked = ~lo_avd.allowed & avd->allowed;
    if (likely(!masked))
        return;
    avd->allowed &= ~masked;
    if (security_dump_masked_av_fn)
        security_dump_masked_av_fn(policydb, scontext, tcontext, tclass, masked, "bounds");
}

static int constraint_expr_eval(struct policydb *policydb, struct context *scontext, struct context *tcontext,
                                struct context *xcontext, struct constraint_expr *cexpr)
{
    u32 val1, val2;
    struct context *c;
    struct role_datum *r1, *r2;
    struct mls_level *l1, *l2;
    struct constraint_expr *e;
    int s[CEXPR_MAXDEPTH];
    int sp = -1;
    for (e = cexpr; e; e = e->next) {
        switch (e->expr_type) {
        case CEXPR_NOT:
            BUG_ON(sp < 0);
            s[sp] = !s[sp];
            break;
        case CEXPR_AND:
            BUG_ON(sp < 1);
            sp--;
            s[sp] &= s[sp + 1];
            break;
        case CEXPR_OR:
            BUG_ON(sp < 1);
            sp--;
            s[sp] |= s[sp + 1];
            break;
        case CEXPR_ATTR:
            if (sp == (CEXPR_MAXDEPTH - 1))
                return 0;
            switch (e->attr) {
            case CEXPR_USER:
                val1 = scontext->user;
                val2 = tcontext->user;
                break;
            case CEXPR_TYPE:
                val1 = scontext->type;
                val2 = tcontext->type;
                break;
            case CEXPR_ROLE:
                val1 = scontext->role;
                val2 = tcontext->role;
                r1 = policydb->role_val_to_struct[val1 - 1];
                r2 = policydb->role_val_to_struct[val2 - 1];
                switch (e->op) {
                case CEXPR_DOM:
                    s[++sp] = ebitmap_get_bit(&r1->dominates, val2 - 1);
                    continue;
                case CEXPR_DOMBY:
                    s[++sp] = ebitmap_get_bit(&r2->dominates, val1 - 1);
                    continue;
                case CEXPR_INCOMP:
                    s[++sp] =
                        (!ebitmap_get_bit(&r1->dominates, val2 - 1) && !ebitmap_get_bit(&r2->dominates, val1 - 1));
                    continue;
                default:
                    break;
                }
                break;
            case CEXPR_L1L2:
                l1 = &(scontext->range.level[0]);
                l2 = &(tcontext->range.level[0]);
                goto mls_ops;
            case CEXPR_L1H2:
                l1 = &(scontext->range.level[0]);
                l2 = &(tcontext->range.level[1]);
                goto mls_ops;
            case CEXPR_H1L2:
                l1 = &(scontext->range.level[1]);
                l2 = &(tcontext->range.level[0]);
                goto mls_ops;
            case CEXPR_H1H2:
                l1 = &(scontext->range.level[1]);
                l2 = &(tcontext->range.level[1]);
                goto mls_ops;
            case CEXPR_L1H1:
                l1 = &(scontext->range.level[0]);
                l2 = &(scontext->range.level[1]);
                goto mls_ops;
            case CEXPR_L2H2:
                l1 = &(tcontext->range.level[0]);
                l2 = &(tcontext->range.level[1]);
                goto mls_ops;
            mls_ops:
                switch (e->op) {
                case CEXPR_EQ:
                    s[++sp] = mls_level_eq(l1, l2);
                    continue;
                case CEXPR_NEQ:
                    s[++sp] = !mls_level_eq(l1, l2);
                    continue;
                case CEXPR_DOM:
                    s[++sp] = mls_level_dom(l1, l2);
                    continue;
                case CEXPR_DOMBY:
                    s[++sp] = mls_level_dom(l2, l1);
                    continue;
                case CEXPR_INCOMP:
                    s[++sp] = mls_level_incomp(l2, l1);
                    continue;
                default:
                    BUG();
                    return 0;
                }
                break;
            default:
                BUG();
                return 0;
            }
            switch (e->op) {
            case CEXPR_EQ:
                s[++sp] = val1 == val2;
                break;
            case CEXPR_NEQ:
                s[++sp] = val1 != val2;
                break;
            default:
                BUG();
                return 0;
            }
            break;
        case CEXPR_NAMES:
            if (sp == (CEXPR_MAXDEPTH - 1))
                return 0;
            c = scontext;
            if (e->attr & CEXPR_TARGET)
                c = tcontext;
            else if (e->attr & CEXPR_XTARGET) {
                c = xcontext;
                if (!c)
                    return 0;
            }
            if (e->attr & CEXPR_USER)
                val1 = c->user;
            else if (e->attr & CEXPR_ROLE)
                val1 = c->role;
            else if (e->attr & CEXPR_TYPE)
                val1 = c->type;
            else
                return 0;
            switch (e->op) {
            case CEXPR_EQ:
                s[++sp] = ebitmap_get_bit(&e->names, val1 - 1);
                break;
            case CEXPR_NEQ:
                s[++sp] = !ebitmap_get_bit(&e->names, val1 - 1);
                break;
            default:
                return 0;
            }
            break;
        default:
            return 0;
        }
    }
    BUG_ON(sp != 0);
    return s[0];
}

static void context_struct_compute_av(struct policydb *policydb, struct context *scontext, struct context *tcontext,
                                      u16 tclass, struct av_decision *avd, struct extended_perms *xperms)
{
    struct constraint_node *constraint;
    struct role_allow *ra;
    struct avtab_key avkey;
    struct avtab_node *node;
    struct class_datum *tclass_datum;
    struct ebitmap *sattr, *tattr;
    struct ebitmap_node *snode, *tnode;
    unsigned int i, j;
    avd->allowed = 0;
    avd->auditallow = 0;
    avd->auditdeny = 0xffffffff;
    if (xperms) {
        memset(&xperms->drivers, 0, sizeof(xperms->drivers));
        xperms->len = 0;
    }
    if (unlikely(!tclass || tclass > policydb->p_classes.nprim)) {
        pr_warn_ratelimited("SELinux:  Invalid class %u\n", tclass);
        return;
    }
    tclass_datum = policydb->class_val_to_struct[tclass - 1];
    avkey.target_class = tclass;
    avkey.specified = AVTAB_AV | AVTAB_XPERMS;
    sattr = &policydb->type_attr_map_array[scontext->type - 1];
    tattr = &policydb->type_attr_map_array[tcontext->type - 1];
    ebitmap_for_each_positive_bit(sattr, snode, i)
    {
        ebitmap_for_each_positive_bit(tattr, tnode, j)
        {
            avkey.source_type = i + 1;
            avkey.target_type = j + 1;
            for (node = avtab_search_node(&policydb->te_avtab, &avkey); node;
                 node = avtab_search_node_next(node, avkey.specified)) {
                if (node->key.specified == AVTAB_ALLOWED)
                    avd->allowed |= node->datum.u.data;
                else if (node->key.specified == AVTAB_AUDITALLOW)
                    avd->auditallow |= node->datum.u.data;
                else if (node->key.specified == AVTAB_AUDITDENY)
                    avd->auditdeny &= node->datum.u.data;
                else if (xperms && (node->key.specified & AVTAB_XPERMS))
                    services_compute_xperms_drivers(xperms, node);
            }
            cond_compute_av(&policydb->te_cond_avtab, &avkey, avd, xperms);
        }
    }
    constraint = tclass_datum->constraints;
    while (constraint) {
        if ((constraint->permissions & avd->allowed) &&
            !constraint_expr_eval(policydb, scontext, tcontext, NULL, constraint->expr))
            avd->allowed &= ~constraint->permissions;
        constraint = constraint->next;
    }
    if (tclass == policydb->process_class && (avd->allowed & policydb->process_trans_perms) &&
        scontext->role != tcontext->role) {
        for (ra = policydb->role_allow; ra; ra = ra->next) {
            if (scontext->role == ra->role && tcontext->role == ra->new_role)
                break;
        }
        if (!ra)
            avd->allowed &= ~policydb->process_trans_perms;
    }
    type_attribute_bounds_av(policydb, scontext, tcontext, tclass, avd);
}

static void __nocfi security_compute_av_user_with_policy(struct selinux_policy *policy, u32 ssid, u32 tsid, u16 tclass,
                                                         struct av_decision *avd)
{
    struct policydb *policydb;
    struct sidtab *sidtab;
    struct context *scontext = NULL, *tcontext = NULL;
    avd_init(policy, avd);
    policydb = &policy->policydb;
    sidtab = policy->sidtab;
    scontext = sidtab_search(sidtab, ssid);
    if (!scontext)
        goto out;
    if (ebitmap_get_bit(&policydb->permissive_map, scontext->type))
        avd->flags |= AVD_FLAGS_PERMISSIVE;
    tcontext = sidtab_search(sidtab, tsid);
    if (!tcontext)
        goto out;
    if (unlikely(!tclass)) {
        if (policydb->allow_unknown)
            goto allow;
        goto out;
    }
    if (context_struct_compute_av_fn)
        context_struct_compute_av_fn(policydb, scontext, tcontext, tclass, avd, NULL);
    else
        context_struct_compute_av(policydb, scontext, tcontext, tclass, avd, NULL);
out:
    return;
allow:
    avd->allowed = 0xffffffff;
    goto out;
}
#endif

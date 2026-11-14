#include "selinux_hide.h"
#include "infra/symbol_resolver.h"
#include "linux/jump_label.h"
#include "linux/rcupdate.h"
#include "selinux/sepolicy.h"
#include "selinux/selinux.h"
#include "infra/btf_parser.h"
#include "ss/policydb.h"
#include <linux/audit.h>
#include <linux/build_bug.h>
#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm-generic/errno-base.h>
#include <net/genetlink.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/security.h>
#include <linux/xattr.h>
#if defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF)
#include <uapi/linux/btf.h>
#endif
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

static DEFINE_MUTEX(selinux_hide_mutex);
static bool ksu_selinux_hide_enabled __read_mostly = false;
static bool ksu_selinux_hide_running __read_mostly = false;
static bool ksu_selinux_hide_keep_backup __read_mostly = false;

enum sel_inos {
    SEL_ROOT_INO = 2,
    SEL_LOAD, /* load policy */
    SEL_ENFORCE, /* get or set enforcing status */
    SEL_CONTEXT, /* validate context */
    SEL_ACCESS, /* compute access decision */
    SEL_CREATE, /* compute create labeling decision */
    SEL_RELABEL, /* compute relabeling decision */
    SEL_USER, /* compute reachable user contexts */
    SEL_POLICYVERS, /* return policy version for this kernel */
    SEL_COMMIT_BOOLS, /* commit new boolean values */
    SEL_MLS, /* return if MLS policy is enabled */
    SEL_DISABLE, /* disable SELinux until next reboot */
    SEL_MEMBER, /* compute polyinstantiation membership decision */
    SEL_CHECKREQPROT, /* check requested protection, not kernel-applied one */
    SEL_COMPAT_NET, /* whether to use old compat network packet controls */
    SEL_REJECT_UNKNOWN, /* export unknown reject handling to userspace */
    SEL_DENY_UNKNOWN, /* export unknown deny handling to userspace */
    SEL_STATUS, /* export current status using mmap() */
    SEL_POLICY, /* allow userspace to read the in kernel policy */
    SEL_VALIDATE_TRANS, /* compute validatetrans decision */
    SEL_INO_NEXT, /* The next inode number to use */
};

typedef ssize_t (*write_op_fn)(struct file *, char *, size_t);

static write_op_fn *selinux_write_op;

static int security_context_to_sid_with_policy(struct selinux_policy *policy, const char *scontext, u32 scontext_len,
                                               u32 *sid, u32 def_sid, gfp_t gfp_flags, u32 *orig_sid_p, int *orig_rc_p);
static int security_sid_to_context_with_policy(struct selinux_policy *policy, u32 sid, char **scontext,
                                               u32 *scontext_len);
static int security_context_to_sid_live(const char *scontext, u32 scontext_len, u32 *sid);
static void security_compute_av_user_with_policy(struct selinux_policy *policy, u32 ssid, u32 tsid, u16 tclass,
                                                 struct av_decision *avd);
static void (*security_dump_masked_av_fn)(struct policydb *policydb, struct context *scontext, struct context *tcontext,
                                          u16 tclass, u32 permissions, const char *reason) = NULL;
static void (*context_struct_compute_av_fn)(struct policydb *policydb, struct context *scontext,
                                            struct context *tcontext, u16 tclass, struct av_decision *avd,
                                            struct extended_perms *xperms) = NULL;

static write_op_fn *context_write, *access_write;
static write_op_fn orig_context_write, orig_access_write;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#define ksu_avc_has_perm_compat(...) avc_has_perm(__VA_ARGS__)
#define ksu_security_bounded_transition_compat(...) security_bounded_transition(__VA_ARGS__)
#define ksu_security_validate_transition_compat(...) security_validate_transition(__VA_ARGS__)
#define ksu_selinux_initialized_compat() selinux_initialized()
#else
#define ksu_avc_has_perm_compat(...) avc_has_perm(&selinux_state, __VA_ARGS__)
#define ksu_security_bounded_transition_compat(...) security_bounded_transition(&selinux_state, __VA_ARGS__)
#define ksu_security_validate_transition_compat(...) security_validate_transition(&selinux_state, __VA_ARGS__)
#define ksu_selinux_initialized_compat() selinux_initialized(&selinux_state)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
#define ksu_task_security_struct task_security_struct
#else
#define ksu_task_security_struct cred_security_struct
#endif

static ssize_t my_write_context(struct file *file, char *buf, size_t size)
{
    // apply to all app uids
    if (likely(current_uid().val < 10000)) {
        return orig_context_write(file, buf, size);
    }
    char *canon = NULL;
    u32 sid, len;
    ssize_t length;

    length =
        ksu_avc_has_perm_compat(current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY, SECURITY__CHECK_CONTEXT, NULL);
    if (length)
        goto out;
    length = security_context_to_sid_with_policy(backup_sepolicy, buf, size, &sid, SECSID_NULL, GFP_KERNEL, NULL, NULL);
    if (length) {
        goto out;
    }

    length = security_sid_to_context_with_policy(backup_sepolicy, sid, &canon, &len);
    if (length)
        goto out;

    length = -ERANGE;
    if (len > SIMPLE_TRANSACTION_LIMIT) {
        pr_err("SELinux: %s:  context size (%u) exceeds "
               "payload max\n",
               __func__, len);
        goto out;
    }

    memcpy(buf, canon, len);
    length = len;
out:
    kfree(canon);
    return length;
}

static ssize_t my_write_access(struct file *file, char *buf, size_t size)
{
    // apply to all app uids
    if (likely(current_uid().val < 10000)) {
        return orig_access_write(file, buf, size);
    }
    char *scon = NULL, *tcon = NULL;
    u32 ssid, tsid, sconlen, tconlen;
    u16 tclass;
    struct av_decision avd;
    ssize_t length;

    length = ksu_avc_has_perm_compat(current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY, SECURITY__COMPUTE_AV, NULL);
    if (length)
        goto out;

    length = -ENOMEM;
    scon = kzalloc(size + 1, GFP_KERNEL);
    if (!scon)
        goto out;

    length = -ENOMEM;
    tcon = kzalloc(size + 1, GFP_KERNEL);
    if (!tcon)
        goto out;

    length = -EINVAL;
    if (sscanf(buf, "%s %s %hu", scon, tcon, &tclass) != 3)
        goto out;

    sconlen = strlen(scon);
    tconlen = strlen(tcon);

    length =
        security_context_to_sid_with_policy(backup_sepolicy, scon, sconlen, &ssid, SECSID_NULL, GFP_KERNEL, NULL, NULL);
    if (length) {
        goto out;
    }

    length =
        security_context_to_sid_with_policy(backup_sepolicy, tcon, tconlen, &tsid, SECSID_NULL, GFP_KERNEL, NULL, NULL);
    if (length) {
        goto out;
    }

    security_compute_av_user_with_policy(backup_sepolicy, ssid, tsid, tclass, &avd);

    // stock reads 1; a loader load_policy may have bumped the backup before we load
    avd.seqno = 1;
    length = scnprintf(buf, SIMPLE_TRANSACTION_LIMIT, "%x %x %x %x %u %x", avd.allowed, 0xffffffff, avd.auditallow,
                       avd.auditdeny, avd.seqno, avd.flags);
out:
    kfree(tcon);
    kfree(scon);
    return length;
}

static int my_setprocattr(const char *name, void *value, size_t size);
struct ksu_lsm_hook selinux_setprocattr_hook = KSU_LSM_HOOK_INIT(setprocattr, "selinux_setprocattr", my_setprocattr, 0);

/*
 * get the security ID of a set of credentials
 */
static inline u32 cred_sid(const struct cred *cred)
{
    const struct ksu_task_security_struct *tsec;

    tsec = selinux_cred(cred);
    return tsec->sid;
}

/*
 * get the objective security ID of a task
 */
static inline u32 task_sid_obj(const struct task_struct *task)
{
    u32 sid;

    rcu_read_lock();
    sid = cred_sid(__task_cred(task));
    rcu_read_unlock();
    return sid;
}

static u32 ptrace_parent_sid(void)
{
    u32 sid = 0;
    struct task_struct *tracer;

    rcu_read_lock();
    tracer = ptrace_parent(current);
    if (tracer)
        sid = task_sid_obj(tracer);
    rcu_read_unlock();

    return sid;
}

typedef int (*setprocattr_fn)(const char *name, void *value, size_t size);
static int __nocfi my_setprocattr(const char *name, void *value, size_t size)
{
    if (likely(current_uid().val < 10000)) {
        goto call_orig;
    }

    if (strcmp(name, "current")) {
        goto call_orig;
    }

    struct ksu_task_security_struct *tsec;
    struct cred *new;
    u32 mysid = current_sid(), sid = 0, tmp_sid, ptsid;
    int error, error2;
    char *str = value;

    error = ksu_avc_has_perm_compat(mysid, mysid, SECCLASS_PROCESS, PROCESS__SETCURRENT, NULL);
    if (error)
        return error;

    /* Obtain a SID for the context, if one was specified. */
    if (size && str[0] && str[0] != '\n') {
        if (str[size - 1] == '\n') {
            str[size - 1] = 0;
            size--;
        }
        error = security_context_to_sid_live(value, size, &sid);
        if (error == -EAGAIN) {
            // stale type mark, use the dual parse
            error2 = security_context_to_sid_with_policy(backup_sepolicy, value, size, &tmp_sid, SECSID_NULL,
                                                         GFP_KERNEL, &sid, &error);
            if (error2 || error)
                return error2 ?: error;
        } else if (error) {
            return error;
        }
    }

    new = prepare_creds();
    if (!new)
        return -ENOMEM;

    /* Permission checking based on the specified context is
	   performed during the actual operation (execve,
	   open/mkdir/...), when we know the full context of the
	   operation.  See selinux_bprm_creds_for_exec for the execve
	   checks and may_create for the file creation checks. The
	   operation will then fail if the context is not permitted. */
    tsec = selinux_cred(new);
    error = -EINVAL;
    if (sid == 0)
        goto abort_change;

    if (!current_is_single_threaded()) {
        error = ksu_security_bounded_transition_compat(tsec->sid, sid);
        if (error)
            goto abort_change;
    }

    /* Check permissions for the transition. */
    error = ksu_avc_has_perm_compat(tsec->sid, sid, SECCLASS_PROCESS, PROCESS__DYNTRANSITION, NULL);
    if (error)
        goto abort_change;

    /* Check for ptracing, and update the task SID if ok.
        Otherwise, leave SID unchanged and fail. */
    ptsid = ptrace_parent_sid();
    if (ptsid != 0) {
        error = ksu_avc_has_perm_compat(ptsid, sid, SECCLASS_PROCESS, PROCESS__PTRACE, NULL);
        if (error)
            goto abort_change;
    }

    tsec->sid = sid;

    commit_creds(new);
    return size;

abort_change:
    abort_creds(new);
    return error;

call_orig:
    return ((setprocattr_fn)selinux_setprocattr_hook.original)(name, value, size);
}

static int (*inode_doinit_with_dentry_fn)(struct inode *inode, struct dentry *dentry);
static struct ksu_lsm_hook selinux_inode_setxattr_hook;
typedef typeof(((union security_list_options *)0)->inode_setxattr) inode_setxattr_fn;

#if defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF) && LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
static u32 selinux_hide_sb_blob_offset;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
static int __nocfi my_inode_setxattr(struct mnt_idmap *idmap, struct dentry *dentry, const char *name,
                                     const void *value, size_t size, int flags)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
static int __nocfi my_inode_setxattr(struct user_namespace *mnt_userns, struct dentry *dentry, const char *name,
                                     const void *value, size_t size, int flags)
#else
static int __nocfi my_inode_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags)
#endif
{
    struct inode *inode = d_backing_inode(dentry);
    struct inode_security_struct *isec;
    struct superblock_security_struct *sbsec;
    struct common_audit_data ad;
    u32 sid = current_sid(), newsid, backup_sid;
    int rc, backup_rc;

    if (likely(current_uid().val < 10000) || strcmp(name, XATTR_NAME_SELINUX) || !ksu_selinux_initialized_compat())
        goto call_orig;

    // Keep native force parsing for callers with CAP_MAC_ADMIN
    if (!cap_capable(current_cred(), &init_user_ns, CAP_MAC_ADMIN, CAP_OPT_NOAUDIT))
        goto call_orig;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 13, 0)
    sbsec = inode->i_sb->s_security;
#elif defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF)
    sbsec = (void *)((char *)inode->i_sb->s_security + selinux_hide_sb_blob_offset);
#else
    sbsec = selinux_superblock(inode->i_sb);
#endif
    if (!(sbsec->flags & SBLABEL_MNT))
        return -EOPNOTSUPP;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    if (!inode_owner_or_capable(idmap, inode))
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    if (!inode_owner_or_capable(mnt_userns, inode))
#else
    if (!inode_owner_or_capable(inode))
#endif
        return -EPERM;

    isec = ksu_selinux_inode(inode);
    if (IS_ERR_OR_NULL(isec))
        return isec ? PTR_ERR(isec) : -EINVAL;
    // Match backing_inode_security: keep old label if reload fails
    if (READ_ONCE(isec->initialized) != LABEL_INITIALIZED)
        inode_doinit_with_dentry_fn(inode, dentry);

    ad.type = LSM_AUDIT_DATA_DENTRY;
    ad.u.dentry = dentry;
    rc = ksu_avc_has_perm_compat(sid, isec->sid, isec->sclass, FILE__RELABELFROM, &ad);
    if (rc)
        return rc;

    // Keep context parsing after native relabelfrom check
    rc = security_context_to_sid_live(value, size, &newsid);
    if (rc == -EAGAIN) {
        backup_rc = security_context_to_sid_with_policy(backup_sepolicy, value, size, &backup_sid, SECSID_NULL,
                                                        GFP_KERNEL, &newsid, &rc);
        rc = backup_rc ?: rc;
    }

    if (rc == -EINVAL) {
        struct audit_buffer *ab;
        size_t audit_size = value ? size : 0;

        // Strip only final NUL so embedded NUL bytes remain in audit log
        if (audit_size && ((const char *)value)[audit_size - 1] == '\0')
            audit_size--;
        ab = audit_log_start(audit_context(), GFP_ATOMIC, AUDIT_SELINUX_ERR);
        if (ab) {
            audit_log_format(ab, "op=setxattr invalid_context=");
            audit_log_n_untrustedstring(ab, value, audit_size);
            audit_log_end(ab);
        }
    }
    if (rc)
        return rc;

    rc = ksu_avc_has_perm_compat(sid, newsid, isec->sclass, FILE__RELABELTO, &ad);
    if (rc)
        return rc;
    rc = ksu_security_validate_transition_compat(isec->sid, newsid, sid, isec->sclass);
    if (rc)
        return rc;
    return ksu_avc_has_perm_compat(newsid, sbsec->sid, SECCLASS_FILESYSTEM, FILESYSTEM__ASSOCIATE, &ad);

call_orig:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    return ((inode_setxattr_fn)selinux_inode_setxattr_hook.original)(idmap, dentry, name, value, size, flags);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
    return ((inode_setxattr_fn)selinux_inode_setxattr_hook.original)(mnt_userns, dentry, name, value, size, flags);
#else
    return ((inode_setxattr_fn)selinux_inode_setxattr_hook.original)(dentry, name, value, size, flags);
#endif
}

// Reject mismatched vendor hook signatures at build time
static_assert(__same_type(&my_inode_setxattr, (inode_setxattr_fn)NULL));
static struct ksu_lsm_hook selinux_inode_setxattr_hook =
    KSU_LSM_HOOK_INIT(inode_setxattr, "selinux_inode_setxattr", my_inode_setxattr, 0);

static int selinux_hide_init_setxattr()
{
#if defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF) && LINUX_VERSION_CODE >= KERNEL_VERSION(5, 13, 0)
    struct ksu_btf_field field;
    s32 offset;
    int ret;

    ret = ksu_btf_field("lsm_blob_sizes", "lbs_superblock", BTF_KIND_INT, sizeof(int), &field);
    if (ret)
        return ret;
    ret = copy_from_kernel_nofault(&offset, (const char *)&selinux_blob_sizes + field.offset, sizeof(offset));
    if (ret)
        return ret;
    if (offset < 0 || offset % __alignof__(struct superblock_security_struct))
        return -EINVAL;
    selinux_hide_sb_blob_offset = offset;
#endif

    inode_doinit_with_dentry_fn = ksu_resolve_symbol_for_functable_hook("inode_doinit_with_dentry");
    // Set original before publishing callback to other CPUs
    selinux_inode_setxattr_hook.original = ksu_resolve_symbol_for_functable_hook("selinux_inode_setxattr");
    if (!inode_doinit_with_dentry_fn || !selinux_inode_setxattr_hook.original)
        return -ENOENT;
    return 0;
}

static DEFINE_STATIC_KEY_FALSE(fake_status_initialize_key);
static struct page *fake_status = NULL;

static void initialize_fake_status()
{
    mutex_lock(&selinux_state.status_lock);
    if (fake_status)
        goto out;
    if (!selinux_state.status_page) {
        pr_warn("initialize_fake_status: status_page not exist\n");
        goto out;
    }

    struct selinux_kernel_status *status = page_address(selinux_state.status_page);
    if (!status->enforcing && !ksu_late_loaded) {
        pr_warn("initialize_fake_status: skip not enforcing\n");
        goto out;
    }

    struct page *new_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
    if (!new_page) {
        pr_err("initialize_fake_status: failed to allocate page\n");
        goto out;
    }

    struct selinux_kernel_status *new_status = page_address(new_page);
    memcpy(new_status, status, sizeof(*status));
    if (ksu_late_loaded) {
        // In late_load mode the loader may have reloaded sepolicy before us,
        // so the captured page is not stock. Serve what a stock boot ends
        // with instead: creation sentinel below 6.10, one load plus one
        // setenforce above.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
        new_status->sequence = 4;
        new_status->policyload = 1;
#else
        new_status->sequence = 0;
        new_status->policyload = 0;
#endif
        if (!new_status->enforcing) {
            new_status->enforcing = 1;
        }
    }

    fake_status = new_page;
    pr_info("initialize_fake_status initialized: sequence=%d, policyload=%d, enforcing=%d\n", new_status->sequence,
            new_status->policyload, new_status->enforcing);

out:
    mutex_unlock(&selinux_state.status_lock);
}

typedef int (*sel_open_handle_status_fn)(struct inode *inode, struct file *filp);
static sel_open_handle_status_fn orig_sel_open_handle_status, *sel_open_handle_status_slot;
static int my_sel_open_handle_status(struct inode *inode, struct file *filp)
{
    if (likely(current_uid().val >= 10000 && ksu_selinux_hide_enabled)) {
        void *data = fake_status;
        if (data) {
            filp->private_data = data;
            return 0;
        }
    }

    int ret = orig_sel_open_handle_status(inode, filp);
    if (static_branch_unlikely(&fake_status_initialize_key) && !ret && !fake_status) {
        initialize_fake_status();
    }
    return ret;
}

static void hook_selinux_status_open();
static void ksu_selinux_hide_unhook();
static int ksu_selinux_hide_enable()
{
    int ret;
    pr_info("selinux_hide: init selinux hide\n");
    if (!backup_sepolicy) {
        pr_err("no backup sepolicy available, please save feature and reboot to retry!\n");
        return -EAGAIN;
    }
    ret = selinux_hide_init_setxattr();
    if (ret) {
        pr_err("selinux_hide: init: setxattr helpers err: %d\n", ret);
        return ret;
    }
    selinux_write_op = find_kernel_symbol_exact("write_op");
    if (!selinux_write_op) {
        pr_err("selinux_hide: no write_op found!\n");
        return -ENOSYS;
    }
    hook_selinux_status_open();

    security_dump_masked_av_fn = find_kernel_symbol_exact("security_dump_masked_av");
    if (!security_dump_masked_av_fn) {
        pr_warn("security_dump_masked_av not found!\n");
    }
    context_struct_compute_av_fn = find_kernel_symbol_exact("context_struct_compute_av");
    if (!context_struct_compute_av_fn) {
        pr_warn("context_struct_compute_av not found!\n");
    }

    context_write = &selinux_write_op[SEL_CONTEXT];
    pr_info("selinux_hide: context_write: 0x%lx [%pSb]\n", (unsigned long)*context_write, *context_write);
    write_op_fn my = my_write_context;
    orig_context_write = *context_write;
    // Keep backup once hooks may run, including failed enable attempts
    ksu_selinux_hide_keep_backup = true;
    ret = ksu_patch_text(context_write, &my, sizeof(my), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("selinux_hide: init: patch_text context_write err: %d\n", ret);
        goto unhook;
    }

    access_write = &selinux_write_op[SEL_ACCESS];
    pr_info("selinux_hide: access_write: 0x%lx [%pSb]\n", (unsigned long)*access_write, *access_write);
    my = my_write_access;
    orig_access_write = *access_write;
    ret = ksu_patch_text(access_write, &my, sizeof(my), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("selinux_hide: init: patch_text access_write err: %d\n", ret);
        goto unhook;
    }

    ret = ksu_lsm_hook(&selinux_setprocattr_hook);
    if (ret) {
        pr_err("selinux_hide: init: selinux_setprocattr_hook err: %d\n", ret);
        goto unhook;
    }

    ret = ksu_lsm_hook(&selinux_inode_setxattr_hook);
    if (ret) {
        pr_err("selinux_hide: init: selinux_inode_setxattr_hook err: %d\n", ret);
        goto unhook;
    }

    return 0;

unhook:
    ksu_selinux_hide_unhook();
    return -ENOSYS;
}

static void ksu_selinux_hide_unhook()
{
    int ret;
    ksu_lsm_unhook(&selinux_inode_setxattr_hook);
    if (orig_context_write) {
        ret =
            ksu_patch_text(context_write, &orig_context_write, sizeof(orig_context_write), KSU_PATCH_TEXT_FLUSH_DCACHE);
        orig_context_write = NULL;
        if (ret) {
            pr_err("selinux_hide: exit: patch_text context_write err: %d\n", ret);
        }
    }
    if (orig_access_write) {
        ret = ksu_patch_text(access_write, &orig_access_write, sizeof(orig_access_write), KSU_PATCH_TEXT_FLUSH_DCACHE);
        orig_access_write = NULL;
        if (ret) {
            pr_err("selinux_hide: exit: patch_text access_write err: %d\n", ret);
        }
    }
    if (sel_open_handle_status_slot && orig_sel_open_handle_status) {
        ret = ksu_patch_text(sel_open_handle_status_slot, &orig_sel_open_handle_status,
                             sizeof(orig_sel_open_handle_status), KSU_PATCH_TEXT_FLUSH_DCACHE);
        orig_sel_open_handle_status = NULL;
        if (ret) {
            pr_err("selinux_hide: exit: patch_text sel_open_handle_status err: %d\n", ret);
        }
    }
    ksu_lsm_unhook(&selinux_setprocattr_hook);
}

static void ksu_selinux_hide_disable()
{
    pr_info("selinux_hide: exit selinux hide\n");
    ksu_selinux_hide_unhook();
}

static int selinux_hide_feature_get(u64 *value)
{
    *value = ksu_selinux_hide_enabled ? 1 : 0;
    return 0;
}

static int selinux_hide_feature_set(u64 value)
{
    bool enable = value != 0;
    int ret = 0;
    pr_info("selinux_hide: set to %d\n", enable);
    mutex_lock(&selinux_hide_mutex);
    ksu_selinux_hide_enabled = enable;
    if (enable) {
        if (!ksu_selinux_hide_running) {
            ret = ksu_selinux_hide_enable();
            if (!ret) {
                ksu_selinux_hide_running = true;
            }
        }
    } else {
        if (ksu_selinux_hide_running) {
            ksu_selinux_hide_disable();
            ksu_selinux_hide_running = false;
        }
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

void ksu_selinux_hide_handle_second_stage()
{
    initialize_fake_status();
    // https://github.com/torvalds/linux/blame/e8c2f9fdadee7cbc75134dc463c1e0d856d6e5c7/security/selinux/selinuxfs.c#L2014
    if (fake_status) {
        static_key_disable(&fake_status_initialize_key.key);
    } else {
        pr_warn("selinux_hide: fake status need late initialization\n");
    }
}

void ksu_selinux_hide_handle_post_fs_data()
{
    static_key_disable(&fake_status_initialize_key.key);
    if (!fake_status) {
        pr_err("selinux_hide: fake status is not initialized after post-fs-data!\n");
    }
}

static void hook_selinux_status_open()
{
    if (orig_sel_open_handle_status)
        return;
    if (!sel_open_handle_status_slot) {
        struct file_operations *ops = find_kernel_symbol_exact("sel_handle_status_ops");
        if (!ops) {
            pr_err("selinux_hide: sel_handle_status_ops not found, fake status will not work\n");
            return;
        }
        sel_open_handle_status_slot = &ops->open;
    }
    sel_open_handle_status_fn new_fn = my_sel_open_handle_status;
    orig_sel_open_handle_status = *sel_open_handle_status_slot;
    int ret = ksu_patch_text(sel_open_handle_status_slot, &new_fn, sizeof(new_fn), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("selinux_hide: init: patch_text sel_open_handle_status err: %d\n", ret);
        sel_open_handle_status_slot = NULL;
        orig_sel_open_handle_status = NULL;
    }
}

void __init ksu_selinux_hide_init()
{
    if (ksu_register_feature_handler(&selinux_hide_handler)) {
        pr_err("Failed to register selinux_hide feature handler\n");
    }
    if (ksu_late_loaded) {
        initialize_fake_status();
    } else {
        static_key_enable(&fake_status_initialize_key.key);
    }
    hook_selinux_status_open();
}

void __exit ksu_selinux_hide_exit()
{
    mutex_lock(&selinux_hide_mutex);
    if (ksu_selinux_hide_running) {
        ksu_selinux_hide_disable();
        ksu_selinux_hide_running = false;
    }
    mutex_unlock(&selinux_hide_mutex);
    ksu_unregister_feature_handler(KSU_FEATURE_SELINUX_HIDE);
    mutex_lock(&selinux_state.status_lock);
    if (fake_status)
        __free_page(fake_status);
    fake_status = NULL;
    mutex_unlock(&selinux_state.status_lock);
}

void ksu_selinux_hide_drop_backup_if_unused()
{
    mutex_lock(&selinux_hide_mutex);
    if (!ksu_selinux_hide_keep_backup && backup_sepolicy) {
        pr_info("selinux_hide is not enabled - drop backup_sepolicy\n");
        sidtab_destroy(backup_sepolicy->sidtab);
        kfree(backup_sepolicy->sidtab);
        ksu_destroy_sepolicy(backup_sepolicy);
        backup_sepolicy = NULL;
    }
    mutex_unlock(&selinux_hide_mutex);
}

/*
 * Caveat:  Mutates scontext.
 */
static int string_to_context_struct(struct policydb *pol, struct policydb *orig_pol, struct sidtab *sidtabp,
                                    struct sidtab *orig_sidtabp, char *scontext, const char *source, u32 source_len,
                                    struct context *ctx, struct context *orig_ctx, u32 policy_generation, u32 def_sid,
                                    int *orig_rc_p)
{
    struct role_datum *role, *orig_role;
    struct type_datum *typdatum, *orig_typdatum;
    struct user_datum *usrdatum, *orig_usrdatum;
    char *scontextp, *p, oldc;
    bool orig_valid = true;
    bool live_valid = true;
    int rc = -EINVAL, orig_rc = -EINVAL;
    int type_is_original;

    context_init(ctx);
    context_init(orig_ctx);

    // split fields once
    // look up both policies
    scontextp = scontext;

    /* Extract the user. */
    p = scontextp;
    while (*p && *p != ':')
        p++;

    if (*p == 0)
        goto out;

    *p++ = 0;

    usrdatum = symtab_search(&pol->p_users, scontextp);
    // Match native early return for an unknown user
    // Scanning later fields would add time based on their length
    if (!usrdatum)
        goto out;
    ctx->user = usrdatum->value;

    orig_usrdatum = symtab_search(&orig_pol->p_users, scontextp);
    if (orig_usrdatum)
        orig_ctx->user = orig_usrdatum->value;
    else
        orig_valid = false;

    /* Extract role. */
    scontextp = p;
    while (*p && *p != ':')
        p++;

    if (*p == 0)
        goto out;

    *p++ = 0;

    role = symtab_search(&pol->p_roles, scontextp);
    // Match native early return for an unknown role before scanning type
    if (!role)
        goto out;
    ctx->role = role->value;

    orig_role = symtab_search(&orig_pol->p_roles, scontextp);
    if (orig_role)
        orig_ctx->role = orig_role->value;
    else
        orig_valid = false;

    /* Extract type. */
    scontextp = p;
    while (*p && *p != ':')
        p++;
    oldc = *p;
    *p++ = 0;

    typdatum = symtab_search(&pol->p_types, scontextp);
    if (!typdatum || typdatum->attribute) {
        live_valid = false;
    } else {
        ctx->type = typdatum->value;
        // Type values stay stable when KSU copies the live policy
        type_is_original = ksu_type_is_original(orig_pol, policy_generation, typdatum->value);
        if (type_is_original < 0) {
            orig_typdatum = symtab_search(&orig_pol->p_types, scontextp);
            if (orig_typdatum && !orig_typdatum->attribute)
                orig_ctx->type = orig_typdatum->value;
            else
                orig_valid = false;
        } else if (type_is_original) {
            orig_ctx->type = typdatum->value;
        } else {
            orig_valid = false;
        }
    }

    if (!live_valid)
        goto out;

    rc = mls_context_to_sid(pol, oldc, p, ctx, sidtabp, def_sid);
    if (orig_valid) {
        if (oldc) {
            size_t offset = p - scontext;

            // restore MLS suffix for live policy
            memcpy(p, source + offset, source_len - offset);
            p[source_len - offset] = '\0';
        }
        // validate live context with live policy
        orig_rc = mls_context_to_sid(orig_pol, oldc, p, orig_ctx, orig_sidtabp, def_sid);
        if (!orig_rc && !policydb_context_isvalid(orig_pol, orig_ctx))
            orig_rc = -EINVAL;
    }

    if (!rc && !policydb_context_isvalid(pol, ctx))
        rc = -EINVAL;
out:
    if (rc) {
        context_destroy(ctx);
        // ignore live result if backup fails
        orig_rc = -EINVAL;
    }
    if (rc || orig_rc)
        context_destroy(orig_ctx);
    *orig_rc_p = orig_rc;
    return rc;
}

static int security_context_to_sid_with_policy(struct selinux_policy *policy, const char *scontext, u32 scontext_len,
                                               u32 *sid, u32 def_sid, gfp_t gfp_flags, u32 *orig_sid_p, int *orig_rc_p)
{
    struct selinux_policy *orig_policy;
    struct policydb *policydb, *orig_policydb;
    struct sidtab *sidtab, *orig_sidtab;
    char *scontext2;
    struct context context, orig_context;
    int rc = 0, orig_rc = -EINVAL;
    u32 orig_sid;

    /* An empty security context is never valid. */
    if (!scontext_len)
        return -EINVAL;

    /* Copy the string to allow changes and ensure a NUL terminator */
    scontext2 = kmemdup_nul(scontext, scontext_len, gfp_flags);
    if (!scontext2)
        return -ENOMEM;

    // removed: if (!selinux_initialized())
    *sid = SECSID_NULL;
    if (orig_sid_p)
        *orig_sid_p = SECSID_NULL;

    // removed: if (force)
retry:
    rcu_read_lock();
    policydb = &policy->policydb;
    sidtab = policy->sidtab;
    orig_policy = rcu_dereference(selinux_state.policy);
    orig_policydb = &orig_policy->policydb;
    orig_sidtab = orig_policy->sidtab;

    // parse both policies together
    orig_rc = -EINVAL;
    rc = string_to_context_struct(policydb, orig_policydb, sidtab, orig_sidtab, scontext2, scontext, scontext_len,
                                  &context, &orig_context, orig_policy->latest_granting, def_sid, &orig_rc);
    if (rc)
        goto out_unlock;

    // update backup sidtab first
    rc = sidtab_context_to_sid(sidtab, &context, sid);
    context_destroy(&context);
    if (rc) {
        if (!orig_rc)
            context_destroy(&orig_context);
        // backup sidtab is not reloaded
        orig_rc = rc;
        goto out_unlock;
    }

    if (orig_rc)
        goto out_unlock;

    // only sync valid contexts
    // sync to live sidtab
    orig_rc = sidtab_context_to_sid(orig_sidtab, &orig_context, &orig_sid);
    context_destroy(&orig_context);
    if (!orig_rc && orig_sid_p)
        *orig_sid_p = orig_sid;
    if (orig_rc == -ESTALE) {
        // retry after live policy reload
        rcu_read_unlock();
        memcpy(scontext2, scontext, scontext_len);
        scontext2[scontext_len] = '\0';
        goto retry;
    }
out_unlock:
    rcu_read_unlock();
    if (orig_rc_p)
        *orig_rc_p = orig_rc;
    kfree(scontext2);
    return rc;
}

/*
 * Caveat:  Mutates scontext.
 */
static int backup_string_to_context_struct(struct policydb *pol, struct policydb *live_pol, struct sidtab *sidtabp,
                                           char *scontext, struct context *ctx, u32 policy_generation, u32 def_sid)
{
    struct role_datum *role;
    struct type_datum *typdatum;
    struct user_datum *usrdatum;
    char *scontextp, *p, oldc;
    int rc = 0, type_is_original;

    context_init(ctx);

    /* Parse the security context. */

    rc = -EINVAL;
    scontextp = scontext;

    /* Extract the user. */
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

    /* Extract role. */
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

    /* Extract type. */
    scontextp = p;
    while (*p && *p != ':')
        p++;
    oldc = *p;
    *p++ = 0;

    // ksu types fail like unknown types
    typdatum = symtab_search(&pol->p_types, scontextp);
    if (!typdatum || typdatum->attribute)
        goto out;

    type_is_original = ksu_type_is_original(live_pol, policy_generation, typdatum->value);
    if (type_is_original <= 0) {
        rc = -EAGAIN;
        goto out;
    }

    ctx->type = typdatum->value;

    rc = mls_context_to_sid(pol, oldc, p, ctx, sidtabp, def_sid);
    if (rc)
        goto out;

    /* Check the validity of the new context. */
    // user, role and original type values are the same in both policies
    rc = -EINVAL;
    if (!policydb_context_isvalid(pol, ctx) || !policydb_context_isvalid(live_pol, ctx))
        goto out;
    rc = 0;
out:
    if (rc)
        context_destroy(ctx);
    return rc;
}

// parse once like security_context_to_sid(), -EAGAIN if the type mark is stale
static int security_context_to_sid_live(const char *scontext, u32 scontext_len, u32 *sid)
{
    struct selinux_policy *policy;
    struct context context;
    char *scontext2;
    u32 backup_sid;
    int rc;

    /* An empty security context is never valid. */
    if (!scontext_len)
        return -EINVAL;

    /* Copy the string to allow changes and ensure a NUL terminator */
    scontext2 = kmemdup_nul(scontext, scontext_len, GFP_KERNEL);
    if (!scontext2)
        return -ENOMEM;

    *sid = SECSID_NULL;
retry:
    rcu_read_lock();
    policy = rcu_dereference(selinux_state.policy);
    rc = backup_string_to_context_struct(&backup_sepolicy->policydb, &policy->policydb, backup_sepolicy->sidtab,
                                         scontext2, &context, policy->latest_granting, SECSID_NULL);
    if (rc)
        goto out_unlock;

    // update backup sidtab first
    rc = sidtab_context_to_sid(backup_sepolicy->sidtab, &context, &backup_sid);
    if (!rc)
        rc = sidtab_context_to_sid(policy->sidtab, &context, sid);
    context_destroy(&context);
    if (rc == -ESTALE) {
        // retry after live policy reload
        rcu_read_unlock();
        memcpy(scontext2, scontext, scontext_len);
        scontext2[scontext_len] = '\0';
        goto retry;
    }
out_unlock:
    rcu_read_unlock();
    kfree(scontext2);
    return rc;
}

/*
 * Write the security context string representation of
 * the context structure `context' into a dynamically
 * allocated string of the correct size.  Set `*scontext'
 * to point to this string and set `*scontext_len' to
 * the length of the string.
 */
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

    /* Compute the size of the context. */
    *scontext_len += strlen(sym_name(p, SYM_USERS, context->user - 1)) + 1;
    *scontext_len += strlen(sym_name(p, SYM_ROLES, context->role - 1)) + 1;
    *scontext_len += strlen(sym_name(p, SYM_TYPES, context->type - 1)) + 1;
    *scontext_len += mls_compute_context_len(p, context);

    if (!scontext)
        return 0;

    /* Allocate space for the context; caller must free this space. */
    scontextp = kmalloc(*scontext_len, GFP_ATOMIC);
    if (!scontextp)
        return -ENOMEM;
    *scontext = scontextp;

    /*
     * Copy the user name, role name and type name into the context.
     */
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

    // removed: if (!selinux_initialized())
    // removed: rcu lock
    policydb = &policy->policydb;
    sidtab = policy->sidtab;

    // removed: force
    entry = sidtab_search_entry(sidtab, sid);
    if (!entry) {
        pr_err("SELinux: %s:  unrecognized SID %d\n", __func__, sid);
        rc = -EINVAL;
        goto out_unlock;
    }
    // removed: only_invalid

    rc = sidtab_entry_to_string(policydb, sidtab, entry, scontext, scontext_len);

out_unlock:
    return rc;
}

static void avd_init(struct selinux_policy *policy, struct av_decision *avd)
{
    avd->allowed = 0;
    avd->auditallow = 0;
    avd->auditdeny = 0xffffffff;
    if (policy)
        avd->seqno = policy->latest_granting;
    else
        avd->seqno = 0;
    avd->flags = 0;
}

static void context_struct_compute_av(struct policydb *policydb, struct context *scontext, struct context *tcontext,
                                      u16 tclass, struct av_decision *avd, struct extended_perms *xperms);

/*
 * security_boundary_permission - drops violated permissions
 * on boundary constraint.
 */
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
        return; /* no masked permission */

    /* mask violated permissions */
    avd->allowed &= ~masked;

    /* audit masked permissions */
    if (security_dump_masked_av_fn)
        security_dump_masked_av_fn(policydb, scontext, tcontext, tclass, masked, "bounds");
}

/*
 * Return the boolean value of a constraint expression
 * when it is applied to the specified source and target
 * security contexts.
 *
 * xcontext is a special beast...  It is used by the validatetrans rules
 * only.  For these rules, scontext is the context before the transition,
 * tcontext is the context after the transition, and xcontext is the context
 * of the process performing the transition.  All other callers of
 * constraint_expr_eval should pass in NULL for xcontext.
 */
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
                s[++sp] = (val1 == val2);
                break;
            case CEXPR_NEQ:
                s[++sp] = (val1 != val2);
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
                if (!c) {
                    BUG();
                    return 0;
                }
            }
            if (e->attr & CEXPR_USER)
                val1 = c->user;
            else if (e->attr & CEXPR_ROLE)
                val1 = c->role;
            else if (e->attr & CEXPR_TYPE)
                val1 = c->type;
            else {
                BUG();
                return 0;
            }

            switch (e->op) {
            case CEXPR_EQ:
                s[++sp] = ebitmap_get_bit(&e->names, val1 - 1);
                break;
            case CEXPR_NEQ:
                s[++sp] = !ebitmap_get_bit(&e->names, val1 - 1);
                break;
            default:
                BUG();
                return 0;
            }
            break;
        default:
            BUG();
            return 0;
        }
    }

    BUG_ON(sp != 0);
    return s[0];
}

/*
 * Compute access vectors and extended permissions based on a context
 * structure pair for the permissions in a particular class.
 */
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

    /*
     * If a specific type enforcement rule was defined for
     * this permission check, then use it.
     */
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

            /* Check conditional av table for additional permissions */
            cond_compute_av(&policydb->te_cond_avtab, &avkey, avd, xperms);
        }
    }

    /*
     * Remove any permissions prohibited by a constraint (this includes
     * the MLS policy).
     */
    constraint = tclass_datum->constraints;
    while (constraint) {
        if ((constraint->permissions & (avd->allowed)) &&
            !constraint_expr_eval(policydb, scontext, tcontext, NULL, constraint->expr)) {
            avd->allowed &= ~(constraint->permissions);
        }
        constraint = constraint->next;
    }

    /*
     * If checking process transition permission and the
     * role is changing, then check the (current_role, new_role)
     * pair.
     */
    if (tclass == policydb->process_class && (avd->allowed & policydb->process_trans_perms) &&
        scontext->role != tcontext->role) {
        for (ra = policydb->role_allow; ra; ra = ra->next) {
            if (scontext->role == ra->role && tcontext->role == ra->new_role)
                break;
        }
        if (!ra)
            avd->allowed &= ~policydb->process_trans_perms;
    }

    /*
     * If the given source and target types have boundary
     * constraint, lazy checks have to mask any violated
     * permission and notice it to userspace via audit.
     */
    type_attribute_bounds_av(policydb, scontext, tcontext, tclass, avd);
}

static void __nocfi security_compute_av_user_with_policy(struct selinux_policy *policy, u32 ssid, u32 tsid, u16 tclass,
                                                         struct av_decision *avd)
{
    struct policydb *policydb;
    struct sidtab *sidtab;
    struct context *scontext = NULL, *tcontext = NULL;

    // remove: rcu lock
    avd_init(policy, avd);
    // remove: if (!selinux_initialized())

    policydb = &policy->policydb;
    sidtab = policy->sidtab;

    scontext = sidtab_search(sidtab, ssid);
    if (!scontext) {
        pr_err("SELinux: %s:  unrecognized SID %d\n", __func__, ssid);
        goto out;
    }

    /* permissive domain? */
    if (ebitmap_get_bit(&policydb->permissive_map, scontext->type))
        avd->flags |= AVD_FLAGS_PERMISSIVE;

    tcontext = sidtab_search(sidtab, tsid);
    if (!tcontext) {
        pr_err("SELinux: %s:  unrecognized SID %d\n", __func__, tsid);
        goto out;
    }

    if (unlikely(!tclass)) {
        if (policydb->allow_unknown)
            goto allow;
        goto out;
    }

    if (context_struct_compute_av_fn) {
        context_struct_compute_av_fn(policydb, scontext, tcontext, tclass, avd, NULL);
    } else {
        context_struct_compute_av(policydb, scontext, tcontext, tclass, avd, NULL);
    }
out:
    return;
allow:
    avd->allowed = 0xffffffff;
    goto out;
}

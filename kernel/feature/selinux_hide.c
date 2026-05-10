#include "selinux_hide.h"
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
// security/selinux/include/security.h
#include <security.h>
#include <ss/context.h>
#include <ss/services.h>
#include <ss/mls.h>
#include "avc.h"
#include "klog.h" // IWYU pragma: keep
#include "linux/kallsyms.h"
#include "objsec.h"
#include "hook/patch_memory.h"
#include "ksu.h"

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

// TODO: 12-5.10 ~ 14-6.1

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int security_context_to_sid_with_policy(struct selinux_policy *policy, const char *scontext, u32 scontext_len,
                                               u32 *sid, u32 def_sid, gfp_t gfp_flags);
static int security_sid_to_context_with_policy(struct selinux_policy *policy, u32 sid, char **scontext,
                                               u32 *scontext_len);
#endif

static write_op_fn *context_write;
static write_op_fn orig_context_write;

static ssize_t my_write_context(struct file *file, char *buf, size_t size)
{
    // apply to all app uids
    if (likely(current_uid().val < 10000)) {
        return orig_context_write(file, buf, size);
    }
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
        pr_err("SELinux: %s:  context size (%u) exceeds "
               "payload max\n",
               __func__, len);
        goto out;
    }
#else
    // TODO: move to static alloc
    struct selinux_state fake_state;
    length = avc_has_perm(&selinux_state, current_sid(), SECINITSID_SECURITY, SECCLASS_SECURITY,
                          SECURITY__CHECK_CONTEXT, NULL);
    if (length)
        goto out;
    fake_state.initialized = true;
    fake_state.policy = backup_sepolicy;
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
    // DEBUG!
    pr_info("selinux_hide: handle write_context for uid %d ctx: %s ret %ld\n", current_uid().val, buf, length);
    kfree(canon);
    return length;
}

// TODO: hook write_access

int ksu_selinux_hide_enable()
{
    pr_info("selinux_hide: init selinux hide\n");
    selinux_write_op = kallsyms_lookup_name("write_op");
    if (!selinux_write_op) {
        pr_err("selinux_hide: no write_op found!\n");
        return -ENOSYS;
    }

    context_write = &selinux_write_op[SEL_CONTEXT];
    pr_info("selinux_hide: context_write: 0x%lx [%pSb]\n", (unsigned long)*context_write, *context_write);
    write_op_fn my = my_write_context;
    orig_context_write = *context_write;
    int ret = ksu_patch_text(context_write, &my, sizeof(my), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("selinux_hide: init: patch_text err: %d\n", ret);
    }

    return ret;
}

void ksu_selinux_hide_disable()
{
    pr_info("selinux_hide: exit selinux hide\n");
    int ret =
        ksu_patch_text(context_write, &orig_context_write, sizeof(orig_context_write), KSU_PATCH_TEXT_FLUSH_DCACHE);
    if (ret) {
        pr_err("selinux_hide: exit: patch_text err: %d\n", ret);
    }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
/*
 * Caveat:  Mutates scontext.
 */
static int string_to_context_struct(struct policydb *pol, struct sidtab *sidtabp, char *scontext, struct context *ctx,
                                    u32 def_sid)
{
    struct role_datum *role;
    struct type_datum *typdatum;
    struct user_datum *usrdatum;
    char *scontextp, *p, oldc;
    int rc = 0;

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

    typdatum = symtab_search(&pol->p_types, scontextp);
    if (!typdatum || typdatum->attribute)
        goto out;

    ctx->type = typdatum->value;

    rc = mls_context_to_sid(pol, oldc, p, ctx, sidtabp, def_sid);
    if (rc)
        goto out;

    /* Check the validity of the new context. */
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

    /* An empty security context is never valid. */
    if (!scontext_len)
        return -EINVAL;

    /* Copy the string to allow changes and ensure a NUL terminator */
    scontext2 = kmemdup_nul(scontext, scontext_len, gfp_flags);
    if (!scontext2)
        return -ENOMEM;

    // removed: if (!selinux_initialized())
    *sid = SECSID_NULL;

    // removed: if (force)
    // removed: rcu lock
    policydb = &policy->policydb;
    sidtab = policy->sidtab;
    rc = string_to_context_struct(policydb, sidtab, scontext2, &context, def_sid);
    if (rc)
        goto out;
    rc = sidtab_context_to_sid(sidtab, &context, sid);
    // TODO: rc should not be frozen
    if (rc)
        goto out;
    // removed: if (rc == -ESTALE)
    context_destroy(&context);
out:
    kfree(scontext2);
    kfree(str);
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
    policy = rcu_dereference(selinux_state.policy);
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
#endif

#include "compat/jailbreak.h"

#if defined(CONFIG_KRETPROBES) && defined(__aarch64__)

#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/xarray.h>
#include <asm/ptrace.h>

static DEFINE_XARRAY(sel_hide_guard_xa);

static bool sel_hide_guard_enter(void)
{
    void *old;

    if (xa_load(&sel_hide_guard_xa, (unsigned long)current))
        return false;
    old = xa_store(&sel_hide_guard_xa, (unsigned long)current, current, GFP_ATOMIC);
    return !IS_ERR(old);
}

static void sel_hide_guard_exit(void)
{
    xa_erase(&sel_hide_guard_xa, (unsigned long)current);
}

static sel_hide_write_fn       rkp_my_ctx_fn;
static sel_hide_write_fn       rkp_my_acc_fn;
static sel_hide_setprocattr_fn rkp_my_spa_fn;
static sel_hide_open_fn        rkp_my_open_fn;

static ssize_t __nocfi sel_hide_ctx_tramp(struct file *file, char *buf, size_t size)
{
    ssize_t ret = rkp_my_ctx_fn(file, buf, size);
    sel_hide_guard_exit();
    return ret;
}

static ssize_t __nocfi sel_hide_acc_tramp(struct file *file, char *buf, size_t size)
{
    ssize_t ret = rkp_my_acc_fn(file, buf, size);
    sel_hide_guard_exit();
    return ret;
}

static int __nocfi sel_hide_spa_tramp(const char *name, void *value, size_t size)
{
    int ret = rkp_my_spa_fn(name, value, size);
    sel_hide_guard_exit();
    return ret;
}

static int __nocfi sel_hide_open_tramp(struct inode *inode, struct file *filp)
{
    int ret = rkp_my_open_fn(inode, filp);
    sel_hide_guard_exit();
    return ret;
}

static int sel_hide_ctx_pre(struct kprobe *p, struct pt_regs *regs)
{
    if (!sel_hide_guard_enter())
        return 0;
    instruction_pointer_set(regs, (unsigned long)sel_hide_ctx_tramp);
    return 1;
}

static int sel_hide_acc_pre(struct kprobe *p, struct pt_regs *regs)
{
    if (!sel_hide_guard_enter())
        return 0;
    instruction_pointer_set(regs, (unsigned long)sel_hide_acc_tramp);
    return 1;
}

static int sel_hide_spa_pre(struct kprobe *p, struct pt_regs *regs)
{
    if (!sel_hide_guard_enter())
        return 0;
    instruction_pointer_set(regs, (unsigned long)sel_hide_spa_tramp);
    return 1;
}

static int sel_hide_open_pre(struct kprobe *p, struct pt_regs *regs)
{
    if (!sel_hide_guard_enter())
        return 0;
    instruction_pointer_set(regs, (unsigned long)sel_hide_open_tramp);
    return 1;
}

static struct kprobe sel_hide_ctx_kprobe  = { .pre_handler = sel_hide_ctx_pre };
static struct kprobe sel_hide_acc_kprobe  = { .pre_handler = sel_hide_acc_pre };
static struct kprobe sel_hide_spa_kprobe  = { .pre_handler = sel_hide_spa_pre };
static struct kprobe sel_hide_open_kprobe = { .pre_handler = sel_hide_open_pre };

static int sel_hide_register_probe(struct kprobe *kp, void *target)
{
    int ret;
    char namebuf[KSYM_NAME_LEN];

    kp->addr = (kprobe_opcode_t *)target;
    ret = register_kprobe(kp);
    if (ret == -EINVAL && kallsyms_lookup((unsigned long)target, NULL, NULL, NULL, namebuf)) {
        pr_warn("selinux_hide: addr kprobe on %s rejected (%d), retrying symbol-based\n", namebuf, ret);
        kp->addr = NULL;
        kp->symbol_name = namebuf;
        ret = register_kprobe(kp);
        kp->symbol_name = NULL;
    }
    if (ret)
        kp->addr = NULL;
    return ret;
}

int selinux_hide_rkp_init(sel_hide_write_fn orig_ctx, sel_hide_write_fn my_ctx,
                           sel_hide_write_fn orig_acc, sel_hide_write_fn my_acc,
                           sel_hide_setprocattr_fn orig_spa, sel_hide_setprocattr_fn my_spa)
{
    int ret;

    rkp_my_ctx_fn = my_ctx;
    rkp_my_acc_fn = my_acc;
    rkp_my_spa_fn = my_spa;

    ret = sel_hide_register_probe(&sel_hide_ctx_kprobe, orig_ctx);
    if (ret) {
        pr_err("selinux_hide: context_write kprobe failed: %d\n", ret);
        return ret;
    }

    ret = sel_hide_register_probe(&sel_hide_acc_kprobe, orig_acc);
    if (ret) {
        pr_err("selinux_hide: access_write kprobe failed: %d\n", ret);
        goto unregister_ctx;
    }

    ret = sel_hide_register_probe(&sel_hide_spa_kprobe, orig_spa);
    if (ret) {
        pr_err("selinux_hide: setprocattr kprobe failed: %d\n", ret);
        goto unregister_acc;
    }

    pr_info("KSU: selinux_hide RKP kprobes registered\n");
    return 0;

unregister_acc:
    unregister_kprobe(&sel_hide_acc_kprobe);
    sel_hide_acc_kprobe.addr = NULL;
unregister_ctx:
    unregister_kprobe(&sel_hide_ctx_kprobe);
    sel_hide_ctx_kprobe.addr = NULL;
    return ret;
}

void selinux_hide_rkp_exit(void)
{
    if (sel_hide_spa_kprobe.addr) {
        unregister_kprobe(&sel_hide_spa_kprobe);
        sel_hide_spa_kprobe.addr = NULL;
    }
    if (sel_hide_acc_kprobe.addr) {
        unregister_kprobe(&sel_hide_acc_kprobe);
        sel_hide_acc_kprobe.addr = NULL;
    }
    if (sel_hide_ctx_kprobe.addr) {
        unregister_kprobe(&sel_hide_ctx_kprobe);
        sel_hide_ctx_kprobe.addr = NULL;
    }
}

int selinux_hide_rkp_hook_status_open(sel_hide_open_fn orig, sel_hide_open_fn my)
{
    int ret;

    rkp_my_open_fn = my;
    ret = sel_hide_register_probe(&sel_hide_open_kprobe, orig);
    if (ret)
        pr_err("selinux_hide: status_open kprobe failed: %d\n", ret);
    return ret;
}

void selinux_hide_rkp_unhook_status_open(void)
{
    if (sel_hide_open_kprobe.addr) {
        unregister_kprobe(&sel_hide_open_kprobe);
        sel_hide_open_kprobe.addr = NULL;
    }
}

#endif /* CONFIG_KRETPROBES && __aarch64__ */

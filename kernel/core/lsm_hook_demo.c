#include <linux/export.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/moduleparam.h>

#include "klog.h" // IWYU pragma: keep
#include "hook/lsm_hook.h"

// workaround for A12-5.10 kernel
// Some third-party kernel (e.g. linegaeOS) uses wrong toolchain, which supports
// CC_HAVE_STACKPROTECTOR_SYSREG while gki's toolchain doesn't.
// Therefore, ksu lkm, which uses gki toolchain, requires this __stack_chk_guard,
// while those third-party kernel can't provide.
// Thus, we manually provide it instead of using kernel's
#if defined(CONFIG_STACKPROTECTOR) &&                                                                                  \
    (defined(CONFIG_ARM64) && defined(MODULE) && !defined(CONFIG_STACKPROTECTOR_PER_TASK))
#include <linux/stackprotector.h>
#include <linux/random.h>
unsigned long __stack_chk_guard __ro_after_init __attribute__((visibility("hidden")));

__attribute__((no_stack_protector)) void ksu_setup_stack_chk_guard()
{
    unsigned long canary;

    /* Try to get a semi random initial value. */
    get_random_bytes(&canary, sizeof(canary));
    canary ^= LINUX_VERSION_CODE;
    canary &= CANARY_MASK;
    __stack_chk_guard = canary;
}

__attribute__((naked)) int __init kernelsu_init_early(void)
{
    asm("mov x19, x30;\n"
        "bl ksu_setup_stack_chk_guard;\n"
        "mov x30, x19;\n"
        "b kernelsu_init;\n");
}
#define NEED_OWN_STACKPROTECTOR 1
#else
#define NEED_OWN_STACKPROTECTOR 0
#endif

static int my_inode_create(struct inode *dir, struct dentry *dentry, umode_t mode);

struct ksu_lsm_hook my_hook = KSU_LSM_HOOK_BPF_INIT(inode_create, inode_create, my_inode_create);

typedef int inode_create_fn(struct inode *dir, struct dentry *dentry, umode_t mode);

static int my_inode_create(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    pr_info("my_inode_create: %s\n", (const char *)(dentry->d_name.name ?: (const unsigned char *)"NULL"));
    return ((inode_create_fn *)my_hook.original)(dir, dentry, mode);
}

void lsm_hook_demo_init()
{
    ksu_lsm_hook_init();
    int ret = ksu_register_lsm_hook(&my_hook);
    if (!ret) {
        pr_info("register lsm success\n");
    } else {
        pr_err("register lsm failed: %d\n", ret);
    }
    pr_info("registered lsm orig: 0x%lx [%pSb]\n", (unsigned long)my_hook.original, my_hook.original);
}

void lsm_hook_demo_exit()
{
    ksu_lsm_hook_exit();
}

int __init kernelsu_init(void)
{
    lsm_hook_demo_init();

    return 0;
}

void kernelsu_exit(void)
{
    lsm_hook_demo_exit();
}

#if NEED_OWN_STACKPROTECTOR
module_init(kernelsu_init_early);
#else
module_init(kernelsu_init);
#endif
module_exit(kernelsu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weishu");
MODULE_DESCRIPTION("Android KernelSU");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0)
MODULE_IMPORT_NS("VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver");
#else
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

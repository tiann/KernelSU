#include "linux/module.h"
#include "linux/workqueue.h"

#include "allowlist.h"
#include "arch.h"
#include "core.h"
#include "ksu.h"
#include "uid_observer.h"

static struct workqueue_struct *ksu_workqueue;

void ksu_queue_work(struct work_struct *work)
{
	queue_work(ksu_workqueue, work);
}

extern void enable_sucompat();
extern void enable_ksud();

int __init kernelsu_init(void)
{
#ifdef CONFIG_KSU_DEBUG
	pr_alert("You are running DEBUG version of KernelSU");
#endif

	ksu_core_init();

	ksu_workqueue = alloc_workqueue("kernelsu_work_queue", 0, 0);

	ksu_allowlist_init();

	ksu_uid_observer_init();

#ifdef CONFIG_KPROBES
	enable_sucompat();
	enable_ksud();
#else
#warning("KPROBES is disabled, KernelSU may not work, please check https://kernelsu.org/guide/how-to-integrate-for-non-gki.html")
#endif

	return 0;
}

void kernelsu_exit(void)
{
	ksu_allowlist_exit();

	destroy_workqueue(ksu_workqueue);

	ksu_core_exit();
}

module_init(kernelsu_init);
module_exit(kernelsu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weishu");
MODULE_DESCRIPTION("Android KernelSU");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
MODULE_IMPORT_NS(
	VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver); // 5+才需要导出命名空间
#endif

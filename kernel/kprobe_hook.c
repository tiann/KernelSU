#include <linux/version.h>
#include <linux/kprobes.h>

#include "arch.h"
#include "ksu.h"
#include "klog.h"

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 16, 0)
	struct pt_regs *real_regs = (struct pt_regs *)PT_REGS_PARM1(regs);
#else
	struct pt_regs *real_regs = regs;
#endif
	int option = (int)PT_REGS_PARM1(real_regs);
	unsigned long arg2 = (unsigned long)PT_REGS_PARM2(real_regs);
	unsigned long arg3 = (unsigned long)PT_REGS_PARM3(real_regs);
	unsigned long arg4 = (unsigned long)PT_REGS_PARM4(real_regs);
	unsigned long arg5 = (unsigned long)PT_REGS_PARM5(real_regs);

	return ksu_handle_prctl(option, arg2, arg3, arg4, arg5);
}

static struct kprobe kp = {
	.symbol_name = PRCTL_SYMBOL,
	.pre_handler = handler_pre,
};

__maybe_unused int ksu_kprobe_init()
{
	int rc = 0;
	rc = register_kprobe(&kp);

	if (rc) {
		pr_info("prctl kprobe failed: %d, please check your kernel config.\n",
			rc);
		return rc;
	}

    return rc;
}

__maybe_unused int ksu_kprobe_exit() {
	unregister_kprobe(&kp);
    return 0;
}
#include "linux/version.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
#include "linux/types.h"
#ifdef CONFIG_KPROBES
#include "linux/kprobes.h"
#endif
#include "avc_ss.h"

#include "selinux.h"
#include "../klog.h" // IWYU pragma: keep
#include "../arch.h"
int ksu_handle_security_bounded_transition(u32 *old_sid, u32 *new_sid) {
	u32 init_sid, su_sid;
	int error;

	if (!ss_initialized)
		return 0;

	/* domain unchanged */
	if (*old_sid == *new_sid)
		return 0;

	const char *init_domain = INIT_DOMAIN;
	const char *su_domain = KERNEL_SU_DOMAIN;

	error = security_secctx_to_secid(init_domain, strlen(init_domain), &init_sid);
	if (error) {
		pr_warn("cannot get sid of init context, err %d\n", error);
		return 0;
	}

	error = security_secctx_to_secid(su_domain, strlen(su_domain), &su_sid);
	if (error) {
		pr_warn("cannot get sid of su context, err %d\n", error);
		return 0;
	}

	if (*old_sid == init_sid && *new_sid == su_sid) {
		pr_info("init to su transition found\n");
		*old_sid = *new_sid;  // make the original func return 0
	}

	return 0;
}

#ifdef CONFIG_KPROBES
static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
	u32 *old_sid = (u32 *)&PT_REGS_PARM1(regs);
	u32 *new_sid = (u32 *)&PT_REGS_PARM2(regs);

	return ksu_handle_security_bounded_transition(old_sid, new_sid);
}

static struct kprobe kp = {
	.symbol_name = "security_bounded_transition",
	.pre_handler = handler_pre,
};

// selinux_compat: make ksud init trigger work for kernel < 4.14
void ksu_enable_selinux_compat() {
	int ret;

	ret = register_kprobe(&kp);
	pr_info("selinux_compat: kp: %d\n", ret);
}
#endif
#endif

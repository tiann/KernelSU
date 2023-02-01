#include "linux/types.h"
#ifdef CONFIG_KPROBES
#include "linux/kprobes.h"
#endif
#include "linux/version.h"
#include "avc_ss.h"

#include "kernel_compat.h"
#include "selinux.h"
#include "../klog.h" // IWYU pragma: keep
#include "../arch.h"
#include "objsec.h"
#include "../kernel_compat.h"

#ifndef FILP_OPEN_WORKS_IN_WORKER
static struct group_info root_groups = { .usage = ATOMIC_INIT(2) };

bool ksu_save_cred(struct ksu_cred_t *ksu_cred) {
	struct cred *cred;
	struct task_security_struct *tsec;

	cred = (struct cred *)__task_cred(current);

	tsec = cred->security;
	if (!tsec) {
		pr_err("tsec == NULL!\n");
		return false;
	}

	ksu_cred->uid = cred->uid;
	ksu_cred->gid = cred->gid;
	ksu_cred->suid = cred->suid;
	ksu_cred->euid = cred->euid;
	ksu_cred->egid = cred->egid;
	ksu_cred->fsuid = cred->fsuid;
	ksu_cred->fsgid = cred->fsgid;
	ksu_cred->cap_inheritable = cred->cap_inheritable;
	ksu_cred->cap_permitted = cred->cap_permitted;
	ksu_cred->cap_effective = cred->cap_effective;
	ksu_cred->cap_bset = cred->cap_bset;
	ksu_cred->cap_ambient = cred->cap_ambient;

#if defined(CONFIG_GENERIC_ENTRY) &&                                           \
	LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	ksu_cred->thread_info.flags = current_thread_info()->syscall_work;
#else
	ksu_cred->thread_info.flags = current_thread_info()->flags;
#endif
	ksu_cred->seccomp.mode = current->seccomp.mode;
	ksu_cred->seccomp.filter = current->seccomp.filter;

	ksu_cred->group_info = cred->group_info;

	ksu_cred->tsec.sid = tsec->sid;
	ksu_cred->tsec.create_sid = tsec->create_sid;
	ksu_cred->tsec.keycreate_sid = tsec->keycreate_sid;
	ksu_cred->tsec.sockcreate_sid = tsec->sockcreate_sid;

	return true;
}

bool ksu_restore_cred(struct ksu_cred_t *ksu_cred) {
	struct cred *cred;
	struct task_security_struct *tsec;

	cred = (struct cred *)__task_cred(current);

	tsec = cred->security;
	if (!tsec) {
		pr_err("tsec == NULL!\n");
		return false;
	}

	cred->uid = ksu_cred->uid;
	cred->gid = ksu_cred->gid;
	cred->suid = ksu_cred->suid;
	cred->euid = ksu_cred->euid;
	cred->egid = ksu_cred->egid;
	cred->fsuid = ksu_cred->fsuid;
	cred->fsgid = ksu_cred->fsgid;
	cred->cap_inheritable = ksu_cred->cap_inheritable;
	cred->cap_permitted = ksu_cred->cap_permitted;
	cred->cap_effective = ksu_cred->cap_effective;
	cred->cap_bset = ksu_cred->cap_bset;
	cred->cap_ambient = ksu_cred->cap_ambient;

#if defined(CONFIG_GENERIC_ENTRY) &&                                           \
	LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	current_thread_info()->syscall_work = ksu_cred->thread_info.flags;
#else
	current_thread_info()->flags = ksu_cred->thread_info.flags;
#endif
	current->seccomp.mode = ksu_cred->seccomp.mode;
	current->seccomp.filter = ksu_cred->seccomp.filter;

	cred->group_info = ksu_cred->group_info;

	tsec->sid = ksu_cred->tsec.sid;
	tsec->create_sid = ksu_cred->tsec.create_sid;
	tsec->keycreate_sid = ksu_cred->tsec.keycreate_sid;
	tsec->sockcreate_sid = ksu_cred->tsec.sockcreate_sid;

	return true;
}

bool ksu_tmp_root_begin(void)
{
	struct ksu_cred_t ksu_cred;
	struct cred *cred;
	int error;
	u32 sid;

	cred = (struct cred *)__task_cred(current);

	const char *su_domain = KERNEL_SU_DOMAIN;

	error = security_secctx_to_secid(su_domain, strlen(su_domain), &sid);
	pr_info("error: %d, sid: %d\n", error, sid);

	if (error)
		return false;

	memset(&ksu_cred.uid, 0, sizeof(ksu_cred.uid));
	memset(&ksu_cred.gid, 0, sizeof(ksu_cred.gid));
	memset(&ksu_cred.suid, 0, sizeof(ksu_cred.suid));
	memset(&ksu_cred.euid, 0, sizeof(ksu_cred.euid));
	memset(&ksu_cred.egid, 0, sizeof(ksu_cred.egid));
	memset(&ksu_cred.fsuid, 0, sizeof(ksu_cred.fsuid));
	memset(&ksu_cred.fsgid, 0, sizeof(ksu_cred.fsgid));
	memset(&ksu_cred.cap_inheritable, 0xff, sizeof(ksu_cred.cap_inheritable));
	memset(&ksu_cred.cap_permitted, 0xff, sizeof(ksu_cred.cap_permitted));
	memset(&ksu_cred.cap_effective, 0xff, sizeof(ksu_cred.cap_effective));
	memset(&ksu_cred.cap_bset, 0xff, sizeof(ksu_cred.cap_bset));
	memset(&ksu_cred.cap_ambient, 0xff, sizeof(ksu_cred.cap_ambient));

	// disable seccomp
#if defined(CONFIG_GENERIC_ENTRY) &&                                           \
	LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	ksu_cred.thread_info.flags = current_thread_info()->syscall_work & ~SYSCALL_WORK_SECCOMP;
#else
	ksu_cred.thread_info.flags = current_thread_info()->flags & ~(TIF_SECCOMP | _TIF_SECCOMP);
#endif
	ksu_cred.seccomp.mode = 0;
	ksu_cred.seccomp.filter = NULL;

	// setgroup to root
	ksu_cred.group_info = get_group_info(&root_groups);

	ksu_cred.tsec.sid = sid;
	ksu_cred.tsec.create_sid = 0;
	ksu_cred.tsec.keycreate_sid = 0;
	ksu_cred.tsec.sockcreate_sid = 0;

	return ksu_restore_cred(&ksu_cred);
}

void ksu_tmp_root_end(void)
{
	struct cred *cred;

	cred = (struct cred *)__task_cred(current);

	if (cred->group_info)
		put_group_info(cred->group_info);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
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

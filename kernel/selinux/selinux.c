#include "selinux.h"
#include "objsec.h"
#include "linux/version.h"
#include "../klog.h" // IWYU pragma: keep
#if ((KERNEL_VERSION(4, 14, 0) <= LINUX_VERSION_CODE) && (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 163))) || (LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 337))
#include "avc.h"
#endif

#define KERNEL_SU_DOMAIN "u:r:su:s0"

static u32 ksu_sid;

static int transive_to_domain(const char *domain)
{
	struct cred *cred;
	struct task_security_struct *tsec;
	u32 sid;
	int error;

	cred = (struct cred *)__task_cred(current);

	tsec = cred->security;
	if (!tsec) {
		pr_err("tsec == NULL!\n");
		return -1;
	}

	error = security_secctx_to_secid(domain, strlen(domain), &sid);
	pr_info("error: %d, sid: %d\n", error, sid);
	if (!error) {
		if (!ksu_sid)
			ksu_sid = sid;

		tsec->sid = sid;
		tsec->create_sid = 0;
		tsec->keycreate_sid = 0;
		tsec->sockcreate_sid = 0;
	}
	return error;
}

void setup_selinux()
{
	if (transive_to_domain(KERNEL_SU_DOMAIN)) {
		pr_err("transive domain failed.");
		return;
	}

	/* we didn't need this now, we have change selinux rules when boot!
if (!is_domain_permissive) {
  if (set_domain_permissive() == 0) {
      is_domain_permissive = true;
  }
}*/
}

void setenforce(bool enforce)
{
#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 163)) || ((KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE) && (LINUX_VERSION_CODE  >= KERNEL_VERSION(4, 9, 337)))
	selinux_state.enforcing = enforce;
#else
	selinux_enforcing = enforce;
#endif
#endif
}

bool getenforce()
{
#ifdef CONFIG_SECURITY_SELINUX_DISABLE
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 163)) || ((KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE) && (LINUX_VERSION_CODE  >= KERNEL_VERSION(4, 9, 337)))
	if (selinux_state.disabled) {
#else
	if (selinux_disabled) {
#endif
		return false;
	}
#endif

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 163)) || ((KERNEL_VERSION(4, 10, 0) > LINUX_VERSION_CODE) && (LINUX_VERSION_CODE  >= KERNEL_VERSION(4, 9, 337)))
	return selinux_state.enforcing;
#else
	return selinux_enforcing;
#endif
#else
	return true;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 337)
/*
 * get the subjective security ID of the current task
 */
static inline u32 current_sid(void)
{
	const struct task_security_struct *tsec = current_security();

	return tsec->sid;
}
#endif

bool is_ksu_domain()
{
	return ksu_sid && current_sid() == ksu_sid;
}

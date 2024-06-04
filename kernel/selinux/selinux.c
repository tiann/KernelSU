#include "selinux.h"
#include "objsec.h"
#include "linux/version.h"
#include "../klog.h" // IWYU pragma: keep
#ifndef KSU_COMPAT_USE_SELINUX_STATE
#include "avc.h"
#endif

#define KERNEL_SU_DOMAIN "u:r:su:s0"

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
	if (error) {
		pr_info("security_secctx_to_secid %s -> sid: %d, error: %d\n",
			domain, sid, error);
	}
	if (!error) {
		tsec->sid = sid;
		tsec->create_sid = 0;
		tsec->keycreate_sid = 0;
		tsec->sockcreate_sid = 0;
	}
	return error;
}

void setup_selinux(const char *domain)
{
	if (transive_to_domain(domain)) {
		pr_err("transive domain failed.\n");
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
#ifdef KSU_COMPAT_USE_SELINUX_STATE
	selinux_state.enforcing = enforce;
#else
	selinux_enforcing = enforce;
#endif
#endif
}

bool getenforce()
{
#ifdef CONFIG_SECURITY_SELINUX_DISABLE
#ifdef KSU_COMPAT_USE_SELINUX_STATE
	if (selinux_state.disabled) {
#else
	if (selinux_disabled) {
#endif
		return false;
	}
#endif

#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
#ifdef KSU_COMPAT_USE_SELINUX_STATE
	return selinux_state.enforcing;
#else
	return selinux_enforcing;
#endif
#else
	return true;
#endif
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)) &&                         \
	!defined(KSU_COMPAT_HAS_CURRENT_SID)
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
	char *domain;
	u32 seclen;
	bool result;
	int err = security_secid_to_secctx(current_sid(), &domain, &seclen);
	if (err) {
		return false;
	}
	result = strncmp(KERNEL_SU_DOMAIN, domain, seclen) == 0;
	security_release_secctx(domain, seclen);
	return result;
}

bool is_zygote(void *sec)
{
	struct task_security_struct *tsec = (struct task_security_struct *)sec;
	if (!tsec) {
		return false;
	}
	char *domain;
	u32 seclen;
	bool result;
	int err = security_secid_to_secctx(tsec->sid, &domain, &seclen);
	if (err) {
		return false;
	}
	result = strncmp("u:r:zygote:s0", domain, seclen) == 0;
	security_release_secctx(domain, seclen);
	return result;
}

#define DEVPTS_DOMAIN "u:object_r:ksu_file:s0"

u32 ksu_get_devpts_sid()
{
	u32 devpts_sid = 0;
	int err = security_secctx_to_secid(DEVPTS_DOMAIN, strlen(DEVPTS_DOMAIN),
					   &devpts_sid);
	if (err) {
		pr_info("get devpts sid err %d\n", err);
	}
	return devpts_sid;
}

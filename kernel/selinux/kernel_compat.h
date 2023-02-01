#ifndef __KSU_H_SELINUX_KERNEL_COMPAT
#define __KSU_H_SELINUX_KERNEL_COMPAT

#include "linux/cred.h"
#include "linux/seccomp.h"

#include "../kernel_compat.h"

#ifndef FILP_OPEN_WORKS_IN_WORKER
struct ksu_cred_t {
	kuid_t uid;
	kgid_t gid;
	kuid_t suid;
	kuid_t euid;
	kgid_t egid;
	kuid_t fsuid;
	kgid_t fsgid;
	kernel_cap_t cap_inheritable;
	kernel_cap_t cap_permitted;
	kernel_cap_t cap_effective;
	kernel_cap_t cap_bset;
	kernel_cap_t cap_ambient;
	struct {
		unsigned long flags;
	} thread_info;
	struct {
		int mode;
		struct seccomp_filter *filter;
	} seccomp;
	struct group_info *group_info;
	struct {
		u32 sid;
		u32 create_sid;
		u32 keycreate_sid;
		u32 sockcreate_sid;
	} tsec;
};

bool ksu_save_cred(struct ksu_cred_t *ksu_cred);
bool ksu_restore_cred(struct ksu_cred_t *ksu_cred);

bool ksu_tmp_root_begin(void);
void ksu_tmp_root_end(void);
#endif
#endif

#ifndef __KSU_H_APP_PROFILE
#define __KSU_H_APP_PROFILE

#include <linux/types.h>

// Forward declarations
struct cred;

#define KSU_APP_PROFILE_VER 2
#define KSU_MAX_PACKAGE_NAME 256
// NGROUPS_MAX for Linux is 65535 generally, but we only supports 32 groups.
#define KSU_MAX_GROUPS 32
#define KSU_SELINUX_DOMAIN 64

struct root_profile {
	int32_t uid;
	int32_t gid;

	int32_t groups_count;
	int32_t groups[KSU_MAX_GROUPS];

	// kernel_cap_t is u32[2] for capabilities v3
	struct {
		u64 effective;
		u64 permitted;
		u64 inheritable;
	} capabilities;

	char selinux_domain[KSU_SELINUX_DOMAIN];

	int32_t namespaces;
};

struct non_root_profile {
	bool umount_modules;
};

struct app_profile {
	// It may be utilized for backward compatibility, although we have never explicitly made any promises regarding this.
	u32 version;

	// this is usually the package of the app, but can be other value for special apps
	char key[KSU_MAX_PACKAGE_NAME];
	int32_t current_uid;
	bool allow_su;

	union {
		struct {
			bool use_default;
			char template_name[KSU_MAX_PACKAGE_NAME];

			struct root_profile profile;
		} rp_config;

		struct {
			bool use_default;

			struct non_root_profile profile;
		} nrp_config;
	};
};

// Escalate current process to root with the appropriate profile
void escape_with_root_profile(void);

void escape_to_root_for_init(void);

#endif

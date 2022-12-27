#include "sepolicy.h"
#include "selinux.h"

#define KERNEL_SU_DOMAIN "su"
#define ALL NULL

void apply_kernelsu_rules()
{
	struct selinux_policy *policy;
	struct policydb *db;

	if (!getenforce()) {
		pr_info("SELinux permissive or disabled, don't apply rules.");
		return;
	}

	rcu_read_lock();
	policy = rcu_dereference(selinux_state.policy);
	db = &policy->policydb;

	permissive(db, KERNEL_SU_DOMAIN);
	typeattribute(db, KERNEL_SU_DOMAIN, "mlstrustedsubject");
	typeattribute(db, KERNEL_SU_DOMAIN, "netdomain");
	typeattribute(db, KERNEL_SU_DOMAIN, "bluetoothdomain");

	// allow all!
	allow(db, KERNEL_SU_DOMAIN, ALL, ALL, ALL);

	// allow us do any ioctl
	if (db->policyvers >= POLICYDB_VERSION_XPERMS_IOCTL) {
		allowxperm(db, KERNEL_SU_DOMAIN, ALL, "blk_file", ALL);
		allowxperm(db, KERNEL_SU_DOMAIN, ALL, "fifo_file", ALL);
		allowxperm(db, KERNEL_SU_DOMAIN, ALL, "chr_file", ALL);
	}

	// we need to save allowlist in /data/adb
	allow(db, "kernel", "adb_data_file", "dir", ALL);
	allow(db, "kernel", "adb_data_file", "file", ALL);
	// we may need to do mount on shell
	allow(db, "kernel", "shell_data_file", "file", ALL);

	// copied from Magisk rules
	// suRights
	allow(db, "servicemanager", KERNEL_SU_DOMAIN, "dir", "search");
	allow(db, "servicemanager", KERNEL_SU_DOMAIN, "dir", "read");
	allow(db, "servicemanager", KERNEL_SU_DOMAIN, "file", "open");
	allow(db, "servicemanager", KERNEL_SU_DOMAIN, "file", "read");
	allow(db, "servicemanager", KERNEL_SU_DOMAIN, "process", "getattr");
	allow(db, ALL, KERNEL_SU_DOMAIN, "process", "sigchld");

	// allowLog
	allow(db, "logd", KERNEL_SU_DOMAIN, "dir", "search");
	allow(db, "logd", KERNEL_SU_DOMAIN, "file", "read");
	allow(db, "logd", KERNEL_SU_DOMAIN, "file", "open");
	allow(db, "logd", KERNEL_SU_DOMAIN, "file", "getattr");

	// dumpsys
	allow(db, ALL, KERNEL_SU_DOMAIN, "fd", "use");
	allow(db, ALL, KERNEL_SU_DOMAIN, "fifo_file", "write");
	allow(db, ALL, KERNEL_SU_DOMAIN, "fifo_file", "read");
	allow(db, ALL, KERNEL_SU_DOMAIN, "fifo_file", "open");
	allow(db, ALL, KERNEL_SU_DOMAIN, "fifo_file", "getattr");

	// bootctl
	allow(db, "hwservicemanager", KERNEL_SU_DOMAIN, "dir", "search");
	allow(db, "hwservicemanager", KERNEL_SU_DOMAIN, "file", "read");
	allow(db, "hwservicemanager", KERNEL_SU_DOMAIN, "file", "open");
	allow(db, "hwservicemanager", KERNEL_SU_DOMAIN, "process", "getattr");

	// Allow all binder transactions
	allow(db, ALL, KERNEL_SU_DOMAIN, "binder", ALL);

	rcu_read_unlock();
}
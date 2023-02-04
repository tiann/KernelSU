#include "linux/cred.h"
#include "linux/err.h"
#include "linux/init.h"
#include "linux/kernel.h"
#include "linux/kprobes.h"
#include "linux/lsm_hooks.h"
#include "linux/printk.h"
#include "linux/uaccess.h"
#include "linux/uidgid.h"
#include "linux/version.h"

#include "linux/fs.h"
#include "linux/namei.h"
#include "linux/rcupdate.h"

#include "allowlist.h"
#include "arch.h"
#include "core_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "ksud.h"
#include "manager.h"
#include "selinux/selinux.h"
#include "uid_observer.h"
#include "kernel_compat.h"

extern int handle_sepolicy(unsigned long arg3, void __user *arg4);

static inline bool is_allow_su()
{
	if (is_manager()) {
		// we are manager, allow!
		return true;
	}
	return ksu_is_allow_uid(current_uid().val);
}

static struct group_info root_groups = { .usage = ATOMIC_INIT(2) };

void escape_to_root(void)
{
	struct cred *cred;

	cred = (struct cred *)__task_cred(current);

	memset(&cred->uid, 0, sizeof(cred->uid));
	memset(&cred->gid, 0, sizeof(cred->gid));
	memset(&cred->suid, 0, sizeof(cred->suid));
	memset(&cred->euid, 0, sizeof(cred->euid));
	memset(&cred->egid, 0, sizeof(cred->egid));
	memset(&cred->fsuid, 0, sizeof(cred->fsuid));
	memset(&cred->fsgid, 0, sizeof(cred->fsgid));
	memset(&cred->cap_inheritable, 0xff, sizeof(cred->cap_inheritable));
	memset(&cred->cap_permitted, 0xff, sizeof(cred->cap_permitted));
	memset(&cred->cap_effective, 0xff, sizeof(cred->cap_effective));
	memset(&cred->cap_bset, 0xff, sizeof(cred->cap_bset));
	memset(&cred->cap_ambient, 0xff, sizeof(cred->cap_ambient));

	// disable seccomp
#if defined(CONFIG_GENERIC_ENTRY) &&                                           \
	LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	current_thread_info()->syscall_work &= ~SYSCALL_WORK_SECCOMP;
#else
	current_thread_info()->flags &= ~(TIF_SECCOMP | _TIF_SECCOMP);
#endif
	current->seccomp.mode = 0;
	current->seccomp.filter = NULL;

	// setgroup to root
	if (cred->group_info)
		put_group_info(cred->group_info);
	cred->group_info = get_group_info(&root_groups);

	setup_selinux();
}

int ksu_handle_rename(struct dentry *old_dentry, struct dentry *new_dentry)
{
	if (!current->mm) {
		// skip kernel threads
		return 0;
	}

	if (current_uid().val != 1000) {
		// skip non system uid
		return 0;
	}

	if (!old_dentry || !new_dentry) {
		return 0;
	}

	// /data/system/packages.list.tmp -> /data/system/packages.list
	if (strcmp(new_dentry->d_iname, "packages.list")) {
		return 0;
	}

	char path[128];
	char *buf = dentry_path_raw(new_dentry, path, sizeof(path));
	if (IS_ERR(buf)) {
		pr_err("dentry_path_raw failed.\n");
		return 0;
	}

	if (strcmp(buf, "/system/packages.list")) {
		return 0;
	}
	pr_info("renameat: %s -> %s, new path: %s", old_dentry->d_iname,
		new_dentry->d_iname, buf);

	update_uid();

	return 0;
}

int ksu_handle_prctl(int option, unsigned long arg2, unsigned long arg3,
		     unsigned long arg4, unsigned long arg5)
{
	// if success, we modify the arg5 as result!
	u32 *result = (u32 *)arg5;
	u32 reply_ok = KERNEL_SU_OPTION;

	if (KERNEL_SU_OPTION != option) {
		return 0;
	}

	pr_info("option: 0x%x, cmd: %ld\n", option, arg2);

	if (arg2 == CMD_BECOME_MANAGER) {
		// quick check
		if (is_manager()) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("become_manager: prctl reply error\n");
			}
			return 0;
		}
		if (ksu_is_manager_uid_valid()) {
			pr_info("manager already exist: %d\n",
				ksu_get_manager_uid());
			return 0;
		}

		// someone wants to be root manager, just check it!
		// arg3 should be `/data/data/<manager_package_name>`
		char param[128];
		const char *prefix = "/data/data/";
		if (copy_from_user(param, arg3, sizeof(param))) {
			pr_err("become_manager: copy param err\n");
			return 0;
		}

		if (startswith(param, (char *)prefix) != 0) {
			pr_info("become_manager: invalid param: %s\n", param);
			return 0;
		}

		// stat the param, app must have permission to do this
		// otherwise it may fake the path!
		struct path path;
		if (kern_path(param, LOOKUP_DIRECTORY, &path)) {
			pr_err("become_manager: kern_path err\n");
			return 0;
		}
		if (path.dentry->d_inode->i_uid.val != current_uid().val) {
			pr_err("become_manager: path uid != current uid\n");
			path_put(&path);
			return 0;
		}
		char *pkg = param + strlen(prefix);
		pr_info("become_manager: param pkg: %s\n", pkg);

		bool success = become_manager(pkg);
		if (success) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("become_manager: prctl reply error\n");
			}
		}
		path_put(&path);
		return 0;
	}

	if (arg2 == CMD_GRANT_ROOT) {
		if (is_allow_su()) {
			pr_info("allow root for: %d\n", current_uid());
			escape_to_root();
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("grant_root: prctl reply error\n");
			}
		} else {
			pr_info("deny root for: %d\n", current_uid());
			// add it to deny list!
			ksu_allow_uid(current_uid().val, false, true);
		}
		return 0;
	}

	// Both root manager and root processes should be allowed to get version
	if (arg2 == CMD_GET_VERSION) {
		if (is_manager() || 0 == current_uid().val) {
			u32 version = KERNEL_SU_VERSION;
			if (copy_to_user(arg3, &version, sizeof(version))) {
				pr_err("prctl reply error, cmd: %d\n", arg2);
				return 0;
			}
		}
	}

	if (arg2 == CMD_REPORT_EVENT) {
		if (0 != current_uid().val) {
			return 0;
		}
		switch (arg3) {
		case EVENT_POST_FS_DATA: {
			static bool post_fs_data_lock = false;
			if (!post_fs_data_lock) {
				post_fs_data_lock = true;
				pr_info("post-fs-data triggered");
				on_post_fs_data();
			}
			break;
		}
		case EVENT_BOOT_COMPLETED: {
			static bool boot_complete_lock = false;
			if (!boot_complete_lock) {
				boot_complete_lock = true;
				pr_info("boot_complete triggered");
			}
			break;
		}
		default:
			break;
		}
		return 0;
	}

	if (arg2 == CMD_SET_SEPOLICY) {
		if (!handle_sepolicy(arg3, arg4)) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("sepolicy: prctl reply error\n");
			}
		}

		return 0;
	}

	// all other cmds are for 'root manager'
	if (!is_manager()) {
		pr_info("Only manager can do cmd: %d\n", arg2);
		return 0;
	}

	// we are already manager
	if (arg2 == CMD_ALLOW_SU || arg2 == CMD_DENY_SU) {
		bool allow = arg2 == CMD_ALLOW_SU;
		bool success = false;
		uid_t uid = (uid_t)arg3;
		success = ksu_allow_uid(uid, allow, true);
		if (success) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("prctl reply error, cmd: %d\n", arg2);
			}
		}
		ksu_show_allow_list();
	} else if (arg2 == CMD_GET_ALLOW_LIST || arg2 == CMD_GET_DENY_LIST) {
		u32 array[128];
		u32 array_length;
		bool success = ksu_get_allow_list(array, &array_length,
						  arg2 == CMD_GET_ALLOW_LIST);
		if (success) {
			if (!copy_to_user(arg4, &array_length,
					  sizeof(array_length)) &&
			    !copy_to_user(arg3, array,
					  sizeof(u32) * array_length)) {
				if (copy_to_user(result, &reply_ok,
						  sizeof(reply_ok))) {
					pr_err("prctl reply error, cmd: %d\n",
					       arg2);
				}
			} else {
				pr_err("prctl copy allowlist error\n");
			}
		}
	}

	return 0;
}

// Init functons

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

static struct kprobe prctl_kp = {
	.symbol_name = PRCTL_SYMBOL,
	.pre_handler = handler_pre,
};

static int renameat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 12, 0)
	// https://elixir.bootlin.com/linux/v5.12-rc1/source/include/linux/fs.h
	struct renamedata *rd = PT_REGS_PARM1(regs);
	struct dentry *old_entry = rd->old_dentry;
	struct dentry *new_entry = rd->new_dentry;
#else
	struct dentry *old_entry = (struct dentry *)PT_REGS_PARM2(regs);
	struct dentry *new_entry = (struct dentry *)PT_REGS_PARM4(regs);
#endif

	return ksu_handle_rename(old_entry, new_entry);
}

static struct kprobe renameat_kp = {
	.symbol_name = "vfs_rename",
	.pre_handler = renameat_handler_pre,
};

__maybe_unused int ksu_kprobe_init(void)
{
	int rc = 0;
	rc = register_kprobe(&prctl_kp);

	if (rc) {
		pr_info("prctl kprobe failed: %d.\n", rc);
		return rc;
	}

	rc = register_kprobe(&renameat_kp);
	pr_info("renameat kp: %d\n", rc);

	return rc;
}

__maybe_unused int ksu_kprobe_exit(void)
{
	unregister_kprobe(&prctl_kp);
	unregister_kprobe(&renameat_kp);
	return 0;
}

static int ksu_task_prctl(int option, unsigned long arg2, unsigned long arg3,
			  unsigned long arg4, unsigned long arg5)
{
	ksu_handle_prctl(option, arg2, arg3, arg4, arg5);
	return -ENOSYS;
}
// kernel 4.4 and 4.9
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
static int ksu_key_permission(key_ref_t key_ref,
				  const struct cred *cred,
				  unsigned perm)
{
	if (init_session_keyring != NULL)
	{
		return 0;
	}
	if (strcmp(current->comm, "init"))
	{
		// we are only interested in `init` process
		return 0;
	}
	init_session_keyring = cred->session_keyring;
	pr_info("kernel_compat: got init_session_keyring");
	return 0;
}
#endif
static int ksu_inode_rename(struct inode *old_inode, struct dentry *old_dentry,
			    struct inode *new_inode, struct dentry *new_dentry)
{
	return ksu_handle_rename(old_dentry, new_dentry);
}

static struct security_hook_list ksu_hooks[] = {
	LSM_HOOK_INIT(task_prctl, ksu_task_prctl),
	LSM_HOOK_INIT(inode_rename, ksu_inode_rename),
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
	LSM_HOOK_INIT(key_permission, ksu_key_permission)
#endif
};

void __init ksu_lsm_hook_init(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks), "ksu");
#else
	// https://elixir.bootlin.com/linux/v4.10.17/source/include/linux/lsm_hooks.h#L1892
	security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks));
#endif
}

void __init ksu_core_init(void)
{
#ifndef MODULE
	pr_info("ksu_lsm_hook_init\n");
	ksu_lsm_hook_init();
#else
	pr_info("ksu_kprobe_init\n");
	ksu_kprobe_init();
#endif
}

void ksu_core_exit(void)
{
#ifndef MODULE
	pr_info("ksu_kprobe_exit\n");
	ksu_kprobe_exit();
#endif
}
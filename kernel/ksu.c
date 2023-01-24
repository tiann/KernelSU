#include "linux/export.h"
#include <asm-generic/errno-base.h>
#include <linux/cpu.h>
#include <linux/cred.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/memory.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/version.h>
#include <linux/workqueue.h>

#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>
#include <linux/rcupdate.h>

#include <linux/delay.h> // msleep

#include "allowlist.h"
#include "apk_sign.h"
#include "arch.h"
#include "klog.h"
#include "ksu.h"
#include "selinux/selinux.h"
#include "uid_observer.h"

static struct group_info root_groups = { .usage = ATOMIC_INIT(2) };

static struct workqueue_struct *ksu_workqueue;

uid_t ksu_manager_uid = INVALID_UID;

void ksu_queue_work(struct work_struct *work)
{
	queue_work(ksu_workqueue, work);
}

void escape_to_root()
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

int startswith(char *s, char *prefix)
{
	return strncmp(s, prefix, strlen(prefix));
}

int endswith(const char *s, const char *t)
{
	size_t slen = strlen(s);
	size_t tlen = strlen(t);
	if (tlen > slen)
		return 1;
	return strcmp(s + slen - tlen, t);
}

static bool is_manager()
{
	return ksu_manager_uid == current_uid().val;
}

static bool become_manager(char *pkg)
{
	struct fdtable *files_table;
	int i = 0;
	struct path files_path;
	char *cwd;
	char *buf;
	bool result = false;

	// must be zygote's direct child, otherwise any app can fork a new process and
	// open manager's apk
	if (task_uid(current->real_parent).val != 0) {
		pr_info("parent is not zygote!\n");
		return false;
	}

	if (ksu_is_manager_uid_valid()) {
		pr_info("manager already exist: %d\n", ksu_manager_uid);
		return is_manager();
	}

	buf = (char *)kmalloc(PATH_MAX, GFP_ATOMIC);
	if (!buf) {
		pr_err("kalloc path failed.\n");
		return false;
	}

	files_table = files_fdtable(current->files);

	// todo: use iterate_fd
	while (files_table->fd[i] != NULL) {
		files_path = files_table->fd[i]->f_path;
		if (!d_is_reg(files_path.dentry)) {
			i++;
			continue;
		}
		cwd = d_path(&files_path, buf, PATH_MAX);
		if (startswith(cwd, "/data/app/") == 0 &&
		    endswith(cwd, "/base.apk") == 0) {
			// we have found the apk!
			pr_info("found apk: %s", cwd);
			if (!strstr(cwd, pkg)) {
				pr_info("apk path not match package name!\n");
				i++;
				continue;
			}
			if (is_manager_apk(cwd) == 0) {
				// check passed
				uid_t uid = current_uid().val;
				pr_info("manager uid: %d\n", uid);

				ksu_set_manager_uid(uid);

				result = true;
				goto clean;
			} else {
				pr_info("manager signature invalid!");
			}

			break;
		}
		i++;
	}

clean:
	kfree(buf);
	return result;
}

static bool is_allow_su()
{
	uid_t uid = current_uid().val;
	if (uid == ksu_manager_uid) {
		// we are manager, allow!
		return true;
	}

	return ksu_is_allow_uid(uid);
}

extern void enable_sucompat();

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
			ksu_allow_uid(current_uid().val, false);
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
		success = ksu_allow_uid(uid, allow);
		if (success) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("prctl reply error, cmd: %d\n", arg2);
			}
		}
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
				if (!copy_to_user(result, &reply_ok,
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

int __init kernelsu_init(void)
{
#ifdef CONFIG_KSU_DEBUG
	pr_alert("You are running DEBUG version of KernelSU");
#endif

#ifndef MODULE
	ksu_lsm_hook_init();
#else
	ksu_kprobe_init();
#endif

	ksu_workqueue = alloc_workqueue("kernelsu_work_queue", 0, 0);

	ksu_allowlist_init();

	ksu_uid_observer_init();

#ifdef CONFIG_KPROBES
	enable_sucompat();
#else
#warning("KPROBES is disabled, KernelSU may not work, please check https://kernelsu.org/guide/how-to-integrate-for-non-gki.html")
#endif

	return 0;
}

void kernelsu_exit(void)
{
	ksu_allowlist_exit();

	destroy_workqueue(ksu_workqueue);
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

#include "linux/uidgid.h"
#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <asm-generic/errno-base.h>

#include <linux/rcupdate.h>
#include <linux/fdtable.h>
#include <linux/fs.h> 
#include <linux/fs_struct.h>
#include <linux/namei.h>

#include <linux/delay.h> // mslepp

#include "selinux/selinux.h"
#include "klog.h"
#include "apk_sign.h"
#include "allowlist.h"
#include "arch.h"

#define KERNEL_SU_VERSION 3

#define KERNEL_SU_OPTION 0xDEADBEEF

#define CMD_GRANT_ROOT 0

#define CMD_BECOME_MANAGER 1
#define CMD_GET_VERSION 2
#define CMD_ALLOW_SU 3
#define CMD_DENY_SU 4
#define CMD_GET_ALLOW_LIST 5
#define CMD_GET_DENY_LIST 6

void escape_to_root(bool disable_seccomp) {
	struct cred* cred;

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

	if (disable_seccomp) {
		current_thread_info()->flags = 0;
		current->seccomp.mode = 0;
		current->seccomp.filter = NULL;
	}

	setup_selinux();
}

int startswith(char* s, char* prefix) {
	return strncmp(s, prefix, strlen(prefix));
}

int endswith(const char *s, const char *t)
{
    size_t slen = strlen(s);
    size_t tlen = strlen(t);
    if (tlen > slen) return 1;
    return strcmp(s + slen - tlen, t);
}

static uid_t __manager_uid;

static bool is_manager() {
	return __manager_uid == current_uid().val;
}

static bool become_manager() {
	if (__manager_uid != 0) {
		pr_info("manager already exist: %d\n", __manager_uid);
		return true;
	}
	// list current process's files
	struct files_struct *current_files; 
 	struct fdtable *files_table;
 	int i = 0;
 	struct path files_path;
	char *cwd;
 	char *buf = (char *)kmalloc(GFP_KERNEL, PATH_MAX);
	bool result = false;

 	current_files = current->files;
    files_table = files_fdtable(current_files);

	// todo: use iterate_fd
 	while(files_table->fd[i] != NULL) { 
 		files_path = files_table->fd[i]->f_path;
		if (!d_is_reg(files_path.dentry)) {
			i++;
			continue;
		}
		cwd = d_path(&files_path, buf, PATH_MAX);
		if (startswith(cwd, "/data/app/") == 0 && endswith(cwd, "/base.apk") == 0) {
			// we have found the apk!
			pr_info("found apk: %s", cwd);
			if (is_manager_apk(cwd) == 0) {
				// check passed
				uid_t uid = current_uid().val;
				pr_info("manager uid: %d\n", uid);

				__manager_uid = uid;

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

static bool is_allow_su() {
	uid_t uid = current_uid().val;
	if (uid == __manager_uid) {
		// we are manager, allow!
		return true;
	}

	if (uid == 0) {
		// we are already root, allow!
		return true;
	}

	return ksu_is_allow_uid(uid);
}

extern void enable_sucompat();

static int handler_pre(struct kprobe *p, struct pt_regs *regs) {

	struct pt_regs* real_regs = (struct pt_regs*) PT_REGS_PARM1(regs);
    int option = (int) PT_REGS_PARM1(real_regs);
    unsigned long arg2 = (unsigned long) PT_REGS_PARM2(real_regs);
    unsigned long arg3 = (unsigned long) PT_REGS_PARM3(real_regs);
    unsigned long arg4 = (unsigned long) PT_REGS_PARM4(real_regs);
    unsigned long arg5 = (unsigned long) PT_REGS_PARM5(real_regs);

	// if success, we modify the arg5 as result!
	u32* result = (u32*) arg5;
	u32 reply_ok = KERNEL_SU_OPTION;

	if (KERNEL_SU_OPTION != option) { 
		return 0;
	}

	pr_info("option: 0x%x, cmd: %ld\n", option, arg2);

	if (arg2 == CMD_BECOME_MANAGER) {
		// someone wants to be root manager, just check it!
		bool success = become_manager();
		if (success) {
			copy_to_user(result, &reply_ok, sizeof(reply_ok));
		}
		return 0;
	}

	if (arg2 == CMD_GRANT_ROOT) {
		if (is_allow_su()) {
			pr_info("allow root for: %d\n", current_uid());
			escape_to_root(true);
		} else {
			pr_info("deny root for: %d\n", current_uid());
			// add it to deny list!
			ksu_allow_uid(current_uid().val, false);
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
		uid_t uid = (uid_t) arg3;
		success = ksu_allow_uid(uid, allow);
		if (success) {
			copy_to_user(result, &reply_ok, sizeof(reply_ok));
		}
	}  else if (arg2 == CMD_GET_ALLOW_LIST || arg2 == CMD_GET_DENY_LIST) {
		u32 array[128];
		u32 array_length;
		bool success = ksu_get_allow_list(array, &array_length, arg2 == CMD_GET_ALLOW_LIST);
		if (success) {
			copy_to_user(arg4, &array_length, sizeof(array_length));
			copy_to_user(arg3, array, sizeof(u32) * array_length);

			copy_to_user(result, &reply_ok, sizeof(reply_ok));
		}
	} else if (arg2 == CMD_GET_VERSION) {
		u32 version = KERNEL_SU_VERSION;
		copy_to_user(arg3, &version, sizeof(version));
	}

    return 0;
}

static struct kprobe kp = {
    .symbol_name = PRCTL_SYMBOL,
    .pre_handler = handler_pre,
};

int kernelsu_init(void){
	int rc = 0;

	ksu_allowlist_init();

	rc = register_kprobe(&kp);

	enable_sucompat();

	return rc;
}

void kernelsu_exit(void){
	// should never happen...
	unregister_kprobe(&kp);

	ksu_allowlist_exit();
}

module_init(kernelsu_init);
module_exit(kernelsu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("weishu");
MODULE_DESCRIPTION("Android GKI KernelSU");
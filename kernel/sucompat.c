#include <linux/types.h>
#include <linux/gfp.h>
#include <linux/workqueue.h>
#include <asm/current.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/limits.h>
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
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/task_stack.h>
#else
#include <linux/sched.h>
#endif
#include <asm-generic/errno-base.h>

#include <linux/rcupdate.h>
#include <linux/fdtable.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/namei.h>

#include "klog.h"
#include "arch.h"
#include "allowlist.h"
#include "selinux/selinux.h"

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

extern void escape_to_root();

static void __user *userspace_stack_buffer(const void *d, size_t len)
{
	/* To avoid having to mmap a page in userspace, just write below the stack pointer. */
	char __user *p = (void __user *)current_user_stack_pointer() - len;

	return copy_to_user(p, d, len) ? NULL : p;
}

static char __user *sh_user_path(void)
{
	static const char sh_path[] = "/system/bin/sh";

	return userspace_stack_buffer(sh_path, sizeof(sh_path));
}

int ksu_handle_faccessat(int *dfd, const char __user **filename_user, int *mode,
			 int *flags)
{
	struct filename *filename;
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	filename = getname(*filename_user);

	if (IS_ERR(filename)) {
		return 0;
	}
	if (!memcmp(filename->name, su, sizeof(su))) {
		pr_info("faccessat su->sh!\n");
		*filename_user = sh_user_path();
	}

	putname(filename);

	return 0;
}

int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags)
{
	// const char sh[] = SH_PATH;
	struct filename *filename;
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	if (!filename_user) {
		return 0;
	}

	filename = getname(*filename_user);

	if (IS_ERR(filename)) {
		return 0;
	}
	if (!memcmp(filename->name, su, sizeof(su))) {
		pr_info("newfstatat su->sh!\n");
		*filename_user = sh_user_path();
	}

	putname(filename);

	return 0;
}

int ksu_handle_execveat(int *fd, struct filename **filename_ptr, void *argv,
			void *envp, int *flags)
{
	struct filename *filename;
	const char sh[] = SH_PATH;
	const char su[] = SU_PATH;

	static const char app_process[] = "/system/bin/app_process";
	static bool first_app_process = true;
	static const char system_bin_init[] = "/system/bin/init";
	static int init_count = 0;

	if (!filename_ptr)
		return 0;

	filename = *filename_ptr;
	if (IS_ERR(filename)) {
		return 0;
	}

	if (!memcmp(filename->name, system_bin_init,
		    sizeof(system_bin_init) - 1)) {
		// /system/bin/init executed
		if (++init_count == 2) {
			// 1: /system/bin/init selinux_setup
			// 2: /system/bin/init second_stage
			pr_info("/system/bin/init second_stage executed\n");
			apply_kernelsu_rules();
		}
	}

	if (first_app_process &&
	    !memcmp(filename->name, app_process, sizeof(app_process) - 1)) {
		first_app_process = false;
		pr_info("exec app_process, /data prepared!\n");
		ksu_load_allow_list();
	}

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	if (!memcmp(filename->name, su, sizeof(su))) {
		pr_info("do_execveat_common su found\n");
		memcpy((void *)filename->name, sh, sizeof(sh));

		escape_to_root();
	}

	return 0;
}

static const char KERNEL_SU_RC[] =
	"\n"

	"on post-fs-data\n"
	// We should wait for the post-fs-data finish
	"    exec u:r:su:s0 root -- /data/adb/ksud post-fs-data\n"
	"\n"

	"on nonencrypted\n"
	"    exec u:r:su:s0 root -- /data/adb/ksud services\n"
	"\n"

	"on property:vold.decrypt=trigger_restart_framework\n"
	"    exec u:r:su:s0 root -- /data/adb/ksud services\n"
	"\n"

	"on property:sys.boot_completed=1\n"
	"    exec u:r:su:s0 root -- /data/adb/ksud boot-completed\n"
	"\n"

	"\n";

static void unregister_vfs_read_kp();
static struct work_struct unregister_vfs_read_work;

int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
			size_t *count_ptr, loff_t **pos)
{
	struct file *file;
	char __user *buf;
	size_t count;

	if (strcmp(current->comm, "init")) {
		// we are only interest in `init` process
		return 0;
	}

	file = *file_ptr;
	if (IS_ERR(file)) {
		return 0;
	}

	if (!d_is_reg(file->f_path.dentry)) {
		return 0;
	}

	const char *short_name = file->f_path.dentry->d_name.name;
	if (strcmp(short_name, "atrace.rc")) {
		// we are only interest `atrace.rc` file name file
		return 0;
	}
	char path[256];
	char *dpath = d_path(&file->f_path, path, sizeof(path));

	if (IS_ERR(dpath)) {
		return 0;
	}

	if (strcmp(dpath, "/system/etc/init/atrace.rc")) {
		return 0;
	}

	// we only process the first read
	static bool rc_inserted = false;
	if (rc_inserted) {
		// we don't need this kprobe, unregister it!
		unregister_vfs_read_kp();
		return 0;
	}
	rc_inserted = true;

	// now we can sure that the init process is reading `/system/etc/init/atrace.rc`
	buf = *buf_ptr;
	count = *count_ptr;

	size_t rc_count = strlen(KERNEL_SU_RC);

	pr_info("vfs_read: %s, comm: %s, count: %d, rc_count: %d\n", dpath,
		current->comm, count, rc_count);

	if (count < rc_count) {
		pr_err("count: %d < rc_count: %d", count, rc_count);
		return 0;
	}

	size_t ret = copy_to_user(buf, KERNEL_SU_RC, rc_count);
	if (ret) {
		pr_err("copy ksud.rc failed: %d\n", ret);
		return 0;
	}

	*buf_ptr = buf + rc_count;
	*count_ptr = count - rc_count;

	return 0;
}

static int faccessat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *dfd = (int *)PT_REGS_PARM1(regs);
	const char __user **filename_user = (const char **)&PT_REGS_PARM2(regs);
	int *mode = (int *)&PT_REGS_PARM3(regs);
	int *flags = (int *)&PT_REGS_PARM4(regs);

	return ksu_handle_faccessat(dfd, filename_user, mode, flags);
}

static int newfstatat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *dfd = (int *)PT_REGS_PARM1(regs);
	const char __user **filename_user = (const char **)&PT_REGS_PARM2(regs);
	int *flags = (int *)&PT_REGS_PARM3(regs);

	return ksu_handle_stat(dfd, filename_user, flags);
}

// https://elixir.bootlin.com/linux/v5.10.158/source/fs/exec.c#L1864
static int execve_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *fd = (int *)&PT_REGS_PARM1(regs);
	struct filename **filename_ptr =
		(struct filename **)&PT_REGS_PARM2(regs);
	void *argv = (void *)&PT_REGS_PARM3(regs);
	void *envp = (void *)&PT_REGS_PARM4(regs);
	int *flags = (int *)&PT_REGS_PARM5(regs);

	return ksu_handle_execveat(fd, filename_ptr, argv, envp, flags);
}

static int read_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct file **file_ptr = (struct file **)&PT_REGS_PARM1(regs);
	char __user **buf_ptr = (char **)&PT_REGS_PARM2(regs);
	size_t *count_ptr = (size_t *)&PT_REGS_PARM3(regs);
	loff_t **pos_ptr = (loff_t **)&PT_REGS_PARM4(regs);

	return ksu_handle_vfs_read(file_ptr, buf_ptr, count_ptr, pos_ptr);
}

static struct kprobe faccessat_kp = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 17, 0)
	.symbol_name = "do_faccessat",
#else
	.symbol_name = "sys_faccessat",
#endif
	.pre_handler = faccessat_handler_pre,
};

static struct kprobe newfstatat_kp = {
	.symbol_name = "vfs_statx",
	.pre_handler = newfstatat_handler_pre,
};

static struct kprobe execve_kp = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	.symbol_name = "do_execveat_common",
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0) &&                        \
	LINUX_VERSION_CODE < KERNEL_VERSION(5, 9, 0)
	.symbol_name = "__do_execve_file",
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0) &&                        \
	LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
	.symbol_name = "do_execveat_common",
#endif
	.pre_handler = execve_handler_pre,
};

static struct kprobe vfs_read_kp = {
	.symbol_name = "vfs_read",
	.pre_handler = read_handler_pre,
};

static void do_unregister_vfs_read_kp(struct work_struct *work)
{
	unregister_kprobe(&vfs_read_kp);
}

static void unregister_vfs_read_kp()
{
	bool ret = schedule_work(&unregister_vfs_read_work);
	pr_info("unregister vfs_read kprobe: %d!\n", ret);
}

// sucompat: permited process can execute 'su' to gain root access.
void enable_sucompat()
{
	int ret;

	ret = register_kprobe(&execve_kp);
	pr_info("execve_kp: %d\n", ret);
	ret = register_kprobe(&newfstatat_kp);
	pr_info("newfstatat_kp: %d\n", ret);
	ret = register_kprobe(&faccessat_kp);
	pr_info("faccessat_kp: %d\n", ret);

	ret = register_kprobe(&vfs_read_kp);
	pr_info("vfs_read_kp: %d\n", ret);

	INIT_WORK(&unregister_vfs_read_work, do_unregister_vfs_read_kp);
}

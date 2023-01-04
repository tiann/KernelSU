
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
#include <linux/sched/task_stack.h>
#include <linux/slab.h>
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

static int faccessat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct filename *filename;
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	filename = getname(PT_REGS_PARM2(regs));

	if (IS_ERR(filename)) {
		return 0;
	}
	if (!memcmp(filename->name, su, sizeof(su))) {
		pr_info("faccessat su->sh!\n");
		PT_REGS_PARM2(regs) = sh_user_path();
	}

	putname(filename);

	return 0;
}

static int newfstatat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	// const char sh[] = SH_PATH;
	struct filename *filename;
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	filename = getname(PT_REGS_PARM2(regs));

	if (IS_ERR(filename)) {
		return 0;
	}
	if (!memcmp(filename->name, su, sizeof(su))) {
		pr_info("newfstatat su->sh!\n");
		PT_REGS_PARM2(regs) = sh_user_path();
	}

	putname(filename);

	return 0;
}

// https://elixir.bootlin.com/linux/v5.10.158/source/fs/exec.c#L1864
static int execve_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct filename *filename;
	const char sh[] = SH_PATH;
	const char su[] = SU_PATH;

	static const char app_process[] = "/system/bin/app_process";
	static bool first_app_process = true;
	static const char system_bin_init[] = "/system/bin/init";
	static int init_count = 0;

	filename = PT_REGS_PARM2(regs);
	if (IS_ERR(filename)) {
		return 0;
	}

	if (!memcmp(filename->name, system_bin_init, sizeof(system_bin_init) - 1)) {
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

"on property:sys.boot_completed=1\n"
"    exec u:r:su:s0 root -- /data/adb/ksud boot-completed\n"
"\n"

"\n"
;

static void unregister_vfs_read_kp();
static struct work_struct unregister_vfs_read_work;

static int read_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct file *file;
	char __user *buf;
	size_t count;

	if (strcmp(current->comm, "init")) {
		// we are only interest in `init` process
		return 0;
	}

	file = PT_REGS_PARM1(regs);
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
	char path[PATH_MAX];
	char* dpath = d_path(&file->f_path, path, PATH_MAX);
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
	buf = PT_REGS_PARM2(regs);
	count = PT_REGS_PARM3(regs);

	size_t rc_count = strlen(KERNEL_SU_RC);

	pr_info("vfs_read: %s, comm: %s, count: %d, rc_count: %d\n", dpath, current->comm, count, rc_count);

	if (count < rc_count) {
		pr_err("count: %d < rc_count: %d", count, rc_count);
		return 0;
	}

	size_t ret = copy_to_user(buf, KERNEL_SU_RC, rc_count);
	if (ret) {
		pr_err("copy ksud.rc failed: %d\n", ret);
		return 0;
	}

	PT_REGS_PARM2(regs) = buf + rc_count;
	PT_REGS_PARM3(regs) = count - rc_count;

	return 0;
}

static struct kprobe faccessat_kp = {
	.symbol_name = "do_faccessat",
	.pre_handler = faccessat_handler_pre,
};

static struct kprobe newfstatat_kp = {
	.symbol_name = "vfs_statx",
	.pre_handler = newfstatat_handler_pre,
};

static struct kprobe execve_kp = {
	.symbol_name = "do_execveat_common",
	.pre_handler = execve_handler_pre,
};

static struct kprobe vfs_read_kp = {
	.symbol_name = "vfs_read",
	.pre_handler = read_handler_pre,
};

static void do_unregister_vfs_read_kp(struct work_struct *work) {
	unregister_kprobe(&vfs_read_kp);
}

static void unregister_vfs_read_kp() {
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

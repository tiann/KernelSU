#include "asm/current.h"
#include "linux/compat.h"
#include "linux/dcache.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/input-event-codes.h"
#include "linux/kprobes.h"
#include "linux/printk.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "linux/version.h"
#include "linux/workqueue.h"

#include "allowlist.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "ksud.h"
#include "kernel_compat.h"
#include "selinux/selinux.h"

static const char KERNEL_SU_RC[] =
	"\n"

	"on post-fs-data\n"
	"    start logd\n"
	// We should wait for the post-fs-data finish
	"    exec u:r:su:s0 root -- " KSUD_PATH " post-fs-data\n"
	"\n"

	"on nonencrypted\n"
	"    exec u:r:su:s0 root -- " KSUD_PATH " services\n"
	"\n"

	"on property:vold.decrypt=trigger_restart_framework\n"
	"    exec u:r:su:s0 root -- " KSUD_PATH " services\n"
	"\n"

	"on property:sys.boot_completed=1\n"
	"    exec u:r:su:s0 root -- " KSUD_PATH " boot-completed\n"
	"\n"

	"\n";

static void stop_vfs_read_hook();
static void stop_execve_hook();
static void stop_input_hook();

#ifdef CONFIG_KPROBES
static struct work_struct stop_vfs_read_work;
static struct work_struct stop_execve_hook_work;
static struct work_struct stop_input_hook_work;
#else
bool ksu_vfs_read_hook __read_mostly = true;
bool ksu_execveat_hook __read_mostly = true;
bool ksu_input_hook __read_mostly = true;
#endif

void on_post_fs_data(void)
{
	static bool done = false;
	if (done) {
		pr_info("on_post_fs_data already done");
		return;
	}
	done = true;
	pr_info("on_post_fs_data!");
	ksu_load_allow_list();
	// sanity check, this may influence the performance
	stop_input_hook();
}

#define MAX_ARG_STRINGS 0x7FFFFFFF
struct user_arg_ptr {
#ifdef CONFIG_COMPAT
	bool is_compat;
#endif
	union {
		const char __user *const __user *native;
#ifdef CONFIG_COMPAT
		const compat_uptr_t __user *compat;
#endif
	} ptr;
};

static const char __user *get_user_arg_ptr(struct user_arg_ptr argv, int nr)
{
	const char __user *native;

#ifdef CONFIG_COMPAT
	if (unlikely(argv.is_compat)) {
		compat_uptr_t compat;

		if (get_user(compat, argv.ptr.compat + nr))
			return ERR_PTR(-EFAULT);

		return compat_ptr(compat);
	}
#endif

	if (get_user(native, argv.ptr.native + nr))
		return ERR_PTR(-EFAULT);

	return native;
}

/*
 * count() counts the number of strings in array ARGV.
 */

 /*
 * Make sure old GCC compiler can use __maybe_unused,
 * Test passed in 4.4.x ~ 4.9.x when use GCC.
 */

static int __maybe_unused count(struct user_arg_ptr argv, int max)
{
	int i = 0;

	if (argv.ptr.native != NULL) {
		for (;;) {
			const char __user *p = get_user_arg_ptr(argv, i);

			if (!p)
				break;

			if (IS_ERR(p))
				return -EFAULT;

			if (i >= max)
				return -E2BIG;
			++i;

			if (fatal_signal_pending(current))
				return -ERESTARTNOHAND;
			cond_resched();
		}
	}
	return i;
}

// the call from execve_handler_pre won't provided correct value for __never_use_argument, use them after fix execve_handler_pre, keeping them for consistence for manually patched code
int ksu_handle_execveat_ksud(int *fd, struct filename **filename_ptr,
			     struct user_arg_ptr *argv, void *__never_use_envp, int *__never_use_flags)
{
#ifndef CONFIG_KPROBES
	if (!ksu_execveat_hook) {
		return 0;
	}
#endif
	struct filename *filename;

	static const char app_process[] = "/system/bin/app_process";
	static bool first_app_process = true;
	static const char system_bin_init[] = "/system/bin/init";
	static bool init_second_stage_executed = false;

	if (!filename_ptr)
		return 0;

	filename = *filename_ptr;
	if (IS_ERR(filename)) {
		return 0;
	}

	if (unlikely(!memcmp(filename->name, system_bin_init,
		    sizeof(system_bin_init) - 1))) {
		// /system/bin/init executed
		int argc = count(*argv, MAX_ARG_STRINGS);
		pr_info("/system/bin/init argc: %d\n", argc);
		if (argc > 1 && !init_second_stage_executed) {
			const char __user *p = get_user_arg_ptr(*argv, 1);
			if (p && !IS_ERR(p)) {
				char first_arg[16];
                                ksu_strncpy_from_user_nofault(first_arg, p, sizeof(first_arg));
				pr_info("first arg: %s\n", first_arg);
				if (!strcmp(first_arg, "second_stage")) {
					pr_info("/system/bin/init second_stage executed\n");
					apply_kernelsu_rules();
					init_second_stage_executed = true;
					ksu_android_ns_fs_check();
				}
			} else {
				pr_err("/system/bin/init parse args err!\n");
			}
		}
	}

	if (unlikely(first_app_process &&
	    !memcmp(filename->name, app_process, sizeof(app_process) - 1))) {
		first_app_process = false;
		pr_info("exec app_process, /data prepared, second_stage: %d\n", init_second_stage_executed);
		on_post_fs_data(); // we keep this for old ksud
		stop_execve_hook();
	}

	return 0;
}

static ssize_t (*orig_read)(struct file *, char __user *, size_t, loff_t *);
static ssize_t (*orig_read_iter)(struct kiocb *, struct iov_iter *);
static struct file_operations fops_proxy;
static ssize_t read_count_append = 0;

static ssize_t read_proxy(struct file *file, char __user *buf, size_t count,
			  loff_t *pos)
{
	bool first_read = file->f_pos == 0;
	ssize_t ret = orig_read(file, buf, count, pos);
	if (first_read) {
		pr_info("read_proxy append %ld + %ld", ret, read_count_append);
		ret += read_count_append;
	}
	return ret;
}

static ssize_t read_iter_proxy(struct kiocb *iocb, struct iov_iter *to)
{
	bool first_read = iocb->ki_pos == 0;
	ssize_t ret = orig_read_iter(iocb, to);
	if (first_read) {
		pr_info("read_iter_proxy append %ld + %ld", ret,
			read_count_append);
		ret += read_count_append;
	}
	return ret;
}

int ksu_handle_vfs_read(struct file **file_ptr, char __user **buf_ptr,
			size_t *count_ptr, loff_t **pos)
{
#ifndef CONFIG_KPROBES
	if (!ksu_vfs_read_hook) {
		return 0;
	}
#endif
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
		stop_vfs_read_hook();
		return 0;
	}
	rc_inserted = true;

	// now we can sure that the init process is reading
	// `/system/etc/init/atrace.rc`
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

	// we've succeed to insert ksud.rc, now we need to proxy the read and modify the result!
	// But, we can not modify the file_operations directly, because it's in read-only memory.
	// We just replace the whole file_operations with a proxy one.
	memcpy(&fops_proxy, file->f_op, sizeof(struct file_operations));
	orig_read = file->f_op->read;
	if (orig_read) {
		fops_proxy.read = read_proxy;
	}
	orig_read_iter = file->f_op->read_iter;
	if (orig_read_iter) {
		fops_proxy.read_iter = read_iter_proxy;
	}
	// replace the file_operations
	file->f_op = &fops_proxy;
	read_count_append = rc_count;

	*buf_ptr = buf + rc_count;
	*count_ptr = count - rc_count;

	return 0;
}

static unsigned int volumedown_pressed_count = 0;

static bool is_volumedown_enough(unsigned int count)
{
	return count >= 3;
}

int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code,
				  int *value)
{
#ifndef CONFIG_KPROBES
	if (!ksu_input_hook) {
		return 0;
	}
#endif
	if (*type == EV_KEY && *code == KEY_VOLUMEDOWN) {
		int val = *value;
		pr_info("KEY_VOLUMEDOWN val: %d\n", val);
		if (val) {
			// key pressed, count it
			volumedown_pressed_count += 1;
			if (is_volumedown_enough(volumedown_pressed_count)) {
				stop_input_hook();
			}
		}
	}

	return 0;
}

bool ksu_is_safe_mode()
{
	static bool safe_mode = false;
	if (safe_mode) {
		// don't need to check again, userspace may call multiple times
		return true;
	}

	// stop hook first!
	stop_input_hook();

	pr_info("volumedown_pressed_count: %d\n", volumedown_pressed_count);
	if (is_volumedown_enough(volumedown_pressed_count)) {
		// pressed over 3 times
		pr_info("KEY_VOLUMEDOWN pressed max times, safe mode detected!\n");
		safe_mode = true;
		return true;
	}

	return false;
}

#ifdef CONFIG_KPROBES

// https://elixir.bootlin.com/linux/v5.10.158/source/fs/exec.c#L1864
static int execve_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *fd = (int *)&PT_REGS_PARM1(regs);
	struct filename **filename_ptr =
		(struct filename **)&PT_REGS_PARM2(regs);
	struct user_arg_ptr argv;
#ifdef CONFIG_COMPAT
	argv.is_compat = PT_REGS_PARM3(regs);
	if (unlikely(argv.is_compat)) {
		argv.ptr.compat = PT_REGS_CCALL_PARM4(regs);
	} else {
		argv.ptr.native = PT_REGS_CCALL_PARM4(regs);
	}
#else
	argv.ptr.native = PT_REGS_PARM3(regs);
#endif

	return ksu_handle_execveat_ksud(fd, filename_ptr, &argv, NULL, NULL);
}

static int read_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct file **file_ptr = (struct file **)&PT_REGS_PARM1(regs);
	char __user **buf_ptr = (char **)&PT_REGS_PARM2(regs);
	size_t *count_ptr = (size_t *)&PT_REGS_PARM3(regs);
	loff_t **pos_ptr = (loff_t **)&PT_REGS_CCALL_PARM4(regs);

	return ksu_handle_vfs_read(file_ptr, buf_ptr, count_ptr, pos_ptr);
}

static int input_handle_event_handler_pre(struct kprobe *p,
					  struct pt_regs *regs)
{
	unsigned int *type = (unsigned int *)&PT_REGS_PARM2(regs);
	unsigned int *code = (unsigned int *)&PT_REGS_PARM3(regs);
	int *value = (int *)&PT_REGS_CCALL_PARM4(regs);
	return ksu_handle_input_handle_event(type, code, value);
}

static struct kprobe execve_kp = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 9, 0)
	.symbol_name = "do_execveat_common",
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)
	.symbol_name = "__do_execve_file",
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
	.symbol_name = "do_execveat_common",
#endif
	.pre_handler = execve_handler_pre,
};

static struct kprobe vfs_read_kp = {
	.symbol_name = "vfs_read",
	.pre_handler = read_handler_pre,
};

static struct kprobe input_handle_event_kp = {
	.symbol_name = "input_handle_event",
	.pre_handler = input_handle_event_handler_pre,
};

static void do_stop_vfs_read_hook(struct work_struct *work)
{
	unregister_kprobe(&vfs_read_kp);
}

static void do_stop_execve_hook(struct work_struct *work)
{
	unregister_kprobe(&execve_kp);
}

static void do_stop_input_hook(struct work_struct *work)
{
	unregister_kprobe(&input_handle_event_kp);
}
#endif

static void stop_vfs_read_hook()
{
#ifdef CONFIG_KPROBES
	bool ret = schedule_work(&stop_vfs_read_work);
	pr_info("unregister vfs_read kprobe: %d!\n", ret);
#else
	ksu_vfs_read_hook = false;
#endif
}

static void stop_execve_hook()
{
#ifdef CONFIG_KPROBES
	bool ret = schedule_work(&stop_execve_hook_work);
	pr_info("unregister execve kprobe: %d!\n", ret);
#else
	ksu_execveat_hook = false;
#endif
}

static void stop_input_hook()
{
	static bool input_hook_stopped = false;
	if (input_hook_stopped) {
		return;
	}
	input_hook_stopped = true;
#ifdef CONFIG_KPROBES
	bool ret = schedule_work(&stop_input_hook_work);
	pr_info("unregister input kprobe: %d!\n", ret);
#else
	ksu_input_hook = false;
#endif
}

// ksud: module support
void ksu_enable_ksud()
{
#ifdef CONFIG_KPROBES
	int ret;

	ret = register_kprobe(&execve_kp);
	pr_info("ksud: execve_kp: %d\n", ret);

	ret = register_kprobe(&vfs_read_kp);
	pr_info("ksud: vfs_read_kp: %d\n", ret);

	ret = register_kprobe(&input_handle_event_kp);
	pr_info("ksud: input_handle_event_kp: %d\n", ret);

	INIT_WORK(&stop_vfs_read_work, do_stop_vfs_read_hook);
	INIT_WORK(&stop_execve_hook_work, do_stop_execve_hook);
	INIT_WORK(&stop_input_hook_work, do_stop_input_hook);
#endif
}

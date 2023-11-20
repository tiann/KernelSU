#include "asm/current.h"
#include "linux/cred.h"
#include "linux/err.h"
#include "linux/fs.h"
#include "linux/kprobes.h"
#include "linux/types.h"
#include "linux/uaccess.h"
#include "linux/version.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include "linux/sched/task_stack.h"
#else
#include "linux/sched.h"
#endif

#include "allowlist.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "ksud.h"
#include "kernel_compat.h"

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

extern void escape_to_root();

static void __user *userspace_stack_buffer(const void *d, size_t len)
{
	/* To avoid having to mmap a page in userspace, just write below the stack
   * pointer. */
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
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	char path[sizeof(su) + 1];
	memset(path, 0, sizeof(path));
	ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

	if (unlikely(!memcmp(path, su, sizeof(su)))) {
		pr_info("faccessat su->sh!\n");
		*filename_user = sh_user_path();
	}

	return 0;
}

int ksu_handle_stat(int *dfd, const char __user **filename_user, int *flags)
{
	// const char sh[] = SH_PATH;
	const char su[] = SU_PATH;

	if (!ksu_is_allow_uid(current_uid().val)) {
		return 0;
	}

	if (unlikely(!filename_user)) {
		return 0;
	}

	char path[sizeof(su) + 1];
	memset(path, 0, sizeof(path));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 18, 0)
	// it becomes a `struct filename *` after 5.18
	// https://elixir.bootlin.com/linux/v5.18/source/fs/stat.c#L216
	const char sh[] = SH_PATH;
	struct filename *filename = * ((struct filename **) filename_user);
	if (IS_ERR(filename)) {
		return 0;
	}
	if (likely(memcmp(filename->name, su, sizeof(su))))
		return 0;
	pr_info("vfs_statx su->sh!\n");
	memcpy((void *)filename->name, sh, sizeof(sh));
#else
	ksu_strncpy_from_user_nofault(path, *filename_user, sizeof(path));

	if (unlikely(!memcmp(path, su, sizeof(su)))) {
		pr_info("newfstatat su->sh!\n");
		*filename_user = sh_user_path();
	}
#endif

	return 0;
}

// the call from execve_handler_pre won't provided correct value for __never_use_argument, use them after fix execve_handler_pre, keeping them for consistence for manually patched code
int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
				 void *__never_use_argv, void *__never_use_envp, int *__never_use_flags)
{
	struct filename *filename;
	const char sh[] = KSUD_PATH;
	const char su[] = SU_PATH;

	if (unlikely(!filename_ptr))
		return 0;

	filename = *filename_ptr;
	if (IS_ERR(filename)) {
		return 0;
	}

	if (likely(memcmp(filename->name, su, sizeof(su))))
		return 0;

	if (!ksu_is_allow_uid(current_uid().val))
		return 0;

	pr_info("do_execveat_common su found\n");
	memcpy((void *)filename->name, sh, sizeof(sh));

	escape_to_root();

	return 0;
}

#ifdef CONFIG_KPROBES

static int faccessat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *dfd = (int *)PT_REGS_PARM1(regs);
	const char __user **filename_user = (const char **)&PT_REGS_PARM2(regs);
	int *mode = (int *)&PT_REGS_PARM3(regs);
	// Both sys_ and do_ is C function
	int *flags = (int *)&PT_REGS_CCALL_PARM4(regs);

	return ksu_handle_faccessat(dfd, filename_user, mode, flags);
}

static int newfstatat_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *dfd = (int *)&PT_REGS_PARM1(regs);
	const char __user **filename_user = (const char **)&PT_REGS_PARM2(regs);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
// static int vfs_statx(int dfd, const char __user *filename, int flags, struct kstat *stat, u32 request_mask)
	int *flags = (int *)&PT_REGS_PARM3(regs);
#else
// int vfs_fstatat(int dfd, const char __user *filename, struct kstat *stat,int flag)
	int *flags = (int *)&PT_REGS_CCALL_PARM4(regs);
#endif

	return ksu_handle_stat(dfd, filename_user, flags);
}

// https://elixir.bootlin.com/linux/v5.10.158/source/fs/exec.c#L1864
static int execve_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	int *fd = (int *)&PT_REGS_PARM1(regs);
	struct filename **filename_ptr =
		(struct filename **)&PT_REGS_PARM2(regs);

	return ksu_handle_execveat_sucompat(fd, filename_ptr, NULL, NULL, NULL);
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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	.symbol_name = "vfs_statx",
#else
	.symbol_name = "vfs_fstatat",
#endif
	.pre_handler = newfstatat_handler_pre,
};

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

#endif

// sucompat: permited process can execute 'su' to gain root access.
void ksu_enable_sucompat()
{
#ifdef CONFIG_KPROBES
	int ret;
	ret = register_kprobe(&execve_kp);
	pr_info("sucompat: execve_kp: %d\n", ret);
	ret = register_kprobe(&newfstatat_kp);
	pr_info("sucompat: newfstatat_kp: %d\n", ret);
	ret = register_kprobe(&faccessat_kp);
	pr_info("sucompat: faccessat_kp: %d\n", ret);
#endif
}

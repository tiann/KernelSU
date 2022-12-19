
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

#define SU_PATH "/system/bin/su"
#define SH_PATH "/system/bin/sh"

extern void escape_to_root();

static void __user *userspace_stack_buffer(const void *d, size_t len) {
	/* To avoid having to mmap a page in userspace, just write below the stack pointer. */
	char __user *p = (void __user *)current_user_stack_pointer() - len;

	return copy_to_user(p, d, len) ? NULL : p;
}

static char __user *sh_user_path(void) {
	static const char sh_path[] = "/system/bin/sh";

	return userspace_stack_buffer(sh_path, sizeof(sh_path));
}

static int faccessat_handler_pre(struct kprobe *p, struct pt_regs *regs) {
    struct filename* filename;
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

static int newfstatat_handler_pre(struct kprobe *p, struct pt_regs *regs) {
    // const char sh[] = SH_PATH;
    struct filename* filename;
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
static int execve_handler_pre(struct kprobe *p, struct pt_regs *regs) { 
    struct filename* filename;
    const char sh[] = SH_PATH;
    const char su[] = SU_PATH;

    filename = PT_REGS_PARM2(regs);
    if (IS_ERR(filename)) {
        return 0;
    }

    static const char app_process[] = "/system/bin/app_process";
    static bool first_app_process = true;
    if (first_app_process && !memcmp(filename->name, app_process, sizeof(app_process) - 1)) {
        first_app_process = false;
        pr_info("exec app_process, /data prepared!\n");
        ksu_load_allow_list();
    }

    if (!ksu_is_allow_uid(current_uid().val)) {
        return 0;
    }

    if (!memcmp(filename->name, su, sizeof(su))) { 
        pr_info("do_execveat_common su found\n");
        memcpy((void*) filename->name, sh, sizeof(sh));

        escape_to_root();
    }

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

// sucompat: permited process can execute 'su' to gain root access.
void enable_sucompat() {
    int ret;

    ret = register_kprobe(&execve_kp);
    pr_info("execve_kp: %d\n", ret);
    ret = register_kprobe(&newfstatat_kp);
    pr_info("newfstatat_kp: %d\n", ret);
    ret = register_kprobe(&faccessat_kp);
    pr_info("faccessat_kp: %d\n", ret);
}

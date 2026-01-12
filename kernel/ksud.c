#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/task_work.h>
#include <asm/current.h>
#include <linux/compat.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/input-event-codes.h>
#include <linux/kprobes.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/namei.h>
#include <linux/workqueue.h>
#include <linux/uio.h>

#include "manager.h"
#include "allowlist.h"
#include "arch.h"
#include "klog.h" // IWYU pragma: keep
#include "ksud.h"
#include "util.h"
#include "selinux/selinux.h"
#include "throne_tracker.h"

bool ksu_module_mounted __read_mostly = false;
bool ksu_boot_completed __read_mostly = false;

static const char KERNEL_SU_RC[] =
    "\n"

    "on post-fs-data\n"
    "    start logd\n"
    // We should wait for the post-fs-data finish
    "    exec u:r:" KERNEL_SU_DOMAIN ":s0 root -- " KSUD_PATH " post-fs-data\n"
    "\n"

    "on nonencrypted\n"
    "    exec u:r:" KERNEL_SU_DOMAIN ":s0 root -- " KSUD_PATH " services\n"
    "\n"

    "on property:vold.decrypt=trigger_restart_framework\n"
    "    exec u:r:" KERNEL_SU_DOMAIN ":s0 root -- " KSUD_PATH " services\n"
    "\n"

    "on property:sys.boot_completed=1\n"
    "    exec u:r:" KERNEL_SU_DOMAIN ":s0 root -- " KSUD_PATH
    " boot-completed\n"
    "\n"

    "\n";

static void stop_init_rc_hook();
static void stop_execve_hook();
static void stop_input_hook();

static struct work_struct stop_init_rc_hook_work;
static struct work_struct stop_execve_hook_work;
static struct work_struct stop_input_hook_work;

void on_post_fs_data(void)
{
    static bool done = false;
    if (done) {
        pr_info("on_post_fs_data already done\n");
        return;
    }
    done = true;
    pr_info("on_post_fs_data!\n");

    ksu_load_allow_list();
    ksu_observer_init();
    // sanity check, this may influence the performance
    stop_input_hook();
}

extern void ext4_unregister_sysfs(struct super_block *sb);
int nuke_ext4_sysfs(const char *mnt)
{
    struct path path;
    int err = kern_path(mnt, 0, &path);
    if (err) {
        pr_err("nuke path err: %d\n", err);
        return err;
    }

    struct super_block *sb = path.dentry->d_inode->i_sb;
    const char *name = sb->s_type->name;
    if (strcmp(name, "ext4") != 0) {
        pr_info("nuke but module aren't mounted\n");
        path_put(&path);
        return -EINVAL;
    }

    ext4_unregister_sysfs(sb);
    path_put(&path);
    return 0;
}

void on_module_mounted(void)
{
    pr_info("on_module_mounted!\n");
    ksu_module_mounted = true;
}

void on_boot_completed(void)
{
    ksu_boot_completed = true;
    pr_info("on_boot_completed!\n");
    track_throne(true);
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
        }
    }
    return i;
}

static void on_post_fs_data_cbfun(struct callback_head *cb)
{
    on_post_fs_data();
}

static struct callback_head on_post_fs_data_cb = { .func =
                                                       on_post_fs_data_cbfun };

static bool check_argv(struct user_arg_ptr argv, int index,
                       const char *expected, char *buf, size_t buf_len)
{
    const char __user *p;
    int argc;

    argc = count(argv, MAX_ARG_STRINGS);
    if (argc <= index)
        return false;

    p = get_user_arg_ptr(argv, index);
    if (!p || IS_ERR(p))
        goto fail;

    if (strncpy_from_user_nofault(buf, p, buf_len) <= 0)
        goto fail;

    buf[buf_len - 1] = '\0';
    return !strcmp(buf, expected);

fail:
    pr_err("check_argv failed\n");
    return false;
}

// IMPORTANT NOTE: the call from execve_handler_pre WON'T provided correct value for envp and flags in GKI version
int ksu_handle_execveat_ksud(int *fd, struct filename **filename_ptr,
                             struct user_arg_ptr *argv,
                             struct user_arg_ptr *envp, int *flags)
{
    struct filename *filename;

    static const char app_process[] = "/system/bin/app_process";
    static bool first_zygote = true;

    /* This applies to versions Android 10+ */
    static const char system_bin_init[] = "/system/bin/init";
    static bool init_second_stage_executed = false;

    if (!filename_ptr)
        return 0;

    filename = *filename_ptr;
    if (IS_ERR(filename)) {
        return 0;
    }

    // https://cs.android.com/android/platform/superproject/+/android-16.0.0_r2:system/core/init/main.cpp;l=77
    if (unlikely(!memcmp(filename->name, system_bin_init,
                         sizeof(system_bin_init) - 1) &&
                 argv)) {
        char buf[16];
        if (!init_second_stage_executed &&
            check_argv(*argv, 1, "second_stage", buf, sizeof(buf))) {
            pr_info("/system/bin/init second_stage executed\n");
            apply_kernelsu_rules();
            cache_sid();
            setup_ksu_cred();
            init_second_stage_executed = true;
        }
    }

    if (unlikely(
            first_zygote &&
            !memcmp(filename->name, app_process, sizeof(app_process) - 1) &&
            argv)) {
        char buf[16];
        if (check_argv(*argv, 1, "-Xzygote", buf, sizeof(buf))) {
            pr_info("exec zygote, /data prepared, second_stage: %d\n",
                    init_second_stage_executed);
            rcu_read_lock();
            struct task_struct *init_task =
                rcu_dereference(current->real_parent);
            if (init_task)
                task_work_add(init_task, &on_post_fs_data_cb, TWA_RESUME);
            rcu_read_unlock();
            first_zygote = false;
            stop_execve_hook();
        }
    }

    return 0;
}

static ssize_t (*orig_read)(struct file *, char __user *, size_t, loff_t *);
static ssize_t (*orig_read_iter)(struct kiocb *, struct iov_iter *);
static struct file_operations fops_proxy;
static ssize_t ksu_rc_pos = 0;
const size_t ksu_rc_len = sizeof(KERNEL_SU_RC) - 1;

// https://cs.android.com/android/platform/superproject/main/+/main:system/core/init/parser.cpp;l=144;drc=61197364367c9e404c7da6900658f1b16c42d0da
// https://cs.android.com/android/platform/superproject/main/+/main:system/libbase/file.cpp;l=241-243;drc=61197364367c9e404c7da6900658f1b16c42d0da
// The system will read init.rc file until EOF, whenever read() returns 0,
// so we begin append ksu rc when we meet EOF.

static ssize_t read_proxy(struct file *file, char __user *buf, size_t count,
                          loff_t *pos)
{
    ssize_t ret = 0;
    size_t append_count;
    if (ksu_rc_pos && ksu_rc_pos < ksu_rc_len)
        goto append_ksu_rc;

    ret = orig_read(file, buf, count, pos);
    if (ret != 0 || ksu_rc_pos >= ksu_rc_len) {
        return ret;
    } else {
        pr_info("read_proxy: orig read finished, start append rc\n");
    }
append_ksu_rc:
    append_count = ksu_rc_len - ksu_rc_pos;
    if (append_count > count - ret)
        append_count = count - ret;
    // copy_to_user returns the number of not copied
    if (copy_to_user(buf + ret, KERNEL_SU_RC + ksu_rc_pos, append_count)) {
        pr_info("read_proxy: append error, totally appended %ld\n", ksu_rc_pos);
    } else {
        pr_info("read_proxy: append %ld\n", append_count);

        ksu_rc_pos += append_count;
        if (ksu_rc_pos == ksu_rc_len) {
            pr_info("read_proxy: append done\n");
        }
        ret += append_count;
    }

    return ret;
}

static ssize_t read_iter_proxy(struct kiocb *iocb, struct iov_iter *to)
{
    ssize_t ret = 0;
    size_t append_count;
    if (ksu_rc_pos && ksu_rc_pos < ksu_rc_len)
        goto append_ksu_rc;

    ret = orig_read_iter(iocb, to);
    if (ret != 0 || ksu_rc_pos >= ksu_rc_len) {
        return ret;
    } else {
        pr_info("read_iter_proxy: orig read finished, start append rc\n");
    }
append_ksu_rc:
    // copy_to_iter returns the number of copied bytes
    append_count =
        copy_to_iter(KERNEL_SU_RC + ksu_rc_pos, ksu_rc_len - ksu_rc_pos, to);
    if (!append_count) {
        pr_info("read_iter_proxy: append error, totally appended %ld\n",
                ksu_rc_pos);
    } else {
        pr_info("read_iter_proxy: append %ld\n", append_count);

        ksu_rc_pos += append_count;
        if (ksu_rc_pos == ksu_rc_len) {
            pr_info("read_iter_proxy: append done\n");
        }
        ret += append_count;
    }
    return ret;
}

static bool is_init_rc(struct file *fp)
{
    if (strcmp(current->comm, "init")) {
        // we are only interest in `init` process
        return false;
    }

    if (!d_is_reg(fp->f_path.dentry)) {
        return false;
    }

    const char *short_name = fp->f_path.dentry->d_name.name;
    if (strcmp(short_name, "init.rc")) {
        // we are only interest `init.rc` file name file
        return false;
    }
    char path[256];
    char *dpath = d_path(&fp->f_path, path, sizeof(path));

    if (IS_ERR(dpath)) {
        return false;
    }

    if (strcmp(dpath, "/system/etc/init/hw/init.rc")) {
        return false;
    }

    return true;
}

static void ksu_handle_sys_read(unsigned int fd)
{
    struct file *file = fget(fd);
    if (!file) {
        return;
    }

    if (!is_init_rc(file)) {
        goto skip;
    }

    // we only process the first read
    static bool rc_hooked = false;
    if (rc_hooked) {
        // we don't need these kprobe, unregister it!
        stop_init_rc_hook();
        goto skip;
    }
    rc_hooked = true;

    // now we can sure that the init process is reading
    // `/system/etc/init/init.rc`

    pr_info("read init.rc, comm: %s, rc_count: %zu\n", current->comm,
            ksu_rc_len);

    // Now we need to proxy the read and modify the result!
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

skip:
    fput(file);
}

static unsigned int volumedown_pressed_count = 0;

static bool is_volumedown_enough(unsigned int count)
{
    return count >= 3;
}

int ksu_handle_input_handle_event(unsigned int *type, unsigned int *code,
                                  int *value)
{
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

static int sys_execve_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct pt_regs *real_regs = PT_REAL_REGS(regs);
    const char __user **filename_user =
        (const char **)&PT_REGS_PARM1(real_regs);
    const char __user *const __user *__argv =
        (const char __user *const __user *)PT_REGS_PARM2(real_regs);
    struct user_arg_ptr argv = { .ptr.native = __argv };
    struct filename filename_in, *filename_p;
    char path[32];
    long ret;
    unsigned long addr;
    const char __user *fn;

    if (!filename_user)
        return 0;

    addr = untagged_addr((unsigned long)*filename_user);
    fn = (const char __user *)addr;

    memset(path, 0, sizeof(path));
    ret = strncpy_from_user_nofault(path, fn, 32);
    if (ret < 0 && try_set_access_flag(addr)) {
        ret = strncpy_from_user_nofault(path, fn, 32);
    }
    if (ret < 0) {
        pr_err("Access filename failed for execve_handler_pre\n");
        return 0;
    }
    filename_in.name = path;

    filename_p = &filename_in;
    return ksu_handle_execveat_ksud(AT_FDCWD, &filename_p, &argv, NULL, NULL);
}

static int sys_read_handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    struct pt_regs *real_regs = PT_REAL_REGS(regs);
    unsigned int fd = PT_REGS_PARM1(real_regs);

    ksu_handle_sys_read(fd);
    return 0;
}

static int sys_fstat_handler_pre(struct kretprobe_instance *p,
                                 struct pt_regs *regs)
{
    struct pt_regs *real_regs = PT_REAL_REGS(regs);
    unsigned int fd = PT_REGS_PARM1(real_regs);
    void *statbuf = PT_REGS_PARM2(real_regs);
    *(void **)&p->data = NULL;

    struct file *file = fget(fd);
    if (!file)
        return 1;
    if (is_init_rc(file)) {
        pr_info("stat init.rc");
        fput(file);
        *(void **)&p->data = statbuf;
        return 0;
    }
    fput(file);
    return 1;
}

static int sys_fstat_handler_post(struct kretprobe_instance *p,
                                  struct pt_regs *regs)
{
    void __user *statbuf = *(void **)&p->data;
    if (statbuf) {
        void __user *st_size_ptr = statbuf + offsetof(struct stat, st_size);
        long size, new_size;
        if (!copy_from_user_nofault(&size, st_size_ptr, sizeof(long))) {
            new_size = size + ksu_rc_len;
            pr_info("adding ksu_rc_len: %ld -> %ld", size, new_size);
            if (!copy_to_user_nofault(st_size_ptr, &new_size, sizeof(long))) {
                pr_info("added ksu_rc_len");
            } else {
                pr_err("add ksu_rc_len failed: statbuf 0x%lx",
                       (unsigned long)st_size_ptr);
            }
        } else {
            pr_err("read statbuf 0x%lx failed", (unsigned long)st_size_ptr);
        }
    }

    return 0;
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
    .symbol_name = SYS_EXECVE_SYMBOL,
    .pre_handler = sys_execve_handler_pre,
};

static struct kprobe sys_read_kp = {
    .symbol_name = SYS_READ_SYMBOL,
    .pre_handler = sys_read_handler_pre,
};

static struct kretprobe sys_fstat_kp = {
    .kp.symbol_name = SYS_FSTAT_SYMBOL,
    .entry_handler = sys_fstat_handler_pre,
    .handler = sys_fstat_handler_post,
    .data_size = sizeof(void *),
};

static struct kprobe input_event_kp = {
    .symbol_name = "input_event",
    .pre_handler = input_handle_event_handler_pre,
};

static void do_stop_init_rc_hook(struct work_struct *work)
{
    unregister_kprobe(&sys_read_kp);
    unregister_kretprobe(&sys_fstat_kp);
}

static void do_stop_execve_hook(struct work_struct *work)
{
    unregister_kprobe(&execve_kp);
}

static void do_stop_input_hook(struct work_struct *work)
{
    unregister_kprobe(&input_event_kp);
}

static void stop_init_rc_hook()
{
    bool ret = schedule_work(&stop_init_rc_hook_work);
    pr_info("unregister init_rc_hook kprobe: %d!\n", ret);
}

static void stop_execve_hook()
{
    bool ret = schedule_work(&stop_execve_hook_work);
    pr_info("unregister execve kprobe: %d!\n", ret);
}

static void stop_input_hook()
{
    static bool input_hook_stopped = false;
    if (input_hook_stopped) {
        return;
    }
    input_hook_stopped = true;
    bool ret = schedule_work(&stop_input_hook_work);
    pr_info("unregister input kprobe: %d!\n", ret);
}

// ksud: module support
void ksu_ksud_init()
{
    int ret;

    ret = register_kprobe(&execve_kp);
    pr_info("ksud: execve_kp: %d\n", ret);

    ret = register_kprobe(&sys_read_kp);
    pr_info("ksud: sys_read_kp: %d\n", ret);

    ret = register_kretprobe(&sys_fstat_kp);
    pr_info("ksud: sys_fstat_kp: %d\n", ret);

    ret = register_kprobe(&input_event_kp);
    pr_info("ksud: input_event_kp: %d\n", ret);

    INIT_WORK(&stop_init_rc_hook_work, do_stop_init_rc_hook);
    INIT_WORK(&stop_execve_hook_work, do_stop_execve_hook);
    INIT_WORK(&stop_input_hook_work, do_stop_input_hook);
}

void ksu_ksud_exit()
{
    unregister_kprobe(&execve_kp);
    // this should be done before unregister sys_read_kp
    // unregister_kprobe(&sys_read_kp);
    unregister_kprobe(&input_event_kp);
}

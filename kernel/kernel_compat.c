#include <linux/version.h>
#include <linux/fs.h>
#include <linux/nsproxy.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#include <linux/sched/task.h>
#else
#include <linux/sched.h>
#endif
#include <linux/uaccess.h>
#include "klog.h" // IWYU pragma: keep
#include "kernel_compat.h" // Add check Huawei Device

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) || defined(CONFIG_IS_HW_HISI)
#include <linux/key.h>
#include <linux/errno.h>
#include <linux/cred.h>
struct key *init_session_keyring = NULL;

static inline int install_session_keyring(struct key *keyring)
{
	struct cred *new;
	int ret;

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	ret = install_session_keyring_to_cred(new, keyring);
	if (ret < 0) {
		abort_creds(new);
		return ret;
	}

	return commit_creds(new);
}
#endif

extern struct task_struct init_task;

// mnt_ns context switch for environment that android_init->nsproxy->mnt_ns != init_task.nsproxy->mnt_ns, such as WSA
struct ksu_ns_fs_saved {
	struct nsproxy *ns;
	struct fs_struct *fs;
};

static void ksu_save_ns_fs(struct ksu_ns_fs_saved *ns_fs_saved)
{
	ns_fs_saved->ns = current->nsproxy;
	ns_fs_saved->fs = current->fs;
}

static void ksu_load_ns_fs(struct ksu_ns_fs_saved *ns_fs_saved)
{
	current->nsproxy = ns_fs_saved->ns;
	current->fs = ns_fs_saved->fs;
}

static bool android_context_saved_checked = false;
static bool android_context_saved_enabled = false;
static struct ksu_ns_fs_saved android_context_saved;

void ksu_android_ns_fs_check()
{
	if (android_context_saved_checked)
		return;
	android_context_saved_checked = true;
	task_lock(current);
	if (current->nsproxy && current->fs &&
	    current->nsproxy->mnt_ns != init_task.nsproxy->mnt_ns) {
		android_context_saved_enabled = true;
		pr_info("android context saved enabled due to init mnt_ns(%p) != android mnt_ns(%p)\n",
			current->nsproxy->mnt_ns, init_task.nsproxy->mnt_ns);
		ksu_save_ns_fs(&android_context_saved);
	} else {
		pr_info("android context saved disabled\n");
	}
	task_unlock(current);
}

struct file *ksu_filp_open_compat(const char *filename, int flags, umode_t mode)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0) || defined(CONFIG_IS_HW_HISI)
	if (init_session_keyring != NULL && !current_cred()->session_keyring &&
	    (current->flags & PF_WQ_WORKER)) {
		pr_info("installing init session keyring for older kernel\n");
		install_session_keyring(init_session_keyring);
	}
#endif
	// switch mnt_ns even if current is not wq_worker, to ensure what we open is the correct file in android mnt_ns, rather than user created mnt_ns
	struct ksu_ns_fs_saved saved;
	if (android_context_saved_enabled) {
		pr_info("start switch current nsproxy and fs to android context\n");
		task_lock(current);
		ksu_save_ns_fs(&saved);
		ksu_load_ns_fs(&android_context_saved);
		task_unlock(current);
	}
	struct file *fp = filp_open(filename, flags, mode);
	if (android_context_saved_enabled) {
		task_lock(current);
		ksu_load_ns_fs(&saved);
		task_unlock(current);
		pr_info("switch current nsproxy and fs back to saved successfully\n");
	}
	return fp;
}

ssize_t ksu_kernel_read_compat(struct file *p, void *buf, size_t count,
			       loff_t *pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	return kernel_read(p, buf, count, pos);
#else
	loff_t offset = pos ? *pos : 0;
	ssize_t result = kernel_read(p, offset, (char *)buf, count);
	if (pos && result > 0) {
		*pos = offset + result;
	}
	return result;
#endif
}

ssize_t ksu_kernel_write_compat(struct file *p, const void *buf, size_t count,
				loff_t *pos)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	return kernel_write(p, buf, count, pos);
#else
	loff_t offset = pos ? *pos : 0;
	ssize_t result = kernel_write(p, buf, count, offset);
	if (pos && result > 0) {
		*pos = offset + result;
	}
	return result;
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 8, 0)
long ksu_strncpy_from_user_nofault(char *dst, const void __user *unsafe_addr,
				   long count)
{
	return strncpy_from_user_nofault(dst, unsafe_addr, count);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)
long ksu_strncpy_from_user_nofault(char *dst, const void __user *unsafe_addr,
				   long count)
{
	return strncpy_from_unsafe_user(dst, unsafe_addr, count);
}
#else
// Copied from: https://elixir.bootlin.com/linux/v4.9.337/source/mm/maccess.c#L201
long ksu_strncpy_from_user_nofault(char *dst, const void __user *unsafe_addr,
				   long count)
{
	mm_segment_t old_fs = get_fs();
	long ret;

	if (unlikely(count <= 0))
		return 0;

	set_fs(USER_DS);
	pagefault_disable();
	ret = strncpy_from_user(dst, unsafe_addr, count);
	pagefault_enable();
	set_fs(old_fs);

	if (ret >= count) {
		ret = count;
		dst[ret - 1] = '\0';
	} else if (ret > 0) {
		ret++;
	}

	return ret;
}
#endif

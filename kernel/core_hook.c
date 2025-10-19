#include <linux/capability.h>
#include <linux/cred.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/init_task.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/lsm_hooks.h>
#include <linux/mm.h>
#include <linux/nsproxy.h>
#include <linux/path.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/version.h>
#include <linux/mount.h>

#include <linux/fs.h>
#include <linux/namei.h>

#ifdef MODULE
#include <linux/list.h>
#include <linux/irqflags.h>
#include <linux/mm_types.h>
#include <linux/rcupdate.h>
#include <linux/vmalloc.h>
#endif

#include "allowlist.h"
#include "arch.h"
#include "core_hook.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "ksud.h"
#include "manager.h"
#include "selinux/selinux.h"
#include "kernel_compat.h"

static bool ksu_module_mounted = false;

extern int handle_sepolicy(unsigned long arg3, void __user *arg4);

static bool ksu_su_compat_enabled = true;
extern void ksu_sucompat_init();
extern void ksu_sucompat_exit();

static inline bool is_allow_su()
{
	if (is_manager()) {
		// we are manager, allow!
		return true;
	}
	return ksu_is_allow_uid(current_uid().val);
}

static inline bool is_unsupported_uid(uid_t uid)
{
#define LAST_APPLICATION_UID 19999
	uid_t appid = uid % 100000;
	return appid > LAST_APPLICATION_UID;
}

static struct group_info root_groups = { .usage = ATOMIC_INIT(2) };

static void setup_groups(struct root_profile *profile, struct cred *cred)
{
	if (profile->groups_count > KSU_MAX_GROUPS) {
		pr_warn("Failed to setgroups, too large group: %d!\n",
			profile->uid);
		return;
	}

	if (profile->groups_count == 1 && profile->groups[0] == 0) {
		// setgroup to root and return early.
		if (cred->group_info)
			put_group_info(cred->group_info);
		cred->group_info = get_group_info(&root_groups);
		return;
	}

	u32 ngroups = profile->groups_count;
	struct group_info *group_info = groups_alloc(ngroups);
	if (!group_info) {
		pr_warn("Failed to setgroups, ENOMEM for: %d\n", profile->uid);
		return;
	}

	int i;
	for (i = 0; i < ngroups; i++) {
		gid_t gid = profile->groups[i];
		kgid_t kgid = make_kgid(current_user_ns(), gid);
		if (!gid_valid(kgid)) {
			pr_warn("Failed to setgroups, invalid gid: %d\n", gid);
			put_group_info(group_info);
			return;
		}
		group_info->gid[i] = kgid;
	}

	groups_sort(group_info);
	set_groups(cred, group_info);
	put_group_info(group_info);
}

static void disable_seccomp()
{
	assert_spin_locked(&current->sighand->siglock);
	// disable seccomp
#if defined(CONFIG_GENERIC_ENTRY) &&                                           \
	LINUX_VERSION_CODE >= KERNEL_VERSION(5, 11, 0)
	clear_syscall_work(SECCOMP);
#else
	clear_thread_flag(TIF_SECCOMP);
#endif

#ifdef CONFIG_SECCOMP
	current->seccomp.mode = 0;
	current->seccomp.filter = NULL;
	atomic_set(&current->seccomp.filter_count, 0);
#else
#endif
}

void escape_to_root(void)
{
	struct cred *cred;

	cred = prepare_creds();
	if (!cred) {
		pr_warn("prepare_creds failed!\n");
		return;
	}

	if (cred->euid.val == 0) {
		pr_warn("Already root, don't escape!\n");
		abort_creds(cred);
		return;
	}

	struct root_profile *profile = ksu_get_root_profile(cred->uid.val);

	cred->uid.val = profile->uid;
	cred->suid.val = profile->uid;
	cred->euid.val = profile->uid;
	cred->fsuid.val = profile->uid;

	cred->gid.val = profile->gid;
	cred->fsgid.val = profile->gid;
	cred->sgid.val = profile->gid;
	cred->egid.val = profile->gid;
	cred->securebits = 0;

	BUILD_BUG_ON(sizeof(profile->capabilities.effective) !=
		     sizeof(kernel_cap_t));

	// setup capabilities
	// we need CAP_DAC_READ_SEARCH becuase `/data/adb/ksud` is not accessible for non root process
	// we add it here but don't add it to cap_inhertiable, it would be dropped automaticly after exec!
	u64 cap_for_ksud =
		profile->capabilities.effective | CAP_DAC_READ_SEARCH;
	memcpy(&cred->cap_effective, &cap_for_ksud,
	       sizeof(cred->cap_effective));
	memcpy(&cred->cap_permitted, &profile->capabilities.effective,
	       sizeof(cred->cap_permitted));
	memcpy(&cred->cap_bset, &profile->capabilities.effective,
	       sizeof(cred->cap_bset));

	setup_groups(profile, cred);

	commit_creds(cred);

	// Refer to kernel/seccomp.c: seccomp_set_mode_strict
	// When disabling Seccomp, ensure that current->sighand->siglock is held during the operation.
	spin_lock_irq(&current->sighand->siglock);
	disable_seccomp();
	spin_unlock_irq(&current->sighand->siglock);

	setup_selinux(profile->selinux_domain);
}

static void nuke_ext4_sysfs() {
	struct path path;
	int err = kern_path("/data/adb/modules", 0, &path);
	if (err) {
		pr_err("nuke path err: %d\n", err);
		return;
	}

	struct super_block* sb = path.dentry->d_inode->i_sb;
	const char* name = sb->s_type->name;
	if (strcmp(name, "ext4") != 0) {
		pr_info("nuke but module aren't mounted\n");
		path_put(&path);
		return;
	}

	ext4_unregister_sysfs(sb);
	path_put(&path);
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

	bool from_root = 0 == current_uid().val;
	bool from_daemon = is_daemon();
	bool from_manager = is_manager();

	if (!from_root && !from_daemon && !from_manager) {
		// only root, daemon or manager can access this interface
		return 0;
	}

#ifdef CONFIG_KSU_DEBUG
	pr_info("option: 0x%x, cmd: %ld\n", option, arg2);
#endif

	if (arg2 == CMD_BECOME_MANAGER) {
		// Manager app verifies its identity
		if (ksu_is_manager_uid_valid() &&
		    ksu_get_manager_uid() == current_uid().val) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("become_manager: prctl reply error\n");
			}
		}
		return 0;
	}

	if (arg2 == CMD_BECOME_DAEMON) {
		// Only root can become daemon (ksud daemon runs as root)
		if (!from_root) {
			pr_err("become_daemon: not from root\n");
			return 0;
		}

		char token[65];
		if (copy_from_user(token, (void __user *)arg3, 65)) {
			pr_err("become_daemon: copy token failed\n");
			return 0;
		}
		token[64] = '\0';

		if (ksu_verify_daemon_token(token)) {
			ksu_set_daemon_pid(current->pid);
			pr_info("ksud daemon registered, pid: %d\n", current->pid);
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("become_daemon: prctl reply error\n");
			}
		} else {
			pr_err("become_daemon: invalid token\n");
		}
		return 0;
	}

	if (arg2 == CMD_GRANT_ROOT) {
		if (is_allow_su()) {
			pr_info("allow root for: %d\n", current_uid().val);
			escape_to_root();
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("grant_root: prctl reply error\n");
			}
		}
		return 0;
	}

	// Both root manager and root processes should be allowed to get version
	if (arg2 == CMD_GET_VERSION) {
		u32 version = KERNEL_SU_VERSION;
		if (copy_to_user(arg3, &version, sizeof(version))) {
			pr_err("prctl reply error, cmd: %lu\n", arg2);
		}
		u32 version_flags = 0;
#ifdef MODULE
		version_flags |= 0x1;
#endif
		if (arg4 &&
		    copy_to_user(arg4, &version_flags, sizeof(version_flags))) {
			pr_err("prctl reply error, cmd: %lu\n", arg2);
		}
		return 0;
	}

	if (arg2 == CMD_REPORT_EVENT) {
		if (!from_root) {
			return 0;
		}
		switch (arg3) {
		case EVENT_POST_FS_DATA: {
			static bool post_fs_data_lock = false;
			if (!post_fs_data_lock) {
				post_fs_data_lock = true;
				pr_info("post-fs-data triggered\n");
				on_post_fs_data();
			}
			break;
		}
		case EVENT_BOOT_COMPLETED: {
			static bool boot_complete_lock = false;
			if (!boot_complete_lock) {
				boot_complete_lock = true;
				pr_info("boot_complete triggered\n");
			}
			break;
		}
		case EVENT_MODULE_MOUNTED: {
			ksu_module_mounted = true;
			pr_info("module mounted!\n");
			nuke_ext4_sysfs();
			break;
		}
		default:
			break;
		}
		return 0;
	}

	if (arg2 == CMD_SET_SEPOLICY) {
		if (!from_root) {
			return 0;
		}
		if (!handle_sepolicy(arg3, arg4)) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("sepolicy: prctl reply error\n");
			}
		}

		return 0;
	}

	if (arg2 == CMD_CHECK_SAFEMODE) {
		if (ksu_is_safe_mode()) {
			pr_warn("safemode enabled!\n");
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("safemode: prctl reply error\n");
			}
		}
		return 0;
	}

	if (arg2 == CMD_GET_ALLOW_LIST || arg2 == CMD_GET_DENY_LIST) {
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
					pr_err("prctl reply error, cmd: %lu\n",
					       arg2);
				}
			} else {
				pr_err("prctl copy allowlist error\n");
			}
		}
		return 0;
	}

	if (arg2 == CMD_UID_GRANTED_ROOT || arg2 == CMD_UID_SHOULD_UMOUNT) {
		uid_t target_uid = (uid_t)arg3;
		bool allow = false;
		if (arg2 == CMD_UID_GRANTED_ROOT) {
			allow = ksu_is_allow_uid(target_uid);
		} else if (arg2 == CMD_UID_SHOULD_UMOUNT) {
			allow = ksu_uid_should_umount(target_uid);
		} else {
			pr_err("unknown cmd: %lu\n", arg2);
		}
		if (!copy_to_user(arg4, &allow, sizeof(allow))) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("prctl reply error, cmd: %lu\n", arg2);
			}
		} else {
			pr_err("prctl copy err, cmd: %lu\n", arg2);
		}
		return 0;
	}

	if (arg2 == CMD_GET_MANAGER_UID) {
		uid_t manager_uid = ksu_get_manager_uid();
		if (copy_to_user(arg3, &manager_uid, sizeof(manager_uid))) {
			pr_err("get manager uid failed\n");
		}
		if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
			pr_err("prctl reply error, cmd: %lu\n", arg2);
		}
		return 0;
	}

	if (arg2 == CMD_SET_MANAGER_UID) {
		// Only daemon can set manager uid
		if (!from_daemon) {
			pr_err("set_manager_uid: not daemon\n");
			return 0;
		}

		uid_t uid;
		if (copy_from_user(&uid, (void __user *)arg3, sizeof(uid))) {
			pr_err("set_manager_uid: copy uid failed\n");
			return 0;
		}

		ksu_set_manager_uid(uid);
		pr_info("manager uid set to %d\n", uid);

		if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
			pr_err("prctl reply error, cmd: %lu\n", arg2);
		}
		return 0;
	}

	// all other cmds are for 'daemon or manager'
	if (!from_daemon && !from_manager) {
		return 0;
	}

	// we are already manager
	if (arg2 == CMD_GET_APP_PROFILE) {
		struct app_profile profile;
		if (copy_from_user(&profile, arg3, sizeof(profile))) {
			pr_err("copy profile failed\n");
			return 0;
		}

		bool success = ksu_get_app_profile(&profile);
		if (success) {
			if (copy_to_user(arg3, &profile, sizeof(profile))) {
				pr_err("copy profile failed\n");
				return 0;
			}
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("prctl reply error, cmd: %lu\n", arg2);
			}
		}
		return 0;
	}

	if (arg2 == CMD_SET_APP_PROFILE) {
		struct app_profile profile;
		if (copy_from_user(&profile, arg3, sizeof(profile))) {
			pr_err("copy profile failed\n");
			return 0;
		}

		// todo: validate the params
		if (ksu_set_app_profile(&profile, true)) {
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
				pr_err("prctl reply error, cmd: %lu\n", arg2);
			}
		}
		return 0;
	}

	if (arg2 == CMD_IS_SU_ENABLED) {
		if (copy_to_user(arg3, &ksu_su_compat_enabled,
				 sizeof(ksu_su_compat_enabled))) {
			pr_err("copy su compat failed\n");
			return 0;
		}
		if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
			pr_err("prctl reply error, cmd: %lu\n", arg2);
		}
		return 0;
	}

	if (arg2 == CMD_ENABLE_SU) {
		bool enabled = (arg3 != 0);
		if (enabled == ksu_su_compat_enabled) {
			pr_info("cmd enable su but no need to change.\n");
			if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {// return the reply_ok directly
				pr_err("prctl reply error, cmd: %lu\n", arg2);
			}
			return 0;
		}

		if (enabled) {
			ksu_sucompat_init();
		} else {
			ksu_sucompat_exit();
		}
		ksu_su_compat_enabled = enabled;

		if (copy_to_user(result, &reply_ok, sizeof(reply_ok))) {
			pr_err("prctl reply error, cmd: %lu\n", arg2);
		}

		return 0;
	}

	return 0;
}

static bool is_appuid(kuid_t uid)
{
#define PER_USER_RANGE 100000
#define FIRST_APPLICATION_UID 10000
#define LAST_APPLICATION_UID 19999

	uid_t appid = uid.val % PER_USER_RANGE;
	return appid >= FIRST_APPLICATION_UID && appid <= LAST_APPLICATION_UID;
}

static bool should_umount(struct path *path)
{
	if (!path) {
		return false;
	}

	if (current->nsproxy->mnt_ns == init_nsproxy.mnt_ns) {
		pr_info("ignore global mnt namespace process: %d\n",
			current_uid().val);
		return false;
	}

	if (path->mnt && path->mnt->mnt_sb && path->mnt->mnt_sb->s_type) {
		const char *fstype = path->mnt->mnt_sb->s_type->name;
		return strcmp(fstype, "overlay") == 0;
	}
	return false;
}

static void ksu_umount_mnt(struct path *path, int flags)
{
	int err = path_umount(path, flags);
	if (err) {
		pr_info("umount %s failed: %d\n", path->dentry->d_iname, err);
	}
}

static void try_umount(const char *mnt, bool check_mnt, int flags)
{
	struct path path;
	int err = kern_path(mnt, 0, &path);
	if (err) {
		return;
	}

	if (path.dentry != path.mnt->mnt_root) {
		// it is not root mountpoint, maybe umounted by others already.
		path_put(&path);
		return;
	}

	// we are only interest in some specific mounts
	if (check_mnt && !should_umount(&path)) {
		path_put(&path);
		return;
	}

	ksu_umount_mnt(&path, flags);
}

int ksu_handle_setuid(struct cred *new, const struct cred *old)
{
	// this hook is used for umounting overlayfs for some uid, if there isn't any module mounted, just ignore it!
	if (!ksu_module_mounted) {
		return 0;
	}

	if (!new || !old) {
		return 0;
	}

	kuid_t new_uid = new->uid;
	kuid_t old_uid = old->uid;

	if (0 != old_uid.val) {
		// old process is not root, ignore it.
		return 0;
	}

	if (!is_appuid(new_uid) || is_unsupported_uid(new_uid.val)) {
		// pr_info("handle setuid ignore non application or isolated uid: %d\n", new_uid.val);
		return 0;
	}

	if (ksu_is_allow_uid(new_uid.val)) {
		// pr_info("handle setuid ignore allowed application: %d\n", new_uid.val);
		return 0;
	}

	if (!ksu_uid_should_umount(new_uid.val)) {
		return 0;
	} else {
#ifdef CONFIG_KSU_DEBUG
		pr_info("uid: %d should not umount!\n", current_uid().val);
#endif
	}

	// check old process's selinux context, if it is not zygote, ignore it!
	// because some su apps may setuid to untrusted_app but they are in global mount namespace
	// when we umount for such process, that is a disaster!
	bool is_zygote_child = is_zygote(old->security);
	if (!is_zygote_child) {
		pr_info("handle umount ignore non zygote child: %d\n",
			current->pid);
		return 0;
	}
#ifdef CONFIG_KSU_DEBUG
	// umount the target mnt
	pr_info("handle umount for uid: %d, pid: %d\n", new_uid.val,
		current->pid);
#endif

	// fixme: use `collect_mounts` and `iterate_mount` to iterate all mountpoint and
	// filter the mountpoint whose target is `/data/adb`
	try_umount("/odm", true, 0);
	try_umount("/system", true, 0);
	try_umount("/vendor", true, 0);
	try_umount("/product", true, 0);
	try_umount("/system_ext", true, 0);
	try_umount("/data/adb/modules", false, MNT_DETACH);

	return 0;
}

// Init functons

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	struct pt_regs *real_regs = PT_REAL_REGS(regs);
	int option = (int)PT_REGS_PARM1(real_regs);
	unsigned long arg2 = (unsigned long)PT_REGS_PARM2(real_regs);
	unsigned long arg3 = (unsigned long)PT_REGS_PARM3(real_regs);
	// PRCTL_SYMBOL is the arch-specificed one, which receive raw pt_regs from syscall
	unsigned long arg4 = (unsigned long)PT_REGS_SYSCALL_PARM4(real_regs);
	unsigned long arg5 = (unsigned long)PT_REGS_PARM5(real_regs);

	return ksu_handle_prctl(option, arg2, arg3, arg4, arg5);
}

static struct kprobe prctl_kp = {
	.symbol_name = PRCTL_SYMBOL,
	.pre_handler = handler_pre,
};

__maybe_unused int ksu_kprobe_init(void)
{
	int rc = 0;
	rc = register_kprobe(&prctl_kp);

	if (rc) {
		pr_info("prctl kprobe failed: %d.\n", rc);
		return rc;
	}

	return rc;
}

__maybe_unused int ksu_kprobe_exit(void)
{
	unregister_kprobe(&prctl_kp);
	return 0;
}

static int ksu_task_prctl(int option, unsigned long arg2, unsigned long arg3,
			  unsigned long arg4, unsigned long arg5)
{
	ksu_handle_prctl(option, arg2, arg3, arg4, arg5);
	return -ENOSYS;
}

static int ksu_task_fix_setuid(struct cred *new, const struct cred *old,
			       int flags)
{
	return ksu_handle_setuid(new, old);
}

#ifndef MODULE
static struct security_hook_list ksu_hooks[] = {
	LSM_HOOK_INIT(task_prctl, ksu_task_prctl),
	LSM_HOOK_INIT(task_fix_setuid, ksu_task_fix_setuid),
};

void __init ksu_lsm_hook_init(void)
{
	security_add_hooks(ksu_hooks, ARRAY_SIZE(ksu_hooks), "ksu");
}

#else
static int override_security_head(void *head, const void *new_head, size_t len)
{
	unsigned long base = (unsigned long)head & PAGE_MASK;
	unsigned long offset = offset_in_page(head);

	// this is impossible for our case because the page alignment
	// but be careful for other cases!
	BUG_ON(offset + len > PAGE_SIZE);
	struct page *page = phys_to_page(__pa(base));
	if (!page) {
		return -EFAULT;
	}

	void *addr = vmap(&page, 1, VM_MAP, PAGE_KERNEL);
	if (!addr) {
		return -ENOMEM;
	}
	local_irq_disable();
	memcpy(addr + offset, new_head, len);
	local_irq_enable();
	vunmap(addr);
	return 0;
}

static void free_security_hook_list(struct hlist_head *head)
{
	struct hlist_node *temp;
	struct security_hook_list *entry;

	if (!head)
		return;

	hlist_for_each_entry_safe (entry, temp, head, list) {
		hlist_del(&entry->list);
		kfree(entry);
	}

	kfree(head);
}

struct hlist_head *copy_security_hlist(struct hlist_head *orig)
{
	struct hlist_head *new_head = kmalloc(sizeof(*new_head), GFP_KERNEL);
	if (!new_head)
		return NULL;

	INIT_HLIST_HEAD(new_head);

	struct security_hook_list *entry;
	struct security_hook_list *new_entry;

	hlist_for_each_entry (entry, orig, list) {
		new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
		if (!new_entry) {
			free_security_hook_list(new_head);
			return NULL;
		}

		*new_entry = *entry;

		hlist_add_tail_rcu(&new_entry->list, new_head);
	}

	return new_head;
}

#define LSM_SEARCH_MAX 180 // This should be enough to iterate
static void *find_head_addr(void *security_ptr, int *index)
{
	if (!security_ptr) {
		return NULL;
	}
	struct hlist_head *head_start =
		(struct hlist_head *)&security_hook_heads;

	for (int i = 0; i < LSM_SEARCH_MAX; i++) {
		struct hlist_head *head = head_start + i;
		struct security_hook_list *pos;
		hlist_for_each_entry (pos, head, list) {
			if (pos->hook.capget == security_ptr) {
				if (index) {
					*index = i;
				}
				return head;
			}
		}
	}

	return NULL;
}

#define GET_SYMBOL_ADDR(sym)                                                   \
	({                                                                     \
		void *addr = kallsyms_lookup_name(#sym ".cfi_jt");             \
		if (!addr) {                                                   \
			addr = kallsyms_lookup_name(#sym);                     \
		}                                                              \
		addr;                                                          \
	})

#define KSU_LSM_HOOK_HACK_INIT(head_ptr, name, func)                           \
	do {                                                                   \
		static struct security_hook_list hook = {                      \
			.hook = { .name = func }                               \
		};                                                             \
		hook.head = head_ptr;                                          \
		hook.lsm = "ksu";                                              \
		struct hlist_head *new_head = copy_security_hlist(hook.head);  \
		if (!new_head) {                                               \
			pr_err("Failed to copy security list: %s\n", #name);   \
			break;                                                 \
		}                                                              \
		hlist_add_tail_rcu(&hook.list, new_head);                      \
		if (override_security_head(hook.head, new_head,                \
					   sizeof(*new_head))) {               \
			free_security_hook_list(new_head);                     \
			pr_err("Failed to hack lsm for: %s\n", #name);         \
		}                                                              \
	} while (0)

void __init ksu_lsm_hook_init(void)
{
	void *cap_prctl = GET_SYMBOL_ADDR(cap_task_prctl);
	void *prctl_head = find_head_addr(cap_prctl, NULL);
	if (prctl_head) {
		if (prctl_head != &security_hook_heads.task_prctl) {
			pr_warn("prctl's address has shifted!\n");
		}
		KSU_LSM_HOOK_HACK_INIT(prctl_head, task_prctl, ksu_task_prctl);
	} else {
		pr_warn("Failed to find task_prctl!\n");
	}

	void *cap_setuid = GET_SYMBOL_ADDR(cap_task_fix_setuid);
	void *setuid_head = find_head_addr(cap_setuid, NULL);
	if (setuid_head) {
		if (setuid_head != &security_hook_heads.task_fix_setuid) {
			pr_warn("setuid's address has shifted!\n");
		}
		KSU_LSM_HOOK_HACK_INIT(setuid_head, task_fix_setuid,
				       ksu_task_fix_setuid);
	} else {
		pr_warn("Failed to find task_fix_setuid!\n");
	}
	smp_mb();
}
#endif

void __init ksu_core_init(void)
{
	ksu_lsm_hook_init();
}

void ksu_core_exit(void)
{
#ifdef CONFIG_KPROBES
	pr_info("ksu_core_kprobe_exit\n");
	// we dont use this now
	// ksu_kprobe_exit();
#endif
}

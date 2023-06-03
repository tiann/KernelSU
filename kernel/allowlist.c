#include "ksu.h"
#include "linux/compiler.h"
#include "linux/compiler_types.h"
#include "linux/fs.h"
#include "linux/gfp.h"
#include "linux/kernel.h"
#include "linux/list.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/version.h"
#include "klog.h" // IWYU pragma: keep
#include "selinux/selinux.h"
#include "kernel_compat.h"
#include "allowlist.h"

#define FILE_MAGIC 0x7f4b5355 // ' KSU', u32
#define FILE_FORMAT_VERSION 2 // u32

#define KSU_APP_PROFILE_PRESERVE_UID 9999 // NOBODY_UID

static DEFINE_MUTEX(allowlist_mutex);

// default profiles, these may be used frequently, so we cache it
static struct root_profile default_root_profile;
static struct non_root_profile default_non_root_profile;

static void init_default_profiles()
{
	default_root_profile.uid = 0;
	default_root_profile.gid = 0;
	default_root_profile.groups_count = 1;
	default_root_profile.groups[0] = 0;
	memset(&default_root_profile.capabilities, 0xff,
	       sizeof(default_root_profile.capabilities));
	default_root_profile.namespaces = 0;
	strcpy(default_root_profile.selinux_domain, "su");

	// This means that we will umount modules by default!
	default_non_root_profile.umount_modules = true;
}

struct perm_data {
	struct list_head list;
	struct app_profile profile;
};

static struct list_head allow_list;

#define KERNEL_SU_ALLOWLIST "/data/adb/ksu/.allowlist"

static struct work_struct ksu_save_work;
static struct work_struct ksu_load_work;

bool persistent_allow_list(void);

void ksu_show_allow_list(void)
{
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	pr_info("ksu_show_allow_list");
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		pr_info("uid :%d, allow: %d\n", p->profile.current_uid,
			p->profile.allow_su);
	}
}

#ifdef CONFIG_KSU_DEBUG
static void ksu_grant_root_to_shell()
{
	struct app_profile profile = {
		.allow_su = true,
		.current_uid = 2000,
	};
	strcpy(profile.key, "com.android.shell");
	ksu_set_app_profile(&profile, false);
}
#endif

bool ksu_get_app_profile(struct app_profile *profile, bool query_by_uid)
{
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	bool found = false;

	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		bool uid_match =
			(query_by_uid &&
			 profile->current_uid == p->profile.current_uid);
		if (uid_match || !strcmp(profile->key, p->profile.key)) {
			// found it, override it with ours
			memcpy(profile, &p->profile, sizeof(*profile));
			found = true;
			goto exit;
		}
	}

exit:
	return found;
}

bool ksu_set_app_profile(struct app_profile *profile, bool persist)
{
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	bool result = false;

	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		if (!strcmp(profile->key, p->profile.key)) {
			// found it, just override it all!
			memcpy(&p->profile, profile, sizeof(*profile));
			result = true;
			goto exit;
		}
	}

	// not found, alloc a new node!
	p = (struct perm_data *)kmalloc(sizeof(struct perm_data), GFP_KERNEL);
	if (!p) {
		pr_err("ksu_set_app_profile alloc failed\n");
		return false;
	}

	memcpy(&p->profile, profile, sizeof(*profile));
	if (profile->allow_su) {
		pr_info("set root profile, key: %s, uid: %d, gid: %d, context: %s\n",
			profile->key, profile->current_uid,
			profile->rp_config.profile.gid,
			profile->rp_config.profile.selinux_domain);
	} else {
		pr_info("set app profile, key: %s, uid: %d, umount modules: %d\n",
			profile->key, profile->current_uid,
			profile->nrp_config.profile.umount_modules);
	}
	list_add_tail(&p->list, &allow_list);
	result = true;

	// check if the default profiles is changed, cache it to a single struct to accelerate access.
	if (unlikely(!strcmp(profile->key, "$"))) {
		// set default non root profile
		memcpy(&default_non_root_profile, &profile->nrp_config.profile,
		       sizeof(default_non_root_profile));
	}

	if (unlikely(!strcmp(profile->key, "#"))) {
		// set default root profile
		memcpy(&default_root_profile, &profile->rp_config.profile,
		       sizeof(default_root_profile));
	}
exit:
	if (persist)
		persistent_allow_list();

	return result;
}

bool ksu_is_allow_uid(uid_t uid)
{
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;

	if (uid == 0) {
		// already root, but only allow our domain.
		return is_ksu_domain();
	}

	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		// pr_info("is_allow_uid uid :%d, allow: %d\n", p->uid, p->allow);
		if (uid == p->profile.current_uid) {
			return p->profile.allow_su;
		}
	}

	return false;
}

bool ksu_is_uid_should_umount(uid_t uid)
{
	struct app_profile profile = { .current_uid = uid };
	bool found = ksu_get_app_profile(&profile, true);
	if (!found) {
		// no app profile found, it must be non root app
		return default_non_root_profile.umount_modules;
	}
	if (profile.allow_su) {
		// if found and it is granted to su, we shouldn't umount for it
		return false;
	} else {
		// found an app profile
		if (profile.nrp_config.use_default) {
			return default_non_root_profile.umount_modules;
		} else {
			return profile.nrp_config.profile.umount_modules;
		}
	}
}

bool ksu_get_allow_list(int *array, int *length, bool allow)
{
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	int i = 0;
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		// pr_info("get_allow_list uid: %d allow: %d\n", p->uid, p->allow);
		if (p->profile.allow_su == allow) {
			array[i++] = p->profile.current_uid;
		}
	}
	*length = i;

	return true;
}

void do_save_allow_list(struct work_struct *work)
{
	u32 magic = FILE_MAGIC;
	u32 version = FILE_FORMAT_VERSION;
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	loff_t off = 0;
	KWORKER_INSTALL_KEYRING();
	struct file *fp =
		filp_open(KERNEL_SU_ALLOWLIST, O_WRONLY | O_CREAT, 0644);

	if (IS_ERR(fp)) {
		pr_err("save_allow_list creat file failed: %ld\n", PTR_ERR(fp));
		return;
	}

	// store magic and version
	if (ksu_kernel_write_compat(fp, &magic, sizeof(magic), &off) !=
	    sizeof(magic)) {
		pr_err("save_allow_list write magic failed.\n");
		goto exit;
	}

	if (ksu_kernel_write_compat(fp, &version, sizeof(version), &off) !=
	    sizeof(version)) {
		pr_err("save_allow_list write version failed.\n");
		goto exit;
	}

	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		pr_info("save allow list, name: %s uid :%d, allow: %d\n",
			p->profile.key, p->profile.current_uid,
			p->profile.allow_su);

		ksu_kernel_write_compat(fp, &p->profile, sizeof(p->profile),
					&off);
	}

exit:
	filp_close(fp, 0);
}

void do_load_allow_list(struct work_struct *work)
{
	loff_t off = 0;
	ssize_t ret = 0;
	struct file *fp = NULL;
	u32 magic;
	u32 version;
	KWORKER_INSTALL_KEYRING();

#ifdef CONFIG_KSU_DEBUG
	// always allow adb shell by default
	ksu_grant_root_to_shell();
#endif
	// load allowlist now!
	fp = filp_open(KERNEL_SU_ALLOWLIST, O_RDONLY, 0);

	if (IS_ERR(fp)) {
		pr_err("load_allow_list open file failed: %ld\n", PTR_ERR(fp));
		return;
	}

	// verify magic
	if (ksu_kernel_read_compat(fp, &magic, sizeof(magic), &off) !=
		    sizeof(magic) ||
	    magic != FILE_MAGIC) {
		pr_err("allowlist file invalid: %d!\n", magic);
		goto exit;
	}

	if (ksu_kernel_read_compat(fp, &version, sizeof(version), &off) !=
	    sizeof(version)) {
		pr_err("allowlist read version: %d failed\n", version);
		goto exit;
	}

	pr_info("allowlist version: %d\n", version);

	while (true) {
		struct app_profile profile;

		ret = ksu_kernel_read_compat(fp, &profile, sizeof(profile),
					     &off);

		if (ret <= 0) {
			pr_info("load_allow_list read err: %zd\n", ret);
			break;
		}

		pr_info("load_allow_uid, name: %s, uid: %d, allow: %d\n",
			profile.key, profile.current_uid, profile.allow_su);
		ksu_set_app_profile(&profile, false);
	}

exit:
	ksu_show_allow_list();
	filp_close(fp, 0);
}

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, void *), void *data)
{
	struct perm_data *np = NULL;
	struct perm_data *n = NULL;

	bool modified = false;
	// TODO: use RCU!
	mutex_lock(&allowlist_mutex);
	list_for_each_entry_safe (np, n, &allow_list, list) {
		uid_t uid = np->profile.current_uid;
		// we use this uid for special cases, don't prune it!
		bool is_preserved_uid = uid == KSU_APP_PROFILE_PRESERVE_UID;
		if (!is_preserved_uid && !is_uid_exist(uid, data)) {
			modified = true;
			pr_info("prune uid: %d\n", uid);
			list_del(&np->list);
			kfree(np);
		}
	}
	mutex_unlock(&allowlist_mutex);

	if (modified) {
		persistent_allow_list();
	}
}

// make sure allow list works cross boot
bool persistent_allow_list(void)
{
	return ksu_queue_work(&ksu_save_work);
}

bool ksu_load_allow_list(void)
{
	return ksu_queue_work(&ksu_load_work);
}

void ksu_allowlist_init(void)
{
	INIT_LIST_HEAD(&allow_list);

	INIT_WORK(&ksu_save_work, do_save_allow_list);
	INIT_WORK(&ksu_load_work, do_load_allow_list);

	init_default_profiles();
}

void ksu_allowlist_exit(void)
{
	struct perm_data *np = NULL;
	struct perm_data *n = NULL;

	do_save_allow_list(NULL);

	// free allowlist
	mutex_lock(&allowlist_mutex);
	list_for_each_entry_safe (np, n, &allow_list, list) {
		list_del(&np->list);
		kfree(np);
	}
	mutex_unlock(&allowlist_mutex);
}
#include "linux/delay.h"
#include "linux/fs.h"
#include "linux/kernel.h"
#include "linux/list.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include "linux/version.h"
#include "klog.h" // IWYU pragma: keep
#include "selinux/selinux.h"
#include "kernel_compat.h"

#define FILE_MAGIC 0x7f4b5355 // ' KSU', u32
#define FILE_FORMAT_VERSION 1 // u32

static DEFINE_MUTEX(allowlist_mutex);

struct perm_data {
	struct list_head list;
	uid_t uid;
	bool allow;
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
		pr_info("uid :%d, allow: %d\n", p->uid, p->allow);
	}
}

bool ksu_allow_uid(uid_t uid, bool allow, bool persist)
{
	// find the node first!
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	bool result = false;
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		if (uid == p->uid) {
			p->allow = allow;
			result = true;
			goto exit;
		}
	}

	// not found, alloc a new node!
	p = (struct perm_data *)kmalloc(sizeof(struct perm_data), GFP_KERNEL);
	if (!p) {
		pr_err("alloc allow node failed.\n");
		return false;
	}
	p->uid = uid;
	p->allow = allow;

	pr_info("allow_uid: %d, allow: %d", uid, allow);

	list_add_tail(&p->list, &allow_list);
	result = true;

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
		if (uid == p->uid) {
			return p->allow;
		}
	}

	return false;
}

bool ksu_get_allow_list(int *array, int *length, bool allow)
{
	struct perm_data *p = NULL;
	struct list_head *pos = NULL;
	int i = 0;
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_data, list);
		// pr_info("get_allow_list uid: %d allow: %d\n", p->uid, p->allow);
		if (p->allow == allow) {
			array[i++] = p->uid;
		}
	}
	*length = i;

	return true;
}

void do_persistent_allow_list(struct work_struct *work)
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
	if (ksu_kernel_write_compat(fp, &magic, sizeof(magic), &off) != sizeof(magic)) {
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
		pr_info("save allow list uid :%d, allow: %d\n", p->uid,
			p->allow);
		ksu_kernel_write_compat(fp, &p->uid, sizeof(p->uid), &off);
		ksu_kernel_write_compat(fp, &p->allow, sizeof(p->allow), &off);
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

	// load allowlist now!
	fp = filp_open(KERNEL_SU_ALLOWLIST, O_RDONLY, 0);

	if (IS_ERR(fp)) {
#ifdef CONFIG_KSU_DEBUG
		int errno = PTR_ERR(fp);
		if (errno == -ENOENT) {
			ksu_allow_uid(2000, true,
				      true); // allow adb shell by default
		} else {
			pr_err("load_allow_list open file failed: %ld\n",
			       PTR_ERR(fp));
		}
#else
		pr_err("load_allow_list open file failed: %ld\n", PTR_ERR(fp));
#endif
		return;
	}

	// verify magic
	if (ksu_kernel_read_compat(fp, &magic, sizeof(magic), &off) != sizeof(magic) ||
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
		u32 uid;
		bool allow = false;
		ret = ksu_kernel_read_compat(fp, &uid, sizeof(uid), &off);
		if (ret <= 0) {
			pr_info("load_allow_list read err: %zd\n", ret);
			break;
		}
		ret = ksu_kernel_read_compat(fp, &allow, sizeof(allow), &off);

		pr_info("load_allow_uid: %d, allow: %d\n", uid, allow);

		ksu_allow_uid(uid, allow, false);
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
		uid_t uid = np->uid;
		if (!is_uid_exist(uid, data)) {
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

	INIT_WORK(&ksu_save_work, do_persistent_allow_list);
	INIT_WORK(&ksu_load_work, do_load_allow_list);
}

void ksu_allowlist_exit(void)
{
	struct perm_data *np = NULL;
	struct perm_data *n = NULL;

	do_persistent_allow_list(NULL);

	// free allowlist
	mutex_lock(&allowlist_mutex);
	list_for_each_entry_safe (np, n, &allow_list, list) {
		list_del(&np->list);
		kfree(np);
	}
	mutex_unlock(&allowlist_mutex);
}
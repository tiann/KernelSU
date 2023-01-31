#include "asm-generic/int-ll64.h"
#include "linux/capability.h"
#include "linux/delay.h"
#include "linux/fs.h"
#include "linux/kernel.h"
#include "linux/list.h"
#include "linux/printk.h"
#include "linux/slab.h"

#include "allowlist.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "selinux/selinux.h"

#define FILE_MAGIC 0x7f4b5355 // ' KSU', u32
#define FILE_FORMAT_VERSION 2 // u32

static DEFINE_MUTEX(allowlist_mutex);

const struct perm_data NO_PERM = {
	.allow = false,
	.cap = CAP_EMPTY_SET,
};

const struct perm_data ALL_PERM = {
	.allow = true,
	.cap = CAP_FULL_SET,
};

struct perm_list_node {
	struct list_head list;
	uid_t uid;
	struct perm_data data;
};

static struct list_head allow_list;
static unsigned int allow_list_count;

#define KERNEL_SU_ALLOWLIST "/data/adb/.ksu_allowlist"

static struct work_struct ksu_save_work;
static struct work_struct ksu_load_work;

bool persistent_allow_list(void);

void ksu_show_perm_data(struct perm_data data)
{
	pr_cont("allow: %d, cap: %08x | %08x", data.allow, data.cap.cap[0],
		data.cap.cap[1]);
}

void ksu_show_perm_list_node(const char* str, struct perm_list_node data)
{
	pr_info("%s uid: %d allow: %d, cap: %08x | %08x", str, data.uid,
		data.data.allow, data.data.cap.cap[0], data.data.cap.cap[1]);
}

void ksu_show_allow_list(void)
{
	struct perm_list_node *p = NULL;
	struct list_head *pos = NULL;
	pr_info("ksu_show_allow_list");
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_list_node, list);
		ksu_show_perm_list_node(">", *p);
	}
}

bool ksu_set_uid_data(uid_t uid, struct perm_data data, bool persist)
{
	// find the node first!
	struct perm_list_node *p = NULL;
	struct list_head *pos = NULL;
	bool result = false;
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_list_node, list);
		if (uid == p->uid) {
			p->data = data;
			result = true;
			goto exit;
		}
	}

	// not found, alloc a new node!
	p = (struct perm_list_node *)kmalloc(sizeof(struct perm_list_node),
					     GFP_KERNEL);
	if (!p) {
		pr_err("alloc allow node failed.\n");
		return false;
	}
	p->uid = uid;
	p->data = data;

	list_add_tail(&p->list, &allow_list);
	allow_list_count++;
	result = true;

exit:
	if (persist)
		persistent_allow_list();

	return result;
}

struct perm_data ksu_get_uid_data(uid_t uid)
{
	struct perm_list_node *p = NULL;
	struct list_head *pos = NULL;

	if (uid == 0) {
		// already root, but only allow our domain.
		if (!is_ksu_domain()) {
			return NO_PERM;
		}
	}

	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_list_node, list);
		// pr_info("ksu_get_uid_data uid :%d ", p->uid);
		// ksu_show_perm_data(p->data);
		if (uid == p->uid) {
			return p->data;
		}
	}

	return NO_PERM;
}

bool ksu_get_uid_data_list(struct perm_uid_data *array, int *length, bool kbuf)
{
	struct perm_list_node *p = NULL;
	struct list_head *pos = NULL;
	int i = 0;
	int max = *length;
	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_list_node, list);
		ksu_show_perm_list_node("ksu_get_uid_data_list", *p);
		if (i > max) {
			*length = i;
			pr_err("ksu_get_uid_data_list array too small.\n");
			return false;
		}
		i++;
		if (kbuf) {
			array[i].uid = p->uid;
			array[i].data = p->data;
		} else {
			struct perm_uid_data tmp = {
				.uid = p->uid,
				.data = p->data,
			};
			if (copy_to_user(&array[i], &tmp, sizeof(tmp))) {
				pr_err("ksu_get_uid_data_list copy_to_user failed.\n");
				return false;
			}
		}
	}
	*length = i;

	return true;
}

unsigned int ksu_get_uid_data_list_count(void)
{
	return allow_list_count;
}

void do_persistent_allow_list(struct work_struct *work)
{
	u32 magic = FILE_MAGIC;
	u32 version = FILE_FORMAT_VERSION;
	struct perm_list_node *p = NULL;
	struct list_head *pos = NULL;
	loff_t off = 0;

	struct file *fp =
		filp_open(KERNEL_SU_ALLOWLIST, O_WRONLY | O_CREAT, 0644);

	if (IS_ERR(fp)) {
		pr_err("save_allow_list creat file failed: %d\n", PTR_ERR(fp));
		return;
	}

	// store magic and version
	if (kernel_write(fp, &magic, sizeof(magic), &off) != sizeof(magic)) {
		pr_err("save_allow_list write magic failed.\n");
		goto exit;
	}

	if (kernel_write(fp, &version, sizeof(version), &off) !=
	    sizeof(version)) {
		pr_err("save_allow_list write version failed.\n");
		goto exit;
	}

	list_for_each (pos, &allow_list) {
		p = list_entry(pos, struct perm_list_node, list);
		pr_info("save allow list uid :%d, allow: %d\n", p->uid,
			p->data.allow);
		kernel_write(fp, &p->uid, sizeof(p->uid), &off);
		kernel_write(fp, &p->data, sizeof(p->data), &off);
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

	fp = filp_open("/data/adb/", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		int errno = PTR_ERR(fp);
		pr_err("load_allow_list open '/data/adb': %d\n", PTR_ERR(fp));
		if (errno == -ENOENT) {
			msleep(2000);
			ksu_queue_work(&ksu_load_work);
			return;
		} else {
			pr_info("load_allow list dir exist now!");
		}
	} else {
		filp_close(fp, 0);
	}

	// load allowlist now!
	fp = filp_open(KERNEL_SU_ALLOWLIST, O_RDONLY, 0);

	if (IS_ERR(fp)) {
#ifdef CONFIG_KSU_DEBUG
		int errno = PTR_ERR(fp);
		if (errno == -ENOENT) {
			// allow adb shell by default
			ksu_set_uid_data(2000, ALL_PERM, true);
		} else {
			pr_err("load_allow_list open file failed: %d\n",
			       PTR_ERR(fp));
		}
#else
		pr_err("load_allow_list open file failed: %d\n", PTR_ERR(fp));
#endif
		return;
	}

	// verify magic
	if (kernel_read(fp, &magic, sizeof(magic), &off) != sizeof(magic) ||
	    magic != FILE_MAGIC) {
		pr_err("allowlist file invalid: %d!\n", magic);
		goto exit;
	}

	if (kernel_read(fp, &version, sizeof(version), &off) !=
	    sizeof(version)) {
		pr_err("allowlist read version: %d failed\n", version);
		goto exit;
	}

	pr_info("allowlist version: %d\n", version);
	if (version < FILE_FORMAT_VERSION) {
		pr_info("allowlist version too old");
#ifdef CONFIG_KSU_DEBUG
		// allow adb shell by default
		ksu_set_uid_data(2000, ALL_PERM, true);
#endif
		return;
	}

	while (true) {
		u32 uid;
		struct perm_data data = NO_PERM;
		ret = kernel_read(fp, &uid, sizeof(uid), &off);
		if (ret <= 0) {
			pr_info("load_allow_list read err: %d\n", ret);
			break;
		}
		ret = kernel_read(fp, &data, sizeof(data), &off);

		pr_info("load_allow_uid: %d, allow: %d\n", uid, data.allow);

		ksu_set_uid_data(uid, data, false);
	}

exit:
	ksu_show_allow_list();
	filp_close(fp, 0);
}

void ksu_prune_allowlist(bool (*is_uid_exist)(uid_t, void *), void *data)
{
	struct perm_list_node *np = NULL;
	struct perm_list_node *n = NULL;

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
			allow_list_count--;
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
	allow_list_count = 0;

	INIT_WORK(&ksu_save_work, do_persistent_allow_list);
	INIT_WORK(&ksu_load_work, do_load_allow_list);
}

void ksu_allowlist_exit(void)
{
	struct perm_list_node *np = NULL;
	struct perm_list_node *n = NULL;

	do_persistent_allow_list(NULL);

	// free allowlist
	mutex_lock(&allowlist_mutex);
	list_for_each_entry_safe (np, n, &allow_list, list) {
		list_del(&np->list);
		kfree(np);
		allow_list_count--;
	}
	mutex_unlock(&allowlist_mutex);
}
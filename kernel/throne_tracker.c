#include "linux/err.h"
#include "linux/fs.h"
#include "linux/list.h"
#include "linux/slab.h"
#include "linux/string.h"
#include "linux/types.h"
#include "linux/version.h"
#include "linux/workqueue.h"

#include "allowlist.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "manager.h"
#include "throne_tracker.h"
#include "kernel_compat.h"

uid_t ksu_manager_uid = KSU_INVALID_UID;

#define SYSTEM_PACKAGES_LIST_PATH "/data/system/packages.list"
static struct work_struct ksu_update_uid_work;

struct uid_data {
	struct list_head list;
	u32 uid;
	char package[KSU_MAX_PACKAGE_NAME];
};

static int get_pkg_from_apk_path(char *pkg, const char *path)
{
	int len = strlen(path);
	if (len >= KSU_MAX_PACKAGE_NAME || len < 1)
		return -1;

	const char *last_slash = NULL;
	const char *second_last_slash = NULL;

	int i;
	for (i = len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			if (!last_slash) {
				last_slash = &path[i];
			} else {
				second_last_slash = &path[i];
				break;
			}
		}
	}

	if (!last_slash || !second_last_slash)
		return -1;

	const char *last_hyphen = strchr(second_last_slash, '-');
	if (!last_hyphen || last_hyphen > last_slash)
		return -1;

	int pkg_len = last_hyphen - second_last_slash - 1;
	if (pkg_len >= KSU_MAX_PACKAGE_NAME || pkg_len <= 0)
		return -1;

	// Copying the package name
	strncpy(pkg, second_last_slash + 1, pkg_len);
	pkg[pkg_len] = '\0';

	return 0;
}

static void crown_manager(const char *apk, struct list_head *uid_data)
{
	char pkg[KSU_MAX_PACKAGE_NAME];
	if (get_pkg_from_apk_path(pkg, apk) < 0) {
		pr_err("Failed to get package name from apk path: %s\n", apk);
		return;
	}

	pr_info("manager pkg: %s\n", pkg);

#ifdef KSU_MANAGER_PACKAGE
	// pkg is `/<real package>`
	if (strncmp(pkg, KSU_MANAGER_PACKAGE, sizeof(KSU_MANAGER_PACKAGE))) {
		pr_info("manager package is inconsistent with kernel build: %s\n",
			KSU_MANAGER_PACKAGE);
		return;
	}
#endif
	struct list_head *list = (struct list_head *)uid_data;
	struct uid_data *np;

	list_for_each_entry (np, list, list) {
		if (strncmp(np->package, pkg, KSU_MAX_PACKAGE_NAME) == 0) {
			pr_info("Crowning manager: %s(uid=%d)\n", pkg, np->uid);
			ksu_set_manager_uid(np->uid);
			break;
		}
	}
}

struct my_dir_context {
	struct dir_context ctx;
	char *parent_dir;
	void *private_data;
	int depth;
	int *stop;
};
// https://docs.kernel.org/filesystems/porting.html
// filldir_t (readdir callbacks) calling conventions have changed. Instead of returning 0 or -E... it returns bool now. false means "no more" (as -E... used to) and true - "keep going" (as 0 in old calling conventions). Rationale: callers never looked at specific -E... values anyway. -> iterate_shared() instances require no changes at all, all filldir_t ones in the tree converted.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define FILLDIR_RETURN_TYPE bool
#define FILLDIR_ACTOR_CONTINUE true
#define FILLDIR_ACTOR_STOP false
#else
#define FILLDIR_RETURN_TYPE int
#define FILLDIR_ACTOR_CONTINUE 0
#define FILLDIR_ACTOR_STOP -EINVAL
#endif

FILLDIR_RETURN_TYPE my_actor(struct dir_context *ctx, const char *name,
			     int namelen, loff_t off, u64 ino,
			     unsigned int d_type)
{
	struct my_dir_context *my_ctx =
		container_of(ctx, struct my_dir_context, ctx);
	struct file *file;
	char *dirpath;

	if (!my_ctx) {
		pr_err("Invalid context\n");
		return FILLDIR_ACTOR_STOP;
	}
	if (my_ctx->stop && *my_ctx->stop) {
		return FILLDIR_ACTOR_STOP;
	}

	if (!strncmp(name, "..", namelen) || !strncmp(name, ".", namelen))
		return FILLDIR_ACTOR_CONTINUE; // Skip "." and ".."

	dirpath = kmalloc(PATH_MAX, GFP_KERNEL);
	if (!dirpath) {
		return FILLDIR_ACTOR_STOP; // Failed to obtain directory path
	}
	snprintf(dirpath, PATH_MAX, "%s/%.*s", my_ctx->parent_dir, namelen,
		 name);

	if (d_type == DT_DIR && my_ctx->depth > 0 &&
	    (my_ctx->stop && !*my_ctx->stop)) {
		struct my_dir_context sub_ctx = { .ctx.actor = my_actor,
						  .parent_dir = dirpath,
						  .private_data =
							  my_ctx->private_data,
						  .depth = my_ctx->depth - 1,
						  .stop = my_ctx->stop };
		file = ksu_filp_open_compat(dirpath, O_RDONLY, 0);
		if (IS_ERR(file)) {
			pr_err("Failed to open directory: %s, err: %ld\n",
			       dirpath, PTR_ERR(file));
			kfree(dirpath);
			return FILLDIR_ACTOR_STOP;
		}

		iterate_dir(file, &sub_ctx.ctx);
		filp_close(file, NULL);
	} else {
		if ((strlen(name) == strlen("base.apk")) &&
		    (strncmp(name, "base.apk", strlen("base.apk")) == 0)) {
			bool is_manager = is_manager_apk(dirpath);
			pr_info("Found base.apk at path: %s, is_manager: %d\n",
				dirpath, is_manager);
			if (is_manager) {
				crown_manager(dirpath, my_ctx->private_data);
				*my_ctx->stop = 1;
			}
		}
		kfree(dirpath);
	}

	return FILLDIR_ACTOR_CONTINUE;
}

void search_manager(const char *path, int depth, struct list_head *uid_data)
{
	struct file *file;
	int stop = 0;
	struct my_dir_context ctx = { .ctx.actor = my_actor,
				      .parent_dir = (char *)path,
				      .private_data = uid_data,
				      .depth = depth,
				      .stop = &stop };

	file = ksu_filp_open_compat(path, O_RDONLY, 0);
	if (IS_ERR(file)) {
		pr_err("Failed to open directory: %s\n", path);
		return;
	}

	iterate_dir(file, &ctx.ctx);
	filp_close(file, NULL);
}

static bool is_uid_exist(uid_t uid, char *package, void *data)
{
	struct list_head *list = (struct list_head *)data;
	struct uid_data *np;

	bool exist = false;
	list_for_each_entry (np, list, list) {
		if (np->uid == uid % 100000 &&
		    strncmp(np->package, package, KSU_MAX_PACKAGE_NAME) == 0) {
			exist = true;
			break;
		}
	}
	return exist;
}

static void do_update_uid(struct work_struct *work)
{
	struct file *fp =
		ksu_filp_open_compat(SYSTEM_PACKAGES_LIST_PATH, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("do_update_uid, open " SYSTEM_PACKAGES_LIST_PATH
		       " failed: %ld\n",
		       PTR_ERR(fp));
		return;
	}

	struct list_head uid_list;
	INIT_LIST_HEAD(&uid_list);

	char chr = 0;
	loff_t pos = 0;
	loff_t line_start = 0;
	char buf[KSU_MAX_PACKAGE_NAME];
	for (;;) {
		ssize_t count =
			ksu_kernel_read_compat(fp, &chr, sizeof(chr), &pos);
		if (count != sizeof(chr))
			break;
		if (chr != '\n')
			continue;

		count = ksu_kernel_read_compat(fp, buf, sizeof(buf),
					       &line_start);

		struct uid_data *data =
			kzalloc(sizeof(struct uid_data), GFP_ATOMIC);
		if (!data) {
			goto out;
		}

		char *tmp = buf;
		const char *delim = " ";
		char *package = strsep(&tmp, delim);
		char *uid = strsep(&tmp, delim);
		if (!uid || !package) {
			pr_err("update_uid: package or uid is NULL!\n");
			break;
		}

		u32 res;
		if (kstrtou32(uid, 10, &res)) {
			pr_err("update_uid: uid parse err\n");
			break;
		}
		data->uid = res;
		strncpy(data->package, package, KSU_MAX_PACKAGE_NAME);
		list_add_tail(&data->list, &uid_list);
		// reset line start
		line_start = pos;
	}

	// now update uid list
	struct uid_data *np;
	struct uid_data *n;

	// first, check if manager_uid exist!
	bool manager_exist = false;
	list_for_each_entry (np, &uid_list, list) {
		// if manager is installed in work profile, the uid in packages.list is still equals main profile
		// don't delete it in this case!
		int manager_uid = ksu_get_manager_uid() % 100000;
		if (np->uid == manager_uid) {
			manager_exist = true;
			break;
		}
	}

	if (!manager_exist) {
		if (ksu_is_manager_uid_valid()) {
			pr_info("manager is uninstalled, invalidate it!\n");
			ksu_invalidate_manager_uid();
		}
		pr_info("Searching manager...\n");
		search_manager("/data/app", 2, &uid_list);
	}

	// then prune the allowlist
	ksu_prune_allowlist(is_uid_exist, &uid_list);
out:
	// free uid_list
	list_for_each_entry_safe (np, n, &uid_list, list) {
		list_del(&np->list);
		kfree(np);
	}
	filp_close(fp, 0);
}

void track_throne()
{
	ksu_queue_work(&ksu_update_uid_work);
}

void ksu_throne_tracker_init()
{
	INIT_WORK(&ksu_update_uid_work, do_update_uid);
}

void ksu_throne_tracker_exit()
{
	// nothing to do
}

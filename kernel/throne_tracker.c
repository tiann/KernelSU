#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/namei.h>

#include "allowlist.h"
#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "manager.h"
#include "throne_tracker.h"
#include "kernel_compat.h"

uid_t ksu_manager_uid = KSU_INVALID_UID;

#define USER_DATA_BASE_PATH "/data/user_de"
#define MAX_SUPPORTED_USERS 32 // Supports up to 32 users
#define USER_DATA_PATH_LEN 384 // 384 is enough for /data/user_de/{userid}/<package>

struct uid_data {
	struct list_head list;
	u32 uid;
	char package[KSU_MAX_PACKAGE_NAME];
};

struct user_scan_context {
	struct list_head *uid_list;
	uid_t user_id;
	size_t packages_found;
	size_t errors_count;
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

#define DATA_PATH_LEN 384 // 384 is enough for /data/app/<package>/base.apk

struct data_path {
	char dirpath[DATA_PATH_LEN];
	int depth;
	struct list_head list;
};

struct apk_path_hash {
	unsigned int hash;
	bool exists;
	struct list_head list;
};

static struct list_head apk_path_hash_list = LIST_HEAD_INIT(apk_path_hash_list);

struct my_dir_context {
	struct dir_context ctx;
	struct list_head *data_path_list;
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

struct uid_scan_stats {
	size_t total_found;
	size_t errors_encountered;
};

struct user_data_context {
	struct dir_context ctx;
	struct user_scan_context *scan_ctx;
};

struct user_de_context {
    struct dir_context ctx;
    uid_t *user_ids;
    size_t count;
    size_t max_users;
};

// Actor function to collect user IDs from /data/user_de
FILLDIR_RETURN_TYPE user_de_actor(struct dir_context *ctx, const char *name,
                                  int namelen, loff_t off, u64 ino,
                                  unsigned int d_type)
{
    struct user_de_context *data = container_of(ctx, struct user_de_context, ctx);

    if (d_type != DT_DIR)
        return FILLDIR_ACTOR_CONTINUE;

    if (strncmp(name, ".", namelen) == 0 || strncmp(name, "..", namelen) == 0)
        return FILLDIR_ACTOR_CONTINUE;

    uid_t uid = 0;
    for (int i = 0; i < namelen; i++) {
        if (name[i] < '0' || name[i] > '9')
            return FILLDIR_ACTOR_CONTINUE;
        uid = uid * 10 + (name[i] - '0');
    }

    if (data->count >= data->max_users)
        return FILLDIR_ACTOR_STOP;

    data->user_ids[data->count++] = uid;
    return FILLDIR_ACTOR_CONTINUE;
}

// Retrieve a list of all active Android user IDs in the system
static int get_active_user_ids(uid_t *user_ids, size_t max_users, size_t *found_users)
{
    struct file *dir_file;
    int ret = 0;

    *found_users = 0;

    dir_file = ksu_filp_open_compat(USER_DATA_BASE_PATH, O_RDONLY, 0);
    if (IS_ERR(dir_file)) {
        pr_err("Failed to open user data path %s: %ld\n",
               USER_DATA_BASE_PATH, PTR_ERR(dir_file));
        return PTR_ERR(dir_file);
    }

    struct {
        struct dir_context ctx;
        uid_t *user_ids;
        size_t count;
        size_t max_users;
    } ctx = {
        .ctx.actor = user_de_actor,
        .user_ids = user_ids,
        .count = 0,
        .max_users = max_users
    };

    ret = iterate_dir(dir_file, &ctx.ctx);
    filp_close(dir_file, NULL);

    *found_users = ctx.count;

    if (ctx.count > 0)
        pr_info("UserDE UID: Found %zu active users\n", ctx.count);

    return ret;
}

FILLDIR_RETURN_TYPE user_data_actor(struct dir_context *ctx, const char *name,
				     int namelen, loff_t off, u64 ino,
				     unsigned int d_type)
{
	struct user_data_context *my_ctx = 
		container_of(ctx, struct user_data_context, ctx);
	
	if (!my_ctx || !my_ctx->scan_ctx || !my_ctx->scan_ctx->uid_list) {
		return FILLDIR_ACTOR_STOP;
	}

	struct user_scan_context *scan_ctx = my_ctx->scan_ctx;

	if (!strncmp(name, "..", namelen) || !strncmp(name, ".", namelen))
		return FILLDIR_ACTOR_CONTINUE;

	if (d_type != DT_DIR)
		return FILLDIR_ACTOR_CONTINUE;

	if (namelen >= KSU_MAX_PACKAGE_NAME) {
		pr_warn("Package name too long: %.*s (user %u)\n", namelen, name, scan_ctx->user_id);
		scan_ctx->errors_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	char package_path[USER_DATA_PATH_LEN];
	if (snprintf(package_path, sizeof(package_path), "%s/%u/%.*s", 
		     USER_DATA_BASE_PATH, scan_ctx->user_id, namelen, name) >= sizeof(package_path)) {
		pr_err("Path too long for package: %.*s (user %u)\n", namelen, name, scan_ctx->user_id);
		scan_ctx->errors_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	struct path path;
	int err = kern_path(package_path, LOOKUP_FOLLOW, &path);
	if (err) {
		pr_debug("Package path lookup failed: %s (err: %d)\n", package_path, err);
		scan_ctx->errors_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	struct kstat stat;
	err = vfs_getattr(&path, &stat, STATX_UID, AT_STATX_SYNC_AS_STAT);
	path_put(&path);
	
	if (err) {
		pr_debug("Failed to get attributes for: %s (err: %d)\n", package_path, err);
		scan_ctx->errors_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	uid_t uid = from_kuid(&init_user_ns, stat.uid);
	if (uid == (uid_t)-1) {
		pr_warn("Invalid UID for package: %.*s (user %u)\n", namelen, name, scan_ctx->user_id);
		scan_ctx->errors_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	struct uid_data *data = kzalloc(sizeof(struct uid_data), GFP_KERNEL);
	if (!data) {
		pr_err("Failed to allocate memory for package: %.*s (user %u)\n", namelen, name, scan_ctx->user_id);
		scan_ctx->errors_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	data->uid = uid;
	size_t copy_len = min_t(size_t, namelen, KSU_MAX_PACKAGE_NAME - 1);
	strncpy(data->package, name, copy_len);
	data->package[copy_len] = '\0';
	
	list_add_tail(&data->list, scan_ctx->uid_list);
	scan_ctx->packages_found++;
	
	pr_info("UserDE UID: Found package: %s, uid: %u (user %u)\n", 
		 data->package, data->uid, scan_ctx->user_id);
	
	return FILLDIR_ACTOR_CONTINUE;
}

static int scan_user_data_for_user(uid_t user_id, struct list_head *uid_list, 
					   size_t *packages_found, size_t *errors_count)
{
	struct file *dir_file;
	char user_path[USER_DATA_PATH_LEN];
	int ret = 0;
	
	*packages_found = 0;
	*errors_count = 0;
	
	snprintf(user_path, sizeof(user_path), "%s/%u", USER_DATA_BASE_PATH, user_id);
	
	dir_file = ksu_filp_open_compat(user_path, O_RDONLY, 0);
	if (IS_ERR(dir_file)) {
		pr_debug("Failed to open user data path: %s (%ld)\n", user_path, PTR_ERR(dir_file));
		return PTR_ERR(dir_file);
	}

	struct user_scan_context scan_ctx = {
		.uid_list = uid_list,
		.user_id = user_id,
		.packages_found = 0,
		.errors_count = 0
	};
	
	struct user_data_context ctx = {
		.ctx.actor = user_data_actor,
		.scan_ctx = &scan_ctx
	};

	ret = iterate_dir(dir_file, &ctx.ctx);
	filp_close(dir_file, NULL);

	*packages_found = scan_ctx.packages_found;
	*errors_count = scan_ctx.errors_count;
	
	if (scan_ctx.packages_found > 0 && scan_ctx.errors_count > 0) {
		pr_info("UserDE UID: Scanned user %u, found %zu packages with %zu errors\n", 
			user_id, scan_ctx.packages_found, scan_ctx.errors_count);
	}

	return ret;
}

int scan_user_data_for_uids(struct list_head *uid_list)
{
	uid_t user_ids[MAX_SUPPORTED_USERS];
	size_t active_users = 0;
	size_t total_packages = 0;
	size_t total_errors = 0;
	int ret = 0;
	
	if (!uid_list) {
		return -EINVAL;
	}

	// Retrieve all active user IDs
	ret = get_active_user_ids(user_ids, ARRAY_SIZE(user_ids), &active_users);
	if (ret < 0 || active_users == 0) {
		pr_err("Failed to get active user IDs or no users found: %d\n", ret);
		return -ENOENT;
	}

	// Scan each user's data directory
	for (size_t i = 0; i < active_users; i++) {
		uid_t user_id = user_ids[i];
		size_t packages_found = 0;
		size_t errors_count = 0;
		
		ret = scan_user_data_for_user(user_id, uid_list, &packages_found, &errors_count);
		
		if (ret < 0) {
			pr_warn("Failed to scan user %u data directory: %d\n", user_id, ret);
			total_errors++;
			continue;
		}
		
		total_packages += packages_found;
		total_errors += errors_count;
	}

	if (total_errors > 0) {
		pr_warn("UserDE UID: Encountered %zu errors while scanning user data directories\n", 
			total_errors);
	}

	pr_info("UserDE UID: Scan completed - %zu users, %zu packages found\n", 
		active_users, total_packages);

	return total_packages > 0 ? 0 : -ENOENT;
}

FILLDIR_RETURN_TYPE my_actor(struct dir_context *ctx, const char *name,
			     int namelen, loff_t off, u64 ino,
			     unsigned int d_type)
{
	struct my_dir_context *my_ctx =
		container_of(ctx, struct my_dir_context, ctx);
	char dirpath[DATA_PATH_LEN];

	if (!my_ctx) {
		pr_err("Invalid context\n");
		return FILLDIR_ACTOR_STOP;
	}
	if (my_ctx->stop && *my_ctx->stop) {
		pr_info("Stop searching\n");
		return FILLDIR_ACTOR_STOP;
	}

	if (!strncmp(name, "..", namelen) || !strncmp(name, ".", namelen))
		return FILLDIR_ACTOR_CONTINUE; // Skip "." and ".."

	if (d_type == DT_DIR && namelen >= 8 && !strncmp(name, "vmdl", 4) &&
 	    !strncmp(name + namelen - 4, ".tmp", 4)) {
 		pr_info("Skipping directory: %.*s\n", namelen, name);
 		return FILLDIR_ACTOR_CONTINUE; // Skip staging package
 	}
	
	if (snprintf(dirpath, DATA_PATH_LEN, "%s/%.*s", my_ctx->parent_dir,
		     namelen, name) >= DATA_PATH_LEN) {
		pr_err("Path too long: %s/%.*s\n", my_ctx->parent_dir, namelen,
		       name);
		return FILLDIR_ACTOR_CONTINUE;
	}

	if (d_type == DT_DIR && my_ctx->depth > 0 &&
	    (my_ctx->stop && !*my_ctx->stop)) {
		struct data_path *data = kmalloc(sizeof(struct data_path), GFP_ATOMIC);

		if (!data) {
			pr_err("Failed to allocate memory for %s\n", dirpath);
			return FILLDIR_ACTOR_CONTINUE;
		}

		strscpy(data->dirpath, dirpath, DATA_PATH_LEN);
		data->depth = my_ctx->depth - 1;
		list_add_tail(&data->list, my_ctx->data_path_list);
	} else {
		if ((namelen == 8) && (strncmp(name, "base.apk", namelen) == 0)) {
			struct apk_path_hash *pos, *n;
			unsigned int hash = full_name_hash(NULL, dirpath, strlen(dirpath));
			list_for_each_entry(pos, &apk_path_hash_list, list) {
				if (hash == pos->hash) {
					pos->exists = true;
					return FILLDIR_ACTOR_CONTINUE;
				}
			}

			bool is_manager = is_manager_apk(dirpath);
			pr_info("Found new base.apk at path: %s, is_manager: %d\n",
				dirpath, is_manager);
			if (is_manager) {
				crown_manager(dirpath, my_ctx->private_data);
				*my_ctx->stop = 1;

				// Manager found, clear APK cache list
				list_for_each_entry_safe(pos, n, &apk_path_hash_list, list) {
					list_del(&pos->list);
					kfree(pos);
				}
			} else {
				struct apk_path_hash *apk_data = kmalloc(sizeof(struct apk_path_hash), GFP_ATOMIC);
					apk_data->hash = hash;
					apk_data->exists = true;
					list_add_tail(&apk_data->list, &apk_path_hash_list);
			}
		}
	}

	return FILLDIR_ACTOR_CONTINUE;
}

void search_manager(const char *path, int depth, struct list_head *uid_data)
{
	int i, stop = 0;
	struct list_head data_path_list;
	INIT_LIST_HEAD(&data_path_list);
	unsigned long data_app_magic = 0;
	
	// Initialize APK cache list
	struct apk_path_hash *pos, *n;
	list_for_each_entry(pos, &apk_path_hash_list, list) {
		pos->exists = false;
	}

	// First depth
	struct data_path data;
	strscpy(data.dirpath, path, DATA_PATH_LEN);
	data.depth = depth;
	list_add_tail(&data.list, &data_path_list);

	for (i = depth; i >= 0; i--) {
		struct data_path *pos, *n;

		list_for_each_entry_safe(pos, n, &data_path_list, list) {
			struct my_dir_context ctx = { .ctx.actor = my_actor,
						      .data_path_list = &data_path_list,
						      .parent_dir = pos->dirpath,
						      .private_data = uid_data,
						      .depth = pos->depth,
						      .stop = &stop };
			struct file *file;

			if (!stop) {
				file = ksu_filp_open_compat(pos->dirpath, O_RDONLY | O_NOFOLLOW, 0);
				if (IS_ERR(file)) {
					pr_err("Failed to open directory: %s, err: %ld\n", pos->dirpath, PTR_ERR(file));
					goto skip_iterate;
				}
				
				// grab magic on first folder, which is /data/app
				if (!data_app_magic) {
					if (file->f_inode->i_sb->s_magic) {
						data_app_magic = file->f_inode->i_sb->s_magic;
						pr_info("%s: dir: %s got magic! 0x%lx\n", __func__, pos->dirpath, data_app_magic);
					} else {
						filp_close(file, NULL);
						goto skip_iterate;
					}
				}
				
				if (file->f_inode->i_sb->s_magic != data_app_magic) {
					pr_info("%s: skip: %s magic: 0x%lx expected: 0x%lx\n", __func__, pos->dirpath, 
						file->f_inode->i_sb->s_magic, data_app_magic);
					filp_close(file, NULL);
					goto skip_iterate;
				}

				iterate_dir(file, &ctx.ctx);
				filp_close(file, NULL);
			}
skip_iterate:
			list_del(&pos->list);
			if (pos != &data)
				kfree(pos);
		}
	}

	// Remove stale cached APK entries
	list_for_each_entry_safe(pos, n, &apk_path_hash_list, list) {
		if (!pos->exists) {
			list_del(&pos->list);
			kfree(pos);
		}
	}
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

void track_throne()
{
	struct list_head uid_list;
	INIT_LIST_HEAD(&uid_list);
	// scan user data for uids
	int ret = scan_user_data_for_uids(&uid_list);
	
	if (ret < 0) {
		pr_err("UserDE UID scan user data failed: %d.\n", ret);
		goto out;
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
			goto prune;
		}
		pr_info("Searching manager...\n");
		search_manager("/data/app", 2, &uid_list);
		pr_info("Search manager finished\n");
	}
	
prune:
	// then prune the allowlist
	ksu_prune_allowlist(is_uid_exist, &uid_list);
out:
	// free uid_list
	list_for_each_entry_safe (np, n, &uid_list, list) {
		list_del(&np->list);
		kfree(np);
	}
}

void ksu_throne_tracker_init()
{
	// nothing to do
}

void ksu_throne_tracker_exit()
{
	// nothing to do
}

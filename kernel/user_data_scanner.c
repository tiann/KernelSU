#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/stat.h>
#include <linux/namei.h>
#include <linux/sched.h>
#include <linux/mount.h>
#include <linux/magic.h>
#include <linux/jiffies.h>

#include "klog.h"
#include "ksu.h"
#include "kernel_compat.h"
#include "user_data_scanner.h"

static bool scan_all_users __read_mostly = false; // Whether to scan all users' data directories

#define KERN_PATH_TIMEOUT_MS 100
#define MAX_FUSE_CHECK_RETRIES 3

struct work_buffers *get_work_buffer(void)
{
	static struct work_buffers global_buffer;
	return &global_buffer;
}

// Check the file system type
static bool is_dangerous_fs_magic(unsigned long magic)
{
	switch (magic) {
	case 0x65735546:
	case 0x794c7630:
	case 0x01021994:
		return true;
	default:
		return false;
	}
}

static bool is_path_for_kern_path(const char *path, struct super_block *expected_sb)
{
	if (fatal_signal_pending(current)) {
		pr_warn("Fatal signal pending, skip path: %s\n", path);
		return false;
	}

	if (need_resched()) {
		cond_resched();
		if (fatal_signal_pending(current))
			return false;
	}

	if (expected_sb && is_dangerous_fs_magic(expected_sb->s_magic)) {
		pr_info("Skipping dangerous filesystem (magic=0x%lx): %s\n", 
			expected_sb->s_magic, path);
		return false;
	}

	if (!path || strlen(path) == 0 || strlen(path) >= PATH_MAX) {
		return false;
	}

	if (strstr(path, ".tmp") || strstr(path, ".removing") || strstr(path, ".unmounting")) {
		pr_debug("Path appears to be in transition state: %s\n", path);
		return false;
	}

	return true;
}

static int kern_path_with_timeout(const char *path, unsigned int flags, struct path *result)
{
	unsigned long start_time = jiffies;
	unsigned long timeout = start_time + msecs_to_jiffies(KERN_PATH_TIMEOUT_MS);
	int retries = 0;
	int err;

	do {
		if (time_after(jiffies, timeout)) {
			pr_warn("kern_path timeout for: %s\n", path);
			return -ETIMEDOUT;
		}

		if (fatal_signal_pending(current)) {
			pr_warn("Fatal signal during kern_path: %s\n", path);
			return -EINTR;
		}

		err = kern_path(path, flags, result);
		
		if (err == 0) {
			return 0;
		}

		if (err == -ENOENT || err == -ENOTDIR || err == -EACCES) {
			return err;
		}

		if (err == -EBUSY || err == -EAGAIN) {
			retries++;
			if (retries >= MAX_FUSE_CHECK_RETRIES) {
				pr_warn("Max retries reached for: %s (err=%d)\n", path, err);
				return err;
			}
			
			usleep_range(1000, 2000);
			continue;
		}

		return err;

	} while (retries < MAX_FUSE_CHECK_RETRIES);

	return err;
}

FILLDIR_RETURN_TYPE scan_user_packages(struct dir_context *ctx, const char *name,
				       int namelen, loff_t off, u64 ino, unsigned int d_type)
{
	struct user_dir_ctx *uctx = container_of(ctx, struct user_dir_ctx, ctx);
	struct user_scan_ctx *scan_ctx = uctx->scan_ctx;

	if (!scan_ctx || !scan_ctx->deferred_paths)
		return FILLDIR_ACTOR_STOP;

	scan_ctx->processed_count++;
	if (scan_ctx->processed_count % SCHEDULE_INTERVAL == 0) {
		cond_resched();
		if (fatal_signal_pending(current)) {
			pr_info("Fatal signal received, stopping scan\n");
			return FILLDIR_ACTOR_STOP;
		}
	}

	if (d_type != DT_DIR || namelen <= 0)
		return FILLDIR_ACTOR_CONTINUE;
	if (name[0] == '.' && (namelen == 1 || (namelen == 2 && name[1] == '.')))
		return FILLDIR_ACTOR_CONTINUE;

	if (namelen >= KSU_MAX_PACKAGE_NAME) {
		pr_warn("Package name too long: %.*s (user %u)\n", namelen, name, scan_ctx->user_id);
		scan_ctx->error_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	struct deferred_path_info *path_info = kzalloc(sizeof(struct deferred_path_info), GFP_KERNEL);
	if (!path_info) {
		pr_err("Memory allocation failed for path info: %.*s\n", namelen, name);
		scan_ctx->error_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	int path_len = snprintf(path_info->path, sizeof(path_info->path), 
				"%s/%u/%.*s", USER_DATA_BASE_PATH, scan_ctx->user_id, namelen, name);
	if (path_len >= sizeof(path_info->path)) {
		pr_err("Path too long for: %.*s (user %u)\n", namelen, name, scan_ctx->user_id);
		kfree(path_info);
		scan_ctx->error_count++;
		return FILLDIR_ACTOR_CONTINUE;
	}

	path_info->user_id = scan_ctx->user_id;
	size_t copy_len = min_t(size_t, namelen, KSU_MAX_PACKAGE_NAME - 1);
	strncpy(path_info->package_name, name, copy_len);
	path_info->package_name[copy_len] = '\0';

	list_add_tail(&path_info->list, scan_ctx->deferred_paths);
	scan_ctx->pkg_count++;

	return FILLDIR_ACTOR_CONTINUE;
}


static int process_deferred_paths(struct list_head *deferred_paths, struct list_head *uid_list)
{
	struct deferred_path_info *path_info, *n;
	int success_count = 0;
	int skip_count = 0;

	list_for_each_entry_safe(path_info, n, deferred_paths, list) {
		if (!is_path_for_kern_path(path_info->path, NULL)) {
			pr_debug("Skipping unsafe path: %s\n", path_info->path);
			skip_count++;
			list_del(&path_info->list);
			kfree(path_info);
			continue;
		}

		// Retrieve path information
		struct path path;
		int err = kern_path_with_timeout(path_info->path, LOOKUP_FOLLOW, &path);
		if (err) {
			if (err != -ENOENT) {
				pr_debug("Path lookup failed: %s (%d)\n", path_info->path, err);
			}
			list_del(&path_info->list);
			kfree(path_info);
			continue;
		}

		// Check the file system type
		if (is_dangerous_fs_magic(path.mnt->mnt_sb->s_magic)) {
			pr_info("Skipping path on dangerous filesystem: %s (magic=0x%lx)\n", 
				path_info->path, path.mnt->mnt_sb->s_magic);
			path_put(&path);
			list_del(&path_info->list);
			kfree(path_info);
			skip_count++;
			continue;
		}

		struct kstat stat;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0) || defined(KSU_HAS_NEW_VFS_GETATTR)
		err = vfs_getattr(&path, &stat, STATX_UID, AT_STATX_SYNC_AS_STAT);
#else
		err = vfs_getattr(&path, &stat);
#endif
		path_put(&path);

		if (err) {
			pr_debug("Failed to get attributes: %s (%d)\n", path_info->path, err);
			list_del(&path_info->list);
			kfree(path_info);
			continue;
		}

		uid_t uid = from_kuid(&init_user_ns, stat.uid);
		if (uid == (uid_t)-1) {
			pr_warn("Invalid UID for: %s\n", path_info->path);
			list_del(&path_info->list);
			kfree(path_info);
			continue;
		}

		struct uid_data *uid_entry = kzalloc(sizeof(struct uid_data), GFP_KERNEL);
		if (!uid_entry) {
			pr_err("Memory allocation failed for UID entry: %s\n", path_info->path);
			list_del(&path_info->list);
			kfree(path_info);
			continue;
		}

		uid_entry->uid = uid;
		uid_entry->user_id = path_info->user_id;
		strncpy(uid_entry->package, path_info->package_name, KSU_MAX_PACKAGE_NAME - 1);
		uid_entry->package[KSU_MAX_PACKAGE_NAME - 1] = '\0';

		list_add_tail(&uid_entry->list, uid_list);
		success_count++;

		pr_info("Package: %s, UID: %u, User: %u\n", uid_entry->package, uid, path_info->user_id);

		list_del(&path_info->list);
		kfree(path_info);
		
		if (success_count % 10 == 0) {
			cond_resched();
			if (fatal_signal_pending(current)) {
				pr_info("Fatal signal received, stopping path processing\n");
				break;
			}
		}
	}

	if (skip_count > 0) {
		pr_info("Skipped %d potentially dangerous paths for safety\n", skip_count);
	}

	return success_count;
}

static int scan_primary_user_apps(struct list_head *uid_list, 
				   size_t *pkg_count, size_t *error_count,
				   struct work_buffers *work_buf)
{
	struct file *dir_file;
	struct list_head deferred_paths;
	int ret;
	
	*pkg_count = *error_count = 0;
	INIT_LIST_HEAD(&deferred_paths);

	pr_info("Scanning primary user (0) applications in %s\n", PRIMARY_USER_PATH);

	dir_file = ksu_filp_open_compat(PRIMARY_USER_PATH, O_RDONLY, 0);
	if (IS_ERR(dir_file)) {
		pr_err("Cannot open primary user path: %s (%ld)\n", PRIMARY_USER_PATH, PTR_ERR(dir_file));
		return PTR_ERR(dir_file);
	}

	// Check the file system type
	if (is_dangerous_fs_magic(dir_file->f_inode->i_sb->s_magic)) {
		pr_err("Primary user path is on dangerous filesystem (magic=0x%lx), aborting\n",
		       dir_file->f_inode->i_sb->s_magic);
		filp_close(dir_file, NULL);
		return -EOPNOTSUPP;
	}

	struct user_scan_ctx scan_ctx = {
		.deferred_paths = &deferred_paths,
		.user_id = 0,
		.pkg_count = 0,
		.error_count = 0,
		.work_buf = work_buf,
		.processed_count = 0
	};

	struct user_dir_ctx uctx = {
		.ctx.actor = scan_user_packages,
		.scan_ctx = &scan_ctx
	};

	ret = iterate_dir(dir_file, &uctx.ctx);
	filp_close(dir_file, NULL);

	int processed = process_deferred_paths(&deferred_paths, uid_list);
	
	*pkg_count = processed;
	*error_count = scan_ctx.error_count;

	pr_info("Primary user scan completed: %zu packages found, %zu errors\n", 
		*pkg_count, *error_count);

	return ret;
}

FILLDIR_RETURN_TYPE collect_user_ids(struct dir_context *ctx, const char *name,
				     int namelen, loff_t off, u64 ino, unsigned int d_type)
{
	struct user_id_ctx *uctx = container_of(ctx, struct user_id_ctx, ctx);

	uctx->processed_count++;
	if (uctx->processed_count % SCHEDULE_INTERVAL == 0) {
		cond_resched();
		if (fatal_signal_pending(current))
			return FILLDIR_ACTOR_STOP;
	}

	if (d_type != DT_DIR || namelen <= 0)
		return FILLDIR_ACTOR_CONTINUE;
	if (name[0] == '.' && (namelen == 1 || (namelen == 2 && name[1] == '.')))
		return FILLDIR_ACTOR_CONTINUE;

	uid_t uid = 0;
	for (int i = 0; i < namelen; i++) {
		if (name[i] < '0' || name[i] > '9')
			return FILLDIR_ACTOR_CONTINUE;
		uid = uid * 10 + (name[i] - '0');
	}

	if (uctx->count >= uctx->max_count)
		return FILLDIR_ACTOR_STOP;

	uctx->user_ids[uctx->count++] = uid;
	return FILLDIR_ACTOR_CONTINUE;
}

static int get_all_active_users(struct work_buffers *work_buf, size_t *found_count)
{
	struct file *dir_file;
	int ret;

	*found_count = 0;

	dir_file = ksu_filp_open_compat(USER_DATA_BASE_PATH, O_RDONLY, 0);
	if (IS_ERR(dir_file)) {
		pr_err("Cannot open user data base path: %s (%ld)\n", USER_DATA_BASE_PATH, PTR_ERR(dir_file));
		return PTR_ERR(dir_file);
	}

	// Check the file system type of the base path
	if (is_dangerous_fs_magic(dir_file->f_inode->i_sb->s_magic)) {
		pr_err("User data base path is on dangerous filesystem (magic=0x%lx), aborting\n",
		       dir_file->f_inode->i_sb->s_magic);
		filp_close(dir_file, NULL);
		return -EOPNOTSUPP;
	}

	struct user_id_ctx uctx = {
		.ctx.actor = collect_user_ids,
		.user_ids = work_buf->user_ids_buffer,
		.count = 0,
		.max_count = MAX_SUPPORTED_USERS,
		.processed_count = 0
	};

	ret = iterate_dir(dir_file, &uctx.ctx);
	filp_close(dir_file, NULL);

	*found_count = uctx.count;
	if (uctx.count > 0) {
		pr_info("Found %zu active users: ", uctx.count);
		for (size_t i = 0; i < uctx.count; i++) {
			pr_cont("%u ", work_buf->user_ids_buffer[i]);
		}
		pr_cont("\n");
	}

	return ret;
}

static int scan_secondary_users_apps(struct list_head *uid_list, 
				     struct work_buffers *work_buf, size_t user_count,
				     size_t *total_pkg_count, size_t *total_error_count)
{
	int ret = 0;
	*total_pkg_count = *total_error_count = 0;

	for (size_t i = 0; i < user_count; i++) {
		if (fatal_signal_pending(current)) {
			pr_info("Fatal signal received, stopping secondary user scan\n");
			break;
		}

		// Skip the main user since it was already scanned in the first step
		if (work_buf->user_ids_buffer[i] == 0)
			continue;

		struct file *dir_file;
		struct list_head deferred_paths;
		INIT_LIST_HEAD(&deferred_paths);
		
		snprintf(work_buf->path_buffer, sizeof(work_buf->path_buffer), 
			"%s/%u", USER_DATA_BASE_PATH, work_buf->user_ids_buffer[i]);

		dir_file = ksu_filp_open_compat(work_buf->path_buffer, O_RDONLY, 0);
		if (IS_ERR(dir_file)) {
			pr_debug("Cannot open user path: %s (%ld)\n", work_buf->path_buffer, PTR_ERR(dir_file));
			(*total_error_count)++;
			continue;
		}

		// Check the file system type of the user directory
		if (is_dangerous_fs_magic(dir_file->f_inode->i_sb->s_magic)) {
			pr_info("User path %s is on dangerous filesystem (magic=0x%lx), skipping\n",
				work_buf->path_buffer, dir_file->f_inode->i_sb->s_magic);
			filp_close(dir_file, NULL);
			continue;
		}

		struct user_scan_ctx scan_ctx = {
			.deferred_paths = &deferred_paths,
			.user_id = work_buf->user_ids_buffer[i],
			.pkg_count = 0,
			.error_count = 0,
			.work_buf = work_buf,
			.processed_count = 0
		};

		struct user_dir_ctx uctx = {
			.ctx.actor = scan_user_packages,
			.scan_ctx = &scan_ctx
		};

		ret = iterate_dir(dir_file, &uctx.ctx);
		filp_close(dir_file, NULL);

		int processed = process_deferred_paths(&deferred_paths, uid_list);
		
		*total_pkg_count += processed;
		*total_error_count += scan_ctx.error_count;

		if (processed > 0 || scan_ctx.error_count > 0)
			pr_info("User %u: %d packages, %zu errors\n",
				work_buf->user_ids_buffer[i], processed, scan_ctx.error_count);

		cond_resched();
	}

	return ret;
}

int scan_user_data_for_uids(struct list_head *uid_list, bool scan_all_users)
{
	if (!uid_list)
		return -EINVAL;

	struct work_buffers *work_buf = get_work_buffer();
	if (!work_buf) {
		pr_err("Failed to get work buffer\n");
		return -ENOMEM;
	}

	// Scan primary user (User 0)
	size_t primary_pkg_count, primary_error_count;
	int ret = scan_primary_user_apps(uid_list, &primary_pkg_count, &primary_error_count, work_buf);
	if (ret < 0 && primary_pkg_count == 0) {
		pr_err("Primary user scan failed completely: %d\n", ret);
		return ret;
	}

	// If scanning all users is not required, stop here.
	if (!scan_all_users) {
		pr_info("Scan completed (primary user only): %zu packages, %zu errors\n",
			primary_pkg_count, primary_error_count);
		return primary_pkg_count > 0 ? 0 : -ENOENT;
	}

	// Retrieve all active users
	size_t active_users;
	ret = get_all_active_users(work_buf, &active_users);
	if (ret < 0 || active_users == 0) {
		pr_warn("Failed to get active users, using primary user only: %d\n", ret);
		return primary_pkg_count > 0 ? 0 : -ENOENT;
	}

	size_t secondary_pkg_count, secondary_error_count;
	ret = scan_secondary_users_apps(uid_list, work_buf, active_users,
					&secondary_pkg_count, &secondary_error_count);

	size_t total_packages = primary_pkg_count + secondary_pkg_count;
	size_t total_errors = primary_error_count + secondary_error_count;

	if (total_errors > 0)
		pr_warn("Scan completed with %zu errors\n", total_errors);

	pr_info("Complete scan finished: %zu users, %zu total packages\n", 
		active_users, total_packages);

	return total_packages > 0 ? 0 : -ENOENT;
}
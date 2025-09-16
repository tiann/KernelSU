#ifndef _KSU_USER_DATA_SCANNER_H_
#define _KSU_USER_DATA_SCANNER_H_

#include <linux/list.h>
#include <linux/types.h>
#include <linux/fs.h>

#define USER_DATA_BASE_PATH "/data/user_de"
#define PRIMARY_USER_PATH "/data/user_de/0"
#define DATA_PATH_LEN 384 // 384 is enough for /data/user_de/{userid}/<package> and /data/app/<package>/base.apk
#define MAX_SUPPORTED_USERS 32 // Supports up to 32 users
#define SMALL_BUFFER_SIZE 64
#define SCHEDULE_INTERVAL 100
#define MAX_CONCURRENT_WORKERS 8

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

// Global work buffer to avoid stack allocation
struct work_buffers {
	char path_buffer[DATA_PATH_LEN];
	char package_buffer[KSU_MAX_PACKAGE_NAME];
	char small_buffer[SMALL_BUFFER_SIZE];
	uid_t user_ids_buffer[MAX_SUPPORTED_USERS];
};

struct work_buffers *get_work_buffer(void);

struct uid_data {
	struct list_head list;
	u32 uid;
	char package[KSU_MAX_PACKAGE_NAME];
	uid_t user_id;
};

struct deferred_path_info {
	struct list_head list;
	char path[DATA_PATH_LEN];
	char package_name[KSU_MAX_PACKAGE_NAME];
	uid_t user_id;
};

struct user_scan_ctx {
	struct list_head *deferred_paths;
	uid_t user_id;
	size_t pkg_count;
	size_t error_count;
	struct work_buffers *work_buf;
	size_t processed_count;
};

struct user_dir_ctx {
	struct dir_context ctx;
	struct user_scan_ctx *scan_ctx;
};

struct user_id_ctx {
	struct dir_context ctx;
	uid_t *user_ids;
	size_t count;
	size_t max_count;
	size_t processed_count;
};

struct scan_work_item {
	struct work_struct work;
	uid_t user_id;
	struct list_head *uid_list;
	struct mutex *uid_list_mutex;
	atomic_t *total_pkg_count;
	atomic_t *total_error_count;
	struct completion *work_completion;
	atomic_t *remaining_workers;
};

int scan_user_data_for_uids(struct list_head *uid_list, bool scan_all_users);
FILLDIR_RETURN_TYPE scan_user_packages(struct dir_context *ctx, const char *name,
				       int namelen, loff_t off, u64 ino, unsigned int d_type);
FILLDIR_RETURN_TYPE collect_user_ids(struct dir_context *ctx, const char *name,
				     int namelen, loff_t off, u64 ino, unsigned int d_type);
static int process_deferred_paths(struct list_head *deferred_paths, struct list_head *uid_list);
static int scan_primary_user_apps(struct list_head *uid_list, size_t *pkg_count, 
				   size_t *error_count, struct work_buffers *work_buf);
static int get_all_active_users(struct work_buffers *work_buf, size_t *found_count);
static void scan_user_worker(struct work_struct *work);

#endif /* _KSU_USER_DATA_SCANNER_H_ */
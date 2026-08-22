#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/version.h>

#include "policy/allowlist.h"
#include "manager/apk_sign.h"
#include "klog.h" // IWYU pragma: keep
#include "manager/manager_identity.h"
#include "manager/throne_tracker.h"

uid_t ksu_manager_appid = KSU_INVALID_APPID;

#define SYSTEM_PACKAGES_LIST_PATH "/data/system/packages.list"
#define PACKAGES_READ_CHUNK 4096
#define PACKAGES_LINE_PREFIX (KSU_MAX_PACKAGE_NAME + 32)
#define DATA_PATH_LEN 384 // 384 is enough for /data/app/<package>/base.apk

struct uid_data {
    struct list_head list;
    u32 uid;
    char package[KSU_MAX_PACKAGE_NAME];
};

static DEFINE_MUTEX(throne_lock);
static char manager_package[KSU_MAX_PACKAGE_NAME];
static char manager_apk_path[DATA_PATH_LEN];

static void crown_manager(const char *apk, struct list_head *uid_data)
{
    char pkg[KSU_MAX_PACKAGE_NAME];
    if (get_pkg_from_apk_path(pkg, apk) < 0) {
        pr_err("Failed to get package name from apk path: %s\n", apk);
        return;
    }

    pr_info("manager pkg: %s\n", pkg);

    struct list_head *list = (struct list_head *)uid_data;
    struct uid_data *np;

    list_for_each_entry (np, list, list) {
        if (strncmp(np->package, pkg, KSU_MAX_PACKAGE_NAME) == 0) {
            pr_info("Crowning manager: %s(uid=%d)\n", pkg, np->uid);
            strscpy(manager_package, pkg, sizeof(manager_package));
            strscpy(manager_apk_path, apk, sizeof(manager_apk_path));
            ksu_set_manager_appid(np->uid);
            break;
        }
    }
}

struct data_path {
    char dirpath[DATA_PATH_LEN];
    int depth;
    struct list_head list;
};

struct apk_path_hash {
    unsigned int hash;
    u64 ino;
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
extern bool is_manager_apk(char *path);
FILLDIR_RETURN_TYPE my_actor(struct dir_context *ctx, const char *name, int namelen, loff_t off, u64 ino,
                             unsigned int d_type)
{
    struct my_dir_context *my_ctx = container_of(ctx, struct my_dir_context, ctx);
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

    if (d_type == DT_DIR && namelen >= 8 && !strncmp(name, "vmdl", 4) && !strncmp(name + namelen - 4, ".tmp", 4)) {
        pr_info("Skipping directory: %.*s\n", namelen, name);
        return FILLDIR_ACTOR_CONTINUE; // Skip staging package
    }

    if (snprintf(dirpath, DATA_PATH_LEN, "%s/%.*s", my_ctx->parent_dir, namelen, name) >= DATA_PATH_LEN) {
        pr_err("Path too long: %s/%.*s\n", my_ctx->parent_dir, namelen, name);
        return FILLDIR_ACTOR_CONTINUE;
    }

    if (d_type == DT_DIR && my_ctx->depth > 0 && (my_ctx->stop && !*my_ctx->stop)) {
        struct data_path *data = kzalloc(sizeof(struct data_path), GFP_KERNEL);

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
            list_for_each_entry (pos, &apk_path_hash_list, list) {
                if (hash == pos->hash && ino == pos->ino) {
                    pos->exists = true;
                    return FILLDIR_ACTOR_CONTINUE;
                }
            }

            bool is_manager = is_manager_apk(dirpath);
            pr_info("Found new base.apk at path: %s, is_manager: %d\n", dirpath, is_manager);
            if (is_manager) {
                crown_manager(dirpath, my_ctx->private_data);
                *my_ctx->stop = 1;

                // Manager found, clear APK cache list
                list_for_each_entry_safe (pos, n, &apk_path_hash_list, list) {
                    list_del(&pos->list);
                    kfree(pos);
                }
            } else {
                struct apk_path_hash *apk_data = kzalloc(sizeof(struct apk_path_hash), GFP_KERNEL);
                if (!apk_data) {
                    pr_err("Failed to allocate apk_path_hash for %s\n", dirpath);
                    return FILLDIR_ACTOR_CONTINUE;
                }
                apk_data->hash = hash;
                apk_data->ino = ino;
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
    list_for_each_entry (pos, &apk_path_hash_list, list) {
        pos->exists = false;
    }

    // First depth
    struct data_path data;
    strscpy(data.dirpath, path, DATA_PATH_LEN);
    data.depth = depth;
    list_add_tail(&data.list, &data_path_list);

    for (i = depth; i >= 0; i--) {
        struct data_path *pos, *n;

        list_for_each_entry_safe (pos, n, &data_path_list, list) {
            struct my_dir_context ctx = { .ctx.actor = my_actor,
                                          .data_path_list = &data_path_list,
                                          .parent_dir = pos->dirpath,
                                          .private_data = uid_data,
                                          .depth = pos->depth,
                                          .stop = &stop };
            struct file *file;

            if (!stop) {
                file = filp_open(pos->dirpath, O_RDONLY | O_NOFOLLOW, 0);
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
    list_for_each_entry_safe (pos, n, &apk_path_hash_list, list) {
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
        if (np->uid == uid % PER_USER_RANGE && strncmp(np->package, package, KSU_MAX_PACKAGE_NAME) == 0) {
            exist = true;
            break;
        }
    }
    return exist;
}

static int add_uid_line(char *line, struct list_head *uid_list)
{
    struct uid_data *data;
    char *tmp = line;
    char *package;
    char *uid;
    u32 value;

    package = strsep(&tmp, " ");
    uid = strsep(&tmp, " ");
    if (!uid || !package || strnlen(package, KSU_MAX_PACKAGE_NAME) >= KSU_MAX_PACKAGE_NAME)
        return 0;
    if (kstrtou32(uid, 10, &value))
        return 0;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->uid = value;
    strscpy(data->package, package, sizeof(data->package));
    list_add_tail(&data->list, uid_list);
    return 0;
}

static int read_uid_list(struct file *fp, struct list_head *uid_list)
{
    char line[PACKAGES_LINE_PREFIX];
    char *chunk;
    size_t line_len = 0;
    loff_t pos = 0;
    ssize_t count;
    int ret = 0;

    chunk = kmalloc(PACKAGES_READ_CHUNK, GFP_KERNEL);
    if (!chunk)
        return -ENOMEM;

    while ((count = kernel_read(fp, chunk, PACKAGES_READ_CHUNK, &pos)) > 0) {
        ssize_t i;

        for (i = 0; i < count; i++) {
            if (chunk[i] == '\n') {
                line[line_len] = '\0';
                ret = add_uid_line(line, uid_list);
                if (ret)
                    goto out;
                line_len = 0;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = chunk[i];
            }
        }
    }

    if (count < 0) {
        ret = count;
        goto out;
    }

    if (line_len) {
        line[line_len] = '\0';
        ret = add_uid_line(line, uid_list);
    }

out:
    kfree(chunk);
    return ret;
}

static bool manager_entry_exists(struct list_head *uid_list)
{
    struct uid_data *np;
    uid_t appid;

    if (!ksu_is_manager_appid_valid() || !manager_package[0] || !manager_apk_path[0])
        return false;

    appid = ksu_get_manager_appid();
    list_for_each_entry (np, uid_list, list) {
        if (np->uid != appid)
            continue;
        if (strncmp(np->package, manager_package, sizeof(manager_package)))
            continue;
        return is_manager_apk(manager_apk_path);
    }
    return false;
}

void track_throne(bool prune_only)
{
    struct file *fp;
    struct list_head uid_list;
    struct uid_data *np, *n;
    int ret;

    mutex_lock(&throne_lock);
    INIT_LIST_HEAD(&uid_list);

    fp = filp_open(SYSTEM_PACKAGES_LIST_PATH, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_err("%s: open " SYSTEM_PACKAGES_LIST_PATH " failed: %ld\n", __func__, PTR_ERR(fp));
        goto out_unlock;
    }

    ret = read_uid_list(fp, &uid_list);
    filp_close(fp, 0);
    if (ret) {
        pr_err("%s: read " SYSTEM_PACKAGES_LIST_PATH " failed: %d\n", __func__, ret);
        goto out;
    }

#ifndef CONFIG_KSU_DISABLE_MANAGER
    if (!prune_only && !manager_entry_exists(&uid_list)) {
        if (ksu_is_manager_appid_valid()) {
            pr_info("manager is uninstalled or replaced, invalidate it!\n");
            ksu_invalidate_manager_uid();
            manager_package[0] = '\0';
            manager_apk_path[0] = '\0';
        }
        pr_info("Searching manager...\n");
        search_manager("/data/app", 2, &uid_list);
        pr_info("Search manager finished\n");
    }
#endif

    ksu_prune_allowlist(is_uid_exist, &uid_list);
out:
    list_for_each_entry_safe (np, n, &uid_list, list) {
        list_del(&np->list);
        kfree(np);
    }
out_unlock:
    mutex_unlock(&throne_lock);
}

void __init ksu_throne_tracker_init()
{
    manager_package[0] = '\0';
    manager_apk_path[0] = '\0';
}

void __exit ksu_throne_tracker_exit()
{
    struct apk_path_hash *pos, *n;

    mutex_lock(&throne_lock);
    list_for_each_entry_safe (pos, n, &apk_path_hash_list, list) {
        list_del(&pos->list);
        kfree(pos);
    }
    manager_package[0] = '\0';
    manager_apk_path[0] = '\0';
    mutex_unlock(&throne_lock);
}

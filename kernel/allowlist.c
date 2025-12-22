#include <linux/rcupdate.h>
#include <linux/limits.h>
#include <linux/rculist.h>
#include <linux/mutex.h>
#include <linux/task_work.h>
#include <linux/capability.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/compiler_types.h>

#include "klog.h" // IWYU pragma: keep
#include "ksud.h"
#include "selinux/selinux.h"
#include "allowlist.h"
#include "manager.h"
#include "su_mount_ns.h"

#define FILE_MAGIC 0x7f4b5355 // ' KSU', u32
#define FILE_FORMAT_VERSION 3 // u32

#define KSU_APP_PROFILE_PRESERVE_UID 9999 // NOBODY_UID
#define KSU_DEFAULT_SELINUX_DOMAIN "u:r:" KERNEL_SU_DOMAIN ":s0"

static DEFINE_MUTEX(allowlist_mutex);

// default profiles, these may be used frequently, so we cache it
static struct root_profile default_root_profile;
static struct non_root_profile default_non_root_profile;

static int allow_list_arr[PAGE_SIZE / sizeof(int)] __read_mostly
    __aligned(PAGE_SIZE);
static int allow_list_pointer __read_mostly = 0;

static void remove_uid_from_arr(uid_t uid)
{
    int *temp_arr;
    int i, j;

    if (allow_list_pointer == 0)
        return;

    temp_arr = kzalloc(sizeof(allow_list_arr), GFP_KERNEL);
    if (temp_arr == NULL) {
        pr_err("%s: unable to allocate memory\n", __func__);
        return;
    }

    for (i = j = 0; i < allow_list_pointer; i++) {
        if (allow_list_arr[i] == uid)
            continue;
        temp_arr[j++] = allow_list_arr[i];
    }

    allow_list_pointer = j;

    for (; j < ARRAY_SIZE(allow_list_arr); j++)
        temp_arr[j] = -1;

    memcpy(&allow_list_arr, temp_arr, PAGE_SIZE);
    kfree(temp_arr);
}

static void init_default_profiles()
{
    kernel_cap_t full_cap = CAP_FULL_SET;

    default_root_profile.uid = 0;
    default_root_profile.gid = 0;
    default_root_profile.groups_count = 1;
    default_root_profile.groups[0] = 0;
    memcpy(&default_root_profile.capabilities.effective, &full_cap,
           sizeof(default_root_profile.capabilities.effective));
    default_root_profile.namespaces = KSU_NS_INHERITED;
    strcpy(default_root_profile.selinux_domain, KSU_DEFAULT_SELINUX_DOMAIN);

    // This means that we will umount modules by default!
    default_non_root_profile.umount_modules = true;
}

struct perm_data {
    struct list_head list;
    struct rcu_head rcu;
    struct app_profile profile;
};

static struct list_head allow_list;

static uint8_t allow_list_bitmap[PAGE_SIZE] __read_mostly __aligned(PAGE_SIZE);
#define BITMAP_UID_MAX ((sizeof(allow_list_bitmap) * BITS_PER_BYTE) - 1)

#define KERNEL_SU_ALLOWLIST "/data/adb/ksu/.allowlist"

void ksu_persistent_allow_list(void);

void ksu_show_allow_list(void)
{
    struct perm_data *p = NULL;
    pr_info("ksu_show_allow_list\n");
    rcu_read_lock();
    list_for_each_entry_rcu (p, &allow_list, list) {
        pr_info("uid :%d, allow: %d\n", p->profile.current_uid,
                p->profile.allow_su);
    }
    rcu_read_unlock();
}

#ifdef CONFIG_KSU_DEBUG
static void ksu_grant_root_to_shell()
{
    struct app_profile profile = {
        .version = KSU_APP_PROFILE_VER,
        .allow_su = true,
        .current_uid = 2000,
    };
    strcpy(profile.key, "com.android.shell");
    strcpy(profile.rp_config.profile.selinux_domain,
           KSU_DEFAULT_SELINUX_DOMAIN);
    ksu_set_app_profile(&profile);
}
#endif

bool ksu_get_app_profile(struct app_profile *profile)
{
    struct perm_data *p = NULL;
    bool found = false;

    rcu_read_lock();
    list_for_each_entry_rcu (p, &allow_list, list) {
        bool uid_match = profile->current_uid == p->profile.current_uid;
        if (uid_match) {
            // found it, override it with ours
            memcpy(profile, &p->profile, sizeof(*profile));
            found = true;
            goto exit;
        }
    }

exit:
    rcu_read_unlock();
    return found;
}

static inline bool forbid_system_uid(uid_t uid)
{
#define SHELL_UID 2000
#define SYSTEM_UID 1000
    return uid < SHELL_UID && uid != SYSTEM_UID;
}

static bool profile_valid(struct app_profile *profile)
{
    if (!profile) {
        return false;
    }

    if (profile->version < KSU_APP_PROFILE_VER) {
        pr_info("Unsupported profile version: %d\n", profile->version);
        return false;
    }

    if (profile->allow_su) {
        if (profile->rp_config.profile.groups_count > KSU_MAX_GROUPS) {
            return false;
        }

        if (strlen(profile->rp_config.profile.selinux_domain) == 0) {
            return false;
        }
    }

    return true;
}

int ksu_set_app_profile(struct app_profile *profile)
{
    struct perm_data *p = NULL, *np;
    int result = 0;
    u16 count = 0;

    if (!profile_valid(profile)) {
        pr_err("Failed to set app profile: invalid profile!\n");
        return -EINVAL;
    }

    mutex_lock(&allowlist_mutex);

    list_for_each_entry (p, &allow_list, list) {
        ++count;
        // both uid and package must match, otherwise it will break multiple package with different user id
        if (profile->current_uid == p->profile.current_uid &&
            !strcmp(profile->key, p->profile.key)) {
            // found it, just override it all!
            np = (struct perm_data *)kzalloc(sizeof(struct perm_data),
                                             GFP_KERNEL);
            if (!np) {
                result = -ENOMEM;
                goto out_unlock;
            }
            memcpy(&np->profile, profile, sizeof(*profile));
            list_replace_rcu(&p->list, &np->list);
            kfree_rcu(p, rcu);
            goto out;
        }
    }

    if (unlikely(count == U16_MAX)) {
        pr_err("too many app profile\n");
        result = -E2BIG;
        goto out_unlock;
    }

    // not found, alloc a new node!
    p = (struct perm_data *)kzalloc(sizeof(struct perm_data), GFP_KERNEL);
    if (!p) {
        pr_err("ksu_set_app_profile alloc failed\n");
        result = -ENOMEM;
        goto out_unlock;
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

    list_add_tail_rcu(&p->list, &allow_list);

out:
    result = 0;

    // check if the default profiles is changed, cache it to a single struct to accelerate access.
    if (unlikely(!strcmp(profile->key, "$"))) {
        // set default non root profile
        memcpy(&default_non_root_profile, &profile->nrp_config.profile,
               sizeof(default_non_root_profile));
    } else if (unlikely(!strcmp(profile->key, "#"))) {
        // set default root profile
        // TODO: Do we really need this?
        memcpy(&default_root_profile, &profile->rp_config.profile,
               sizeof(default_root_profile));
    } else if (profile->current_uid <= BITMAP_UID_MAX) {
        if (profile->allow_su)
            allow_list_bitmap[profile->current_uid / BITS_PER_BYTE] |=
                1 << (profile->current_uid % BITS_PER_BYTE);
        else
            allow_list_bitmap[profile->current_uid / BITS_PER_BYTE] &=
                ~(1 << (profile->current_uid % BITS_PER_BYTE));
    } else {
        if (profile->allow_su) {
            /*
             * 1024 apps with uid higher than BITMAP_UID_MAX
             * registered to request superuser?
             */
            if (allow_list_pointer >= ARRAY_SIZE(allow_list_arr)) {
                pr_err("too many apps registered\n");
                WARN_ON(1);
            } else {
                allow_list_arr[allow_list_pointer++] = profile->current_uid;
            }
        } else {
            remove_uid_from_arr(profile->current_uid);
        }
    }

out_unlock:
    mutex_unlock(&allowlist_mutex);
    return result;
}

bool __ksu_is_allow_uid(uid_t uid)
{
    int i;

    if (forbid_system_uid(uid)) {
        // do not bother going through the list if it's system
        return false;
    }

    if (likely(ksu_is_manager_appid_valid()) &&
        unlikely(ksu_get_manager_appid() == uid % PER_USER_RANGE)) {
        // manager is always allowed!
        return true;
    }

    if (likely(uid <= BITMAP_UID_MAX)) {
        return !!(allow_list_bitmap[uid / BITS_PER_BYTE] &
                  (1 << (uid % BITS_PER_BYTE)));
    } else {
        for (i = 0; i < allow_list_pointer; i++) {
            if (allow_list_arr[i] == uid)
                return true;
        }
    }

    return false;
}

bool __ksu_is_allow_uid_for_current(uid_t uid)
{
    if (unlikely(uid == 0)) {
        // already root, but only allow our domain.
        return is_ksu_domain();
    }
    return __ksu_is_allow_uid(uid);
}

bool ksu_uid_should_umount(uid_t uid)
{
    struct app_profile profile = { .current_uid = uid };
    if (likely(ksu_is_manager_appid_valid()) &&
        unlikely(ksu_get_manager_appid() == uid % PER_USER_RANGE)) {
        // we should not umount on manager!
        return false;
    }
    bool found = ksu_get_app_profile(&profile);
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

void ksu_get_root_profile(uid_t uid, struct root_profile *profile)
{
    struct perm_data *p = NULL;

    if (is_uid_manager(uid)) {
        goto use_default;
    }

    rcu_read_lock();
    list_for_each_entry_rcu (p, &allow_list, list) {
        if (uid == p->profile.current_uid && p->profile.allow_su) {
            if (!p->profile.rp_config.use_default) {
                memcpy(profile, &p->profile.rp_config.profile,
                       sizeof(*profile));
                rcu_read_unlock();
                return;
            }
        }
    }
    rcu_read_unlock();

use_default:
    // use default profile
    memcpy(profile, &default_root_profile, sizeof(*profile));
}

bool ksu_get_allow_list(int *array, u16 length, u16 *out_length, u16 *out_total,
                        bool allow)
{
    struct perm_data *p = NULL;
    u16 i = 0, j = 0;
    rcu_read_lock();
    list_for_each_entry_rcu (p, &allow_list, list) {
        // pr_info("get_allow_list uid: %d allow: %d\n", p->uid, p->allow);
        if (p->profile.allow_su == allow &&
            !is_uid_manager(p->profile.current_uid)) {
            if (j < length) {
                array[j++] = p->profile.current_uid;
            }
            ++i;
        }
    }
    rcu_read_unlock();
    if (out_length) {
        *out_length = j;
    }
    if (out_total) {
        *out_total = i;
    }

    return true;
}

// TODO: move to kernel thread or work queue
static void do_persistent_allow_list(struct callback_head *_cb)
{
    u32 magic = FILE_MAGIC;
    u32 version = FILE_FORMAT_VERSION;
    struct perm_data *p = NULL;
    loff_t off = 0;

    struct file *fp =
        filp_open(KERNEL_SU_ALLOWLIST, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (IS_ERR(fp)) {
        pr_err("save_allow_list create file failed: %ld\n", PTR_ERR(fp));
        goto out;
    }

    // store magic and version
    if (kernel_write(fp, &magic, sizeof(magic), &off) != sizeof(magic)) {
        pr_err("save_allow_list write magic failed.\n");
        goto close_file;
    }

    if (kernel_write(fp, &version, sizeof(version), &off) != sizeof(version)) {
        pr_err("save_allow_list write version failed.\n");
        goto close_file;
    }

    mutex_lock(&allowlist_mutex);
    list_for_each_entry (p, &allow_list, list) {
        pr_info("save allow list, name: %s uid :%d, allow: %d\n",
                p->profile.key, p->profile.current_uid, p->profile.allow_su);

        kernel_write(fp, &p->profile, sizeof(p->profile), &off);
    }
    mutex_unlock(&allowlist_mutex);

close_file:
    filp_close(fp, 0);
out:
    kfree(_cb);
}

void ksu_persistent_allow_list()
{
    struct task_struct *tsk;

    tsk = get_pid_task(find_vpid(1), PIDTYPE_PID);
    if (!tsk) {
        pr_err("save_allow_list find init task err\n");
        return;
    }

    struct callback_head *cb =
        kzalloc(sizeof(struct callback_head), GFP_KERNEL);
    if (!cb) {
        pr_err("save_allow_list alloc cb err\b");
        goto put_task;
    }
    cb->func = do_persistent_allow_list;
    if (task_work_add(tsk, cb, TWA_RESUME)) {
        kfree(cb);
        pr_warn("save_allow_list add task_work failed\n");
    }

put_task:
    put_task_struct(tsk);
}

void ksu_load_allow_list()
{
    loff_t off = 0;
    ssize_t ret = 0;
    struct file *fp = NULL;
    u32 magic;
    u32 version;

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
    if (kernel_read(fp, &magic, sizeof(magic), &off) != sizeof(magic) ||
        magic != FILE_MAGIC) {
        pr_err("allowlist file invalid: %d!\n", magic);
        goto exit;
    }

    if (kernel_read(fp, &version, sizeof(version), &off) != sizeof(version)) {
        pr_err("allowlist read version: %d failed\n", version);
        goto exit;
    }

    pr_info("allowlist version: %d\n", version);

    while (true) {
        struct app_profile profile;

        ret = kernel_read(fp, &profile, sizeof(profile), &off);

        if (ret <= 0) {
            pr_info("load_allow_list read err: %zd\n", ret);
            break;
        }

        pr_info("load_allow_uid, name: %s, uid: %d, allow: %d\n", profile.key,
                profile.current_uid, profile.allow_su);
        ksu_set_app_profile(&profile);
    }

exit:
    ksu_show_allow_list();
    filp_close(fp, 0);
}

void ksu_prune_allowlist(bool (*is_uid_valid)(uid_t, char *, void *),
                         void *data)
{
    struct perm_data *np = NULL;
    struct perm_data *n = NULL;

    if (!ksu_boot_completed) {
        pr_info("boot not completed, skip prune\n");
        return;
    }

    bool modified = false;
    mutex_lock(&allowlist_mutex);
    list_for_each_entry_safe (np, n, &allow_list, list) {
        uid_t uid = np->profile.current_uid;
        char *package = np->profile.key;
        // we use this uid for special cases, don't prune it!
        bool is_preserved_uid = uid == KSU_APP_PROFILE_PRESERVE_UID;
        if (!is_preserved_uid && !is_uid_valid(uid, package, data)) {
            modified = true;
            pr_info("prune uid: %d, package: %s\n", uid, package);
            list_del_rcu(&np->list);
            kfree_rcu(np, rcu);
            if (likely(uid <= BITMAP_UID_MAX)) {
                allow_list_bitmap[uid / BITS_PER_BYTE] &=
                    ~(1 << (uid % BITS_PER_BYTE));
            }
            remove_uid_from_arr(uid);
        }
    }
    mutex_unlock(&allowlist_mutex);

    if (modified) {
        smp_mb();
        ksu_persistent_allow_list();
    }
}

void ksu_allowlist_init(void)
{
    int i;

    BUILD_BUG_ON(sizeof(allow_list_bitmap) != PAGE_SIZE);
    BUILD_BUG_ON(sizeof(allow_list_arr) != PAGE_SIZE);

    for (i = 0; i < ARRAY_SIZE(allow_list_arr); i++)
        allow_list_arr[i] = -1;

    INIT_LIST_HEAD(&allow_list);

    init_default_profiles();
}

void ksu_allowlist_exit(void)
{
    struct perm_data *np = NULL;
    struct perm_data *n = NULL;

    // free allowlist
    mutex_lock(&allowlist_mutex);
    list_for_each_entry_safe (np, n, &allow_list, list) {
        list_del(&np->list);
        kfree(np);
    }
    mutex_unlock(&allowlist_mutex);
}

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
#include <linux/hashtable.h>
#include <linux/kref.h>

#include "klog.h" // IWYU pragma: keep
#include "ksu.h"
#include "runtime/ksud_boot.h"
#include "selinux/selinux.h"
#include "policy/allowlist.h"
#include "manager/manager_identity.h"
#include "infra/su_mount_ns.h"

#define FILE_MAGIC 0x7f4b5355 // ' KSU', u32
#define FILE_FORMAT_VERSION 3 // u32

#define KSU_APP_PROFILE_PRESERVE_UID 9999 // NOBODY_UID
#define KSU_DEFAULT_SELINUX_DOMAIN "u:r:" KERNEL_SU_DOMAIN ":s0"

static DEFINE_MUTEX(allowlist_mutex);

// default profiles, these may be used frequently, so we cache it
struct root_profile default_root_profile;
static struct non_root_profile default_non_root_profile;

static void __init init_default_profiles()
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
    struct hlist_node list;
    struct rcu_head rcu;
    struct kref ref;
    struct app_profile profile;
};

// protected by rcu
#define ALLOW_LIST_BITS 8
static DEFINE_HASHTABLE(allow_list, ALLOW_LIST_BITS);
static u16 allow_list_count = 0;

#define KERNEL_SU_ALLOWLIST "/data/adb/ksu/.allowlist"

void ksu_persistent_allow_list(void);

void ksu_show_allow_list(void)
{
    int i;
    struct perm_data *p = NULL;
    pr_info("ksu_show_allow_list\n");
    rcu_read_lock();
    hash_for_each_rcu (allow_list, i, p, list) {
        pr_info("uid :%d, allow: %d\n", p->profile.curr_uid, p->profile.allow_su);
    }
    rcu_read_unlock();
}

struct app_profile *ksu_get_app_profile(uid_t uid)
{
    struct perm_data *p = NULL;
    bool found;

retry:
    found = false;
    hash_for_each_possible_rcu (allow_list, p, list, uid) {
        if (uid == p->profile.curr_uid) {
            // found it, override it with ours
            found = true;
            break;
        }
    }

    if (!found)
        return NULL;

    if (!kref_get_unless_zero(&p->ref)) {
        goto retry;
    }

    return &p->profile;
}

static inline bool forbid_system_uid(uid_t uid)
{
    return uid < SHELL_UID && uid != SYSTEM_UID;
}

static bool profile_valid(struct app_profile *profile)
{
    if (!profile) {
        return false;
    }

    bool need_migrate_su_domain = false;

    if (unlikely(profile->version == 2)) {
        profile->version = KSU_APP_PROFILE_VER;
        need_migrate_su_domain = true;
    }

    if (strnlen(profile->key, sizeof(profile->key)) >= sizeof(profile->key)) {
        pr_err("invalid app_profile key\n");
        return false;
    }

    if (profile->version < KSU_APP_PROFILE_VER) {
        pr_info("Unsupported profile version: %d\n", profile->version);
        return false;
    }

    if (profile->allow_su) {
#ifndef CONFIG_KSU_DISABLE_POLICY
        if (profile->rp_config.profile.groups_count > KSU_MAX_GROUPS) {
            pr_err("invalid groups_count in app_profile: %s\n", profile->key);
            return false;
        }

        char *domain = profile->rp_config.profile.selinux_domain;
        static const size_t domain_len = sizeof(profile->rp_config.profile.selinux_domain);
        if (unlikely(need_migrate_su_domain)) {
            if (strncmp(domain, "u:r:su:s0", domain_len) == 0) {
                strscpy_pad(domain, KSU_DEFAULT_SELINUX_DOMAIN, domain_len);
                pr_info("migrated profile domain: %s\n", profile->key);
            }
        }
        size_t len = strnlen(domain, domain_len);

        if (len == 0 || len >= domain_len) {
            pr_err("invalid selinux_domain in app_profile: %s\n", profile->key);
            return false;
        }
#endif
    }

    return true;
}

static void release_perm_data(struct kref *ref)
{
    struct perm_data *p = container_of(ref, struct perm_data, ref);
    kfree_rcu(p, rcu);
}

static void put_perm_data(struct perm_data *data)
{
    kref_put(&data->ref, release_perm_data);
}

int ksu_set_app_profile(struct app_profile *profile)
{
    struct perm_data *p, *np;
    int result = 0;

    if (!profile_valid(profile)) {
        pr_err("Failed to set app profile: invalid profile!\n");
        return -EINVAL;
    }

#ifdef CONFIG_KSU_DISABLE_POLICY
    if (profile->allow_su) {
        profile->rp_config.use_default = true;
        memset(profile->rp_config.template_name, 0, sizeof(profile->rp_config.template_name));
        memset(&profile->rp_config.profile, 0, sizeof(profile->rp_config.profile));
    } else {
        profile->nrp_config.use_default = true;
        memset(&profile->nrp_config.profile, 0, sizeof(profile->nrp_config.profile));
    }
#endif

    // only allow default non root profile
    if (unlikely(profile->curr_uid == KSU_APP_PROFILE_PRESERVE_UID && strcmp(profile->key, "$") != 0)) {
        return -EINVAL;
    }

    mutex_lock(&allowlist_mutex);

    hash_for_each_possible (allow_list, p, list, profile->curr_uid) {
        if (profile->curr_uid == p->profile.curr_uid) {
            if (strcmp(profile->key, p->profile.key) != 0) {
                pr_warn("ksu_set_app_profile: key changed: uid=%d orig=%s new=%s\n", profile->curr_uid, p->profile.key,
                        profile->key);
            }
            // found it, just override it all!
            np = (struct perm_data *)kzalloc(sizeof(struct perm_data), GFP_KERNEL);
            if (!np) {
                result = -ENOMEM;
                goto out_unlock;
            }
            kref_init(&np->ref);
            memcpy(&np->profile, profile, sizeof(*profile));
            hlist_replace_rcu(&p->list, &np->list);
            put_perm_data(p);
            goto out;
        }
    }

    if (unlikely(allow_list_count == U16_MAX)) {
        pr_err("too many app profile\n");
        result = -E2BIG;
        goto out_unlock;
    }

    // not found, alloc a new node!
    np = (struct perm_data *)kzalloc(sizeof(struct perm_data), GFP_KERNEL);
    if (!np) {
        pr_err("ksu_set_app_profile alloc failed\n");
        result = -ENOMEM;
        goto out_unlock;
    }

    kref_init(&np->ref);
    memcpy(&np->profile, profile, sizeof(*profile));
    if (profile->allow_su) {
        pr_info("set root profile, key: %s, uid: %d, gid: %d, context: %s\n", profile->key, profile->curr_uid,
                profile->rp_config.profile.gid, profile->rp_config.profile.selinux_domain);
    } else {
        pr_info("set app profile, key: %s, uid: %d, umount modules: %d\n", profile->key, profile->curr_uid,
                profile->nrp_config.profile.umount_modules);
    }

    hash_add_rcu(allow_list, &np->list, np->profile.curr_uid);
    ++allow_list_count;

out:
    result = 0;

    if (unlikely(profile->curr_uid == KSU_APP_PROFILE_PRESERVE_UID)) {
        // set default non root profile
        default_non_root_profile.umount_modules = profile->nrp_config.profile.umount_modules;
    }

out_unlock:
    mutex_unlock(&allowlist_mutex);
    return result;
}

bool __ksu_is_allow_uid(uid_t uid)
{
    struct perm_data *p;

    if (forbid_system_uid(uid)) {
        // do not bother going through the list if it's system
        return false;
    }

    if (unlikely(is_uid_manager(uid))) {
        // manager is always allowed!
        return true;
    }

    if (unlikely(allow_shell) && uid == SHELL_UID) {
        return true;
    }

    rcu_read_lock();
    hash_for_each_possible_rcu (allow_list, p, list, uid) {
        if (uid == p->profile.curr_uid && p->profile.allow_su) {
            rcu_read_unlock();
            return true;
        }
    }
    rcu_read_unlock();

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
    struct app_profile *profile;
    bool res;
    if (likely(ksu_is_manager_appid_valid()) && unlikely(ksu_get_manager_appid() == uid % PER_USER_RANGE)) {
        // we should not umount on manager!
        return false;
    }
    if (unlikely(uid == WEBVIEW_ZYGOTE_UID)) {
        // we should not umount for webview zygote
        return false;
    }
#ifdef CONFIG_KSU_DISABLE_POLICY
    return !__ksu_is_allow_uid(uid);
#else
    rcu_read_lock();
    profile = ksu_get_app_profile(uid);
    if (!profile) {
        // no app profile found, it must be non root app
        res = default_non_root_profile.umount_modules;
    } else if (profile->allow_su) {
        // if found and it is granted to su, we shouldn't umount for it
        res = false;
    } else {
        // found an app profile
        if (profile->nrp_config.use_default) {
            res = default_non_root_profile.umount_modules;
        } else {
            res = profile->nrp_config.profile.umount_modules;
        }
    }
    rcu_read_unlock();

    if (profile)
        ksu_put_app_profile(profile);
    return res;
#endif
}

void ksu_put_app_profile(struct app_profile *profile)
{
    struct perm_data *p = container_of(profile, struct perm_data, profile);
    put_perm_data(p);
}

struct app_profile *ksu_get_root_app_profile(uid_t uid)
{
    struct perm_data *p = NULL;
    struct app_profile *res;

    rcu_read_lock();
retry:
    res = NULL;
    hash_for_each_possible_rcu (allow_list, p, list, uid) {
        if (uid == p->profile.curr_uid && p->profile.allow_su) {
            if (!kref_get_unless_zero(&p->ref)) {
                goto retry;
            }
            res = &p->profile;
            break;
        }
    }

    rcu_read_unlock();
    return res;
}

bool ksu_get_allow_list(int *array, u16 length, u16 *out_length, u16 *out_total, bool allow)
{
    struct perm_data *p = NULL;
    u16 i = 0, j = 0;
    int iter;
    rcu_read_lock();
    hash_for_each_rcu (allow_list, iter, p, list) {
        // pr_info("get_allow_list uid: %d allow: %d\n", p->uid, p->allow);
        if (p->profile.allow_su == allow && !is_uid_manager(p->profile.curr_uid)) {
            if (j < length) {
                array[j++] = p->profile.curr_uid;
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
    int i;

    const struct cred *saved = override_creds(ksu_cred);
    struct file *fp = filp_open(KERNEL_SU_ALLOWLIST, O_WRONLY | O_CREAT | O_TRUNC, 0644);
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
    hash_for_each (allow_list, i, p, list) {
        pr_info("save allow list, name: %s uid :%d, allow: %d\n", p->profile.key, p->profile.curr_uid,
                p->profile.allow_su);

        kernel_write(fp, &p->profile, sizeof(p->profile), &off);
    }
    mutex_unlock(&allowlist_mutex);

close_file:
    filp_close(fp, 0);
out:
    revert_creds(saved);
    kfree(_cb);
}

void ksu_persistent_allow_list()
{
    struct task_struct *tsk;

    rcu_read_lock();
    tsk = get_pid_task(find_vpid(1), PIDTYPE_PID);
    if (!tsk) {
        rcu_read_unlock();
        pr_err("save_allow_list find init task err\n");
        return;
    }
    rcu_read_unlock();

    struct callback_head *cb = kzalloc(sizeof(struct callback_head), GFP_KERNEL);
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
#ifdef CONFIG_KSU_DISABLE_POLICY
    pr_info("allowlist load skipped because policy is disabled\n");
    return;
#endif

    loff_t off = 0;
    ssize_t ret = 0;
    struct file *fp = NULL;
    u32 magic;
    u32 version;

    // load allowlist now!
    fp = filp_open(KERNEL_SU_ALLOWLIST, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        pr_err("load_allow_list open file failed: %ld\n", PTR_ERR(fp));
        return;
    }

    // verify magic
    if (kernel_read(fp, &magic, sizeof(magic), &off) != sizeof(magic) || magic != FILE_MAGIC) {
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

        pr_info("load_allow_uid, name: %s, uid: %d, allow: %d\n", profile.key, profile.curr_uid, profile.allow_su);
        ksu_set_app_profile(&profile);
    }

exit:
    ksu_show_allow_list();
    filp_close(fp, 0);
}

void ksu_prune_allowlist(bool (*is_uid_valid)(uid_t, char *, void *), void *data)
{
    struct perm_data *np = NULL;
    struct hlist_node *tmp;
    int i;

    if (!ksu_boot_completed) {
        pr_info("boot not completed, skip prune\n");
        return;
    }

    bool modified = false;
    mutex_lock(&allowlist_mutex);
    hash_for_each_safe (allow_list, i, tmp, np, list) {
        uid_t uid = np->profile.curr_uid;
        char *package = np->profile.key;
        // we use this uid for special cases, don't prune it!
        bool is_preserved_uid = uid == KSU_APP_PROFILE_PRESERVE_UID;
        if (!is_preserved_uid && !is_uid_valid(uid, package, data)) {
            modified = true;
            pr_info("prune uid: %d, package: %s\n", uid, package);
            hlist_del_rcu(&np->list);
            put_perm_data(np);
            --allow_list_count;
        }
    }
    mutex_unlock(&allowlist_mutex);

    if (modified) {
        smp_mb();
        ksu_persistent_allow_list();
    }
}

void __init ksu_allowlist_init(void)
{
    init_default_profiles();
}

void __exit ksu_allowlist_exit(void)
{
    struct perm_data *np = NULL;
    struct hlist_node *tmp;
    int i;

    // free allowlist
    mutex_lock(&allowlist_mutex);
    hash_for_each_safe (allow_list, i, tmp, np, list) {
        hlist_del(&np->list);
        put_perm_data(np);
    }
    mutex_unlock(&allowlist_mutex);
}

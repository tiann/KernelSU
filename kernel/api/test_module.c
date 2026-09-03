// Out-of-tree smoke test for the KernelSU kernel plugin API.
//
// This file is intentionally not part of kernelsu.ko. Build it as a separate
// module with CONFIG_KSU_API_TEST=y (see Kbuild); it only depends on the
// exported ksu_get_api() symbol and the public headers under api/.
//
// The module exercises registration rules at load time (duplicate handlers,
// priority ordering, replay, self-unregister, reboot magic conflicts) and
// then counts every runtime event until it is unloaded.
#include <linux/atomic.h>
#include <linux/cred.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/string.h>

#include "api/ksu_api.h"

#define TEST_TAG "ksu_api_test: "
#define TEST_REBOOT_MAGIC1 0x4B535541U /* "KSUA" */
#define TEST_REBOOT_MAGIC2 0x54455354U /* "TEST" */

static struct ksu_api api;
static void *cookies[KSU_EVENT_MAX];
static atomic_t counts[KSU_EVENT_MAX];
static void *reboot_cookie;
static atomic_t reboot_hits = ATOMIC_INIT(0);
static atomic_t order_seq = ATOMIC_INIT(0);
static atomic_t order_pre_seq = ATOMIC_INIT(0);
static int order_seen[3];
static int order_pre_seen[3];
static void *throwaway_cookie;
static void *order_cookies[3];
static void *self_cookie;
static int self_unregister_ret = 0xdead;
static int cross_unregister_ret = 0xdead;
static int verbose = 1;
module_param(verbose, int, 0644);
MODULE_PARM_DESC(verbose, "log every event (0 only counts, 1 first 8 of noisy events, 2 all)");

static const char *const event_names[KSU_EVENT_MAX] = {
    [KSU_EVENT_CORE_READY] = "CORE_READY",
    [KSU_EVENT_POST_FS_DATA] = "POST_FS_DATA",
    [KSU_EVENT_MODULE_MOUNTED] = "MODULE_MOUNTED",
    [KSU_EVENT_BOOT_COMPLETED] = "BOOT_COMPLETED",
    [KSU_EVENT_CORE_EXITING] = "CORE_EXITING",
    [KSU_EVENT_UID_COMMITTED] = "UID_COMMITTED",
    [KSU_EVENT_EXEC_POST] = "EXEC_POST",
    [KSU_EVENT_ROOT_GRANTED] = "ROOT_GRANTED",
    [KSU_EVENT_MANAGER_READY] = "MANAGER_READY",
    [KSU_EVENT_KSU_UMOUNT_PRE] = "KSU_UMOUNT_PRE",
    [KSU_EVENT_KSU_UMOUNT_ITEM] = "KSU_UMOUNT_ITEM",
    [KSU_EVENT_KSU_UMOUNT_POST] = "KSU_UMOUNT_POST",
    [KSU_EVENT_ALLOWLIST_CHANGED] = "ALLOWLIST_CHANGED",
    [KSU_EVENT_PROFILE_CHANGED] = "PROFILE_CHANGED",
    [KSU_EVENT_FEATURE_CHANGED] = "FEATURE_CHANGED",
    [KSU_EVENT_SELINUX_READY] = "SELINUX_READY",
    [KSU_EVENT_SUPERCALL_POST] = "SUPERCALL_POST",
};

static bool should_log(enum ksu_event event, int n)
{
    if (verbose >= 2)
        return true;
    if (verbose <= 0)
        return false;
    if (event == KSU_EVENT_EXEC_POST || event == KSU_EVENT_UID_COMMITTED || event == KSU_EVENT_KSU_UMOUNT_ITEM ||
        event == KSU_EVENT_SUPERCALL_POST)
        return n <= 8;
    return true;
}

static void generic_post(const void *ctx, long result, void *data)
{
    enum ksu_event event = (enum ksu_event)(unsigned long)data;
    int n;

    if (event >= KSU_EVENT_MAX)
        return;
    n = atomic_inc_return(&counts[event]);
    if (!should_log(event, n))
        return;

    switch (event) {
    case KSU_EVENT_UID_COMMITTED: {
        const struct ksu_uid_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d nr=%u %u->%u pid=%u tgid=%u allow=%u/%u zygote=%u\n", event_names[event], n,
                    e->syscall_nr, e->old_uid, e->new_uid, e->pid, e->tgid, e->old_allowlisted, e->new_allowlisted,
                    e->is_zygote_child);
        break;
    }
    case KSU_EVENT_EXEC_POST: {
        const struct ksu_exec_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d nr=%u ret=%d pid=%u path=%s\n", event_names[event], n, e->syscall_nr, e->result,
                    e->pid, e->path);
        break;
    }
    case KSU_EVENT_KSU_UMOUNT_PRE:
    case KSU_EVENT_KSU_UMOUNT_ITEM:
    case KSU_EVENT_KSU_UMOUNT_POST: {
        const struct ksu_umount_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d %u->%u pid=%u flags=0x%x ret=%d target=%s\n", event_names[event], n, e->old_uid,
                    e->new_uid, e->pid, e->flags, e->result, e->target ? e->target : "-");
        break;
    }
    case KSU_EVENT_ROOT_GRANTED: {
        const struct ksu_root_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d uid=%u pid=%u ret=%d\n", event_names[event], n, e->uid, e->pid, e->result);
        break;
    }
    case KSU_EVENT_MANAGER_READY:
    case KSU_EVENT_ALLOWLIST_CHANGED:
    case KSU_EVENT_PROFILE_CHANGED: {
        const struct ksu_policy_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d uid=%u id=%u ret=%d\n", event_names[event], n, e->uid, e->id, e->result);
        break;
    }
    case KSU_EVENT_FEATURE_CHANGED: {
        const struct ksu_feature_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d feature=%u value=%llu ret=%d\n", event_names[event], n, e->feature_id, e->value,
                    e->result);
        break;
    }
    case KSU_EVENT_SUPERCALL_POST: {
        const struct ksu_supercall_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d cmd=0x%x ret=%d\n", event_names[event], n, e->command, e->result);
        break;
    }
    default: {
        const struct ksu_state_event *e = ctx;
        if (e->size >= sizeof(*e))
            pr_info(TEST_TAG "%s #%d event=%u result=%ld\n", event_names[event], n, e->event, result);
        break;
    }
    }
}

static int generic_pre(const void *ctx, void *data)
{
    /* Observer PRE callbacks are notification only; the return value must be ignored. */
    return 1;
}

/* Priority ordering inside one emission: PRE ascending, POST descending.
 * Only the first UID_COMMITTED emission is recorded. */
static int order_pre(const void *ctx, void *data)
{
    int slot = (int)(unsigned long)data;

    if (slot >= 0 && slot < 3 && !order_pre_seen[slot])
        order_pre_seen[slot] = atomic_inc_return(&order_pre_seq);
    return 0;
}

static void order_post(const void *ctx, long result, void *data)
{
    int slot = (int)(unsigned long)data;

    if (slot >= 0 && slot < 3 && !order_seen[slot])
        order_seen[slot] = atomic_inc_return(&order_seq);
}

static void self_post(const void *ctx, long result, void *data)
{
    /* Unregistering the running handler must be deferred and succeed. */
    self_unregister_ret = api.unregister_event_handler(self_cookie);
    /* Unregistering another handler from a callback must be refused. */
    cross_unregister_ret = api.unregister_event_handler(order_cookies[0]);
}

static int reboot_fn(__u32 magic1, __u32 magic2, unsigned long arg, void *data)
{
    int n = atomic_inc_return(&reboot_hits);

    pr_info(TEST_TAG "reboot handler #%d magic=0x%x/0x%x arg=0x%lx pid=%d uid=%u\n", n, magic1, magic2, arg,
            current->pid, current_uid().val);
    return 0;
}

static int register_generic(enum ksu_event event)
{
    struct ksu_event_handler_desc desc = {
        .event = event,
        .priority = 0,
        .flags = KSU_EVENT_HANDLER_REPLAY,
        .pre = generic_pre,
        .post = generic_post,
        .data = (void *)(unsigned long)event,
    };

    return api.register_event_handler(&desc, THIS_MODULE, &cookies[event]);
}

static void unregister_all(void)
{
    int i;

    for (i = 0; i < 3; i++) {
        if (order_cookies[i])
            api.unregister_event_handler(order_cookies[i]);
        order_cookies[i] = NULL;
    }
    for (i = 0; i < KSU_EVENT_MAX; i++) {
        if (cookies[i])
            api.unregister_event_handler(cookies[i]);
        cookies[i] = NULL;
    }
    if (reboot_cookie)
        api.unregister_reboot_handler(reboot_cookie);
    reboot_cookie = NULL;
}

static int __init ksu_api_test_init(void)
{
    struct ksu_event_handler_desc desc;
    struct ksu_reboot_handler_desc rdesc;
    void *dup_cookie = NULL;
    int ret, i;

    pr_info(TEST_TAG "init\n");

    ret = ksu_get_api(KSU_KERNEL_API_VERSION, &api, sizeof(api));
    pr_info(TEST_TAG "ksu_get_api(%u) = %d api_version=%u table_size=%u kernelsu_version=%u caps=0x%llx\n",
            KSU_KERNEL_API_VERSION, ret, api.api_version, api.table_size, api.kernelsu_version, api.capabilities);
    if (ret < 0)
        return ret;
    if (!(api.capabilities & KSU_API_CAP_EVENTS) || !(api.capabilities & KSU_API_CAP_REBOOT_HANDLERS)) {
        api.release();
        return -EOPNOTSUPP;
    }
    pr_info(TEST_TAG "runtime_flags=0x%llx manager_appid=%u allow_uid(0)=%d allow_uid(2000)=%d\n",
            api.get_runtime_flags(), api.get_manager_appid(), api.is_allow_uid(0), api.is_allow_uid(2000));

    /* Newer version must be rejected, older/equal accepted. */
    {
        struct ksu_api tmp;
        int r = ksu_get_api(KSU_KERNEL_API_VERSION + 1, &tmp, sizeof(tmp));
        pr_info(TEST_TAG "ksu_get_api(newer) = %d (expect -EOPNOTSUPP=%d)\n", r, -EOPNOTSUPP);
        if (r >= 0)
            tmp.release();
        r = ksu_get_api(KSU_KERNEL_API_VERSION, &tmp, 8);
        pr_info(TEST_TAG "ksu_get_api(tiny table) = %d (expect -EINVAL=%d)\n", r, -EINVAL);
        if (r >= 0)
            tmp.release();
    }

    /* 1. Generic observer on every event. */
    for (i = 0; i < KSU_EVENT_MAX; i++) {
        ret = register_generic(i);
        if (ret) {
            pr_err(TEST_TAG "register %s failed: %d\n", event_names[i], ret);
            goto fail;
        }
    }
    pr_info(
        TEST_TAG
        "registered %d observers, replay counts: CORE_READY=%d POST_FS_DATA=%d MODULE_MOUNTED=%d BOOT_COMPLETED=%d\n",
        KSU_EVENT_MAX, atomic_read(&counts[KSU_EVENT_CORE_READY]), atomic_read(&counts[KSU_EVENT_POST_FS_DATA]),
        atomic_read(&counts[KSU_EVENT_MODULE_MOUNTED]), atomic_read(&counts[KSU_EVENT_BOOT_COMPLETED]));

    /* 2. Duplicate (event, pre, post, data, owner) must be rejected. */
    desc = (struct ksu_event_handler_desc){
        .event = KSU_EVENT_CORE_READY,
        .flags = KSU_EVENT_HANDLER_REPLAY,
        .pre = generic_pre,
        .post = generic_post,
        .data = (void *)(unsigned long)KSU_EVENT_CORE_READY,
    };
    ret = api.register_event_handler(&desc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "duplicate register = %d (expect -EEXIST=%d)\n", ret, -EEXIST);
    if (!ret)
        api.unregister_event_handler(dup_cookie);

    /* 3. Invalid descriptors. */
    desc = (struct ksu_event_handler_desc){ .event = KSU_EVENT_MAX, .post = generic_post };
    ret = api.register_event_handler(&desc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "bad event register = %d (expect -EINVAL=%d)\n", ret, -EINVAL);
    desc = (struct ksu_event_handler_desc){ .event = KSU_EVENT_CORE_READY };
    ret = api.register_event_handler(&desc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "no callback register = %d (expect -EINVAL=%d)\n", ret, -EINVAL);
    desc = (struct ksu_event_handler_desc){ .event = KSU_EVENT_CORE_READY, .post = generic_post };
    ret = api.register_event_handler(&desc, NULL, &dup_cookie);
    pr_info(TEST_TAG "no owner register = %d (expect -EINVAL=%d)\n", ret, -EINVAL);
    desc = (struct ksu_event_handler_desc){ .event = KSU_EVENT_CORE_READY,
                                            .flags = KSU_EVENT_HANDLER_POLICY,
                                            .pre = generic_pre };
    ret = api.register_event_handler(&desc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "policy flag register = %d (expect -EINVAL=%d, no policy events in v1)\n", ret, -EINVAL);
    if (!ret)
        api.unregister_event_handler(dup_cookie);

    /* 4. Priority ordering: three handlers on UID_COMMITTED, slots 0/1/2 =
     * priority 10/-10/0.  Expected on the first emission: PRE 3,1,2 and
     * POST 1,3,2.  The result is printed on unload. */
    for (i = 0; i < 3; i++) {
        static const int prio[3] = { 10, -10, 0 };
        desc = (struct ksu_event_handler_desc){
            .event = KSU_EVENT_UID_COMMITTED,
            .priority = prio[i],
            .pre = order_pre,
            .post = order_post,
            .data = (void *)(unsigned long)i,
        };
        ret = api.register_event_handler(&desc, THIS_MODULE, &order_cookies[i]);
        if (ret) {
            pr_err(TEST_TAG "order register %d failed: %d\n", i, ret);
            goto fail;
        }
    }

    /* 5. Self unregister inside callback (deferred) and cross unregister (-EDEADLK). */
    desc = (struct ksu_event_handler_desc){
        .event = KSU_EVENT_CORE_READY,
        .flags = KSU_EVENT_HANDLER_REPLAY,
        .post = self_post,
    };
    ret = api.register_event_handler(&desc, THIS_MODULE, &self_cookie);
    pr_info(
        TEST_TAG
        "self register = %d, self unregister in callback = %d (expect 0), cross unregister = %d (expect -EDEADLK=%d)\n",
        ret, self_unregister_ret, cross_unregister_ret, -EDEADLK);
    if (!ret && self_unregister_ret != 0) {
        ret = api.unregister_event_handler(self_cookie);
        pr_info(TEST_TAG "self unregister fallback = %d\n", ret);
    }
    self_cookie = NULL;

    /* 6. Synchronous unregister (a cookie is single-use afterwards). */
    desc = (struct ksu_event_handler_desc){ .event = KSU_EVENT_CORE_READY, .post = order_post, .data = (void *)3UL };
    ret = api.register_event_handler(&desc, THIS_MODULE, &throwaway_cookie);
    pr_info(TEST_TAG "throwaway register = %d (expect 0)\n", ret);
    if (!ret) {
        ret = api.unregister_event_handler(throwaway_cookie);
        pr_info(TEST_TAG "sync unregister = %d (expect 0)\n", ret);
    }
    throwaway_cookie = NULL;
    ret = api.unregister_event_handler(NULL);
    pr_info(TEST_TAG "null unregister = %d (expect -EINVAL=%d)\n", ret, -EINVAL);

    /* 7. Reboot handlers: reserved magic, registration, duplicate. */
    rdesc = (struct ksu_reboot_handler_desc){ .magic1 = 0xDEADBEEF, .magic2 = 0xCAFEBABE, .fn = reboot_fn };
    ret = api.register_reboot_handler(&rdesc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "reserved reboot magic register = %d (expect -EEXIST=%d)\n", ret, -EEXIST);
    if (!ret)
        api.unregister_reboot_handler(dup_cookie);
    rdesc =
        (struct ksu_reboot_handler_desc){ .magic1 = TEST_REBOOT_MAGIC1, .magic2 = TEST_REBOOT_MAGIC2, .fn = reboot_fn };
    ret = api.register_reboot_handler(&rdesc, THIS_MODULE, &reboot_cookie);
    pr_info(TEST_TAG "reboot register = %d (expect 0)\n", ret);
    if (ret)
        goto fail;
    ret = api.register_reboot_handler(&rdesc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "duplicate reboot register = %d (expect -EEXIST=%d)\n", ret, -EEXIST);
    if (!ret)
        api.unregister_reboot_handler(dup_cookie);
    rdesc.fn = NULL;
    ret = api.register_reboot_handler(&rdesc, THIS_MODULE, &dup_cookie);
    pr_info(TEST_TAG "reboot register without fn = %d (expect -EINVAL=%d)\n", ret, -EINVAL);

    pr_info(TEST_TAG "init done; trigger reboot(0x%x, 0x%x, ...) from userspace to test the reboot registry\n",
            TEST_REBOOT_MAGIC1, TEST_REBOOT_MAGIC2);
    return 0;

fail:
    unregister_all();
    api.release();
    return ret;
}

static void __exit ksu_api_test_exit(void)
{
    int i;

    unregister_all();
    for (i = 0; i < KSU_EVENT_MAX; i++)
        pr_info(TEST_TAG "count %-18s = %d\n", event_names[i], atomic_read(&counts[i]));
    pr_info(TEST_TAG "count reboot handler   = %d\n", atomic_read(&reboot_hits));
    pr_info(
        TEST_TAG
        "order on first UID_COMMITTED: pre prio10=%d prio-10=%d prio0=%d (expect 3 1 2); post prio10=%d prio-10=%d prio0=%d (expect 1 3 2)\n",
        order_pre_seen[0], order_pre_seen[1], order_pre_seen[2], order_seen[0], order_seen[1], order_seen[2]);
    if (api.release)
        api.release();
    pr_info(TEST_TAG "exit\n");
}

module_init(ksu_api_test_init);
module_exit(ksu_api_test_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("KernelSU kernel plugin API smoke test");

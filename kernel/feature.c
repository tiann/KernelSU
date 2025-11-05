#include "feature.h"
#include "klog.h" // IWYU pragma: keep

#include <linux/mutex.h>

static const struct ksu_feature_handler *feature_handlers[KSU_FEATURE_MAX];

static DEFINE_MUTEX(feature_mutex);

int ksu_register_feature_handler(const struct ksu_feature_handler *handler)
{
    if (!handler) {
        pr_err("feature: register handler is NULL\n");
        return -EINVAL;
    }

    if (handler->feature_id >= KSU_FEATURE_MAX) {
        pr_err("feature: invalid feature_id %u\n", handler->feature_id);
        return -EINVAL;
    }

    if (!handler->get_handler && !handler->set_handler) {
        pr_err("feature: no handler provided for feature %u\n", handler->feature_id);
        return -EINVAL;
    }

    mutex_lock(&feature_mutex);

    if (feature_handlers[handler->feature_id]) {
        pr_warn("feature: handler for %u already registered, overwriting\n",
                handler->feature_id);
    }

    feature_handlers[handler->feature_id] = handler;

    pr_info("feature: registered handler for %s (id=%u)\n",
            handler->name ? handler->name : "unknown", handler->feature_id);

    mutex_unlock(&feature_mutex);
    return 0;
}

int ksu_unregister_feature_handler(u32 feature_id)
{
    int ret = 0;

    if (feature_id >= KSU_FEATURE_MAX) {
        pr_err("feature: invalid feature_id %u\n", feature_id);
        return -EINVAL;
    }

    mutex_lock(&feature_mutex);

    if (!feature_handlers[feature_id]) {
        pr_warn("feature: no handler registered for %u\n", feature_id);
        ret = -ENOENT;
        goto out;
    }

    feature_handlers[feature_id] = NULL;

    pr_info("feature: unregistered handler for id=%u\n", feature_id);

out:
    mutex_unlock(&feature_mutex);
    return ret;
}

int ksu_get_feature(u32 feature_id, u64 *value, bool *supported)
{
    int ret = 0;
    const struct ksu_feature_handler *handler;

    if (feature_id >= KSU_FEATURE_MAX) {
        pr_err("feature: invalid feature_id %u\n", feature_id);
        return -EINVAL;
    }

    if (!value || !supported) {
        pr_err("feature: invalid parameters\n");
        return -EINVAL;
    }

    mutex_lock(&feature_mutex);

    handler = feature_handlers[feature_id];

    if (!handler) {
        *supported = false;
        *value = 0;
        pr_debug("feature: feature %u not supported\n", feature_id);
        goto out;
    }

    *supported = true;

    if (!handler->get_handler) {
        pr_warn("feature: no get_handler for feature %u\n", feature_id);
        ret = -EOPNOTSUPP;
        goto out;
    }

    ret = handler->get_handler(value);
    if (ret) {
        pr_err("feature: get_handler for %u failed: %d\n", feature_id, ret);
    }

out:
    mutex_unlock(&feature_mutex);
    return ret;
}

int ksu_set_feature(u32 feature_id, u64 value)
{
    int ret = 0;
    const struct ksu_feature_handler *handler;

    if (feature_id >= KSU_FEATURE_MAX) {
        pr_err("feature: invalid feature_id %u\n", feature_id);
        return -EINVAL;
    }

    mutex_lock(&feature_mutex);

    handler = feature_handlers[feature_id];

    if (!handler) {
        pr_err("feature: feature %u not registered\n", feature_id);
        ret = -EOPNOTSUPP;
        goto out;
    }

    if (!handler->set_handler) {
        pr_warn("feature: no set_handler for feature %u\n", feature_id);
        ret = -EOPNOTSUPP;
        goto out;
    }

    ret = handler->set_handler(value);
    if (ret) {
        pr_err("feature: set_handler for %u failed: %d\n", feature_id, ret);
    }

out:
    mutex_unlock(&feature_mutex);
    return ret;
}

void ksu_feature_init(void)
{
    int i;

    for (i = 0; i < KSU_FEATURE_MAX; i++) {
        feature_handlers[i] = NULL;
    }

    pr_info("feature: feature management initialized\n");
}

void ksu_feature_exit(void)
{
    int i;

    mutex_lock(&feature_mutex);

    for (i = 0; i < KSU_FEATURE_MAX; i++) {
        feature_handlers[i] = NULL;
    }

    mutex_unlock(&feature_mutex);

    pr_info("feature: feature management cleaned up\n");
}

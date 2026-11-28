#include <linux/types.h>

#include "feature/fastbootd_rescue.h"
#include "klog.h" // IWYU pragma: keep
#include "policy/feature.h"

static bool ksu_fastbootd_rescue_enabled = true;

static int fastbootd_rescue_feature_get(u64 *value)
{
    *value = ksu_fastbootd_rescue_enabled ? 1 : 0;
    return 0;
}

static int fastbootd_rescue_feature_set(u64 value)
{
    bool enable = value != 0;

    ksu_fastbootd_rescue_enabled = enable;
    pr_info("fastbootd_rescue: set to %d\n", enable);
    return 0;
}

static const struct ksu_feature_handler fastbootd_rescue_handler = {
    .feature_id = KSU_FEATURE_FASTBOOTD_RESCUE,
    .name = "fastbootd_rescue",
    .get_handler = fastbootd_rescue_feature_get,
    .set_handler = fastbootd_rescue_feature_set,
};

void __init ksu_fastbootd_rescue_init(void)
{
    ksu_fastbootd_rescue_enabled = true;

    if (ksu_register_feature_handler(&fastbootd_rescue_handler)) {
        pr_err("Failed to register fastbootd_rescue feature handler\n");
    }
}

void __exit ksu_fastbootd_rescue_exit(void)
{
    ksu_unregister_feature_handler(KSU_FEATURE_FASTBOOTD_RESCUE);
}

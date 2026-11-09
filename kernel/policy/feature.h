#ifndef __KSU_H_FEATURE
#define __KSU_H_FEATURE

#include <linux/types.h>
#include "uapi/feature.h" // IWYU pragma: keep

typedef int (*ksu_feature_get_t)(u64 *value);
typedef int (*ksu_feature_set_t)(u64 value);

struct ksu_feature_handler {
    u32 feature_id;
    const char *name;
    ksu_feature_get_t get_handler;
    ksu_feature_set_t set_handler;
};

int ksu_register_feature_handler(const struct ksu_feature_handler *handler);

int ksu_unregister_feature_handler(u32 feature_id);

int ksu_get_feature(u32 feature_id, u64 *value, bool *supported);

int ksu_set_feature(u32 feature_id, u64 value);

// Whether the feature is forced by the force_feature module param, in which
// case ksu_set_feature() refuses to change it.
bool ksu_feature_is_forced(u32 feature_id);

// Apply forced feature values not applied yet. Called at several boot stages
// since some handlers can only succeed late (e.g. selinux_hide needs the
// sepolicy backup).
void ksu_feature_apply_forced(void);

void ksu_feature_init(void);

void ksu_feature_exit(void);

#endif // __KSU_H_FEATURE

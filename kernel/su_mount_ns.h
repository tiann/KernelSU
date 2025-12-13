#ifndef __KSU_SU_MOUNT_NS_H
#define __KSU_SU_MOUNT_NS_H

struct ksu_mns_tw {
    struct callback_head cb;
    int32_t ns_mode;
};

void ksu_setup_mount_namespace_tw_func(struct callback_head *cb);

#endif

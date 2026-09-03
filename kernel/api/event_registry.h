#ifndef __KSU_EVENT_REGISTRY_H
#define __KSU_EVENT_REGISTRY_H

#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>

/* Stable kernel-plugin events. Keep values append-only. */
enum ksu_event {
    KSU_EVENT_CORE_READY = 0,
    KSU_EVENT_POST_FS_DATA,
    KSU_EVENT_MODULE_MOUNTED,
    KSU_EVENT_BOOT_COMPLETED,
    KSU_EVENT_CORE_EXITING,
    KSU_EVENT_UID_COMMITTED,
    KSU_EVENT_EXEC_POST,
    KSU_EVENT_ROOT_GRANTED,
    KSU_EVENT_MANAGER_READY,
    KSU_EVENT_KSU_UMOUNT_PRE,
    KSU_EVENT_KSU_UMOUNT_ITEM,
    KSU_EVENT_KSU_UMOUNT_POST,
    KSU_EVENT_ALLOWLIST_CHANGED,
    KSU_EVENT_PROFILE_CHANGED,
    KSU_EVENT_FEATURE_CHANGED,
    KSU_EVENT_SELINUX_READY,
    KSU_EVENT_SUPERCALL_POST,
    KSU_EVENT_MAX,
};

#define KSU_EVENT_HANDLER_POLICY (1U << 0)
#define KSU_EVENT_HANDLER_REPLAY (1U << 1)

struct ksu_event_handler_desc {
    __u32 event;
    __s32 priority;
    __u32 flags;
    int (*pre)(const void *ctx, void *data);
    void (*post)(const void *ctx, long result, void *data);
    void *data;
};

struct ksu_uid_event {
    __u32 size;
    __u32 version;
    __u32 syscall_nr;
    __u32 old_uid;
    __u32 new_uid;
    __u32 pid;
    __u32 tgid;
    __u8 old_allowlisted;
    __u8 new_allowlisted;
    __u8 is_zygote_child;
    __u8 reserved;
};

struct ksu_state_event {
    __u32 size;
    __u32 version;
    __u32 event;
    __u32 reserved;
};

struct ksu_feature_event {
    __u32 size;
    __u32 version;
    __u32 feature_id;
    __u32 reserved;
    __u64 value;
    __s32 result;
    __u32 reserved2;
};

struct ksu_root_event {
    __u32 size;
    __u32 version;
    __u32 uid;
    __u32 pid;
    __u32 tgid;
    __s32 result;
};

struct ksu_policy_event {
    __u32 size;
    __u32 version;
    __u32 uid;
    __u32 id;
    __s32 result;
    __u32 reserved;
};

struct ksu_supercall_event {
    __u32 size;
    __u32 version;
    __u32 command;
    __s32 result;
};

struct ksu_exec_event {
    __u32 size;
    __u32 version;
    __u32 syscall_nr;
    __s32 result;
    __u32 pid;
    __u32 tgid;
    char path[128];
};

struct ksu_umount_event {
    __u32 size;
    __u32 version;
    __u32 reason;
    __u32 old_uid;
    __u32 new_uid;
    __u32 pid;
    __u32 tgid;
    __u32 flags;
    __s32 result;
    const char *target;
};

struct ksu_reboot_handler_desc {
    __u32 magic1;
    __u32 magic2;
    __u32 flags;
    int (*fn)(__u32 magic1, __u32 magic2, unsigned long arg, void *data);
    void *data;
};

int ksu_event_registry_init(void);
void ksu_event_registry_exit(void);
int ksu_register_event_handler(const struct ksu_event_handler_desc *desc, struct module *owner, void **cookie);
int ksu_unregister_event_handler(void *cookie);
int ksu_event_emit(enum ksu_event event, const void *ctx, long result);
/* Cheap check so hot paths can skip building an event context. */
bool ksu_event_has_handlers(enum ksu_event event);
void ksu_event_set_state(enum ksu_event event);
bool ksu_event_is_stopping(void);

int ksu_register_reboot_handler(const struct ksu_reboot_handler_desc *desc, struct module *owner, void **cookie);
int ksu_unregister_reboot_handler(void *cookie);
int ksu_reboot_dispatch(__u32 magic1, __u32 magic2, unsigned long arg);

#endif

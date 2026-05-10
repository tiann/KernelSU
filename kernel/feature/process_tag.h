#ifndef __KSU_PROCESS_TAG_H
#define __KSU_PROCESS_TAG_H

#include "linux/sched.h"
#include <linux/atomic.h>
#include <linux/rcupdate.h>
#include <linux/types.h>

#include "uapi/process_tag.h"

enum process_tag_type {
    PROCESS_TAG_NONE = KSU_PROCESS_TAG_NONE,
    PROCESS_TAG_KSUD = KSU_PROCESS_TAG_KSUD,
    PROCESS_TAG_APP = KSU_PROCESS_TAG_APP,
    PROCESS_TAG_MODULE = KSU_PROCESS_TAG_MODULE,
    PROCESS_TAG_MANAGER = KSU_PROCESS_TAG_MANAGER,
};

struct process_tag {
    enum process_tag_type type;
    char name[64];
    atomic_t refcount;
    struct rcu_head rcu;
};

int ksu_process_tag_set(struct task_struct *task, enum process_tag_type type, const char *name);

struct process_tag *ksu_process_tag_get(struct task_struct *task);

void ksu_process_tag_put(struct process_tag *tag);

void ksu_process_tag_delete(struct task_struct *task);

void ksu_process_tag_flush(void);

bool ksu_process_tag_is_enabled(void);

void ksu_process_tag_init(void);
void ksu_process_tag_exit(void);

#endif

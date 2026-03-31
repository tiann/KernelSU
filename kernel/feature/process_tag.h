#ifndef __KSU_PROCESS_TAG_H
#define __KSU_PROCESS_TAG_H

#include <linux/atomic.h>
#include <linux/rcupdate.h>
#include <linux/types.h>

enum process_tag_type {
    PROCESS_TAG_KSUD = 0,
    PROCESS_TAG_APP = 1,
    PROCESS_TAG_MODULE = 2,
    PROCESS_TAG_MANAGER = 3,
    PROCESS_TAG_NONE = 255,
};

struct process_tag {
    enum process_tag_type type;
    char name[64];
    atomic_t refcount;
    struct rcu_head rcu;
};

int ksu_process_tag_set(pid_t pid, enum process_tag_type type, const char *name);

struct process_tag *ksu_process_tag_get(pid_t pid);

void ksu_process_tag_put(struct process_tag *tag);

void ksu_process_tag_delete(pid_t pid);

void ksu_process_tag_flush(void);

bool ksu_process_tag_is_enabled(void);

void ksu_process_tag_init(void);
void ksu_process_tag_exit(void);

#endif

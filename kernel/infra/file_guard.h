#ifndef __KSU_H_FILE_GUARD
#define __KSU_H_FILE_GUARD
#include "linux/fs.h"

struct ksu_file {
    struct list_head list;
    struct file *f;
    void (*cleanup)(struct file *f);
};

int ksu_file_guard_init();
void ksu_file_guard_exit();

int ksu_file_add(struct file *f, struct ksu_file *kf);
void ksu_file_release(struct ksu_file *f);
#endif

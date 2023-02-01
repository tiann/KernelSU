#ifndef __KSU_H_KERNEL_COMPAT
#define __KSU_H_KERNEL_COMPAT

#include "linux/fs.h"
#include "linux/version.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
#define FILP_OPEN_WORKS_IN_WORKER
#elif defined(MODULE)
#warning building as module may not work due to Kprobes bugs on old kernels
#endif

extern ssize_t kernel_read_compat(struct file *p, void* buf, size_t count, loff_t *pos);
extern ssize_t kernel_write_compat(struct file *p, const void *buf, size_t count, loff_t *pos);
#endif

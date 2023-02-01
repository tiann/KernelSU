#include "linux/fs.h"
extern ssize_t kernel_read_compat(struct file *p, void* buf, size_t count, loff_t *pos);
extern ssize_t kernel_write_compat(struct file *p, const void *buf, size_t count, loff_t *pos);
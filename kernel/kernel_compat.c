#include "linux/version.h"
#include "linux/init.h"
#include "linux/fs.h"
ssize_t kernel_read_compat(struct file *p, void *buf, size_t count, loff_t *pos){
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    return kernel_read(p, buf, count, pos);
#else
    loff_t offset = pos ? *pos : 0;
    ssize_t result = kernel_read(p, offset, (char *)buf, count);
    if (pos && result > 0)
    {
        *pos = offset + result;
    }
    return result;
#endif
}

ssize_t kernel_write_compat(struct file *p, const void *buf, size_t count, loff_t *pos){
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
    return kernel_write(p, buf, count, pos);
#else
    loff_t offset = pos ? *pos : 0;
    ssize_t result = kernel_write(p, buf, count, offset);
    if (pos && result > 0)
    {
        *pos = offset + result;
    }
    return result;
#endif
}
#ifndef __KSU_H_KSU
#define __KSU_H_KSU

#include <linux/types.h>
#include <linux/workqueue.h>

#define KERNEL_SU_VERSION KSU_VERSION

#define EVENT_POST_FS_DATA 1
#define EVENT_BOOT_COMPLETED 2
#define EVENT_MODULE_MOUNTED 3

static inline int startswith(char *s, char *prefix)
{
    return strncmp(s, prefix, strlen(prefix));
}

static inline int endswith(const char *s, const char *t)
{
    size_t slen = strlen(s);
    size_t tlen = strlen(t);
    if (tlen > slen)
        return 1;
    return strcmp(s + slen - tlen, t);
}

#endif

#ifndef __LIBSEPOL_H_KERNEL
#define __LIBSEPOL_H_KERNEL

#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/socket.h>

#define malloc(size) kmalloc(size, GFP_KERNEL)
#define calloc(nitems, size) kzalloc(nitems * size, GFP_KERNEL)
#define realloc(ptr, size) krealloc(ptr, size, GFP_KERNEL)
#define free(p) kfree(p)

#define printf(...) printk(__VA_ARGS__)

#define assert(...)

#define PRIu32 "d"
#define PRIx64 "x"

#define UINT16_MAX USHRT_MAX
#define UINT8_MAX 0xff

#define strdup(x) kstrdup(x, GFP_KERNEL)
#define strndup(x, y) kstrndup(x, y, GFP_KERNEL)

// http://aospxref.com/android-8.1.0_r81/xref/bionic/libc/include/sys/types.h?fi=socklen_t#109
typedef uint32_t socklen_t;

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

int inet_pton(int af, const char *src, void *dst);

#endif
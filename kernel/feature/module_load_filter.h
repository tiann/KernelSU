#ifndef __KSU_H_MODULE_LOAD_FILTER
#define __KSU_H_MODULE_LOAD_FILTER

#include <linux/types.h>

// 256 should enough
extern char ksu_block_modules[256];

// return is errno, use if to check
// when the if statement is false, return ret to the userspace.
int ksu_handle_init_module(const void __user *umod, unsigned long umod_len);
int ksu_handle_finit_module(int fd, int flags);

void ksu_module_load_filter_hook_init(void);
void ksu_module_load_filter_hook_exit(void);

#endif
#ifndef __KSU_H_KSU_CORE
#define __KSU_H_KSU_CORE

#include <linux/init.h>
#include <linux/types.h>

void ksu_setuid_hook_init(void);
void ksu_setuid_hook_exit(void);

#endif

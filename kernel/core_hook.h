#ifndef __KSU_H_KSU_CORE
#define __KSU_H_KSU_CORE

#include <linux/init.h>

void __init ksu_core_init(void);
void ksu_core_exit(void);

void escape_to_root(void);

#endif

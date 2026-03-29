#ifndef __KSU_H_SULOG
#define __KSU_H_SULOG

#include <linux/types.h>

bool ksu_sulog_is_enabled(void);
void ksu_sulog_init(void);
void ksu_sulog_exit(void);

#endif

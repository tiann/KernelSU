#ifndef __KSU_H_KSUD
#define __KSU_H_KSUD

#include <linux/types.h>

#define KSUD_PATH "/data/adb/ksud"

void on_post_fs_data(void);

bool ksu_is_safe_mode(void);

extern u32 ksu_devpts_sid;

extern bool ksu_execveat_hook __read_mostly;
extern int ksu_handle_pre_ksud(const char *filename);

#endif

#ifndef __KSU_H_KSUD
#define __KSU_H_KSUD

#include <linux/types.h>

#define KSUD_PATH "/data/adb/ksud"

void ksu_ksud_init();
void ksu_ksud_exit();

void on_post_fs_data(void);
void on_module_mounted(void);
void on_boot_completed(void);

bool ksu_is_safe_mode(void);

int nuke_ext4_sysfs(const char* mnt);

extern u32 ksu_file_sid;
extern bool ksu_module_mounted;
extern bool ksu_boot_completed;

#endif

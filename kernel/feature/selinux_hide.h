#ifndef __KSU_H_SELINUX_HIDE
#define __KSU_H_SELINUX_HIDE

void ksu_selinux_hide_init();
void ksu_selinux_hide_exit();
void ksu_selinux_hide_drop_backup_if_unused();
void ksu_selinux_hide_handle_second_stage();
void ksu_selinux_hide_handle_post_fs_data();

#endif

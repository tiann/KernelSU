#ifndef __KSU_H_KERNEL_UMOUNT
#define __KSU_H_KERNEL_UMOUNT

#include <linux/types.h>

void ksu_kernel_umount_init(void);
void ksu_kernel_umount_exit(void);

// Handler function to be called from setresuid hook
int ksu_handle_umount(uid_t old_uid, uid_t new_uid);

#endif

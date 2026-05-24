#ifndef KSU_FILE_WRAPPER_H
#define KSU_FILE_WRAPPER_H

#include <linux/file.h>
#include <linux/fs.h>

int ksu_install_file_wrapper(int fd);
void ksu_file_wrapper_init(void);

int is_wrapper_fd(int fd);

#endif // KSU_FILE_WRAPPER_H

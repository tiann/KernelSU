#ifndef KSU_FILE_WRAPPER_H
#define KSU_FILE_WRAPPER_H

#include <linux/file.h>
#include <linux/fs.h>

int install_file_wrapper(int fd);

#endif // KSU_FILE_WRAPPER_H

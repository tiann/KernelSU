#ifndef __KSU_H_SUCOMPAT
#define __KSU_H_SUCOMPAT
#include <asm/ptrace.h>
#include <linux/types.h>

extern struct static_key_true ksu_su_compat_enabled;

void ksu_sucompat_init(void);
void ksu_sucompat_exit(void);

int ksu_handle_faccessat(int *dfd, struct filename **filename, int *mode, int *__unused_flags);
int ksu_handle_stat(int *dfd, struct filename **filename, int *flags);
int ksu_handle_execveat_sucompat(int *fd, struct filename **filename_ptr,
                 void *argv_user, void *envp_user,
                 int *__never_use_flags);

#endif
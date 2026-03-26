#ifndef __KSU_H_KSUD
#define __KSU_H_KSUD

#include <asm/syscall.h>

#define KSUD_PATH "/data/adb/ksud"

void ksu_ksud_init();
void ksu_ksud_exit();

void ksu_execve_hook_ksud(const struct pt_regs *regs);
void ksu_stop_input_hook_runtime(void);

#endif

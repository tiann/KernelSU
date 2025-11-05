#ifndef __KSU_H_SELINUX
#define __KSU_H_SELINUX

#include "linux/types.h"
#include "linux/version.h"
#include "linux/sched.h"

void setup_selinux(const char *);

void setenforce(bool);

bool getenforce();

bool is_task_ksu_domain(void *sec);

bool is_ksu_domain();

bool is_zygote(void *sec);

void apply_kernelsu_rules();

u32 ksu_get_devpts_sid();

#endif

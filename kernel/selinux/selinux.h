#ifndef __KSU_H_SELINUX
#define __KSU_H_SELINUX

#include "linux/types.h"

void setup_selinux(void);

void setenforce(bool);

bool getenforce(void);

bool is_ksu_domain(void);

void apply_kernelsu_rules(void);

#endif
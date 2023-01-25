#ifndef __KSU_H_SELINUX
#define __KSU_H_SELINUX

#include "linux/types.h"

void setup_selinux();

void setenforce(bool);

bool getenforce();

bool is_ksu_domain();

void apply_kernelsu_rules();

#endif
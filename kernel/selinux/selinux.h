#ifndef __KSU_H_SELINUX
#define __KSU_H_SELINUX

#include "linux/types.h"

#define KERNEL_SU_DOMAIN	"u:r:su:s0"
#define INIT_DOMAIN		"u:r:init:s0"

void setup_selinux();

void setenforce(bool);

bool getenforce();

bool is_ksu_domain();

void apply_kernelsu_rules();

#endif

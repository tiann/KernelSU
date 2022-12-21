#ifndef __KSU_H_SELINUX
#define __KSU_H_SELINUX

void setup_selinux();

void setenforce(bool);

bool getenforce();

bool is_ksu_domain();

#endif
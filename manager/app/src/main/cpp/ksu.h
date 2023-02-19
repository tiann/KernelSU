//
// Created by weishu on 2022/12/9.
//

#ifndef KERNELSU_KSU_H
#define KERNELSU_KSU_H

bool become_manager(const char*);

int get_version();

bool allow_su(int uid, bool allow);

bool get_allow_list(int *uids, int *size);

bool get_deny_list(int *uids, int *size);

bool is_safe_mode();

#endif //KERNELSU_KSU_H

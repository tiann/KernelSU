#ifndef __KSU_H_KSU
#define __KSU_H_KSU

#include "linux/workqueue.h"

#ifndef KSU_GIT_VERSION
#warning "KSU_GIT_VERSION not defined! It is better to make KernelSU a git submodule!"
#define KERNEL_SU_VERSION (16)
#else
#define KERNEL_SU_VERSION (10000 + KSU_GIT_VERSION + 200) // major * 10000 + git version + 200 for historical reasons
#endif

#define KERNEL_SU_OPTION 0xDEADBEEF

#define CMD_GRANT_ROOT 0
#define CMD_BECOME_MANAGER 1
#define CMD_GET_VERSION 2
#define CMD_ALLOW_SU 3
#define CMD_DENY_SU 4
#define CMD_GET_ALLOW_LIST 5
#define CMD_GET_DENY_LIST 6
#define CMD_REPORT_EVENT 7
#define CMD_SET_SEPOLICY 8

#define EVENT_POST_FS_DATA 1
#define EVENT_BOOT_COMPLETED 2

bool ksu_queue_work(struct work_struct *work);

static inline int startswith(char *s, char *prefix)
{
	return strncmp(s, prefix, strlen(prefix));
}

static inline int endswith(const char *s, const char *t)
{
	size_t slen = strlen(s);
	size_t tlen = strlen(t);
	if (tlen > slen)
		return 1;
	return strcmp(s + slen - tlen, t);
}

#endif

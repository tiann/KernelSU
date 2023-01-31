#ifndef __KSU_H_KSU
#define __KSU_H_KSU

#include "linux/workqueue.h"

#define KERNEL_SU_VERSION 13

#define KERNEL_SU_OPTION 0xDEADBEEF


#define CMD_GRANT_ROOT 100
#define CMD_GET_VERSION 101
#define CMD_REPORT_EVENT 102

// require manager
#define CMD_BECOME_MANAGER 200
#define CMD_SET_UID_DATA 201
#define CMD_GET_UID_DATA 202
#define CMD_COUNT_UID_DATA 203
#define CMD_LIST_UID_DATA 204

// evnets triggered by init
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

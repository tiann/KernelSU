#ifndef __KSU_H_KSU
#define __KSU_H_KSU

#include "linux/workqueue.h"

#define KERNEL_SU_VERSION 13

#define KERNEL_SU_OPTION 0xDEADBEEF

// KERNEL_SU_OPTION is ignored in comment, but it's needed

// prctl(CMD_GRANT_ROOT)
#define CMD_GRANT_ROOT 100
// prctl(CMD_GET_VERSION, buf)
#define CMD_GET_VERSION 101
// prctl(CMD_REPORT_EVENT, buf)
#define CMD_REPORT_EVENT 102

// require manager
#define CMD_BECOME_MANAGER 200
// prctl(CMD_SET_UID_DATA, uid, data)
#define CMD_SET_UID_DATA 201
// prctl(CMD_GET_UID_DATA, uid, data)
#define CMD_GET_UID_DATA 202
// prctl(CMD_COUNT_UID_DATA, buf)
#define CMD_COUNT_UID_DATA 203
// prctl(CMD_LIST_UID_DATA, buf, buf_size, out_count)
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

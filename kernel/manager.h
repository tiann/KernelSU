#ifndef __KSU_H_KSU_MANAGER
#define __KSU_H_KSU_MANAGER

#include "linux/cred.h"
#include "linux/types.h"

#define INVALID_UID -1

extern uid_t ksu_manager_uid; // DO NOT DIRECT USE

static inline bool ksu_is_manager_uid_valid()
{
	return ksu_manager_uid != INVALID_UID;
}

static inline bool is_manager()
{
	return ksu_manager_uid == current_uid().val;
}

static inline uid_t ksu_get_manager_uid()
{
	return ksu_manager_uid;
}

static inline void ksu_set_manager_uid(uid_t uid)
{
	ksu_manager_uid = uid;
}

static inline void ksu_invalidate_manager_uid()
{
	ksu_manager_uid = INVALID_UID;
}

bool become_manager(char *pkg);

#endif

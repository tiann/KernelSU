#ifndef __KSU_H_KSU_MANAGER
#define __KSU_H_KSU_MANAGER

#include <linux/cred.h>
#include <linux/types.h>

#define KSU_INVALID_PID -1
#define KSU_INVALID_UID -1

// Daemon (ksud) - identified by PID + token
extern pid_t ksu_daemon_pid;
extern char ksu_daemon_token[65];

static inline bool ksu_is_daemon_pid_valid(void)
{
	return ksu_daemon_pid != KSU_INVALID_PID;
}

static inline bool is_daemon(void)
{
	return unlikely(ksu_daemon_pid == current->pid);
}

static inline pid_t ksu_get_daemon_pid(void)
{
	return ksu_daemon_pid;
}

static inline void ksu_set_daemon_pid(pid_t pid)
{
	ksu_daemon_pid = pid;
}

static inline void ksu_invalidate_daemon_pid(void)
{
	ksu_daemon_pid = KSU_INVALID_PID;
}

void ksu_generate_daemon_token(void);
const char* ksu_get_daemon_token(void);
bool ksu_verify_daemon_token(const char *token);

// Manager (app) - identified by UID
extern uid_t ksu_manager_uid;

static inline bool ksu_is_manager_uid_valid(void)
{
	return ksu_manager_uid != KSU_INVALID_UID;
}

static inline bool is_manager(void)
{
	return unlikely(ksu_manager_uid == current_uid().val);
}

static inline uid_t ksu_get_manager_uid(void)
{
	return ksu_manager_uid;
}

static inline void ksu_set_manager_uid(uid_t uid)
{
	ksu_manager_uid = uid;
}

static inline void ksu_invalidate_manager_uid(void)
{
	ksu_manager_uid = KSU_INVALID_UID;
}

#endif

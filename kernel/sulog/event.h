#ifndef __KSU_H_SULOG_EVENT
#define __KSU_H_SULOG_EVENT

#include <linux/compiler_types.h>
#include <linux/gfp.h>
#include <linux/types.h>
#include "uapi/sulog.h" // IWYU pragma: keep
#include "runtime/ksud.h"

struct ksu_event_queue;
struct ksu_sulog_pending_event;

int ksu_sulog_events_init(void);
void ksu_sulog_events_exit(void);

struct ksu_sulog_pending_event *ksu_sulog_capture_sucompat(const char *filename,
                                                           struct user_arg_ptr *argv_user, gfp_t gfp);
void ksu_sulog_emit_pending(struct ksu_sulog_pending_event *pending, int retval, gfp_t gfp);
int ksu_sulog_emit_grant_root(int retval, __u32 uid, __u32 euid, gfp_t gfp);

struct ksu_event_queue *ksu_sulog_get_queue(void);

#endif

#ifndef __KSU_JAILBREAK_H
#define __KSU_JAILBREAK_H

#include <linux/cred.h>
#include <linux/jump_label.h>

DECLARE_STATIC_KEY_FALSE(ksu_kdp_key);
DECLARE_STATIC_KEY_FALSE(ksu_rkp_key);
DECLARE_STATIC_KEY_FALSE(ksu_defex_key);

int  init_jailbreak(void);
void exit_jailbreak(void);

void ksu_put_cred(const struct cred *cred);
int  ksu_commit_creds(struct cred *cred);
void ksu_defex_sync_current(void);

#if defined(CONFIG_KRETPROBES) && defined(__aarch64__)
int  __init ksu_rkp_hooks_init(void);
void __exit ksu_rkp_hooks_exit(void);

struct file;
struct inode;

typedef ssize_t (*sel_hide_write_fn)(struct file *, char *, size_t);
typedef int     (*sel_hide_open_fn)(struct inode *, struct file *);
typedef int     (*sel_hide_setprocattr_fn)(const char *, void *, size_t);

int  selinux_hide_rkp_init(sel_hide_write_fn orig_ctx, sel_hide_write_fn my_ctx,
                            sel_hide_write_fn orig_acc, sel_hide_write_fn my_acc,
                            sel_hide_setprocattr_fn orig_spa, sel_hide_setprocattr_fn my_spa);
void selinux_hide_rkp_exit(void);
int  selinux_hide_rkp_hook_status_open(sel_hide_open_fn orig, sel_hide_open_fn my);
void selinux_hide_rkp_unhook_status_open(void);
#endif

#endif

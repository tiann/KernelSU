#ifndef __KSU_JAILBREAK_H
#define __KSU_JAILBREAK_H

#include <linux/cred.h>
#include <linux/jump_label.h>

DECLARE_STATIC_KEY_FALSE(ksu_kdp_key);
DECLARE_STATIC_KEY_FALSE(ksu_rkp_key);

int init_jailbreak(void);
void exit_jailbreak(void);

int ksu_rkp_branch_patch(void *dst, void *src);
void *ksu_rkp_get_stub(void *fn);
#ifdef __aarch64__
void ksu_rkp_branch_exit(void);
#endif

void ksu_put_cred(const struct cred *cred);
int ksu_commit_creds(struct cred *cred);

#endif

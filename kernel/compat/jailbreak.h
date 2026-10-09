#ifndef __KSU_JAILBREAK_H
#define __KSU_JAILBREAK_H

#include <linux/cred.h>
#include <linux/jump_label.h>

DECLARE_STATIC_KEY_FALSE(ksu_kdp_key);
DECLARE_STATIC_KEY_FALSE(ksu_defex_key);

int  init_jailbreak(void);
void exit_jailbreak(void);

void ksu_put_cred(const struct cred *cred);
int  ksu_commit_creds(struct cred *cred);
void __nocfi ksu_defex_sync_current(void);

#endif

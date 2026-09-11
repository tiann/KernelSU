#ifndef __KSU_SAMSUNG_KDP_H
#define __KSU_SAMSUNG_KDP_H

#include <linux/cred.h>

int ksu_samsung_kdp_init(void);
void ksu_samsung_kdp_exit(void);
int ksu_samsung_kdp_commit_creds(struct cred *cred);

#ifdef CONFIG_KSU_SAMSUNG_KDP
void ksu_samsung_kdp_put_cred(const struct cred *cred);

static inline void ksu_put_cred(const struct cred *cred)
{
    ksu_samsung_kdp_put_cred(cred);
}
#else
static inline void ksu_put_cred(const struct cred *cred)
{
    put_cred(cred);
}
#endif

#endif

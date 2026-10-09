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
void __nocfi ksu_defex_sync_current(void);

void rkp_jb_init(void);
void rkp_jb_exit(void);

// Replace the function whose address is stored in *slot with @replacement.
// On RKP kernels this patches the original function's entry instruction to
// branch to @replacement; elsewhere it writes the slot through the normal
// table writer. The slot must hold a kernel function pointer.
int ksu_compat_patch_slot(void **slot, void *replacement);

// Return the callable address for the original function of an installed
// branch site: the original itself, or its call-through stub under RKP.
void *ksu_compat_callthrough(void *original);

// Engine internals, shared with the compatibility wrapper.
int ksu_rkp_patch_slot(void **slot, void *replacement);
int ksu_rkp_activate_slot(void **slot, void *replacement);
int ksu_rkp_restore_slot(void *original);
void *ksu_rkp_get_stub(void *fn);

#endif

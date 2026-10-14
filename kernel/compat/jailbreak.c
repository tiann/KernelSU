#include "compat/jailbreak.h"
#include "hook/patch_memory.h"

void kdp_jb_init(void);
void kdp_jb_exit(void);
void defex_jb_init(void);
void defex_jb_exit(void);

int init_jailbreak(void)
{
    kdp_jb_init();
    rkp_jb_init();
    defex_jb_init();
    return 0;
}

void exit_jailbreak(void)
{
    kdp_jb_exit();
    rkp_jb_exit();
    defex_jb_exit();
}

int ksu_compat_patch_slot(void **slot, void *replacement, void **callthrough)
{
    if (static_branch_unlikely(&ksu_rkp_key)) {
        int ret = ksu_rkp_patch_slot(slot, replacement);
        if (ret)
            return ret;
        if (callthrough)
            WRITE_ONCE(*callthrough, ksu_rkp_get_stub(READ_ONCE(*slot)));
        return ksu_rkp_activate_slot(slot, replacement);
    }

    if (callthrough)
        WRITE_ONCE(*callthrough, READ_ONCE(*slot));
    {
        void *fn = replacement;
        return ksu_patch_text(slot, &fn, sizeof(fn), KSU_PATCH_TEXT_FLUSH_DCACHE);
    }
}

void *ksu_compat_callthrough(void *original)
{
    if (static_branch_unlikely(&ksu_rkp_key))
        return ksu_rkp_get_stub(original);
    return original;
}

// Restore a slot patched through ksu_compat_patch_slot to @original. Under
// RKP this removes the entry branch; elsewhere it writes the slot back.
int ksu_compat_restore_slot(void **slot, void *original)
{
    if (static_branch_unlikely(&ksu_rkp_key))
        return ksu_rkp_restore_slot(*slot);
    {
        void *fn = original;
        return ksu_patch_text(slot, &fn, sizeof(fn), KSU_PATCH_TEXT_FLUSH_DCACHE);
    }
}

// Undo a direct branch site on @original. RKP-only; the caller tracks the
// original address itself because the site is not a table slot.
int ksu_compat_restore_branch(void *original)
{
    return ksu_rkp_restore_slot(original);
}

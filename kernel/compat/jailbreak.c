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

int ksu_compat_patch_slot(void **slot, void *replacement)
{
    if (static_branch_unlikely(&ksu_rkp_key))
        return ksu_rkp_patch_slot(slot, replacement);

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

#include <linux/jump_label.h>

#include "compat/jailbreak.h"
#include "infra/symbol_resolver.h"
#include "klog.h"

void kdp_jb_init(void);
void kdp_jb_exit(void);
void defex_jb_init(void);
void defex_jb_exit(void);

int init_jailbreak(void)
{
    kdp_jb_init();

    if (find_kernel_symbol_exact("rkp_started")) {
        static_branch_enable(&ksu_rkp_key);
        pr_info("KSU: Samsung RKP detected\n");
    }

    defex_jb_init();
    return 0;
}

void exit_jailbreak(void)
{
    kdp_jb_exit();
    static_branch_disable(&ksu_rkp_key);
    defex_jb_exit();
}

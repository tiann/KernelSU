#include <linux/jump_label.h>

#include "compat/jailbreak.h"

void kdp_jb_init(void);
void kdp_jb_exit(void);
void rkp_jb_init(void);
void rkp_jb_exit(void);
void defex_jb_init(void);

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
}

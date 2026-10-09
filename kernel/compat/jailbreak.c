#include "compat/jailbreak.h"

void kdp_jb_init(void);
void kdp_jb_exit(void);
void defex_jb_init(void);
void defex_jb_exit(void);

int init_jailbreak(void)
{
    kdp_jb_init();
    defex_jb_init();
    return 0;
}

void exit_jailbreak(void)
{
    kdp_jb_exit();
    defex_jb_exit();
}

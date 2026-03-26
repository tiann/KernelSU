#ifndef __KSU_H_MANAGER_OBSERVER
#define __KSU_H_MANAGER_OBSERVER

#ifdef CONFIG_KSU_DISABLE_MANAGER
static inline int ksu_observer_init(void)
{
    return 0;
}

static inline void ksu_observer_exit(void)
{
}
#else
int ksu_observer_init(void);
void ksu_observer_exit(void);
#endif

#endif // __KSU_H_MANAGER_OBSERVER

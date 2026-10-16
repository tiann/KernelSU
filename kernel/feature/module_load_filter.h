#ifndef __KSU_H_MODULE_LOAD_FILTER
#define __KSU_H_MODULE_LOAD_FILTER

/* A configured match is skipped with a successful syscall return value. */
void ksu_module_load_filter_hook_init(void);
void ksu_module_load_filter_hook_exit(void);

#endif

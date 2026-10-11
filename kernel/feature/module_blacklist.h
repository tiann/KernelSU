#ifndef __KSU_MODULE_BLACKLIST_H
#define __KSU_MODULE_BLACKLIST_H

// Block the modules named in the `block_modules` param from loading by merging
// them into the kernel's native `module_blacklist`. Call once at init, after
// the symbol resolver is up and before vendor modules are loaded.
void ksu_module_blacklist_init(void);

// Teardown hook. Intentionally keeps the merged list installed; see the
// implementation for why the pointer is not restored.
void ksu_module_blacklist_exit(void);

#endif

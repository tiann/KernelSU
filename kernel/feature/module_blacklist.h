#ifndef __KSU_MODULE_BLACKLIST_H
#define __KSU_MODULE_BLACKLIST_H

// Block the modules named in the `block_modules` param from loading by merging
// them into the kernel's native `module_blacklist`. Call when second stage init
// starts: a module that fails to load in first stage init is fatal, see the
// implementation. Only the first call has an effect.
void ksu_module_blacklist_apply(void);

// Teardown hook. Intentionally keeps the merged list installed; see the
// implementation for why the pointer is not restored.
void ksu_module_blacklist_exit(void);

#endif

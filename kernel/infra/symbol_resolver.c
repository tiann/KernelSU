#include <linux/kallsyms.h>
#include <linux/string.h>

#include "infra/symbol_resolver.h"

void *ksu_lookup_symbol(const char *symbol_name)
{
    char cfi_name[KSYM_NAME_LEN];
    void *addr;
    size_t symbol_len;
    static const char cfi_suffix[] = ".cfi_jt";
    size_t cfi_suffix_len = sizeof(cfi_suffix) - 1;

    if (!symbol_name || !symbol_name[0])
        return NULL;

    symbol_len = strlen(symbol_name);

    if ((symbol_len < cfi_suffix_len || strcmp(symbol_name + symbol_len - cfi_suffix_len, cfi_suffix) != 0) &&
        strscpy(cfi_name, symbol_name, sizeof(cfi_name)) > 0 &&
        strlen(cfi_name) + strlen(".cfi_jt") + 1 <= sizeof(cfi_name)) {
        strlcat(cfi_name, ".cfi_jt", sizeof(cfi_name));
        addr = (void *)kallsyms_lookup_name(cfi_name);
        if (addr)
            return addr;
    }

    return (void *)kallsyms_lookup_name(symbol_name);
}

#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/version.h>

#include "infra/symbol_resolver.h"

struct ksu_lookup_symbol_ctx {
    const char *symbol_name;
    size_t symbol_len;
    void *match;
    void *cfi_match;
};

static bool ksu_symbol_has_suffix(const char *name, size_t name_len, const char *suffix, size_t suffix_len)
{
    return name_len >= suffix_len && strcmp(name + name_len - suffix_len, suffix) == 0;
}

static int ksu_lookup_symbol_handle_match(void *data, const char *name, unsigned long addr)
{
    struct ksu_lookup_symbol_ctx *ctx = data;
    static const char cfi_suffix[] = ".cfi_jt";
    const size_t cfi_suffix_len = sizeof(cfi_suffix) - 1;
    size_t name_len;

    if (!name || !addr)
        return 0;

    name_len = strlen(name);

    if (strcmp(name, ctx->symbol_name) != 0) {
        if (name_len <= ctx->symbol_len || strncmp(name, ctx->symbol_name, ctx->symbol_len) != 0 ||
            name[ctx->symbol_len] != '.')
            return 0;
    }

    if (ksu_symbol_has_suffix(name, name_len, cfi_suffix, cfi_suffix_len)) {
        ctx->cfi_match = (void *)addr;
        return 1;
    }

    if (!ctx->match)
        ctx->match = (void *)addr;

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static int ksu_lookup_symbol_cb(void *data, const char *name, unsigned long addr)
{
    return ksu_lookup_symbol_handle_match(data, name, addr);
}
#else
static int ksu_lookup_symbol_cb(void *data, const char *name, struct module *mod, unsigned long addr)
{
    if (mod)
        return 0;

    return ksu_lookup_symbol_handle_match(data, name, addr);
}
#endif

void *ksu_lookup_symbol(const char *symbol_name)
{
    struct ksu_lookup_symbol_ctx ctx = {
        .symbol_name = symbol_name,
    };
    char cfi_name[KSYM_NAME_LEN];
    void *addr;
    static const char cfi_suffix[] = ".cfi_jt";
    const size_t cfi_suffix_len = sizeof(cfi_suffix) - 1;

    if (!symbol_name || !symbol_name[0])
        return NULL;

    ctx.symbol_len = strlen(symbol_name);

    if (!ksu_symbol_has_suffix(symbol_name, ctx.symbol_len, cfi_suffix, cfi_suffix_len) &&
        ctx.symbol_len + cfi_suffix_len + 1 <= sizeof(cfi_name) &&
        strscpy(cfi_name, symbol_name, sizeof(cfi_name)) > 0) {
        strlcat(cfi_name, cfi_suffix, sizeof(cfi_name));
        addr = (void *)kallsyms_lookup_name(cfi_name);
        if (addr)
            return addr;
    }

    kallsyms_on_each_symbol(ksu_lookup_symbol_cb, &ctx);
    if (ctx.cfi_match)
        return ctx.cfi_match;
    if (ctx.match)
        return ctx.match;

    return (void *)kallsyms_lookup_name(symbol_name);
}

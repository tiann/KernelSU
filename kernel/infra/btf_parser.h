#ifndef __KSU_BTF_PARSER_H
#define __KSU_BTF_PARSER_H

#include <linux/types.h>

#if defined(MODULE) && defined(CONFIG_DEBUG_INFO_BTF)

struct ksu_btf_field {
    u32 offset;
    u32 struct_size;
};

int ksu_btf_field(const char *structure, const char *member, u32 kind, u32 size, struct ksu_btf_field *field);
#endif
#endif

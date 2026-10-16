// SPDX-License-Identifier: GPL-2.0-only
/*
 * In-kernel insmod supercall.
 *
 * Loads an out-of-tree kernel module from a caller-provided image, resolving
 * its undefined symbols against the running kernel's kallsyms before handing
 * the image to the kernel's internal load_module(). This lets a module
 * reference non-exported kernel symbols, acting as a self-contained
 * replacement for an external LKM loader.
 *
 * The caller passes the module binary directly (a userspace buffer), not a
 * path or fd, so there is no re-open and no path/TOCTOU concern.
 *
 * Reference: https://github.com/KernelSU2/lkmloader.ko
 */

#include <linux/elf.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>

#include "klog.h" // IWYU pragma: keep
#include "uapi/supercall.h"
#include "supercall/internal.h"
#include "infra/symbol_resolver.h"

// struct load_info lives in the kernel's private module internals header,
// whose path moved in 6.1.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#include "kernel/module/internal.h"
#else
#include "kernel/module-internal.h"
#endif

// load_module() is static in the kernel; resolve it by name at runtime.
typedef int (*load_module_fn_t)(struct load_info *info, const char __user *uargs, int flags);

/*
 * Walk the module ELF symbol table and bind every undefined symbol to its
 * address in the running kernel, so the subsequent load_module() does not
 * reject the module over unresolved (non-exported) symbols.
 */
static int __nocfi ksu_patch_module_symbols(void *buf, size_t len)
{
    Elf64_Ehdr *eh = buf;
    Elf64_Shdr *sh, *shend;
    Elf64_Sym *symtab = NULL, *symtab_end = NULL, *sym;
    char *strtab = NULL, *strtab_end = NULL;
    size_t shend_off;
    int idx, symstr_idx = -1;

    if (len <= sizeof(Elf64_Ehdr)) {
        pr_err("insmod: module too small: %zu\n", len);
        return -EINVAL;
    }
    if (eh->e_shentsize != sizeof(Elf64_Shdr)) {
        pr_err("insmod: invalid shentsize: %u\n", eh->e_shentsize);
        return -EINVAL;
    }
    shend_off = eh->e_shoff + (size_t)eh->e_shentsize * eh->e_shnum;
    if (eh->e_shoff > len || shend_off > len || shend_off < eh->e_shoff) {
        pr_err("insmod: section headers out of bounds\n");
        return -EINVAL;
    }

    sh = buf + eh->e_shoff;
    shend = buf + shend_off;
    for (idx = 0; sh < shend; sh++, idx++) {
        if (sh->sh_type == SHT_SYMTAB && !symtab) {
            if (sh->sh_entsize != sizeof(Elf64_Sym) || sh->sh_size % sizeof(Elf64_Sym) != 0)
                continue;
            if (sh->sh_offset >= len || sh->sh_offset + sh->sh_size > len)
                continue;
            symtab = buf + sh->sh_offset;
            symtab_end = (void *)symtab + sh->sh_size;
            symstr_idx = sh->sh_link;
        } else if (sh->sh_type == SHT_STRTAB && symstr_idx == idx && !strtab) {
            if (sh->sh_offset >= len || sh->sh_offset + sh->sh_size > len)
                continue;
            strtab = buf + sh->sh_offset;
            strtab_end = strtab + sh->sh_size;
        }
    }

    if (!symtab || !strtab) {
        pr_err("insmod: symtab/strtab not found\n");
        return -EINVAL;
    }

    for (sym = symtab, idx = 0; sym < symtab_end; sym++, idx++) {
        char *name;
        unsigned long addr;

        if (sym->st_shndx != SHN_UNDEF)
            continue;

        name = strtab + sym->st_name;
        if (name < strtab || name >= strtab_end || !*name)
            continue;

        addr = find_kernel_symbol_exact(name);
        if (!addr) {
            // Leave it undefined: it may be a genuinely exported symbol that
            // load_module() can still resolve the normal way.
            pr_warn("insmod: symbol not found: %s\n", name);
            continue;
        }

        sym->st_value = addr;
        sym->st_shndx = SHN_ABS;
    }

    return 0;
}

int __nocfi ksu_do_insmod(void __user *arg)
{
    struct ksu_insmod_cmd cmd;
    load_module_fn_t load_module_fn;
    struct load_info info = {};
    void *buf;
    size_t len;
    int ret;

    if (copy_from_user(&cmd, arg, sizeof(cmd)))
        return -EFAULT;

    if (cmd.flags) {
        pr_err("insmod: unsupported flags 0x%x\n", cmd.flags);
        return -EINVAL;
    }
    if (!cmd.data || !cmd.data_len || !cmd.params)
        return -EINVAL;
    if (cmd.data_len > INT_MAX) {
        pr_err("insmod: module too large: %llu\n", cmd.data_len);
        return -EINVAL;
    }

    load_module_fn = (load_module_fn_t)find_kernel_symbol_exact("load_module");
    if (!load_module_fn) {
        pr_err("insmod: load_module symbol not found\n");
        return -ENOSYS;
    }

    len = (size_t)cmd.data_len;

    // load_module() takes ownership of the image as info.hdr and frees it with
    // vfree() on both success and failure, so it must be vmalloc'd and must not
    // be freed here once handed over.
    buf = vmalloc(len);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, (const void __user *)(uintptr_t)cmd.data, len)) {
        pr_err("insmod: copy module image failed\n");
        vfree(buf);
        return -EFAULT;
    }

    ret = ksu_patch_module_symbols(buf, len);
    if (ret) {
        pr_err("insmod: patch symbols failed: %d\n", ret);
        vfree(buf);
        return ret;
    }

    info.hdr = buf;
    info.len = len;

    // From here load_module() owns info.hdr.
    ret = load_module_fn(&info, (const char __user *)(uintptr_t)cmd.params, 0);
    if (ret)
        pr_err("insmod: load_module failed: %d\n", ret);
    else
        pr_info("insmod: module loaded\n");

    return ret;
}

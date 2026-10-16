// SPDX-License-Identifier: GPL-2.0-only
/*
 * In-kernel insmod supercall.
 *
 * Loads an out-of-tree kernel module from a caller-provided image, resolving
 * its undefined symbols against the running kernel's kallsyms before handing
 * the image to the regular init_module syscall. This lets a module reference
 * non-exported kernel symbols, acting as a self-contained replacement for an
 * external LKM loader.
 *
 * Same approach as ksuinit's userspace loader: patch the symbol table, then go
 * through init_module, so CAP_SYS_MODULE, modules_disabled, LSM and module
 * signature checks all still apply.
 *
 * The caller passes the module binary directly (a userspace buffer), not a
 * path or fd, so there is no re-open and no path/TOCTOU concern.
 *
 * Reference: https://github.com/KernelSU2/lkmloader.ko
 */

#include <linux/elf.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "klog.h" // IWYU pragma: keep
#include "util.h"
#include "uapi/supercall.h"
#include "supercall/internal.h"
#include "infra/symbol_resolver.h"

static bool range_ok(u64 off, u64 size, size_t len)
{
    return off <= len && size <= len - off;
}

/*
 * Walk the module ELF symbol table and bind every undefined symbol to its
 * address in the running kernel, so the subsequent init_module does not
 * reject the module over unresolved (non-exported) symbols.
 *
 * This runs before the kernel's own ELF validation, so every offset taken from
 * the image is bounds checked here. Returns -ENOEXEC for a malformed image.
 */
static int ksu_patch_module_symbols(void *buf, size_t len)
{
    Elf64_Ehdr *eh = buf;
    Elf64_Shdr *shdrs, *symsec = NULL, *strsec;
    Elf64_Sym *syms;
    const char *strtab;
    size_t nsyms, strsz, i;

    if (len < sizeof(*eh) || memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_type != ET_REL) {
        pr_err("insmod: not an ELF64 relocatable object\n");
        return -ENOEXEC;
    }
    if (eh->e_shentsize != sizeof(Elf64_Shdr) || !range_ok(eh->e_shoff, (u64)eh->e_shnum * sizeof(Elf64_Shdr), len)) {
        pr_err("insmod: section headers out of bounds\n");
        return -ENOEXEC;
    }

    shdrs = buf + eh->e_shoff;
    for (i = 0; i < eh->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symsec = &shdrs[i];
            break;
        }
    }
    if (!symsec || symsec->sh_entsize != sizeof(Elf64_Sym) || symsec->sh_size % sizeof(Elf64_Sym) ||
        !range_ok(symsec->sh_offset, symsec->sh_size, len) || symsec->sh_link >= eh->e_shnum) {
        pr_err("insmod: bad or missing symtab\n");
        return -ENOEXEC;
    }

    strsec = &shdrs[symsec->sh_link];
    strtab = buf + strsec->sh_offset;
    strsz = strsec->sh_size;
    // A trailing NUL guarantees every name starting inside the table ends there.
    if (strsec->sh_type != SHT_STRTAB || !strsz || !range_ok(strsec->sh_offset, strsz, len) ||
        strtab[strsz - 1] != '\0') {
        pr_err("insmod: bad symbol strtab\n");
        return -ENOEXEC;
    }

    syms = buf + symsec->sh_offset;
    nsyms = symsec->sh_size / sizeof(Elf64_Sym);
    // Entry 0 is the reserved null symbol.
    for (i = 1; i < nsyms; i++) {
        Elf64_Sym *sym = &syms[i];
        const char *name;
        unsigned long addr;

        if (sym->st_shndx != SHN_UNDEF || sym->st_name >= strsz)
            continue;

        name = strtab + sym->st_name;
        if (!*name)
            continue;

        addr = find_kernel_symbol_exact(name);
        if (!addr) {
            // Leave it undefined: it may be a genuinely exported symbol that
            // init_module can still resolve the normal way.
            pr_warn("insmod: symbol not found: %s\n", name);
            continue;
        }

        sym->st_value = addr;
        sym->st_shndx = SHN_ABS;
    }

    return 0;
}

int ksu_do_insmod(void __user *arg)
{
    struct ksu_insmod_cmd cmd;
    unsigned long uaddr;
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

    len = (size_t)cmd.data_len;
    buf = vmalloc(len);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, (const void __user *)(uintptr_t)cmd.data, len)) {
        pr_err("insmod: copy module image failed\n");
        ret = -EFAULT;
        goto out_free;
    }

    ret = ksu_patch_module_symbols(buf, len);
    if (ret)
        goto out_free;

    // init_module takes a userspace image: stage the patched copy in a fresh
    // anonymous mapping of the caller, the source buffer may be read-only.
    uaddr = vm_mmap(NULL, 0, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0);
    if (IS_ERR_VALUE(uaddr)) {
        ret = (int)uaddr;
        pr_err("insmod: map staging buffer failed: %d\n", ret);
        goto out_free;
    }

    if (copy_to_user((void __user *)uaddr, buf, len)) {
        pr_err("insmod: stage module image failed\n");
        ret = -EFAULT;
    } else {
        vfree(buf);
        buf = NULL;
        ret = ksyscall(init_module, uaddr, len, (const char __user *)(uintptr_t)cmd.params);
        if (ret)
            pr_err("insmod: init_module failed: %d\n", ret);
        else
            pr_info("insmod: module loaded\n");
    }

    vm_munmap(uaddr, len);
out_free:
    vfree(buf);
    return ret;
}

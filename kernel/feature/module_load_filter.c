#include <asm/elf.h>
#include <linux/elf.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <uapi/linux/module.h>

#include "feature/module_load_filter.h"
#include "klog.h" // IWYU pragma: keep
#include "arch.h"
#include "hook/syscall_hook.h"

struct ksu_module_name {
    const char *name;
    size_t len;
};

struct ksu_elf_reader {
    const char __user *umod;
    unsigned long umod_len;
    struct file *file;
    loff_t file_size;
};

static int ksu_reader_read(struct ksu_elf_reader *r, loff_t offset, void *buf, size_t len)
{
    loff_t total = r->umod ? (loff_t)r->umod_len : r->file_size;

    if (len == 0)
        return -EINVAL;
    if (offset < 0 || (loff_t)len > total || offset > total - (loff_t)len)
        return -ERANGE;

    if (r->umod) {
        if (copy_from_user(buf, r->umod + offset, len))
            return -EFAULT;
        return 0;
    }

    loff_t pos = offset;
    ssize_t n = kernel_read(r->file, buf, len, &pos);
    if (n < 0 || (size_t)n != len)
        return -EIO;
    return 0;
}

static bool check_module_should_block(const char *name, size_t name_len, bool normalize_filename,
                                      struct ksu_module_name *blocked)
{
    const char *cursor = ksu_block_modules;
    const char *end = cursor + strnlen(cursor, sizeof(ksu_block_modules));

    while (cursor < end) {
        const char *separator = memchr(cursor, ',', end - cursor);
        size_t entry_len = separator ? (size_t)(separator - cursor) : (size_t)(end - cursor);

        if (entry_len && name_len == entry_len) {
            size_t i;

            for (i = 0; i < name_len; i++) {
                char a = name[i];
                char b = cursor[i];

                if (normalize_filename && a == '-')
                    a = '_';

                if (b == '-')
                    b = '_';

                if (a != b)
                    break;
            }

            if (i == name_len) {
                blocked->name = cursor;
                blocked->len = entry_len;
                return true;
            }
        }

        if (!separator)
            break;

        cursor = separator + 1;
    }

    return false;
}

static bool ksu_is_block_module(struct ksu_elf_reader *r, struct ksu_module_name *blocked)
{
    Elf_Ehdr ehdr;
    Elf_Shdr *shdrs = NULL;
    char *shstrtab = NULL;
    char *modinfo = NULL;
    Elf_Shdr *shstr_sh;
    Elf_Shdr *modinfo_sh = NULL;
    unsigned int shnum, i;
    unsigned long shtab_bytes;
    bool should_block = false;

    if (ksu_reader_read(r, 0, &ehdr, sizeof(ehdr)))
        return false;

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0)
        return false;
    if (ehdr.e_ident[EI_CLASS] != ELF_CLASS || ehdr.e_ident[EI_DATA] != ELF_DATA ||
        ehdr.e_ident[EI_VERSION] != EV_CURRENT)
        return false;
    if (ehdr.e_type != ET_REL || !elf_check_arch(&ehdr) || ehdr.e_version != EV_CURRENT)
        return false;
    if (ehdr.e_shentsize != sizeof(Elf_Shdr))
        return false;

    shnum = ehdr.e_shnum;
    if (shnum == 0 || shnum > 512)
        return false;
    if (ehdr.e_shstrndx >= shnum)
        return false;

    shtab_bytes = (unsigned long)shnum * sizeof(Elf_Shdr);
    shdrs = kmalloc(shtab_bytes, GFP_KERNEL);
    if (!shdrs)
        return false;
    if (ksu_reader_read(r, ehdr.e_shoff, shdrs, shtab_bytes))
        goto out;

    shstr_sh = &shdrs[ehdr.e_shstrndx];
    if (shstr_sh->sh_type != SHT_STRTAB || shstr_sh->sh_size == 0 || shstr_sh->sh_size > 65536)
        goto out;
    shstrtab = kmalloc(shstr_sh->sh_size, GFP_KERNEL);
    if (!shstrtab)
        goto out;
    if (ksu_reader_read(r, shstr_sh->sh_offset, shstrtab, shstr_sh->sh_size))
        goto out;

    for (i = 0; i < shnum; i++) {
        Elf_Shdr *sh = &shdrs[i];
        const char *name;
        unsigned long remaining;

        if (sh->sh_name >= shstr_sh->sh_size)
            continue;
        name = shstrtab + sh->sh_name;
        remaining = shstr_sh->sh_size - sh->sh_name;
        if (strnlen(name, remaining) >= remaining)
            continue;
        if (sh->sh_type == SHT_PROGBITS && strcmp(name, ".modinfo") == 0) {
            modinfo_sh = sh;
            break;
        }
    }
    if (!modinfo_sh)
        goto out;

    if (modinfo_sh->sh_size == 0 || modinfo_sh->sh_size > 16384)
        goto out;
    modinfo = kmalloc(modinfo_sh->sh_size, GFP_KERNEL);
    if (!modinfo)
        goto out;
    if (ksu_reader_read(r, modinfo_sh->sh_offset, modinfo, modinfo_sh->sh_size))
        goto out;

    {
        const char *p = modinfo;
        const char *end = modinfo + modinfo_sh->sh_size;

        while (p < end) {
            const char *nul = memchr(p, '\0', end - p);
            size_t entlen;

            if (!nul)
                break;
            entlen = nul - p;
            if (entlen > 5 && memcmp(p, "name=", 5) == 0) {
                const char *val = p + 5;
                size_t vlen = entlen - 5;

                should_block = check_module_should_block(val, vlen, false, blocked);
                break;
            }
            p = nul + 1;
        }
    }

out:
    kfree(modinfo);
    kfree(shstrtab);
    kfree(shdrs);
    return should_block;
}

int ksu_handle_init_module(const void __user *umod, unsigned long umod_len)
{
    struct ksu_module_name blocked = { 0 };
    struct ksu_elf_reader r = {
        .umod = (const char __user *)umod,
        .umod_len = umod_len,
        .file = NULL,
        .file_size = 0,
    };

    if (!ksu_block_modules[0])
        return 0;

    if (r.umod && r.umod_len >= sizeof(Elf_Ehdr) && ksu_is_block_module(&r, &blocked)) {
        pr_info("module_load_filter: block %.*s load due to it in blocklist\n", (int)blocked.len, blocked.name);
        return 0;
    }

    return 1;
}

int ksu_handle_finit_module(int fd, int flags)
{
    struct ksu_module_name blocked = { 0 };
    struct file *file;
    bool should_block = false;

    if (!ksu_block_modules[0])
        return 0;

    file = fget(fd);

    if (!file)
        return 0;

#ifdef MODULE_INIT_COMPRESSED_FILE
    if (flags & MODULE_INIT_COMPRESSED_FILE) {
        static const char *const compression_suffixes[] = { ".gz", ".xz", ".zst" };
        const struct qstr *filename = &file->f_path.dentry->d_name;
        size_t name_len = filename->len;
        size_t i;

        for (i = 0; i < ARRAY_SIZE(compression_suffixes); i++) {
            const char *suffix = compression_suffixes[i];
            size_t suffix_len = strlen(suffix);

            if (name_len > suffix_len && !memcmp(filename->name + name_len - suffix_len, suffix, suffix_len)) {
                name_len -= suffix_len;
                break;
            }
        }

        if (i != ARRAY_SIZE(compression_suffixes)) {
            if (name_len > 3 && !memcmp(filename->name + name_len - 3, ".ko", 3)) {
                name_len -= 3;
                should_block = check_module_should_block(filename->name, name_len, true, &blocked);
            }
        }
    } else
#endif
    {
        struct ksu_elf_reader r = {
            .umod = NULL,
            .umod_len = 0,
            .file = file,
            .file_size = i_size_read(file_inode(file)),
        };

        // https://github.com/torvalds/linux/commit/b1ae6dc41eaaa98bb75671e0f3665bfda248c3e7
        // linux kernel 5.17+
        should_block = r.file_size >= (loff_t)sizeof(Elf_Ehdr) && ksu_is_block_module(&r, &blocked);
    }

    fput(file);

    if (should_block) {
        pr_info("module_load_filter: block %.*s load due to it in blocklist\n", (int)blocked.len, blocked.name);
        return 0;
    }

    return 1;
}

// init_module(2): sys_init_module(void __user *umod, unsigned long len,
//                                const char __user *uargs)
static long (*orig_sys_init_module)(const struct pt_regs *regs);
static long ksu_sys_init_module(const struct pt_regs *regs)
{
    int ret = ksu_handle_init_module((const void __user *)PT_REGS_PARM1(regs), (unsigned long)PT_REGS_PARM2(regs));

    if (!ret)
        return ret;

    return orig_sys_init_module(regs);
}

// finit_module(2): sys_finit_module(int fd, const char __user *uargs, int flags)
static long (*orig_sys_finit_module)(const struct pt_regs *regs);
static long ksu_sys_finit_module(const struct pt_regs *regs)
{
    int ret = ksu_handle_finit_module((int)PT_REGS_PARM1(regs), (int)PT_REGS_PARM3(regs));

    if (!ret)
        return ret;

    return orig_sys_finit_module(regs);
}

void __init ksu_module_load_filter_hook_init(void)
{
    if (!ksu_block_modules[0]) {
        pr_info("module_load_filter: no modules should be blocked\n");
        return;
    }

    ksu_syscall_table_hook(__NR_init_module, ksu_sys_init_module, &orig_sys_init_module);
    ksu_syscall_table_hook(__NR_finit_module, ksu_sys_finit_module, &orig_sys_finit_module);
    pr_info("module_load_filter: target modules: %s\n", ksu_block_modules);
}

void __exit ksu_module_load_filter_hook_exit(void)
{
    if (!ksu_block_modules[0])
        return;

    ksu_syscall_table_unhook(__NR_init_module);
    ksu_syscall_table_unhook(__NR_finit_module);
}

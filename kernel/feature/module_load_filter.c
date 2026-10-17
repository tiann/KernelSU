#include <asm/elf.h>
#include <asm/unistd.h>
#include <linux/capability.h>
#include <linux/dcache.h>
#include <linux/elf.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <uapi/linux/module.h>

#include "arch.h"
#include "feature/module_load_filter.h"
#include "hook/syscall_hook.h"
#include "klog.h" // IWYU pragma: keep

#define BLOCK_MODULES_MAX 256

static char block_modules[BLOCK_MODULES_MAX];
static syscall_fn_t orig_sys_init_module;
static syscall_fn_t orig_sys_finit_module;
static bool hooks_installed;

struct ksu_module_name {
    const char *name;
    size_t len;
};

struct ksu_elf_reader {
    const char __user *user_data;
    unsigned long user_size;
    struct file *file;
    loff_t file_size;
};

static int block_modules_param_set(const char *value, const struct kernel_param *kp)
{
    (void)kp;
    if (!value)
        return 0;
    if (!value[0]) {
        block_modules[0] = '\0';
        return 0;
    }
    if (strscpy(block_modules, value, sizeof(block_modules)) < 0)
        pr_err("module_load_filter: block_modules too long; list truncated\n");
    return 0;
}

static const struct kernel_param_ops block_modules_param_ops = {
    .flags = KERNEL_PARAM_OPS_FL_NOARG,
    .set = block_modules_param_set,
};
module_param_cb(block_modules, &block_modules_param_ops, NULL, 0);

static int ksu_elf_reader_read(struct ksu_elf_reader *reader, loff_t offset, void *buffer, size_t len)
{
    loff_t total = reader->user_data ? (loff_t)reader->user_size : reader->file_size;

    if (!len || len > 65536 || offset < 0 || offset > total || (loff_t)len > total - offset)
        return -ERANGE;

    if (reader->user_data) {
        if (copy_from_user(buffer, reader->user_data + (size_t)offset, len))
            return -EFAULT;
        return 0;
    }

    {
        loff_t pos = offset;
        ssize_t count = kernel_read(reader->file, buffer, len, &pos);

        return count == (ssize_t)len ? 0 : -EIO;
    }
}

static char normalize_module_char(char value)
{
    return value == '-' ? '_' : value;
}

static bool ksu_module_name_is_blocked(const char *name, size_t name_len, struct ksu_module_name *blocked)
{
    const char *cursor = block_modules;
    const char *end = cursor + strnlen(cursor, sizeof(block_modules));

    while (cursor < end) {
        const char *separator = memchr(cursor, ',', end - cursor);
        size_t entry_len = separator ? (size_t)(separator - cursor) : (size_t)(end - cursor);

        if (entry_len && entry_len == name_len) {
            size_t i;

            for (i = 0; i < name_len; i++) {
                if (normalize_module_char(name[i]) != normalize_module_char(cursor[i]))
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

static bool ksu_elf_module_is_blocked(struct ksu_elf_reader *reader, struct ksu_module_name *blocked)
{
    Elf_Ehdr header;
    Elf_Shdr *sections = NULL;
    char *section_names = NULL;
    char *module_info = NULL;
    Elf_Shdr *section_names_header;
    Elf_Shdr *module_info_header = NULL;
    unsigned int section_count;
    unsigned int i;
    size_t sections_size;
    bool should_block = false;

    if (ksu_elf_reader_read(reader, 0, &header, sizeof(header)))
        return false;
    if (memcmp(header.e_ident, ELFMAG, SELFMAG) || header.e_ident[EI_CLASS] != ELF_CLASS ||
        header.e_ident[EI_DATA] != ELF_DATA || header.e_ident[EI_VERSION] != EV_CURRENT)
        return false;
    if (header.e_type != ET_REL || !elf_check_arch(&header) || header.e_version != EV_CURRENT)
        return false;
    if (header.e_shentsize != sizeof(Elf_Shdr))
        return false;

    section_count = header.e_shnum;
    if (!section_count || section_count > 512 || header.e_shstrndx >= section_count)
        return false;

    sections_size = (size_t)section_count * sizeof(Elf_Shdr);
    sections = kmalloc(sections_size, GFP_KERNEL);
    if (!sections)
        return false;
    if (ksu_elf_reader_read(reader, header.e_shoff, sections, sections_size))
        goto out;

    section_names_header = &sections[header.e_shstrndx];
    if (section_names_header->sh_type != SHT_STRTAB || !section_names_header->sh_size ||
        section_names_header->sh_size > 65536)
        goto out;
    section_names = kmalloc(section_names_header->sh_size, GFP_KERNEL);
    if (!section_names)
        goto out;
    if (ksu_elf_reader_read(reader, section_names_header->sh_offset, section_names, section_names_header->sh_size))
        goto out;

    for (i = 0; i < section_count; i++) {
        Elf_Shdr *section = &sections[i];
        size_t remaining;
        const char *name;

        if (section->sh_name >= section_names_header->sh_size)
            continue;
        remaining = section_names_header->sh_size - section->sh_name;
        name = section_names + section->sh_name;
        if (strnlen(name, remaining) == remaining)
            continue;
        if (section->sh_type == SHT_PROGBITS && !strcmp(name, ".modinfo")) {
            module_info_header = section;
            break;
        }
    }
    if (!module_info_header || !module_info_header->sh_size || module_info_header->sh_size > 16384)
        goto out;

    module_info = kmalloc(module_info_header->sh_size, GFP_KERNEL);
    if (!module_info)
        goto out;
    if (ksu_elf_reader_read(reader, module_info_header->sh_offset, module_info, module_info_header->sh_size))
        goto out;

    {
        const char *cursor = module_info;
        const char *end = module_info + module_info_header->sh_size;

        while (cursor < end) {
            const char *terminator = memchr(cursor, '\0', end - cursor);
            size_t entry_len;

            if (!terminator)
                break;
            entry_len = terminator - cursor;
            if (entry_len > 5 && !memcmp(cursor, "name=", 5)) {
                should_block = ksu_module_name_is_blocked(cursor + 5, entry_len - 5, blocked);
                break;
            }
            cursor = terminator + 1;
        }
    }

out:
    kfree(module_info);
    kfree(section_names);
    kfree(sections);
    return should_block;
}

#ifdef MODULE_INIT_COMPRESSED_FILE
static bool ksu_compressed_module_file_is_blocked(struct file *file, struct ksu_module_name *blocked)
{
    static const char *const suffixes[] = { ".gz", ".xz", ".zst" };
    struct dentry *dentry = file->f_path.dentry;
    const struct qstr *filename = &dentry->d_name;
    size_t name_len;
    size_t i;
    bool should_block = false;

    spin_lock(&dentry->d_lock);
    name_len = filename->len;

    for (i = 0; i < ARRAY_SIZE(suffixes); i++) {
        size_t suffix_len = strlen(suffixes[i]);

        if (name_len > suffix_len && !memcmp(filename->name + name_len - suffix_len, suffixes[i], suffix_len)) {
            name_len -= suffix_len;
            break;
        }
    }
    if (i == ARRAY_SIZE(suffixes) || name_len <= 3 || memcmp(filename->name + name_len - 3, ".ko", 3))
        goto out;

    should_block = ksu_module_name_is_blocked(filename->name, name_len - 3, blocked);
out:
    spin_unlock(&dentry->d_lock);
    return should_block;
}
#endif

static bool ksu_init_module_is_blocked(const void __user *module_data, unsigned long module_size,
                                       struct ksu_module_name *blocked)
{
    struct ksu_elf_reader reader = {
        .user_data = (const char __user *)module_data,
        .user_size = module_size,
        .file = NULL,
        .file_size = 0,
    };

    return block_modules[0] && module_data && module_size >= sizeof(Elf_Ehdr) && capable(CAP_SYS_MODULE) &&
           ksu_elf_module_is_blocked(&reader, blocked);
}

static bool ksu_finit_module_is_blocked(int fd, int flags, struct ksu_module_name *blocked)
{
    struct file *file;
    bool should_block = false;
    int valid_flags = MODULE_INIT_IGNORE_MODVERSIONS | MODULE_INIT_IGNORE_VERMAGIC;

#ifdef MODULE_INIT_COMPRESSED_FILE
    valid_flags |= MODULE_INIT_COMPRESSED_FILE;
#endif
    if (!block_modules[0] || (flags & ~valid_flags) || !capable(CAP_SYS_MODULE))
        return false;
    file = fget(fd);
    if (!file)
        return false;
    if (!S_ISREG(file_inode(file)->i_mode))
        goto out;

#ifdef MODULE_INIT_COMPRESSED_FILE
    if (flags & MODULE_INIT_COMPRESSED_FILE) {
        should_block = ksu_compressed_module_file_is_blocked(file, blocked);
    } else
#endif
    {
        struct ksu_elf_reader reader = {
            .user_data = NULL,
            .user_size = 0,
            .file = file,
            .file_size = i_size_read(file_inode(file)),
        };

        should_block = reader.file_size >= (loff_t)sizeof(Elf_Ehdr) && ksu_elf_module_is_blocked(&reader, blocked);
    }

out:
    fput(file);
    return should_block;
}

static long ksu_sys_init_module(const struct pt_regs *regs)
{
    struct ksu_module_name blocked = { 0 };
    const void __user *module_data = (const void __user *)PT_REGS_SYSCALL_PARM1(regs);
    unsigned long module_size = (unsigned long)PT_REGS_PARM2(regs);

    if (ksu_init_module_is_blocked(module_data, module_size, &blocked)) {
        pr_info_ratelimited("module_load_filter: skipped %.*s from init_module\n", (int)blocked.len, blocked.name);
        return 0;
    }

    return orig_sys_init_module ? orig_sys_init_module(regs) : -ENOSYS;
}

static long ksu_sys_finit_module(const struct pt_regs *regs)
{
    struct ksu_module_name blocked = { 0 };
    int fd = (int)PT_REGS_SYSCALL_PARM1(regs);
    int flags = (int)PT_REGS_PARM3(regs);

    if (ksu_finit_module_is_blocked(fd, flags, &blocked)) {
        pr_info_ratelimited("module_load_filter: skipped %.*s from finit_module\n", (int)blocked.len, blocked.name);
        return 0;
    }

    return orig_sys_finit_module ? orig_sys_finit_module(regs) : -ENOSYS;
}

void __init ksu_module_load_filter_hook_init(void)
{
    if (!block_modules[0]) {
        pr_info("module_load_filter: module filter disabled\n");
        return;
    }
    if (!ksu_syscall_table) {
        pr_warn("module_load_filter: syscall table unavailable\n");
        return;
    }

    ksu_syscall_table_hook(__NR_init_module, (syscall_fn_t)ksu_sys_init_module, &orig_sys_init_module);
    ksu_syscall_table_hook(__NR_finit_module, (syscall_fn_t)ksu_sys_finit_module, &orig_sys_finit_module);
    hooks_installed = true;
    pr_info("module_load_filter: filtering module names: %s\n", block_modules);
}

void __exit ksu_module_load_filter_hook_exit(void)
{
    if (!hooks_installed)
        return;

    ksu_syscall_table_unhook(__NR_init_module);
    ksu_syscall_table_unhook(__NR_finit_module);
    hooks_installed = false;
}

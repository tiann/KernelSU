#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

typedef struct {
    void *data;
    size_t size;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdr;
    char *shstrtab;
} ElfFile;

int open_elf(const char *path, ElfFile *elf) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error: Cannot open file %s\n", path);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        fprintf(stderr, "Error: Cannot stat file %s\n", path);
        close(fd);
        return -1;
    }

    elf->size = st.st_size;
    elf->data = mmap(NULL, elf->size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (elf->data == MAP_FAILED) {
        fprintf(stderr, "Error: Cannot mmap file %s\n", path);
        return -1;
    }

    elf->ehdr = (Elf64_Ehdr *)elf->data;

        if (memcmp(elf->ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Error: %s is not a valid ELF file\n", path);
        munmap(elf->data, elf->size);
        return -1;
    }

        if (elf->ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "Error: %s is not a 64-bit ELF file\n", path);
        munmap(elf->data, elf->size);
        return -1;
    }

        elf->shdr = (Elf64_Shdr *)((char *)elf->data + elf->ehdr->e_shoff);

        elf->shstrtab = (char *)elf->data + elf->shdr[elf->ehdr->e_shstrndx].sh_offset;

    return 0;
}

void close_elf(ElfFile *elf) {
    munmap(elf->data, elf->size);
}

Elf64_Shdr *find_symtab(ElfFile *elf) {
    for (int i = 0; i < elf->ehdr->e_shnum; i++) {
        if (elf->shdr[i].sh_type == SHT_SYMTAB) {
            return &elf->shdr[i];
        }
    }
    return NULL;
}

Elf64_Sym *find_symbol(ElfFile *elf, const char *name, Elf64_Shdr *symtab, char *strtab) {
    Elf64_Sym *syms = (Elf64_Sym *)((char *)elf->data + symtab->sh_offset);
    int sym_count = symtab->sh_size / sizeof(Elf64_Sym);

    for (int i = 0; i < sym_count; i++) {
        const char *sym_name = strtab + syms[i].st_name;
        if (strcmp(sym_name, name) == 0) {
            return &syms[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <ko_elf> <vmlinux>\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_path = argv[2];

    ElfFile ko_elf, vmlinux;

        if (open_elf(in_path, &ko_elf) < 0) {
        return 1;
    }

        if (open_elf(out_path, &vmlinux) < 0) {
        close_elf(&ko_elf);
        return 1;
    }

        Elf64_Shdr *in_symtab = find_symtab(&ko_elf);
    Elf64_Shdr *out_symtab = find_symtab(&vmlinux);

    if (!in_symtab) {
        fprintf(stderr, "Error: No symbol table found in %s\n", in_path);
        close_elf(&ko_elf);
        close_elf(&vmlinux);
        return 1;
    }

    if (!out_symtab) {
        fprintf(stderr, "Error: No symbol table found in %s\n", out_path);
        close_elf(&ko_elf);
        close_elf(&vmlinux);
        return 1;
    }

        char *in_strtab = (char *)ko_elf.data + ko_elf.shdr[in_symtab->sh_link].sh_offset;
    char *out_strtab = (char *)vmlinux.data + vmlinux.shdr[out_symtab->sh_link].sh_offset;

        Elf64_Sym *in_syms = (Elf64_Sym *)((char *)ko_elf.data + in_symtab->sh_offset);
    int in_sym_count = in_symtab->sh_size / sizeof(Elf64_Sym);

    int has_error = 0;

    for (int i = 0; i < in_sym_count; i++) {
                if (in_syms[i].st_shndx == SHN_UNDEF && in_syms[i].st_name != 0) {
            const char *sym_name = in_strtab + in_syms[i].st_name;

                        Elf64_Sym *out_sym = find_symbol(&vmlinux, sym_name, out_symtab, out_strtab);

            if (!out_sym || out_sym->st_shndx == SHN_UNDEF) {
                                fprintf(stderr, "Error: Undefined symbol '%s' not found or undefined in %s\n", 
                        sym_name, out_path);
                has_error = 1;
            } else {
                                int binding = ELF64_ST_BIND(out_sym->st_info);
                if (binding != STB_GLOBAL && binding != STB_WEAK) {
                    fprintf(stderr, "Warning: Symbol '%s' is defined in %s but not global (binding=%d)\n", 
                            sym_name, out_path, binding);
                }
            }
        }
    }

    close_elf(&ko_elf);
    close_elf(&vmlinux);

    return has_error ? 1 : 0;
}

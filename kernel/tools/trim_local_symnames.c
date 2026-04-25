#define _XOPEN_SOURCE 700

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} strset_t;

typedef struct {
    void *data;
    size_t size;
    int fd;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdr;
} elf_file_t;

static void die(const char *msg)
{
    fprintf(stderr, "error: %s\n", msg);
    exit(1);
}

static void die_perror(const char *msg)
{
    fprintf(stderr, "error: %s: %s\n", msg, strerror(errno));
    exit(1);
}

static void strset_init(strset_t *s)
{
    memset(s, 0, sizeof(*s));
}

static void strset_free(strset_t *s)
{
    for (size_t i = 0; i < s->len; i++) {
        free(s->items[i]);
    }
    free(s->items);
    memset(s, 0, sizeof(*s));
}

static bool strset_contains(const strset_t *s, const char *v)
{
    for (size_t i = 0; i < s->len; i++) {
        if (strcmp(s->items[i], v) == 0) {
            return true;
        }
    }
    return false;
}

static void strset_add(strset_t *s, const char *v)
{
    if (!v || v[0] == '\0' || strset_contains(s, v)) {
        return;
    }
    if (s->len == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 64;
        char **n = realloc(s->items, ncap * sizeof(*n));
        if (!n) {
            die("out of memory");
        }
        s->items = n;
        s->cap = ncap;
    }
    s->items[s->len] = strdup(v);
    if (!s->items[s->len]) {
        die("out of memory");
    }
    s->len++;
}

static void add_keep_file(strset_t *keep, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        die_perror("open keep file");
    }
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char *hash = strchr(line, '#');
        if (hash) {
            *hash = '\0';
        }
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
        char *e = p + strlen(p);
        while (e > p && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) {
            *--e = '\0';
        }
        if (*p) {
            strset_add(keep, p);
        }
    }
    fclose(fp);
}

static bool elf_open_rw(const char *path, elf_file_t *elf)
{
    memset(elf, 0, sizeof(*elf));
    elf->fd = open(path, O_RDWR);
    if (elf->fd < 0) {
        return false;
    }
    struct stat st;
    if (fstat(elf->fd, &st) < 0) {
        close(elf->fd);
        return false;
    }
    elf->size = (size_t)st.st_size;
    elf->data = mmap(NULL, elf->size, PROT_READ | PROT_WRITE, MAP_SHARED, elf->fd, 0);
    if (elf->data == MAP_FAILED) {
        close(elf->fd);
        return false;
    }
    elf->ehdr = (Elf64_Ehdr *)elf->data;
    if (memcmp(elf->ehdr->e_ident, ELFMAG, SELFMAG) != 0 || elf->ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        munmap(elf->data, elf->size);
        close(elf->fd);
        return false;
    }
    elf->shdr = (Elf64_Shdr *)((char *)elf->data + elf->ehdr->e_shoff);
    return true;
}

static void elf_close(elf_file_t *elf)
{
    if (!elf || !elf->data) {
        return;
    }
    msync(elf->data, elf->size, MS_SYNC);
    munmap(elf->data, elf->size);
    close(elf->fd);
    memset(elf, 0, sizeof(*elf));
}

static Elf64_Shdr *find_symtab(elf_file_t *elf)
{
    for (int i = 0; i < elf->ehdr->e_shnum; i++) {
        if (elf->shdr[i].sh_type == SHT_SYMTAB) {
            return &elf->shdr[i];
        }
    }
    return NULL;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s <target.ko> [--keep-file <path>]\n"
            "Anonymize names of defined LOCAL symbols and compact .strtab\n",
            argv0);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    const char *target = argv[1];
    const char *keep_file = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--keep-file") == 0 && i + 1 < argc) {
            keep_file = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    strset_t keep;
    strset_init(&keep);
    if (keep_file) {
        add_keep_file(&keep, keep_file);
    }

    elf_file_t elf;
    if (!elf_open_rw(target, &elf)) {
        strset_free(&keep);
        die("failed to open target ELF");
    }

    Elf64_Shdr *symtab = find_symtab(&elf);
    if (!symtab) {
        elf_close(&elf);
        strset_free(&keep);
        die("no .symtab found");
    }
    Elf64_Shdr *strsec = &elf.shdr[symtab->sh_link];
    Elf64_Sym *syms = (Elf64_Sym *)((char *)elf.data + symtab->sh_offset);
    char *strtab = (char *)elf.data + strsec->sh_offset;
    size_t sym_count = symtab->sh_size / sizeof(Elf64_Sym);
    size_t str_size = strsec->sh_size;
    size_t old_str_size = str_size;

    size_t anonymized = 0;
    for (size_t i = 0; i < sym_count; i++) {
        unsigned bind = ELF64_ST_BIND(syms[i].st_info);
        unsigned type = ELF64_ST_TYPE(syms[i].st_info);
        if (bind != STB_LOCAL || syms[i].st_name == 0 || syms[i].st_shndx == SHN_UNDEF || type == STT_FILE ||
            type == STT_SECTION) {
            continue;
        }
        const char *name = strtab + syms[i].st_name;
        if (name[0] == '\0' || strset_contains(&keep, name)) {
            continue;
        }
        syms[i].st_name = 0;
        anonymized++;
    }

    uint32_t *map = malloc((str_size ? str_size : 1) * sizeof(uint32_t));
    char *newtab = malloc(str_size ? str_size : 1);
    if (!map || !newtab) {
        free(map);
        free(newtab);
        elf_close(&elf);
        strset_free(&keep);
        die("out of memory");
    }
    for (size_t i = 0; i < str_size; i++) {
        map[i] = UINT32_MAX;
    }
    size_t new_size = 1;
    newtab[0] = '\0';
    if (str_size > 0) {
        map[0] = 0;
    }

    for (size_t i = 0; i < sym_count; i++) {
        uint32_t old = syms[i].st_name;
        if (old == 0) {
            continue;
        }
        if (old >= str_size) {
            syms[i].st_name = 0;
            continue;
        }
        if (map[old] != UINT32_MAX) {
            syms[i].st_name = map[old];
            continue;
        }
        size_t max = str_size - old;
        size_t len = strnlen(strtab + old, max);
        if (len == max || new_size + len + 1 > str_size) {
            syms[i].st_name = 0;
            continue;
        }
        map[old] = (uint32_t)new_size;
        memcpy(newtab + new_size, strtab + old, len + 1);
        syms[i].st_name = (uint32_t)new_size;
        new_size += len + 1;
    }

    memcpy(strtab, newtab, new_size);
    if (new_size < str_size) {
        memset(strtab + new_size, 0, str_size - new_size);
    }
    strsec->sh_size = new_size;

    printf("trimmed: anonymized=%zu strtab=%zu->%zu\n", anonymized, old_str_size, new_size);

    free(map);
    free(newtab);
    elf_close(&elf);
    strset_free(&keep);
    return 0;
}

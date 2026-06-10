#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdbool.h>
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

static strset_t g_keep;

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

static int cmp_strptr(const void *a, const void *b)
{
    const char *const *sa = a;
    const char *const *sb = b;
    return strcmp(*sa, *sb);
}

static bool ends_with(const char *s, const char *suffix)
{
    size_t a = strlen(s);
    size_t b = strlen(suffix);
    if (a < b) {
        return false;
    }
    return strcmp(s + a - b, suffix) == 0;
}

static bool is_ident_start(char c)
{
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool is_ident_char(char c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static char *read_whole_file(const char *path, size_t *out_size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < 0) {
        close(fd);
        return NULL;
    }
    size_t sz = (size_t)st.st_size;
    char *buf = malloc(sz + 1);
    if (!buf) {
        close(fd);
        return NULL;
    }
    size_t off = 0;
    while (off < sz) {
        ssize_t n = read(fd, buf + off, sz - off);
        if (n <= 0) {
            free(buf);
            close(fd);
            return NULL;
        }
        off += (size_t)n;
    }
    close(fd);
    buf[sz] = '\0';
    if (out_size) {
        *out_size = sz;
    }
    return buf;
}

static void parse_export_macros(const char *buf, size_t sz, strset_t *out)
{
    size_t i = 0;
    while (i < sz) {
        const char *needle = "EXPORT_SYMBOL";
        size_t nlen = strlen(needle);

        if (i + nlen >= sz || strncmp(buf + i, needle, nlen) != 0) {
            i++;
            continue;
        }
        if (i > 0 && is_ident_char(buf[i - 1])) {
            i++;
            continue;
        }

        size_t j = i + nlen;
        if (j + 4 <= sz && strncmp(buf + j, "_GPL", 4) == 0) {
            j += 4;
        }
        while (j < sz && isspace((unsigned char)buf[j])) {
            j++;
        }
        if (j >= sz || buf[j] != '(') {
            i++;
            continue;
        }
        j++;
        while (j < sz && isspace((unsigned char)buf[j])) {
            j++;
        }
        if (j >= sz || !is_ident_start(buf[j])) {
            i++;
            continue;
        }
        size_t st = j++;
        while (j < sz && is_ident_char(buf[j])) {
            j++;
        }
        size_t len = j - st;
        while (j < sz && isspace((unsigned char)buf[j])) {
            j++;
        }
        if (j >= sz || buf[j] != ')' || len == 0 || len >= 512) {
            i++;
            continue;
        }
        char sym[512];
        memcpy(sym, buf + st, len);
        sym[len] = '\0';
        strset_add(out, sym);
        i = j + 1;
    }
}

static int collect_src_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void)sb;
    (void)ftwbuf;
    if (typeflag != FTW_F) {
        return 0;
    }
    if (!ends_with(fpath, ".c") && !ends_with(fpath, ".h")) {
        return 0;
    }
    size_t sz = 0;
    char *buf = read_whole_file(fpath, &sz);
    if (!buf) {
        return 0;
    }
    parse_export_macros(buf, sz, &g_keep);
    free(buf);
    return 0;
}

static void collect_exported_from_source(const char *src_root, strset_t *keep)
{
    g_keep = *keep;
    if (nftw(src_root, collect_src_cb, 16, FTW_PHYS) != 0) {
        die_perror("nftw");
    }
    *keep = g_keep;
}

static bool elf_open_ro(const char *path, elf_file_t *elf)
{
    memset(elf, 0, sizeof(*elf));
    elf->fd = open(path, O_RDONLY);
    if (elf->fd < 0) {
        return false;
    }
    struct stat st;
    if (fstat(elf->fd, &st) < 0) {
        close(elf->fd);
        return false;
    }
    elf->size = (size_t)st.st_size;
    elf->data = mmap(NULL, elf->size, PROT_READ, MAP_PRIVATE, elf->fd, 0);
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

static void collect_exported_from_elf(const char *ko_path, strset_t *keep)
{
    elf_file_t elf;
    if (!elf_open_ro(ko_path, &elf)) {
        die("failed to open ELF");
    }
    Elf64_Shdr *symtab = find_symtab(&elf);
    if (!symtab) {
        elf_close(&elf);
        die("no .symtab");
    }
    Elf64_Shdr *strsec = &elf.shdr[symtab->sh_link];
    Elf64_Sym *syms = (Elf64_Sym *)((char *)elf.data + symtab->sh_offset);
    const char *strtab = (const char *)elf.data + strsec->sh_offset;
    size_t n = symtab->sh_size / sizeof(Elf64_Sym);
    for (size_t i = 0; i < n; i++) {
        if (syms[i].st_name == 0) {
            continue;
        }
        const char *name = strtab + syms[i].st_name;
        if (strncmp(name, "__ksymtab_", 10) == 0 && name[10] != '\0') {
            strset_add(keep, name + 10);
        }
    }
    elf_close(&elf);
}

static void split_csv_keep(strset_t *keep, const char *csv)
{
    if (!csv || csv[0] == '\0') {
        return;
    }
    const char *p = csv;
    while (*p) {
        while (*p == ',' || isspace((unsigned char)*p)) {
            p++;
        }
        const char *s = p;
        while (*p && *p != ',') {
            p++;
        }
        const char *e = p;
        while (e > s && isspace((unsigned char)e[-1])) {
            e--;
        }
        if (e > s) {
            size_t len = (size_t)(e - s);
            if (len < 512) {
                char tmp[512];
                memcpy(tmp, s, len);
                tmp[len] = '\0';
                strset_add(keep, tmp);
            }
        }
        if (*p == ',') {
            p++;
        }
    }
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
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) {
            *--e = '\0';
        }
        if (*p) {
            strset_add(keep, p);
        }
    }
    fclose(fp);
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s <input.ko> [options]\n"
            "Options:\n"
            "  --src-root <dir>\n"
            "  --keep <a,b,c>\n"
            "  --keep-file <path>\n"
            "  --no-default-keep\n"
            "  --out <path> (default stdout)\n",
            argv0);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }
    const char *input_ko = argv[1];
    const char *src_root = ".";
    const char *extra_keep = NULL;
    const char *keep_file = NULL;
    const char *out_path = NULL;
    bool default_keep = true;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--src-root") == 0 && i + 1 < argc) {
            src_root = argv[++i];
        } else if (strcmp(argv[i], "--keep") == 0 && i + 1 < argc) {
            extra_keep = argv[++i];
        } else if (strcmp(argv[i], "--keep-file") == 0 && i + 1 < argc) {
            keep_file = argv[++i];
        } else if (strcmp(argv[i], "--no-default-keep") == 0) {
            default_keep = false;
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    strset_t keep;
    strset_init(&keep);
    collect_exported_from_elf(input_ko, &keep);
    collect_exported_from_source(src_root, &keep);
    if (default_keep) {
        strset_add(&keep, "init_module");
        strset_add(&keep, "cleanup_module");
        strset_add(&keep, "__this_module");
    }
    split_csv_keep(&keep, extra_keep);
    if (keep_file) {
        add_keep_file(&keep, keep_file);
    }

    qsort(keep.items, keep.len, sizeof(char *), cmp_strptr);

    FILE *out = stdout;
    if (out_path) {
        out = fopen(out_path, "w");
        if (!out) {
            strset_free(&keep);
            die_perror("open output");
        }
    }
    for (size_t i = 0; i < keep.len; i++) {
        fprintf(out, "%s\n", keep.items[i]);
    }
    if (out_path) {
        fclose(out);
    }
    strset_free(&keep);
    return 0;
}

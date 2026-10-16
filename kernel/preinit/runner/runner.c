// SPDX-License-Identifier: GPL-2.0-only
/*
 * KernelSU preinit runner.
 *
 * The kernel swaps this static binary in for the first
 * "/system/bin/init selinux_setup" execve of PID 1. At that point first stage
 * mounts (/metadata, /system, /vendor, /proc, /sys, /dev) are ready, but no
 * SELinux policy is loaded and init.rc has not been parsed.
 *
 * PID 1 only forks a worker, waits for it with a deadline, kills whatever is
 * left and then execs the real init with the original argv/envp. Everything
 * that can fail runs in the worker, so a crash there only costs the preinit
 * actions, never the boot.
 */
#include "nolibc.h"

#include "uapi/supercall.h"

/* Overridable for host testing. */
#ifndef INIT_PATH
#define INIT_PATH "/system/bin/init"
#endif
#ifndef KSU_DIR_VOLD
#define KSU_DIR_VOLD "/metadata/vold/ksu"
#endif
#ifndef KSU_DIR_WATCHDOG
#define KSU_DIR_WATCHDOG "/metadata/watchdog/ksu"
#endif
#ifndef KSU_DIR_DEFAULT
#define KSU_DIR_DEFAULT "/metadata/ksu"
#endif
#ifndef KMSG_PATH
#define KMSG_PATH "/dev/kmsg"
#endif
#ifndef OWNER_UID
#define OWNER_UID 0
#endif
#define PREINIT_SUBDIR "preinit"
#define CONF_NAME "preinit.conf"
#define BOOTCOUNT_NAME ".preinit_bootcount"

#define TOTAL_TIMEOUT_MS 30000
#define EXEC_TIMEOUT_MS 5000
#define BOOTCOUNT_LIMIT 3

#define CONF_MAX (64 * 1024)
#define LINE_MAX_LEN 4096
#define MAX_TOKENS 128
#define MAX_MODULES 256
#define MODID_MAX 256
#define PATH_LEN 512
#define LOG_LINE_MAX 512

static char conf_buf[CONF_MAX + 1];
static char modules[MAX_MODULES][MODID_MAX];
static char params_buf[LINE_MAX_LEN * 2];

/* ---------------------------------------------------------------- logging */

/*
 * /dev/kmsg rate-limits writers per open file, so open it for every line
 * instead of keeping one fd around.
 */
static void kmsg_write(const char *buf, size_t len)
{
    int fd = open(KMSG_PATH, O_WRONLY | O_CLOEXEC);

    if (fd < 0)
        fd = 2;
    write(fd, buf, len);
    if (fd != 2)
        close(fd);
}

static __attribute__((format(printf, 1, 2))) void klog(const char *fmt, ...)
{
    char buf[LOG_LINE_MAX];
    va_list args;
    int n;

    n = snprintf(buf, sizeof(buf), "ksu_preinit: ");
    va_start(args, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n - 1, fmt, args);
    va_end(args);
    if (n > (int)sizeof(buf) - 2)
        n = sizeof(buf) - 2;
    buf[n++] = '\n';
    kmsg_write(buf, n);
}

/* ------------------------------------------------------------------ utils */

static long now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void sleep_ms(long ms)
{
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L };

    nanosleep(&ts, NULL);
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static bool safe_owner(const struct stat *st)
{
    return st->st_uid == OWNER_UID && !(st->st_mode & (S_IWGRP | S_IWOTH));
}

static bool is_dir(const char *path)
{
    struct stat st;

    return !lstat(path, &st) && S_ISDIR(st.st_mode);
}

/* Read up to size - 1 bytes and NUL terminate. Returns bytes read or -1. */
static ssize_t read_file(const char *path, char *buf, size_t size)
{
    ssize_t total = 0, n;
    int fd = open(path, O_RDONLY | O_CLOEXEC);

    if (fd < 0)
        return -1;
    while ((size_t)total < size - 1) {
        n = read(fd, buf + total, size - 1 - total);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        total += n;
    }
    close(fd);
    buf[total] = '\0';
    return total;
}

static int write_all(int fd, const char *buf, size_t len)
{
    ssize_t n;

    while (len) {
        n = write(fd, buf, len);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        buf += n;
        len -= n;
    }
    return 0;
}

/* ------------------------------------------------------------- boot mode */

/*
 * Find "androidboot.mode" in either bootconfig ("key = \"value\"") or cmdline
 * ("key=value") syntax and copy its value.
 */
static bool find_boot_mode(const char *buf, char *out, size_t size)
{
    static const char key[] = "androidboot.mode";
    const char *p = buf;
    size_t i;

    while ((p = strstr(p, key))) {
        const char *q = p + sizeof(key) - 1;

        if (p != buf && !is_space(p[-1]) && p[-1] != '\n') {
            p = q;
            continue;
        }
        while (is_space(*q))
            q++;
        if (*q != '=') {
            p = q;
            continue;
        }
        q++;
        while (is_space(*q))
            q++;
        if (*q == '"')
            q++;
        for (i = 0; i < size - 1 && *q && *q != '"' && *q != '\n' && !is_space(*q); i++)
            out[i] = *q++;
        out[i] = '\0';
        return true;
    }
    return false;
}

static bool is_charger_mode(void)
{
    static char buf[16384];
    char mode[32];

    if ((read_file("/proc/bootconfig", buf, sizeof(buf)) > 0 && find_boot_mode(buf, mode, sizeof(mode))) ||
        (read_file("/proc/cmdline", buf, sizeof(buf)) > 0 && find_boot_mode(buf, mode, sizeof(mode))))
        return !strcmp(mode, "charger");
    return false;
}

/* ------------------------------------------------------------- bootcount */

/*
 * Returns true when preinit may run. The counter is bumped and synced before
 * any action runs; ksud resets it on boot-completed. If it cannot be
 * persisted we refuse to run, otherwise a bad module could loop forever.
 *
 * Once BOOTCOUNT_LIMIT boots in a row did not complete, preinit is skipped
 * and the counter parks at BOOTCOUNT_LIMIT + 1. ksud leaves that value alone
 * on boot-completed, so preinit stays off until its content changes or the
 * user resets it, instead of looping again on the next boot.
 */
static bool bootcount_check_and_bump(const char *ksu_dir)
{
    char path[PATH_LEN], buf[16];
    long count = 0;
    bool run;
    int fd, n;

    snprintf(path, sizeof(path), "%s/%s", ksu_dir, BOOTCOUNT_NAME);
    if (read_file(path, buf, sizeof(buf)) > 0)
        count = atol(buf);
    if (count < 0)
        count = 0;

    run = count < BOOTCOUNT_LIMIT;
    if (!run) {
        klog("bootcount %ld >= %d, preinit disabled until reset", count, BOOTCOUNT_LIMIT);
        if (count > BOOTCOUNT_LIMIT)
            return false;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        klog("open %s failed: %d, skip all preinit", path, errno);
        return false;
    }
    n = snprintf(buf, sizeof(buf), "%ld\n", count + 1);
    if (write_all(fd, buf, n) || fsync(fd)) {
        klog("write %s failed: %d, skip all preinit", path, errno);
        close(fd);
        return false;
    }
    close(fd);

    fd = open(ksu_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
    klog("bootcount %ld -> %ld", count, count + 1);
    return run;
}

/* -------------------------------------------------------------- parsing */

/*
 * Split a line in place. Tokens are separated by blanks, double quotes group
 * blanks, \" and \\ are escapes. No variable expansion. Returns the token
 * count or -1 on syntax error.
 */
static int tokenize(char *line, char **argv, int max)
{
    char *r = line, *w = line;
    bool in_quote;
    int argc = 0;

    for (;;) {
        while (is_space(*r))
            r++;
        if (!*r)
            break;
        if (argc >= max - 1)
            return -1;
        argv[argc++] = w;
        in_quote = false;
        while (*r) {
            if (*r == '\\' && (r[1] == '"' || r[1] == '\\')) {
                *w++ = r[1];
                r += 2;
                continue;
            }
            if (*r == '"') {
                in_quote = !in_quote;
                r++;
                continue;
            }
            if (!in_quote && is_space(*r))
                break;
            *w++ = *r++;
        }
        if (in_quote)
            return -1;
        if (*r)
            r++;
        *w++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

/* ------------------------------------------------------------- actions */

struct module_ctx {
    const char *id;
    const char *dir;
};

/*
 * KernelSU driver fd, installed lazily on first use via the reboot-magic hook
 * (same mechanism ksud uses). Cached across modules; -1 once we know it is
 * unavailable so we only probe the kernel once.
 */
static int ksu_driver_fd = -1;
static int ksu_driver_probed;

static int ksu_get_driver_fd(void)
{
    int fd = -1;

    if (ksu_driver_probed)
        return ksu_driver_fd;
    ksu_driver_probed = 1;

    /* The KSU reboot kprobe writes the installed anon fd back into *arg. */
    sys_reboot(KSU_INSTALL_MAGIC1, KSU_INSTALL_MAGIC2, 0, &fd);
    if (fd >= 0)
        ksu_driver_fd = fd;
    else
        klog("ksu driver fd unavailable, insmod falls back to finit_module");
    return ksu_driver_fd;
}

static int open_checked(const struct module_ctx *m, const char *path)
{
    struct stat st;
    int fd = open(path, O_RDONLY | O_CLOEXEC);

    if (fd < 0) {
        klog("[%s] open %s failed: %d", m->id, path, errno);
        return -1;
    }
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || !safe_owner(&st)) {
        klog("[%s] %s: not a root owned, non group/other writable regular file", m->id, path);
        close(fd);
        return -1;
    }
    return fd;
}

static bool has_space(const char *s)
{
    for (; *s; s++)
        if (is_space(*s))
            return true;
    return false;
}

static bool append(size_t *len, const char *s, size_t n)
{
    if (n >= sizeof(params_buf) - *len)
        return false;
    memcpy(params_buf + *len, s, n);
    *len += n;
    params_buf[*len] = '\0';
    return true;
}

/*
 * Join module parameters with blanks. Tokens that contain blanks are quoted
 * again (key="a b") so the kernel's parse_args sees a single argument.
 */
static int build_params(char **argv, int argc)
{
    size_t len = 0;
    int i;

    params_buf[0] = '\0';
    for (i = 0; i < argc; i++) {
        const char *a = argv[i], *eq = strchr(a, '=');
        size_t key = eq ? (size_t)(eq - a + 1) : 0;
        bool ok;

        if (strchr(a, '"'))
            return -1;
        ok = !i || append(&len, " ", 1);
        if (!has_space(a))
            ok = ok && append(&len, a, strlen(a));
        else
            ok = ok && append(&len, a, key) && append(&len, "\"", 1) && append(&len, a + key, strlen(a + key)) &&
                 append(&len, "\"", 1);
        if (!ok)
            return -1;
    }
    return 0;
}

static int do_insmod(const struct module_ctx *m, int argc, char **argv)
{
    int fd, ret, drv;

    if (argc < 2) {
        klog("[%s] insmod: missing path", m->id);
        return -1;
    }
    if (build_params(argv + 2, argc - 2)) {
        klog("[%s] insmod %s: bad parameters", m->id, argv[1]);
        return -1;
    }
    fd = open_checked(m, argv[1]);
    if (fd < 0)
        return -1;

    /*
     * Prefer the KSU insmod supercall: it resolves the module's undefined
     * (non-exported) symbols against kallsyms before loading, which plain
     * finit_module cannot do. The module binary is mmap'd from the validated
     * fd and handed to the kernel directly. Fall back to finit_module when the
     * KSU driver is unavailable or the supercall itself fails.
     */
    drv = ksu_get_driver_fd();
    if (drv >= 0) {
        struct stat st;

        if (!fstat(fd, &st) && st.st_size > 0) {
            void *image = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

            if (image != MAP_FAILED) {
                struct ksu_insmod_cmd cmd = {
                    .data = (unsigned long)image,
                    .data_len = (unsigned long long)st.st_size,
                    .params = (unsigned long)params_buf,
                    .flags = 0,
                };

                ret = sys_ioctl(drv, KSU_IOCTL_INSMOD, (unsigned long)&cmd);
                munmap(image, st.st_size);
                if (ret == 0) {
                    close(fd);
                    klog("[%s] insmod %s ok (ksu)", m->id, argv[1]);
                    return 0;
                }
                klog("[%s] insmod %s via ksu failed: %d, trying finit_module", m->id, argv[1], ret);
            }
        }
    }

    ret = syscall(__NR_finit_module, fd, params_buf, 0);
    close(fd);
    if (ret && errno == EEXIST) {
        klog("[%s] insmod %s: already loaded", m->id, argv[1]);
        return 0;
    }
    if (ret) {
        klog("[%s] insmod %s failed: %d", m->id, argv[1], errno);
        return -1;
    }
    klog("[%s] insmod %s ok", m->id, argv[1]);
    return 0;
}

static int do_write(const struct module_ctx *m, int argc, char **argv)
{
    int fd, ret;

    if (argc != 3) {
        klog("[%s] write: usage: write <path> <content>", m->id);
        return -1;
    }
    fd = open(argv[1], O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        klog("[%s] write %s: open failed: %d", m->id, argv[1], errno);
        return -1;
    }
    ret = write_all(fd, argv[2], strlen(argv[2]));
    if (ret)
        klog("[%s] write %s failed: %d", m->id, argv[1], errno);
    close(fd);
    return ret;
}

struct line_fwd {
    const char *id;
    char buf[LOG_LINE_MAX + 1];
    size_t len;
};

static void fwd_flush(struct line_fwd *f)
{
    char out[LOG_LINE_MAX + MODID_MAX + 32];
    int n;

    if (!f->len)
        return;
    f->buf[f->len] = '\0';
    n = snprintf(out, sizeof(out), "ksu_preinit[%s]: %s\n", f->id, f->buf);
    if (n >= (int)sizeof(out)) {
        n = sizeof(out) - 1;
        out[n - 1] = '\n';
    }
    kmsg_write(out, n);
    f->len = 0;
}

static void fwd_feed(struct line_fwd *f, const char *data, size_t len)
{
    for (; len; data++, len--) {
        if (*data == '\n') {
            fwd_flush(f);
            continue;
        }
        if (f->len == sizeof(f->buf) - 1)
            fwd_flush(f);
        f->buf[f->len++] = *data;
    }
}

/* Forward the child's output until it exits or times out. Returns its wait status or -1 on timeout. */
static int wait_forward(const struct module_ctx *m, pid_t pid, int out_fd, long timeout_ms)
{
    static struct line_fwd fwd;
    long deadline = now_ms() + timeout_ms;
    struct pollfd pfd = { .fd = out_fd, .events = POLLIN };
    bool exited = false, eof = false;
    int status = 0, ready;
    char buf[1024];
    ssize_t n;

    fwd.id = m->id;
    fwd.len = 0;
    for (;;) {
        if (!eof) {
            /* After exit, only drain what is buffered: grandchildren may hold the pipe. */
            ready = poll(&pfd, 1, exited ? 0 : 20);
            if (ready > 0) {
                n = read(out_fd, buf, sizeof(buf));
                if (n > 0)
                    fwd_feed(&fwd, buf, n);
                else if (n == 0 || errno != EINTR)
                    eof = true;
            } else if (exited) {
                break;
            }
        } else if (!exited) {
            sleep_ms(20);
        }
        if (!exited && waitpid(pid, &status, WNOHANG) == pid)
            exited = true;
        if (exited && eof)
            break;
        if (now_ms() >= deadline) {
            if (exited)
                break;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            fwd_flush(&fwd);
            return -1;
        }
    }
    fwd_flush(&fwd);
    return status;
}

static int do_exec(const struct module_ctx *m, int argc, char **argv)
{
    char env_modid[MODID_MAX + 8], env_dir[PATH_LEN + 16];
    char *envp[] = {
        "KSU=true", "KSU_PREINIT=1", env_modid, env_dir, "PATH=/system/bin:/vendor/bin", NULL,
    };
    long timeout = EXEC_TIMEOUT_MS;
    int idx = 1, pfd[2], status, fd;
    pid_t pid;

    if (argc > 1 && !strcmp(argv[1], "-t")) {
        if (argc < 3 || (timeout = atol(argv[2])) <= 0) {
            klog("[%s] exec: bad -t value", m->id);
            return -1;
        }
        idx = 3;
    }
    if (idx >= argc) {
        klog("[%s] exec: missing program", m->id);
        return -1;
    }
    fd = open_checked(m, argv[idx]);
    if (fd < 0)
        return -1;
    close(fd);

    snprintf(env_modid, sizeof(env_modid), "MODID=%s", m->id);
    snprintf(env_dir, sizeof(env_dir), "PREINIT_DIR=%s", m->dir);

    if (pipe2(pfd, O_CLOEXEC)) {
        klog("[%s] exec: pipe failed: %d", m->id, errno);
        return -1;
    }
    pid = fork();
    if (pid < 0) {
        klog("[%s] exec: fork failed: %d", m->id, errno);
        close(pfd[0]);
        close(pfd[1]);
        return -1;
    }
    if (pid == 0) {
        fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (fd >= 0)
            dup2(fd, 0);
        else
            close(0);
        dup2(pfd[1], 1);
        dup2(pfd[1], 2);
        execve(argv[idx], argv + idx, envp);
        _exit(127);
    }
    close(pfd[1]);
    status = wait_forward(m, pid, pfd[0], timeout);
    close(pfd[0]);

    if (status == -1) {
        klog("[%s] exec %s: timed out after %ld ms", m->id, argv[idx], timeout);
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        klog("[%s] exec %s ok", m->id, argv[idx]);
        return 0;
    }
    if (WIFEXITED(status))
        klog("[%s] exec %s exited with %d", m->id, argv[idx], WEXITSTATUS(status));
    else
        klog("[%s] exec %s killed by signal %d", m->id, argv[idx], WTERMSIG(status));
    return -1;
}

/* Returns 0 on success, -1 when the action failed. */
static int run_action(const struct module_ctx *m, int argc, char **argv)
{
    if (!strcmp(argv[0], "insmod"))
        return do_insmod(m, argc, argv);
    if (!strcmp(argv[0], "exec"))
        return do_exec(m, argc, argv);
    if (!strcmp(argv[0], "write"))
        return do_write(m, argc, argv);
    klog("[%s] unknown action: %s", m->id, argv[0]);
    return -1;
}

/* ------------------------------------------------------------- modules */

static void run_module(const char *preinit_dir, const char *id)
{
    char dir[PATH_LEN], *argv[MAX_TOKENS], *line, *next;
    struct module_ctx m = { .id = id, .dir = dir };
    struct stat st;
    ssize_t total = 0, n;
    int fd, argc, lineno = 0;
    size_t len;

    snprintf(dir, sizeof(dir), "%s/%s", preinit_dir, id);
    if (lstat(dir, &st) || !S_ISDIR(st.st_mode) || !safe_owner(&st)) {
        klog("[%s] skip: directory not root owned or group/other writable", id);
        return;
    }
    if (chdir(dir)) {
        klog("[%s] skip: chdir failed: %d", id, errno);
        return;
    }
    fd = open(CONF_NAME, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        klog("[%s] skip: no %s", id, CONF_NAME);
        return;
    }
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || !safe_owner(&st) || st.st_size > CONF_MAX) {
        klog("[%s] skip: bad %s", id, CONF_NAME);
        close(fd);
        return;
    }
    while (total < CONF_MAX) {
        n = read(fd, conf_buf + total, CONF_MAX - total);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        total += n;
    }
    close(fd);
    conf_buf[total] = '\0';

    klog("[%s] start", id);
    for (line = conf_buf; line && *line; line = next) {
        bool optional;

        lineno++;
        next = strchr(line, '\n');
        if (next)
            *next++ = '\0';
        len = strlen(line);
        if (len > LINE_MAX_LEN) {
            klog("[%s] line %d: too long, abort module", id, lineno);
            return;
        }
        while (is_space(*line))
            line++;
        if (*line == '#' || !*line)
            continue;

        argc = tokenize(line, argv, MAX_TOKENS);
        if (argc < 0) {
            klog("[%s] line %d: syntax error, abort module", id, lineno);
            return;
        }
        if (!argc)
            continue;
        len = strlen(argv[0]);
        optional = len > 1 && argv[0][len - 1] == '?';
        if (optional)
            argv[0][len - 1] = '\0';

        if (run_action(&m, argc, argv) && !optional) {
            klog("[%s] line %d: %s failed, abort module", id, lineno, argv[0]);
            return;
        }
    }
    klog("[%s] done", id);
}

static int collect_modules(const char *preinit_dir)
{
    char buf[4096];
    struct linux_dirent64 *d;
    int fd, n, count = 0, i, j, off;

    fd = open(preinit_dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0)
        return 0;
    while ((n = getdents64(fd, (void *)buf, sizeof(buf))) > 0) {
        for (off = 0; off < n; off += d->d_reclen) {
            d = (struct linux_dirent64 *)(buf + off);
            if (d->d_name[0] == '.' || strlen(d->d_name) >= MODID_MAX)
                continue;
            if (count == MAX_MODULES) {
                klog("too many modules, ignoring %s", d->d_name);
                continue;
            }
            strcpy(modules[count++], d->d_name);
        }
    }
    close(fd);

    /* Insertion sort: run order is the lexical order of module ids. */
    for (i = 1; i < count; i++) {
        char tmp[MODID_MAX];

        strcpy(tmp, modules[i]);
        for (j = i; j > 0 && strcmp(modules[j - 1], tmp) > 0; j--)
            strcpy(modules[j], modules[j - 1]);
        strcpy(modules[j], tmp);
    }
    return count;
}

static int worker_main(void)
{
    char preinit_dir[PATH_LEN];
    const char *ksu_dir;
    struct stat st;
    int count, i;

    if (is_charger_mode()) {
        klog("charger mode, skip");
        return 0;
    }

    if (is_dir(KSU_DIR_VOLD "/" PREINIT_SUBDIR))
        ksu_dir = KSU_DIR_VOLD;
    else if (is_dir(KSU_DIR_WATCHDOG "/" PREINIT_SUBDIR))
        ksu_dir = KSU_DIR_WATCHDOG;
    else if (is_dir(KSU_DIR_DEFAULT "/" PREINIT_SUBDIR))
        ksu_dir = KSU_DIR_DEFAULT;
    else
        return 0;
    snprintf(preinit_dir, sizeof(preinit_dir), "%s/%s", ksu_dir, PREINIT_SUBDIR);

    if (lstat(preinit_dir, &st) || !safe_owner(&st)) {
        klog("%s is not root owned or is group/other writable, skip", preinit_dir);
        return 0;
    }

    count = collect_modules(preinit_dir);
    if (!count)
        return 0;
    if (!bootcount_check_and_bump(ksu_dir))
        return 0;

    for (i = 0; i < count; i++)
        run_module(preinit_dir, modules[i]);
    return 0;
}

/* ----------------------------------------------------------------- PID 1 */

static void supervise(pid_t worker)
{
    long deadline = now_ms() + TOTAL_TIMEOUT_MS;
    pid_t r;
    int i;

    for (;;) {
        r = waitpid(worker, NULL, WNOHANG);
        if (r == worker || (r < 0 && errno != EINTR))
            break;
        if (now_ms() >= deadline) {
            klog("timed out after %d ms, killing worker", TOTAL_TIMEOUT_MS);
            kill(-worker, SIGKILL);
            kill(worker, SIGKILL);
            waitpid(worker, NULL, 0);
            break;
        }
        sleep_ms(10);
    }

    /*
     * Nothing may outlive preinit: once policy loads, leftovers would keep
     * running in the kernel domain. Orphans are reparented to us, reap them.
     */
    kill(-worker, SIGKILL);
    for (i = 0; i < 100; i++) {
        r = waitpid(-1, NULL, WNOHANG);
        if (r > 0)
            continue;
        if (r < 0 && errno != EINTR)
            break;
        sleep_ms(10);
    }
}

int main(int argc, char **argv, char **envp)
{
    pid_t pid;

    (void)argc;
    pid = fork();
    if (pid == 0) {
        setpgid(0, 0);
        _exit(worker_main());
    }
    if (pid > 0) {
        setpgid(pid, pid);
        supervise(pid);
    } else {
        klog("fork failed: %d", errno);
    }

    execve(INIT_PATH, argv, envp);
    klog("exec " INIT_PATH " failed: %d", errno);
    return 127;
}

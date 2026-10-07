#include <jni.h>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;

static void throw_io(JNIEnv *env, int error) {
    env->ThrowNew(env->FindClass("java/io/IOException"), strerror(error));
}

// Only async-signal-safe operations may run between fork and exec.
static void child_error(int fd) {
    int error = errno;
    TEMP_FAILURE_RETRY(write(fd, &error, sizeof(error)));
    _exit(127);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_me_weishu_kernelsu_terminal_PtyProcess_nativeStart(
        JNIEnv *env, jobject, jobjectArray arguments, jint columns, jint rows) {
    std::vector<std::string> strings;
    for (int i = 0; i < env->GetArrayLength(arguments); ++i) {
        auto argument = static_cast<jbyteArray>(env->GetObjectArrayElement(arguments, i));
        std::string text(env->GetArrayLength(argument), '\0');
        env->GetByteArrayRegion(argument, 0, text.size(), reinterpret_cast<jbyte *>(text.data()));
        env->DeleteLocalRef(argument);
        if (env->ExceptionCheck()) return nullptr;
        if (text.find('\0') != std::string::npos) {
            throw_io(env, EINVAL);
            return nullptr;
        }
        strings.push_back(std::move(text));
    }
    if (strings.empty() || columns < 1 || rows < 1) {
        throw_io(env, EINVAL);
        return nullptr;
    }
    // Resolve PATH before forking, while allocation is safe.
    std::string executable = strings[0];
    if (executable.find('/') == std::string::npos) {
        const char *path_env = getenv("PATH");
        std::string path = path_env ? path_env : "/system/bin";
        size_t begin = 0;
        do {
            size_t end = path.find(':', begin);
            std::string candidate = path.substr(begin, end - begin) + "/" + executable;
            if (access(candidate.c_str(), X_OK) == 0) {
                executable = candidate;
                break;
            }
            if (end == std::string::npos) break;
            begin = end + 1;
        } while (true);
    }
    std::vector<char *> argv;
    for (auto &argument : strings) argv.push_back(argument.data());
    argv.push_back(nullptr);
    std::vector<std::string> environment;
    for (char **entry = environ; *entry; ++entry) {
        if (strncmp(*entry, "TERM=", 5) && strncmp(*entry, "COLORTERM=", 10)) {
            environment.emplace_back(*entry);
        }
    }
    environment.emplace_back("TERM=xterm-256color");
    environment.emplace_back("COLORTERM=truecolor");
    std::vector<char *> envp;
    for (auto &entry : environment) envp.push_back(entry.data());
    envp.push_back(nullptr);

    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    winsize size = {static_cast<unsigned short>(rows), static_cast<unsigned short>(columns), 0, 0};
    if (master == -1) {
        throw_io(env, errno);
        return nullptr;
    }
    char slave_name[PATH_MAX];
    if (grantpt(master) == -1 || unlockpt(master) == -1 ||
        ptsname_r(master, slave_name, sizeof(slave_name)) != 0 ||
        ioctl(master, TIOCSWINSZ, &size) == -1) {
        int error = errno;
        close(master);
        throw_io(env, error);
        return nullptr;
    }
    int slave = open(slave_name, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (slave == -1) {
        int error = errno;
        close(master);
        throw_io(env, error);
        return nullptr;
    }
    int errors[2];
    if (pipe2(errors, O_CLOEXEC) == -1) {
        int error = errno;
        close(master);
        close(slave);
        throw_io(env, error);
        return nullptr;
    }
    const long max_fd = sysconf(_SC_OPEN_MAX);
    const char *program = executable.c_str();
    char **args = argv.data();
    char **vars = envp.data();
    pid_t pid = fork();
    if (pid == 0) {
        close(errors[0]);
        close(master);
        if (setsid() == -1 || ioctl(slave, TIOCSCTTY, 0) == -1) child_error(errors[1]);
        for (int fd = 0; fd < 3; ++fd) {
            if (dup2(slave, fd) == -1) child_error(errors[1]);
        }
        // Do not leak the app's Binder, sockets, or KernelSU driver into commands.
        if (syscall(__NR_close_range, 3u, static_cast<unsigned>(errors[1] - 1), 0u) == -1) {
            for (int fd = 3; fd < errors[1]; ++fd) close(fd);
        }
        if (syscall(__NR_close_range, static_cast<unsigned>(errors[1] + 1), UINT_MAX, 0u) == -1) {
            for (long fd = errors[1] + 1; fd < max_fd; ++fd) close(static_cast<int>(fd));
        }
        sigset_t signals;
        sigemptyset(&signals);
        sigprocmask(SIG_SETMASK, &signals, nullptr);
        struct sigaction action = {};
        action.sa_handler = SIG_DFL;
        sigemptyset(&action.sa_mask);
        for (int signal = 1; signal < NSIG; ++signal) sigaction(signal, &action, nullptr);
        execve(program, args, vars);
        child_error(errors[1]);
    }
    int error = errno;
    close(slave);
    close(errors[1]);
    if (pid == -1) {
        close(errors[0]);
        close(master);
        throw_io(env, error);
        return nullptr;
    }
    int exec_error = 0;
    ssize_t count = TEMP_FAILURE_RETRY(read(errors[0], &exec_error, sizeof(exec_error)));
    close(errors[0]);
    if (count != 0) {
        close(master);
        TEMP_FAILURE_RETRY(waitpid(pid, nullptr, 0));
        throw_io(env, count > 0 ? exec_error : EIO);
        return nullptr;
    }
    jint values[] = {master, pid};
    jintArray result = env->NewIntArray(2);
    if (!result) {
        close(master);
        TEMP_FAILURE_RETRY(waitpid(pid, nullptr, 0));
        return nullptr;
    }
    env->SetIntArrayRegion(result, 0, 2, values);
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_me_weishu_kernelsu_terminal_PtyProcess_nativeRead(JNIEnv *env, jobject, jint fd, jbyteArray bytes) {
    pollfd descriptor = {fd, POLLIN, 0};
    int ready = TEMP_FAILURE_RETRY(poll(&descriptor, 1, 100));
    if (ready == 0) return 0;
    char buffer[8192];
    int capacity = env->GetArrayLength(bytes);
    if (capacity > static_cast<int>(sizeof(buffer))) capacity = sizeof(buffer);
    ssize_t count = ready < 0 ? -1 : TEMP_FAILURE_RETRY(read(fd, buffer, capacity));
    if (count < 0) {
        // Linux PTYs report EIO once the last slave is closed.
        if (errno == EIO) return -1;
        throw_io(env, errno);
        return -1;
    }
    if (count == 0) return -1;
    env->SetByteArrayRegion(bytes, 0, count, reinterpret_cast<jbyte *>(buffer));
    return count;
}

extern "C" JNIEXPORT void JNICALL
Java_me_weishu_kernelsu_terminal_PtyProcess_nativeResize(JNIEnv *, jobject, jint fd, jint columns, jint rows) {
    winsize size = {static_cast<unsigned short>(rows), static_cast<unsigned short>(columns), 0, 0};
    ioctl(fd, TIOCSWINSZ, &size);
}

extern "C" JNIEXPORT void JNICALL
Java_me_weishu_kernelsu_terminal_PtyProcess_nativeClose(JNIEnv *, jobject, jint fd) {
    close(fd);
}

extern "C" JNIEXPORT jint JNICALL
Java_me_weishu_kernelsu_terminal_PtyProcess_nativeWait(JNIEnv *env, jobject, jint pid, jboolean no_hang) {
    int status;
    pid_t result = TEMP_FAILURE_RETRY(waitpid(pid, &status, no_hang ? WNOHANG : 0));
    if (result == 0) return -1;
    if (result < 0) {
        throw_io(env, errno);
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

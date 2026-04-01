#include <string>
#include <dlfcn.h>
#include <cstdlib>

// Skip drop privileges
// https://cs.android.com/android/platform/superproject/+/android-latest-release:packages/modules/adb/daemon/main.cpp;l=123;drc=3b74954ec50836e0c8faeed1877fefb1dc2de006
extern "C"
[[gnu::visibility("default"), gnu::used]]
// NOLINTNEXTLINE(bugprone-reserved-identifier)
int __android_log_is_debuggable() {
    return 1;
}

// This function has a C++ return type, so we can't use extern "C".
// We can't define simply it in android::base namespace, otherwise we will get wrong mangled name (string is in ndk namespace)
// Therefore, we use asm to define its mangled name instead.
// https://stackoverflow.com/a/33321513
[[gnu::visibility("default"), gnu::used]]
std::string GetProperty(const std::string &key, const std::string &default_value)
asm ("_ZN7android4base11GetPropertyERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_");

// Enable adb root by default, without run `adb root` command explicitly or setprop
// https://cs.android.com/android/platform/superproject/+/android-latest-release:packages/modules/adb/daemon/main.cpp;l=84;drc=3b74954ec50836e0c8faeed1877fefb1dc2de006
[[gnu::visibility("default"), gnu::used]]
std::string GetProperty(const std::string &key, const std::string &default_value) {
    static constexpr const auto kSymbolName = "_ZN7android4base11GetPropertyERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_";
    static auto orig_fn = (decltype(GetProperty) *) dlsym(RTLD_NEXT, kSymbolName);

    if (key == "service.adb.root") {
        // allow drop privilege by prop, but don't drop by default
        return orig_fn(key, "1");
    }

    return orig_fn(key, default_value);
}

// skip setcon
extern "C"
[[gnu::visibility("default"), gnu::used]]
int selinux_android_setcon(const char *con) {
    return 0;
}

[[gnu::used, gnu::constructor(0)]]
void Init() {
    unsetenv("LD_PRELOAD");
    unsetenv("LD_LIBRARY_PATH");
    std::string path = getenv("PATH") ?: "";
    if (!path.empty()) {
        path += ":/data/adb/ksu/bin";
    } else {
        path += "/data/adb/ksu/bin";
    }
    setenv("PATH", path.c_str(), 1);
}

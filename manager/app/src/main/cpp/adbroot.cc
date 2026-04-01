#include <string>
#include <dlfcn.h>

// Skip drop privileges
// https://cs.android.com/android/platform/superproject/+/android-latest-release:packages/modules/adb/daemon/main.cpp;l=123;drc=3b74954ec50836e0c8faeed1877fefb1dc2de006
extern "C"
[[gnu::visibility("default"), gnu::used]]
// NOLINTNEXTLINE(bugprone-reserved-identifier)
int __android_log_is_debuggable() {
    return 1;
}

// Enable adb root by default, without run `adb root` command explicitly or setprop
// https://cs.android.com/android/platform/superproject/+/android-latest-release:packages/modules/adb/daemon/main.cpp;l=84;drc=3b74954ec50836e0c8faeed1877fefb1dc2de006
[[gnu::visibility("default"), gnu::used]]
std::string GetProperty(const std::string& key, const std::string& default_value) {
    static constexpr const auto kSymbolName = "_ZN7android4base11GetPropertyERKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEES9_";
    static auto orig_fn = (decltype(GetProperty)*) dlsym(RTLD_NEXT, kSymbolName);

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

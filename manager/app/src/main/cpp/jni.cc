#include <jni.h>

#include <sys/prctl.h>

#include <android/log.h>

#include "ksu.h"

#define LOG_TAG "KernelSu"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_becomeManager(JNIEnv *env, jclass clazz, jstring pkg) {
    auto cpkg = env->GetStringUTFChars(pkg, nullptr);
    auto result = become_manager(cpkg);
    env->ReleaseStringUTFChars(pkg, cpkg);
    return result;
}

extern "C"
JNIEXPORT jint JNICALL
Java_me_weishu_kernelsu_Natives_getVersion(JNIEnv *env, jclass clazz) {
    return get_version();
}

extern "C"
JNIEXPORT jintArray JNICALL
Java_me_weishu_kernelsu_Natives_getAllowList(JNIEnv *env, jclass clazz) {
    int uids[1024];
    int size = 0;
    bool result = get_allow_list(uids, &size);
    LOGD("getAllowList: %d, size: %d", result, size);
    if (result) {
        auto array = env->NewIntArray(size);
        env->SetIntArrayRegion(array, 0, size, uids);
        return array;
    }
    return env->NewIntArray(0);
}

extern "C"
JNIEXPORT jintArray JNICALL
Java_me_weishu_kernelsu_Natives_getDenyList(JNIEnv *env, jclass clazz) {
    int uids[1024];
    int size = 0;
    bool result = get_deny_list(uids, &size);
    if (result) {
        // success!
        auto array = env->NewIntArray(size);
        env->SetIntArrayRegion(array, 0, size, uids);
        return array;
    }
    return env->NewIntArray(0);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_allowRoot(JNIEnv *env, jclass clazz, jint uid, jboolean allow) {
    return allow_su(uid, allow);
}



extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_isSafeMode(JNIEnv *env, jclass clazz) {
    return is_safe_mode();
}
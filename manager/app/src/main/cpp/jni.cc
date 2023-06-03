#include <jni.h>

#include <sys/prctl.h>

#include <android/log.h>
#include <cstring>

#include "ksu.h"

#define LOG_TAG "KernelSU"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_becomeManager(JNIEnv *env, jobject, jstring pkg) {
    auto cpkg = env->GetStringUTFChars(pkg, nullptr);
    auto result = become_manager(cpkg);
    env->ReleaseStringUTFChars(pkg, cpkg);
    return result;
}

extern "C"
JNIEXPORT jint JNICALL
Java_me_weishu_kernelsu_Natives_getVersion(JNIEnv *env, jobject) {
    return get_version();
}

extern "C"
JNIEXPORT jintArray JNICALL
Java_me_weishu_kernelsu_Natives_getAllowList(JNIEnv *env, jobject) {
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
Java_me_weishu_kernelsu_Natives_allowRoot(JNIEnv *env, jobject clazz, jint uid, jboolean allow) {
    return allow_su(uid, allow);
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_isSafeMode(JNIEnv *env, jclass clazz) {
    return is_safe_mode();
}

static void fillIntArray(JNIEnv* env, jobject list, int *data, int count) {
    auto cls = env->GetObjectClass(list);
    auto add = env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
    auto integerCls = env->FindClass("java/lang/Integer");
    auto constructor = env->GetMethodID(integerCls, "<init>", "(I)V");
    for (int i = 0; i < count; ++i) {
        auto integer = env->NewObject(integerCls, constructor, data[i]);
        env->CallBooleanMethod(list, add, integer);
    }
}

static int getListSize(JNIEnv *env, jobject list) {
    auto cls = env->GetObjectClass(list);
    auto size = env->GetMethodID(cls, "size", "()I");
    return env->CallIntMethod(list, size);
}

static void fillArrayWithList(JNIEnv* env, jobject list, int *data, int count) {
    auto cls = env->GetObjectClass(list);
    auto get = env->GetMethodID(cls, "get", "(I)Ljava/lang/Object;");
    auto integerCls = env->FindClass("java/lang/Integer");
    auto intValue = env->GetMethodID(integerCls, "intValue", "()I");
    for (int i = 0; i < count; ++i) {
        auto integer = env->CallObjectMethod(list, get, i);
        data[i] = env->CallIntMethod(integer, intValue);
    }
}

extern "C"
JNIEXPORT jobject JNICALL
Java_me_weishu_kernelsu_Natives_getAppProfile(JNIEnv *env, jobject, jstring pkg, jint uid) {
    if (env->GetStringLength(pkg) > KSU_MAX_PACKAGE_NAME) {
        return nullptr;
    }

    p_key_t key = {};
    auto cpkg = env->GetStringUTFChars(pkg, nullptr);
    strcpy(key, cpkg);
    env->ReleaseStringUTFChars(pkg, cpkg);

    app_profile profile = {};
    strcpy(profile.key, key);
    profile.current_uid = uid;

    // set default value, don't allow root and use default profile!
    profile.allow_su = false;
    profile.non_root_profile.use_default = true;

    if (!get_app_profile(key, &profile)) {
        return nullptr;
    }

    auto cls = env->FindClass("me/weishu/kernelsu/Natives$Profile");
    auto constructor = env->GetMethodID(cls, "<init>", "()V");
    auto obj = env->NewObject(cls, constructor);
    auto keyField = env->GetFieldID(cls, "name", "Ljava/lang/String;");
    auto currentUidField = env->GetFieldID(cls, "currentUid", "I");
    auto allowSuField = env->GetFieldID(cls, "allowSu", "Z");

    auto rootUseDefaultField = env->GetFieldID(cls, "rootUseDefault", "Z");
    auto rootTemplateField = env->GetFieldID(cls, "rootTemplate", "Ljava/lang/String;");

    auto uidField = env->GetFieldID(cls, "uid", "I");
    auto gidField = env->GetFieldID(cls, "gid", "I");
    auto groupsField = env->GetFieldID(cls, "groups", "Ljava/util/List;");
    auto capabilitiesField = env->GetFieldID(cls, "capabilities", "Ljava/util/List;");
    auto domainField = env->GetFieldID(cls, "context", "Ljava/lang/String;");
    // auto namespacesField = env->GetFieldID(cls, "namespace", "I");

    auto nonRootUseDefaultField = env->GetFieldID(cls, "nonRootUseDefault", "Z");
    auto umountModulesField = env->GetFieldID(cls, "umountModules", "Z");

    env->SetObjectField(obj, keyField, env->NewStringUTF(profile.key));
    env->SetIntField(obj, currentUidField, profile.current_uid);

    auto allowSu = profile.allow_su;

    if (allowSu) {
        env->SetBooleanField(obj, rootUseDefaultField, (jboolean) profile.root_profile.use_default);
        if (strlen(profile.root_profile.template_name) > 0) {
            env->SetObjectField(obj, rootTemplateField,
                                env->NewStringUTF(profile.root_profile.template_name));
        }

        env->SetIntField(obj, uidField, profile.root_profile.uid);
        env->SetIntField(obj, gidField, profile.root_profile.gid);

        jobject groupList = env->GetObjectField(obj, groupsField);
        fillIntArray(env, groupList, profile.root_profile.groups, profile.root_profile.groups_count);

        jobject capList = env->GetObjectField(obj, capabilitiesField);
        fillIntArray(env, capList, profile.root_profile.capabilities, 2);

        env->SetObjectField(obj, domainField,
                            env->NewStringUTF(profile.root_profile.selinux_domain));
        // env->SetIntField(obj, namespacesField, profile.root_profile.namespaces);
        env->SetBooleanField(obj, allowSuField, profile.allow_su);
    } else {
        env->SetBooleanField(obj, nonRootUseDefaultField,
                             (jboolean) profile.non_root_profile.use_default);
        env->SetBooleanField(obj, umountModulesField, profile.non_root_profile.umount_modules);
    }

    return obj;
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_setAppProfile(JNIEnv *env, jobject clazz, jobject profile) {
    auto cls = env->FindClass("me/weishu/kernelsu/Natives$Profile");

    auto keyField = env->GetFieldID(cls, "name", "Ljava/lang/String;");
    auto currentUidField = env->GetFieldID(cls, "currentUid", "I");
    auto allowSuField = env->GetFieldID(cls, "allowSu", "Z");

    auto rootUseDefaultField = env->GetFieldID(cls, "rootUseDefault", "Z");
    auto rootTemplateField = env->GetFieldID(cls, "rootTemplate", "Ljava/lang/String;");

    auto uidField = env->GetFieldID(cls, "uid", "I");
    auto gidField = env->GetFieldID(cls, "gid", "I");
    auto groupsField = env->GetFieldID(cls, "groups", "Ljava/util/List;");
    auto capabilitiesField = env->GetFieldID(cls, "capabilities", "Ljava/util/List;");
    auto domainField = env->GetFieldID(cls, "context", "Ljava/lang/String;");
//    auto namespacesField = env->GetFieldID(cls, "namespaces", "I");

    auto nonRootUseDefaultField = env->GetFieldID(cls, "nonRootUseDefault", "Z");
    auto umountModulesField = env->GetFieldID(cls, "umountModules", "Z");

    auto key = env->GetObjectField(profile, keyField);
    if (!key) {
        return false;
    }
    if (env->GetStringLength((jstring) key) > KSU_MAX_PACKAGE_NAME) {
        return false;
    }

    auto cpkg = env->GetStringUTFChars((jstring) key, nullptr);
    p_key_t p_key = {};
    strcpy(p_key, cpkg);
    env->ReleaseStringUTFChars((jstring) key, cpkg);

    auto currentUid = env->GetIntField(profile, currentUidField);

    auto uid = env->GetIntField(profile, uidField);
    auto gid = env->GetIntField(profile, gidField);
    auto groups = env->GetObjectField(profile, groupsField);
    auto capabilities = env->GetObjectField(profile, capabilitiesField);
    auto domain = env->GetObjectField(profile, domainField);
    auto allowSu = env->GetBooleanField(profile, allowSuField);
    auto umountModules = env->GetBooleanField(profile, umountModulesField);

    app_profile p = {};
    strcpy(p.key, p_key);

    p.allow_su = allowSu;
    p.current_uid = currentUid;

    if (allowSu) {
        p.root_profile.use_default = env->GetBooleanField(profile, rootUseDefaultField);
        auto templateName = env->GetObjectField(profile, rootTemplateField);
        if (templateName) {
            auto ctemplateName = env->GetStringUTFChars((jstring) templateName, nullptr);
            strcpy(p.root_profile.template_name, ctemplateName);
            env->ReleaseStringUTFChars((jstring) templateName, ctemplateName);
        }

        p.root_profile.uid = uid;
        p.root_profile.gid = gid;

        int groups_count = getListSize(env, groups);
        p.root_profile.groups_count = groups_count;
        fillArrayWithList(env, groups, p.root_profile.groups, groups_count);

        fillArrayWithList(env, capabilities, p.root_profile.capabilities, 2);

        auto cdomain = env->GetStringUTFChars((jstring) domain, nullptr);
        strcpy(p.root_profile.selinux_domain, cdomain);
        env->ReleaseStringUTFChars((jstring) domain, cdomain);

        // p.root_profile.namespaces = env->GetIntField(profile, namespacesField);
    } else {
        p.non_root_profile.use_default = env->GetBooleanField(profile, nonRootUseDefaultField);
        p.non_root_profile.umount_modules = umountModules;
    }

    return set_app_profile(&p);
}
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
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_isSafeMode(JNIEnv *env, jclass clazz) {
    return is_safe_mode();
}

extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_isLkmMode(JNIEnv *env, jclass clazz) {
    return is_lkm_mode();
}

static void fillIntArray(JNIEnv *env, jobject list, int *data, int count) {
    auto cls = env->GetObjectClass(list);
    auto add = env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
    auto integerCls = env->FindClass("java/lang/Integer");
    auto constructor = env->GetMethodID(integerCls, "<init>", "(I)V");
    for (int i = 0; i < count; ++i) {
        auto integer = env->NewObject(integerCls, constructor, data[i]);
        env->CallBooleanMethod(list, add, integer);
    }
}

static void addIntToList(JNIEnv *env, jobject list, int ele) {
    auto cls = env->GetObjectClass(list);
    auto add = env->GetMethodID(cls, "add", "(Ljava/lang/Object;)Z");
    auto integerCls = env->FindClass("java/lang/Integer");
    auto constructor = env->GetMethodID(integerCls, "<init>", "(I)V");
    auto integer = env->NewObject(integerCls, constructor, ele);
    env->CallBooleanMethod(list, add, integer);
}

static uint64_t capListToBits(JNIEnv *env, jobject list) {
    auto cls = env->GetObjectClass(list);
    auto get = env->GetMethodID(cls, "get", "(I)Ljava/lang/Object;");
    auto size = env->GetMethodID(cls, "size", "()I");
    auto listSize = env->CallIntMethod(list, size);
    auto integerCls = env->FindClass("java/lang/Integer");
    auto intValue = env->GetMethodID(integerCls, "intValue", "()I");
    uint64_t result = 0;
    for (int i = 0; i < listSize; ++i) {
        auto integer = env->CallObjectMethod(list, get, i);
        int data = env->CallIntMethod(integer, intValue);

        if (cap_valid(data)) {
            result |= (1ULL << data);
        }
    }

    return result;
}

static int getListSize(JNIEnv *env, jobject list) {
    auto cls = env->GetObjectClass(list);
    auto size = env->GetMethodID(cls, "size", "()I");
    return env->CallIntMethod(list, size);
}

static void fillArrayWithList(JNIEnv *env, jobject list, int *data, int count) {
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
    profile.version = KSU_APP_PROFILE_VER;

    strcpy(profile.key, key);
    profile.current_uid = uid;

    bool useDefaultProfile = !get_app_profile(key, &profile);

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
    auto namespacesField = env->GetFieldID(cls, "namespace", "I");

    auto nonRootUseDefaultField = env->GetFieldID(cls, "nonRootUseDefault", "Z");
    auto umountModulesField = env->GetFieldID(cls, "umountModules", "Z");

    env->SetObjectField(obj, keyField, env->NewStringUTF(profile.key));
    env->SetIntField(obj, currentUidField, profile.current_uid);

    if (useDefaultProfile) {
        // no profile found, so just use default profile:
        // don't allow root and use default profile!
        LOGD("use default profile for: %s, %d", key, uid);

        // allow_su = false
        // non root use default = true
        env->SetBooleanField(obj, allowSuField, false);
        env->SetBooleanField(obj, nonRootUseDefaultField, true);

        return obj;
    }

    auto allowSu = profile.allow_su;

    if (allowSu) {
        env->SetBooleanField(obj, rootUseDefaultField, (jboolean) profile.rp_config.use_default);
        if (strlen(profile.rp_config.template_name) > 0) {
            env->SetObjectField(obj, rootTemplateField,
                                env->NewStringUTF(profile.rp_config.template_name));
        }

        env->SetIntField(obj, uidField, profile.rp_config.profile.uid);
        env->SetIntField(obj, gidField, profile.rp_config.profile.gid);

        jobject groupList = env->GetObjectField(obj, groupsField);
        int groupCount = profile.rp_config.profile.groups_count;
        if (groupCount > KSU_MAX_GROUPS) {
            LOGD("kernel group count too large: %d???", groupCount);
            groupCount = KSU_MAX_GROUPS;
        }
        fillIntArray(env, groupList, profile.rp_config.profile.groups, groupCount);

        jobject capList = env->GetObjectField(obj, capabilitiesField);
        for (int i = 0; i <= CAP_LAST_CAP; i++) {
            if (profile.rp_config.profile.capabilities.effective & (1ULL << i)) {
                addIntToList(env, capList, i);
            }
        }

        env->SetObjectField(obj, domainField,
                            env->NewStringUTF(profile.rp_config.profile.selinux_domain));
        env->SetIntField(obj, namespacesField, profile.rp_config.profile.namespaces);
        env->SetBooleanField(obj, allowSuField, profile.allow_su);
    } else {
        env->SetBooleanField(obj, nonRootUseDefaultField,
                             (jboolean) profile.nrp_config.use_default);
        env->SetBooleanField(obj, umountModulesField, profile.nrp_config.profile.umount_modules);
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
    auto namespacesField = env->GetFieldID(cls, "namespace", "I");

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
    p.version = KSU_APP_PROFILE_VER;

    strcpy(p.key, p_key);
    p.allow_su = allowSu;
    p.current_uid = currentUid;

    if (allowSu) {
        p.rp_config.use_default = env->GetBooleanField(profile, rootUseDefaultField);
        auto templateName = env->GetObjectField(profile, rootTemplateField);
        if (templateName) {
            auto ctemplateName = env->GetStringUTFChars((jstring) templateName, nullptr);
            strcpy(p.rp_config.template_name, ctemplateName);
            env->ReleaseStringUTFChars((jstring) templateName, ctemplateName);
        }

        p.rp_config.profile.uid = uid;
        p.rp_config.profile.gid = gid;

        int groups_count = getListSize(env, groups);
        if (groups_count > KSU_MAX_GROUPS) {
            LOGD("groups count too large: %d", groups_count);
            return false;
        }
        p.rp_config.profile.groups_count = groups_count;
        fillArrayWithList(env, groups, p.rp_config.profile.groups, groups_count);

        p.rp_config.profile.capabilities.effective = capListToBits(env, capabilities);

        auto cdomain = env->GetStringUTFChars((jstring) domain, nullptr);
        strcpy(p.rp_config.profile.selinux_domain, cdomain);
        env->ReleaseStringUTFChars((jstring) domain, cdomain);

        p.rp_config.profile.namespaces = env->GetIntField(profile, namespacesField);
    } else {
        p.nrp_config.use_default = env->GetBooleanField(profile, nonRootUseDefaultField);
        p.nrp_config.profile.umount_modules = umountModules;
    }

    return set_app_profile(&p);
}
extern "C"
JNIEXPORT jboolean JNICALL
Java_me_weishu_kernelsu_Natives_uidShouldUmount(JNIEnv *env, jobject thiz, jint uid) {
    return uid_should_umount(uid);
}
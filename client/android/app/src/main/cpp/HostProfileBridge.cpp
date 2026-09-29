#include <jni.h>

#include <string>
#include <vector>

#include "JniEnv.h"

#include "deskhubp/ffi/HostProfileFfi.h"

namespace {

constexpr const char* kHostProfileClass = "com/deskhub/app/NativeClient$HostProfile";

using deskhubj::FromJString;

jstring NewString(JNIEnv* env, const char* text) {
    return env->NewStringUTF(text ? text : "");
}

jobject NewHostProfile(JNIEnv* env, jclass cls, jmethodID ctor, const DHHostProfile& row) {
    jstring alias = NewString(env, row.alias);
    jstring endpoint = NewString(env, row.endpoint);
    jstring identity = NewString(env, row.identity);
    jstring fingerprint = NewString(env, row.fingerprint);
    jobject item = env->NewObject(cls, ctor, alias, endpoint, identity, fingerprint);
    env->DeleteLocalRef(fingerprint);
    env->DeleteLocalRef(identity);
    env->DeleteLocalRef(endpoint);
    env->DeleteLocalRef(alias);
    return item;
}

}

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_com_deskhub_app_NativeClient_nativeHostProfiles(JNIEnv* env, jobject) {
    jclass cls = env->FindClass(kHostProfileClass);
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor) return nullptr;

    const int total = dh_host_profiles(nullptr, 0);
    if (total == DH_HOST_PROFILES_UNREADABLE) return nullptr;
    std::vector<DHHostProfile> rows(size_t(total > 0 ? total : 0));
    const int count = rows.empty() ? 0 : dh_host_profiles(rows.data(), int(rows.size()));
    if (count == DH_HOST_PROFILES_UNREADABLE) return nullptr;

    jobjectArray arr = env->NewObjectArray(jsize(count), cls, nullptr);
    for (int i = 0; i < count && arr; ++i) {
        jobject item = NewHostProfile(env, cls, ctor, rows[size_t(i)]);
        env->SetObjectArrayElement(arr, jsize(i), item);
        env->DeleteLocalRef(item);
    }
    return arr;
}

JNIEXPORT jint JNICALL
Java_com_deskhub_app_NativeClient_nativeHostProfileRemove(JNIEnv* env, jobject,
    jstring aliasStr) {
    return jint(dh_host_profile_remove(FromJString(env, aliasStr).c_str()));
}

JNIEXPORT jstring JNICALL
Java_com_deskhub_app_NativeClient_nativeHostProfileErrorText(JNIEnv* env, jobject, jint error) {
    return NewString(env, dh_host_profile_error_text(DHHostProfileError(error)));
}

JNIEXPORT jint JNICALL
Java_com_deskhub_app_NativeClient_nativeHostTrustNew(JNIEnv* env, jobject, jstring addressStr,
    jstring fingerprintStr) {
    const std::string address = FromJString(env, addressStr);
    const std::string fingerprint = FromJString(env, fingerprintStr);
    return jint(dh_host_trust_new(address.c_str(), fingerprint.c_str()));
}

JNIEXPORT jstring JNICALL
Java_com_deskhub_app_NativeClient_nativeTrustNewHostPrompt(JNIEnv* env, jobject,
    jstring addressStr, jstring fingerprintStr) {
    const std::string address = FromJString(env, addressStr);
    const std::string fingerprint = FromJString(env, fingerprintStr);
    const int length = dh_trust_new_host_prompt(address.c_str(), fingerprint.c_str(), nullptr, 0);
    std::vector<char> prompt(size_t(length > 0 ? length : 0) + 1, '\0');
    dh_trust_new_host_prompt(address.c_str(), fingerprint.c_str(), prompt.data(),
        int(prompt.size()));
    return NewString(env, prompt.data());
}
}

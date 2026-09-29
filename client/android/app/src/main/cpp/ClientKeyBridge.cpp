#include <jni.h>

#include <algorithm>
#include <string>
#include <vector>

#include "JniEnv.h"

#include "deskhubp/ffi/ClientKeyFfi.h"

namespace {

constexpr const char* kClientKeyClass = "com/deskhub/app/NativeClient$ClientKey";

using deskhubj::FromJString;

jstring NewString(JNIEnv* env, const char* text) {
    return env->NewStringUTF(text ? text : "");
}

jobject NewClientKey(JNIEnv* env, jclass cls, jmethodID ctor, const DHClientKey& key) {
    jstring name = NewString(env, key.name);
    jstring fingerprint = NewString(env, key.fingerprint);
    jobject item = env->NewObject(cls, ctor, name, fingerprint);
    env->DeleteLocalRef(fingerprint);
    env->DeleteLocalRef(name);
    return item;
}

}

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_com_deskhub_app_NativeClient_nativeClientKeys(JNIEnv* env, jobject) {
    jclass cls = env->FindClass(kClientKeyClass);
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!ctor) return nullptr;

    const int total = dh_client_keys(nullptr, 0);
    std::vector<DHClientKey> keys(size_t(total > 0 ? total : 0));
    const int count = keys.empty() ? 0 : dh_client_keys(keys.data(), int(keys.size()));

    jobjectArray arr = env->NewObjectArray(jsize(count), cls, nullptr);
    for (int i = 0; i < count && arr; ++i) {
        jobject item = NewClientKey(env, cls, ctor, keys[size_t(i)]);
        env->SetObjectArrayElement(arr, jsize(i), item);
        env->DeleteLocalRef(item);
    }
    return arr;
}

JNIEXPORT jstring JNICALL
Java_com_deskhub_app_NativeClient_nativeClientPublicKey(JNIEnv* env, jobject, jstring nameStr) {
    const std::string name = FromJString(env, nameStr);
    std::vector<char> text(DH_PUBLIC_KEY_TEXT_CAP, '\0');
    dh_client_public_key(name.c_str(), text.data(), int(text.size()));
    return NewString(env, text.data());
}

JNIEXPORT jint JNICALL
Java_com_deskhub_app_NativeClient_nativeClientKeyGenerate(JNIEnv* env, jobject, jstring nameStr) {
    return jint(dh_client_key_generate(FromJString(env, nameStr).c_str()));
}

JNIEXPORT jint JNICALL
Java_com_deskhub_app_NativeClient_nativeClientKeyImport(JNIEnv* env, jobject, jstring nameStr,
    jstring privateKeyStr, jstring passphraseStr) {
    const std::string name = FromJString(env, nameStr);
    std::string privateKey = FromJString(env, privateKeyStr);
    std::string passphrase = FromJString(env, passphraseStr);
    const DHClientKeyError error =
        dh_client_key_import(name.c_str(), privateKey.c_str(), passphrase.c_str());
    std::fill(privateKey.begin(), privateKey.end(), '\0');
    std::fill(passphrase.begin(), passphrase.end(), '\0');
    return jint(error);
}

JNIEXPORT jstring JNICALL
Java_com_deskhub_app_NativeClient_nativeClientKeyErrorText(JNIEnv* env, jobject, jint error) {
    return NewString(env, dh_client_key_error_text(DHClientKeyError(error)));
}
}

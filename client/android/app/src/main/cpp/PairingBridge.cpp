#include <jni.h>

#include <string>
#include <vector>

#include "JniEnv.h"

#include "deskhubp/ffi/DevicesFfi.h"
#include "deskhubp/ffi/PairingFfi.h"

namespace {

constexpr const char* kAccessRequestClass = "com/deskhub/app/NativeClient$AccessRequest";
constexpr int kAccessRequestCapacity = 32;
constexpr int kInviteAddressCapacity = 64;

using deskhubj::FromJString;

jstring NewString(JNIEnv* env, const char* text) {
    return env->NewStringUTF(text ? text : "");
}

jobject NewAccessRequest(JNIEnv* env, jclass cls, jmethodID ctor, const DHAccessRequest& row) {
    jstring name = NewString(env, row.name);
    jstring address = NewString(env, row.address);
    jstring shortKey = NewString(env, row.shortKey);
    jstring fingerprint = NewString(env, row.fingerprint);
    jstring requestedAt = NewString(env, row.requestedAt);
    jobject item = env->NewObject(cls, ctor, name, address, shortKey, fingerprint, requestedAt);
    env->DeleteLocalRef(requestedAt);
    env->DeleteLocalRef(fingerprint);
    env->DeleteLocalRef(shortKey);
    env->DeleteLocalRef(address);
    env->DeleteLocalRef(name);
    return item;
}

}

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_deskhub_app_NativeClient_nativePairingInvite(JNIEnv* env, jobject, jint port,
    jstring bindIpStr) {
    const std::string bindIp = FromJString(env, bindIpStr);
    const bool portUsable = port >= 1 && port <= 65535;
    char invite[DH_PAIRING_INVITE_CAP] = {};
    if (portUsable) dh_pairing_invite(uint16_t(port), bindIp.c_str(), invite, int(sizeof(invite)));
    return NewString(env, invite);
}

JNIEXPORT void JNICALL
Java_com_deskhub_app_NativeClient_nativePairingRevoke(JNIEnv*, jobject) {
    dh_pairing_revoke();
}

JNIEXPORT jbyteArray JNICALL
Java_com_deskhub_app_NativeClient_nativeQrEncode(JNIEnv* env, jobject, jstring textStr) {
    const std::string text = FromJString(env, textStr);
    std::vector<uint8_t> modules(size_t(DH_QR_MAX_SIZE) * size_t(DH_QR_MAX_SIZE), 0);
    const int size = dh_qr_encode(text.c_str(), modules.data(), int(modules.size()));
    const jsize count = size > 0 ? jsize(size * size) : 0;
    jbyteArray arr = env->NewByteArray(count);
    if (arr && count > 0)
        env->SetByteArrayRegion(arr, 0, count, reinterpret_cast<const jbyte*>(modules.data()));
    return arr;
}

JNIEXPORT jstring JNICALL
Java_com_deskhub_app_NativeClient_nativePairingInviteAddress(JNIEnv* env, jobject,
    jstring inviteStr) {
    const std::string invite = FromJString(env, inviteStr);
    char address[kInviteAddressCapacity] = {};
    dh_pairing_invite_address(invite.c_str(), address, int(sizeof(address)));
    return NewString(env, address);
}

JNIEXPORT jobjectArray JNICALL
Java_com_deskhub_app_NativeClient_nativeAccessRequests(JNIEnv* env, jobject) {
    jclass cls = env->FindClass(kAccessRequestClass);
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;)V");
    if (!ctor) return nullptr;

    DHAccessRequest rows[kAccessRequestCapacity];
    const int count = dh_access_requests(rows, kAccessRequestCapacity);

    jobjectArray arr = env->NewObjectArray(jsize(count), cls, nullptr);
    for (int i = 0; i < count && arr; ++i) {
        jobject item = NewAccessRequest(env, cls, ctor, rows[i]);
        env->SetObjectArrayElement(arr, jsize(i), item);
        env->DeleteLocalRef(item);
    }
    return arr;
}

JNIEXPORT jboolean JNICALL
Java_com_deskhub_app_NativeClient_nativeAccessApprove(JNIEnv* env, jobject,
    jstring fingerprintStr) {
    return dh_access_approve(FromJString(env, fingerprintStr).c_str()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_deskhub_app_NativeClient_nativeAccessDeny(JNIEnv* env, jobject, jstring fingerprintStr) {
    return dh_access_deny(FromJString(env, fingerprintStr).c_str()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_deskhub_app_NativeClient_nativeAccessRequestsGeneration(JNIEnv*, jobject) {
    return jlong(dh_access_requests_generation());
}
}

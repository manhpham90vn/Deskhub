#include "HostBridge.h"

#include <android/native_window_jni.h>

#include <cstring>
#include <string>
#include <vector>

#include "JniEnv.h"
#include "capture/ScreenCapture.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/ffi/ShareFfi.h"
#include "deskhubp/ffi/ClientFfi.h"
#include "deskhubp/ffi/DiscoveryFfi.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/net/UdpSocket.h"

namespace {

constexpr const char* kNativeHostClass = "com/deskhub/app/NativeHost";
constexpr const char* kHostRowClass = "com/deskhub/app/NativeHost$HostRow";
constexpr int kMaxHostRows = 64;

jclass g_nativeHostClass = nullptr;
jmethodID g_projectionReady = nullptr;
jmethodID g_attachSurface = nullptr;
jmethodID g_detachSurface = nullptr;

jstring NewString(JNIEnv* env, const char* text) {
    return env->NewStringUTF(text ? text : "");
}

}

bool RegisterHostBridge(JNIEnv* env) {
    jclass cls = env->FindClass(kNativeHostClass);
    if (!cls) return false;
    g_nativeHostClass = static_cast<jclass>(env->NewGlobalRef(cls));
    g_projectionReady = env->GetStaticMethodID(g_nativeHostClass, "projectionReady", "()Z");
    g_attachSurface =
        env->GetStaticMethodID(g_nativeHostClass, "attachSurface", "(Landroid/view/Surface;II)Z");
    g_detachSurface = env->GetStaticMethodID(g_nativeHostClass, "detachSurface", "()V");
    return g_projectionReady && g_attachSurface && g_detachSurface;
}

namespace deskhubj {

bool HostProjectionReady() {
    if (!g_nativeHostClass) return false;
    AttachedEnv attached;
    if (!attached) return false;
    return attached.env()->CallStaticBooleanMethod(g_nativeHostClass, g_projectionReady) ==
           JNI_TRUE;
}

bool AttachHostSurface(ANativeWindow* window, uint32_t width, uint32_t height) {
    if (!g_nativeHostClass || !window) return false;
    AttachedEnv attached;
    if (!attached) return false;
    JNIEnv* env = attached.env();

    jobject surface = ANativeWindow_toSurface(env, window);
    if (!surface) return false;

    const jboolean ok = env->CallStaticBooleanMethod(g_nativeHostClass, g_attachSurface, surface,
        jint(width), jint(height));
    env->DeleteLocalRef(surface);
    return ok == JNI_TRUE;
}

void DetachHostSurface() {
    if (!g_nativeHostClass) return;
    AttachedEnv attached;
    if (!attached) return;
    attached.env()->CallStaticVoidMethod(g_nativeHostClass, g_detachSurface);
}

}

extern "C" {

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeSetScreenSize(JNIEnv* env, jobject,
    jint width, jint height, jstring name) {
    deskhubp::SetLocalDisplay(uint32_t(width), uint32_t(height),
        deskhubj::FromJString(env, name));
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeHost_nativeStart(JNIEnv* env, jobject,
    jint fps, jint bitrateMbps, jint maxDim, jint port, jstring passcode, jstring transferDir) {
    DHShareSource source{};
    if (dh_share_list_sources(&source, 1) != 1) return JNI_FALSE;

    const std::string dir = deskhubj::FromJString(env, transferDir);
    if (!dir.empty()) dh_set_transfer_dir(dir.c_str());

    const std::string code = deskhubj::FromJString(env, passcode);
    return dh_share_start(&source, 1, uint32_t(fps), uint32_t(bitrateMbps), uint32_t(maxDim),
               uint16_t(port), false, code.c_str(), false, !dir.empty())
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeHost_nativeStartFilesOnly(JNIEnv* env,
    jobject, jstring transferDir) {
    const std::string dir = deskhubj::FromJString(env, transferDir);
    if (dir.empty()) return JNI_FALSE;
    dh_set_transfer_dir(dir.c_str());

    const DHUiSettings settings = dh_settings_load();
    return dh_share_start(nullptr, 0, 0, 0, 0, uint16_t(settings.port), false, settings.passcode,
               false, true)
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeStopFilesOnly(JNIEnv*, jobject) {
    dh_share_stop();
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeHost_nativeFilesActive(JNIEnv*, jobject) {
    return dh_share_files_active() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeOfferAudio(JNIEnv* env, jobject,
    jshortArray pcm, jint samples) {
    if (!pcm || samples <= 0) return;
    jshort* frame = env->GetShortArrayElements(pcm, nullptr);
    if (!frame) return;
    dh_share_offer_audio(reinterpret_cast<const int16_t*>(frame), samples);
    env->ReleaseShortArrayElements(pcm, frame, JNI_ABORT);
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeHost_nativeAudioRunning(JNIEnv*, jobject) {
    return dh_share_audio_running() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeStop(JNIEnv*, jobject) {
    dh_share_stop();
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeHost_nativeRunning(JNIEnv*, jobject) {
    return dh_share_running() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeLastError(JNIEnv* env, jobject) {
    return NewString(env, dh_share_last_error());
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeLocalAddresses(JNIEnv* env,
    jobject) {
    return NewString(env, dh_share_local_addresses());
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeKickViewer(JNIEnv* env, jobject,
    jint sourceId, jstring viewerAddr) {
    dh_share_kick_viewer(uint8_t(sourceId), deskhubj::FromJString(env, viewerAddr).c_str());
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeProjectionStopped(JNIEnv*, jobject) {
    ScreenCapture::ReportProjectionStopped();
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeDisplayResized(JNIEnv*, jobject,
    jint width, jint height) {
    ScreenCapture::ReportDisplaySize(uint32_t(width), uint32_t(height));
}

JNIEXPORT jobjectArray JNICALL Java_com_deskhub_app_NativeHost_nativeHostRows(JNIEnv* env,
    jobject) {
    std::vector<DHHostRow> rows(kMaxHostRows);
    const int count = dh_share_rows(rows.data(), kMaxHostRows);

    jclass cls = env->FindClass(kHostRowClass);
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>",
        "(ZIZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;"
        "Ljava/lang/String;)V");
    if (!ctor) return nullptr;

    jobjectArray out = env->NewObjectArray(count, cls, nullptr);
    if (!out) return nullptr;

    for (int i = 0; i < count; ++i) {
        const DHHostRow& row = rows[size_t(i)];
        jstring viewerAddr = NewString(env, row.viewerAddr);
        jstring source = NewString(env, row.source);
        jstring size = NewString(env, row.size);
        jstring viewers = NewString(env, row.viewers);
        jstring client = NewString(env, row.client);
        jstring capture = NewString(env, row.capture);
        jstring send = NewString(env, row.send);
        jstring mbps = NewString(env, row.mbps);
        jstring rtt = NewString(env, row.rtt);

        jobject item = env->NewObject(cls, ctor, row.viewer ? JNI_TRUE : JNI_FALSE,
            jint(row.sourceId), row.online ? JNI_TRUE : JNI_FALSE, viewerAddr, source, size,
            viewers, client, capture, send, mbps, rtt);
        env->SetObjectArrayElement(out, i, item);

        env->DeleteLocalRef(item);
        env->DeleteLocalRef(viewerAddr);
        env->DeleteLocalRef(source);
        env->DeleteLocalRef(size);
        env->DeleteLocalRef(viewers);
        env->DeleteLocalRef(client);
        env->DeleteLocalRef(capture);
        env->DeleteLocalRef(send);
        env->DeleteLocalRef(mbps);
        env->DeleteLocalRef(rtt);
    }
    return out;
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeSharingStatus(JNIEnv* env, jobject,
    jint port, jstring passcode, jboolean screen, jboolean files) {
    char buf[320];
    const std::string code = deskhubj::FromJString(env, passcode);
    dh_sharing_status(uint16_t(port), code.c_str(), false, screen == JNI_TRUE, false,
        files == JNI_TRUE, buf, int(sizeof(buf)));
    return NewString(env, buf);
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeIdleStatus(JNIEnv* env, jobject,
    jint port) {
    char buf[160];
    dh_idle_host_status(uint16_t(port), buf, int(sizeof(buf)));
    return NewString(env, buf);
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativePasscodeDisplay(JNIEnv* env,
    jobject, jstring passcode) {
    char buf[32];
    const std::string code = deskhubj::FromJString(env, passcode);
    dh_passcode_display(code.c_str(), buf, int(sizeof(buf)));
    return NewString(env, buf);
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativePasscode(JNIEnv* env, jobject) {
    const DHUiSettings settings = dh_settings_load();
    return NewString(env, settings.passcode);
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeSavePasscode(JNIEnv* env, jobject,
    jstring passcode) {
    const DHUiSettings settings = dh_settings_load();
    const std::string code = deskhubj::FromJString(env, passcode);
    dh_settings_save(settings.fps, settings.bitrateMbps, settings.maxDim, settings.port,
        settings.allowInput, settings.clientControl, code.c_str());
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeBindIp(JNIEnv* env, jobject) {
    char buf[64];
    dh_bind_ip(buf, int(sizeof(buf)));
    return NewString(env, buf);
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeSetBindIp(JNIEnv* env, jobject,
    jstring ip) {
    dh_set_bind_ip(deskhubj::FromJString(env, ip).c_str());
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeClipOffer(JNIEnv* env, jobject,
    jstring text) {
    dh_share_clip_offer(deskhubj::FromJString(env, text).c_str());
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeClipTake(JNIEnv* env, jobject) {
    char buf[deskhub::kMaxClipboardTextBytes + 1];
    dh_share_clip_take(buf, int(sizeof(buf)));
    return NewString(env, buf);
}

JNIEXPORT jobjectArray JNICALL Java_com_deskhub_app_NativeHost_nativeTakePairingRequests(
    JNIEnv* env, jobject) {
    jclass cls = env->FindClass("com/deskhub/app/NativeHost$PairingRequest");
    if (!cls) return nullptr;
    jmethodID ctor = env->GetMethodID(cls, "<init>", "(JLjava/lang/String;Ljava/lang/String;)V");
    if (!ctor) return nullptr;

    DHPairingRequest requests[16];
    const int count =
        dh_share_take_pairing_requests(requests, int(sizeof(requests) / sizeof(requests[0])));

    DHPairedDevice paired[128];
    const int pairedCount = dh_paired_devices(paired, int(sizeof(paired) / sizeof(paired[0])));
    const auto alreadyPaired = [&paired, pairedCount](const char* shortKey) {
        for (int p = 0; p < pairedCount; ++p)
            if (std::strcmp(paired[p].shortKey, shortKey) == 0) return true;
        return false;
    };

    std::vector<DHPairingRequest> ask;
    for (int i = 0; i < count; ++i) {
        if (alreadyPaired(requests[i].shortKey)) {
            dh_share_answer_pairing(requests[i].addrPacked, true);
        } else {
            ask.push_back(requests[i]);
        }
    }

    jobjectArray out = env->NewObjectArray(jsize(ask.size()), cls, nullptr);
    for (size_t i = 0; i < ask.size() && out; ++i) {
        const DHPairingRequest& request = ask[i];
        const std::string addrText = NetAddr::Unpack(request.addrPacked).ToString();
        char body[512];
        dh_pairing_request_body(request.name, addrText.c_str(), request.shortKey, body,
            int(sizeof(body)));
        jstring shortKey = NewString(env, request.shortKey);
        jstring bodyText = NewString(env, body);
        jobject item =
            env->NewObject(cls, ctor, jlong(request.addrPacked), shortKey, bodyText);
        env->SetObjectArrayElement(out, jsize(i), item);
        env->DeleteLocalRef(item);
        env->DeleteLocalRef(bodyText);
        env->DeleteLocalRef(shortKey);
    }
    return out;
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeHost_nativeAnswerPairing(JNIEnv*, jobject,
    jlong addrPacked, jboolean allow) {
    dh_share_answer_pairing(uint64_t(addrPacked), allow == JNI_TRUE);
}

JNIEXPORT jintArray JNICALL Java_com_deskhub_app_NativeHost_nativeShareDefaults(JNIEnv* env,
    jobject) {
    const DHUiSettings stored = dh_settings_load();
    const DHShareDefaults defaults = dh_share_default_options();
    const jint values[3] = {
        jint(stored.fps ? stored.fps : defaults.fps),
        jint(stored.bitrateMbps ? stored.bitrateMbps : defaults.bitrateMbps),
        jint(stored.maxDim ? stored.maxDim : defaults.maxDim),
    };
    jintArray out = env->NewIntArray(3);
    if (out) env->SetIntArrayRegion(out, 0, 3, values);
    return out;
}
}

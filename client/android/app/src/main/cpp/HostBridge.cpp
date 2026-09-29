#include "HostBridge.h"

#include <android/native_window_jni.h>

#include <string>
#include <vector>

#include "JniEnv.h"
#include "capture/ScreenCapture.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/ffi/DevicesFfi.h"
#include "deskhubp/ffi/SettingsFfi.h"
#include "deskhubp/ffi/ShareFfi.h"
#include "deskhubp/media/DisplayEnum.h"

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
    jint fps, jint bitrateMbps, jint maxDim, jint port, jstring transferDir) {
    DHShareSource source{};
    if (dh_share_list_sources(&source, 1) != 1) return JNI_FALSE;

    const std::string dir = deskhubj::FromJString(env, transferDir);
    if (!dir.empty()) dh_set_transfer_dir(dir.c_str());

    return dh_share_start(&source, 1, uint32_t(fps), uint32_t(bitrateMbps), uint32_t(maxDim),
               uint16_t(port), false, false, !dir.empty())
               ? JNI_TRUE
               : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeHost_nativeStartFilesOnly(JNIEnv* env,
    jobject, jstring transferDir) {
    const std::string dir = deskhubj::FromJString(env, transferDir);
    if (dir.empty()) return JNI_FALSE;
    dh_set_transfer_dir(dir.c_str());

    const DHUiSettings settings = dh_settings_load();
    return dh_share_start(nullptr, 0, 0, 0, 0, uint16_t(settings.port), false, false, true)
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
    jint port, jboolean screen, jboolean files) {
    char buf[320];
    dh_sharing_status(uint16_t(port), false, screen == JNI_TRUE, false,
        files == JNI_TRUE, buf, int(sizeof(buf)));
    return NewString(env, buf);
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeHost_nativeIdleStatus(JNIEnv* env, jobject,
    jint port) {
    char buf[160];
    dh_idle_host_status(uint16_t(port), buf, int(sizeof(buf)));
    return NewString(env, buf);
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

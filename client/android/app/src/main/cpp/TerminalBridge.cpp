#include <jni.h>

#include <cstdint>
#include <string>
#include <vector>

#include "JniEnv.h"

#include "deskhubp/ffi/TerminalFfi.h"

namespace {

DHTermSession* g_term = nullptr;

void HoldTrustDecision(int32_t, const char*, void*) {}

constexpr int kShellFields = 3;
constexpr int kGridHeaderInts = 9;
constexpr int kGridIntsPerCell = 3;

jint PackRgb(uint8_t r, uint8_t g, uint8_t b) {
    return jint((uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b));
}

}

extern "C" {

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeTerminal_nativeOpen(JNIEnv* env, jobject,
    jstring addr, jstring passcode, jint cols, jint rows) {
    if (g_term != nullptr) {
        dh_term_stop(g_term);
        g_term = nullptr;
    }
    DHTermCallbacks callbacks{};
    callbacks.onTrustAsked = HoldTrustDecision;
    const std::string address = deskhubj::FromJString(env, addr);
    const std::string code = deskhubj::FromJString(env, passcode);
    g_term = dh_term_open_deferred(address.c_str(), code.c_str(), uint16_t(cols),
        uint16_t(rows), &callbacks);
    return g_term != nullptr ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL Java_com_deskhub_app_NativeTerminal_nativeShells(JNIEnv* env,
    jobject) {
    if (g_term == nullptr) return nullptr;
    const uint32_t count = dh_term_session_count(g_term);
    jclass stringClass = env->FindClass("java/lang/String");
    if (stringClass == nullptr) return nullptr;
    jobjectArray out = env->NewObjectArray(jsize(count) * kShellFields, stringClass, nullptr);
    if (out == nullptr) return nullptr;
    for (uint32_t i = 0; i < count; ++i) {
        DHTermSessionInfo info{};
        if (!dh_term_session_info(g_term, i, &info)) continue;
        const int flags = (info.resumable ? 1 : 0) | (info.closable ? 2 : 0);
        const jsize base = jsize(i) * kShellFields;
        env->SetObjectArrayElement(out, base,
            env->NewStringUTF(std::to_string(info.termId).c_str()));
        env->SetObjectArrayElement(out, base + 1,
            env->NewStringUTF(std::to_string(flags).c_str()));
        env->SetObjectArrayElement(out, base + 2, env->NewStringUTF(info.line));
    }
    return out;
}

JNIEXPORT jboolean JNICALL Java_com_deskhub_app_NativeTerminal_nativeShellsKnown(JNIEnv*,
    jobject) {
    return dh_term_sessions_known(g_term) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeRequestShells(JNIEnv*, jobject) {
    dh_term_request_sessions(g_term);
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeResumeShell(JNIEnv*, jobject,
    jint termId) {
    if (termId > 0) dh_term_resume(g_term, uint32_t(termId));
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeCloseShell(JNIEnv*, jobject,
    jint termId) {
    if (termId > 0) dh_term_close_session(g_term, uint32_t(termId));
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeOpenFresh(JNIEnv*, jobject) {
    dh_term_open_new(g_term);
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeStop(JNIEnv*, jobject) {
    if (g_term == nullptr) return;
    dh_term_stop(g_term);
    g_term = nullptr;
}

JNIEXPORT jint JNICALL Java_com_deskhub_app_NativeTerminal_nativeState(JNIEnv*, jobject) {
    return jint(dh_term_state(g_term));
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeTerminal_nativeMessage(JNIEnv* env, jobject) {
    char buf[512];
    dh_term_message(g_term, buf, int(sizeof(buf)));
    return env->NewStringUTF(buf);
}

JNIEXPORT jstring JNICALL Java_com_deskhub_app_NativeTerminal_nativeFingerprint(JNIEnv* env,
    jobject) {
    char buf[128];
    dh_term_fingerprint(g_term, buf, int(sizeof(buf)));
    return env->NewStringUTF(buf);
}

JNIEXPORT jint JNICALL Java_com_deskhub_app_NativeTerminal_nativeVerdict(JNIEnv*, jobject) {
    return jint(dh_term_verdict(g_term));
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeAcceptKey(JNIEnv*, jobject) {
    dh_term_accept_key(g_term);
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeRejectKey(JNIEnv*, jobject) {
    dh_term_reject_key(g_term);
}

JNIEXPORT jintArray JNICALL Java_com_deskhub_app_NativeTerminal_nativeGrid(JNIEnv* env, jobject,
    jint scrollOffset) {
    if (g_term == nullptr) return nullptr;

    DHTermGrid grid{};
    dh_term_grid(g_term, uint32_t(scrollOffset < 0 ? 0 : scrollOffset), nullptr, 0, &grid);
    const uint32_t needed = uint32_t(grid.rows) * grid.cols;

    std::vector<DHTermCell> cells(needed);
    if (needed > 0 &&
        !dh_term_grid(g_term, uint32_t(scrollOffset < 0 ? 0 : scrollOffset), cells.data(),
            needed, &grid)) {
        return nullptr;
    }

    std::vector<jint> packed(size_t(kGridHeaderInts) + size_t(needed) * kGridIntsPerCell);
    packed[0] = jint(grid.rows);
    packed[1] = jint(grid.cols);
    packed[2] = jint(grid.cursorRow);
    packed[3] = jint(grid.cursorCol);
    packed[4] = grid.cursorVisible ? 1 : 0;
    packed[5] = jint(grid.scrollbackRows);
    packed[6] = jint(grid.scrollOffset);
    packed[7] = jint(uint32_t(grid.revision & 0xFFFFFFFFu));
    packed[8] = jint(uint32_t(grid.revision >> 32));
    for (uint32_t i = 0; i < needed; ++i) {
        const DHTermCell& cell = cells[i];
        const size_t base = size_t(kGridHeaderInts) + size_t(i) * kGridIntsPerCell;
        packed[base] = jint(cell.codepoint);
        packed[base + 1] = PackRgb(cell.fgR, cell.fgG, cell.fgB);
        packed[base + 2] =
            jint((uint32_t(cell.attrs) << 24) |
                 uint32_t(PackRgb(cell.bgR, cell.bgG, cell.bgB)));
    }

    jintArray out = env->NewIntArray(jsize(packed.size()));
    if (out) env->SetIntArrayRegion(out, 0, jsize(packed.size()), packed.data());
    return out;
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeSendKey(JNIEnv*, jobject,
    jint key, jint codepoint, jboolean shift, jboolean alt, jboolean ctrl) {
    dh_term_send_key(g_term, int32_t(key), codepoint > 0 ? uint32_t(codepoint) : 0,
        shift == JNI_TRUE, alt == JNI_TRUE, ctrl == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeSendText(JNIEnv* env, jobject,
    jstring text) {
    dh_term_send_text(g_term, deskhubj::FromJString(env, text).c_str());
}

JNIEXPORT void JNICALL Java_com_deskhub_app_NativeTerminal_nativeResize(JNIEnv*, jobject,
    jint cols, jint rows) {
    if (cols < 1 || rows < 1) return;
    dh_term_resize(g_term, uint16_t(cols), uint16_t(rows));
}
}

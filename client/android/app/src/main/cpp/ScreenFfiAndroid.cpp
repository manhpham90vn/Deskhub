#include "ScreenFfiAndroid.h"

#include <android/native_window.h>

#include <atomic>
#include <memory>

#include "decode/MediaCodecDecoder.h"
#include "deskhubp/ffi/ScreenFfiForward.h"
#include "deskhubp/ffi/ScreenFfiShell.h"
#include "deskhubp/client/ScreenViewer.h"

namespace {

using AndroidScreenViewer = deskhubp::ScreenViewer<MediaCodecDecoder, ANativeWindow*>;

std::atomic<uint32_t> g_screenW{0};
std::atomic<uint32_t> g_screenH{0};

}

void dh_screen_set_screen_hint(uint32_t width, uint32_t height) {
    g_screenW.store(width, std::memory_order_relaxed);
    g_screenH.store(height, std::memory_order_relaxed);
}

struct DHScreen : deskhubp::FfiScreenSession<AndroidScreenViewer> {
    DHScreen() : FfiScreenSession(deskhub::diag::ScreenClientDiagCaps{false, true}) {}
};

namespace {

AndroidScreenViewer& EngineOf(DHScreen* s) {
    return s->engine;
}

}

DHScreen* dh_screen_start(const char* address, uint8_t sourceId, void* surface,
    const DHScreenCallbacks* callbacks) {
    return deskhubp::StartFfiScreenSession<DHScreen, ANativeWindow*>(address, sourceId, surface,
        callbacks, g_screenW.load(std::memory_order_relaxed),
        g_screenH.load(std::memory_order_relaxed));
}

void dh_screen_stop(DHScreen* s) {
    deskhubp::StopFfiScreenSession(s);
}

void dh_screen_set_layer(DHScreen* s, void* layer) {
    if (s) s->engine.SetSurface(static_cast<ANativeWindow*>(layer));
}

DESKHUB_DEFINE_CLIENT_SESSION_FORWARDERS(EngineOf)

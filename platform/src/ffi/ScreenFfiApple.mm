#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <CoreGraphics/CoreGraphics.h>
#endif

#include "deskhubp/ffi/ScreenFfi.h"

#include "deskhubp/ffi/ScreenFfiForward.h"
#include "deskhubp/ffi/ScreenFfiShell.h"
#include "deskhubp/media/VtDecoder.h"
#include "deskhubp/client/ScreenViewer.h"

namespace {

using AppleScreenViewer = deskhubp::ScreenViewer<VtDecoder, void*>;

#if TARGET_OS_IPHONE
UIScreen* ActiveScreen() {
    UIScreen* fallback = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        if (scene.activationState == UISceneActivationStateForegroundActive)
            return windowScene.screen;
        if (!fallback) fallback = windowScene.screen;
    }
    return fallback;
}
#else
constexpr uint32_t kMaxDisplays = 16;
#endif

void LocalScreenPixels(uint32_t& outW, uint32_t& outH) {
    outW = outH = 0;
#if TARGET_OS_IPHONE
    UIScreen* screen = ActiveScreen();
    if (!screen) return;
    const CGRect bounds = screen.nativeBounds;
    if (bounds.size.width <= 0 || bounds.size.height <= 0) return;
    outW = uint32_t(bounds.size.width);
    outH = uint32_t(bounds.size.height);
#else
    CGDirectDisplayID ids[kMaxDisplays];
    uint32_t count = 0;
    if (CGGetActiveDisplayList(kMaxDisplays, ids, &count) != kCGErrorSuccess || !count) return;
    uint64_t bestArea = 0;
    for (uint32_t i = 0; i < count; ++i) {
        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(ids[i]);
        if (!mode) continue;
        const uint64_t w = CGDisplayModeGetPixelWidth(mode);
        const uint64_t h = CGDisplayModeGetPixelHeight(mode);
        CGDisplayModeRelease(mode);
        if (!w || !h || w * h <= bestArea) continue;
        bestArea = w * h;
        outW = uint32_t(w);
        outH = uint32_t(h);
    }
#endif
}

}

struct DHScreen : deskhubp::FfiScreenSession<AppleScreenViewer> {
    DHScreen() : FfiScreenSession(deskhub::diag::ScreenClientDiagCaps{false, true}) {}
};

namespace {

AppleScreenViewer& EngineOf(DHScreen* s) {
    return s->engine;
}

}

DHScreen* dh_screen_start(const char* address, uint8_t sourceId, void* surface,
    const DHScreenCallbacks* callbacks) {
    uint32_t screenW = 0, screenH = 0;
    LocalScreenPixels(screenW, screenH);
    return deskhubp::StartFfiScreenSession<DHScreen, void*>(address, sourceId, surface,
        callbacks, screenW, screenH);
}

void dh_screen_stop(DHScreen* s) {
    deskhubp::StopFfiScreenSession(s);
}

void dh_screen_set_layer(DHScreen* s, void* layer) {
    if (s) s->engine.SetSurface(layer);
}

DESKHUB_DEFINE_CLIENT_SESSION_FORWARDERS(EngineOf)

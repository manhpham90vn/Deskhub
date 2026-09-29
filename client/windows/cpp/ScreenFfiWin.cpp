#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#include <objbase.h>
#include <wrl/client.h>

#include "deskhubp/ffi/ScreenFfi.h"

#include <atomic>
#include <memory>
#include <string>

#include "gpu/GpuSelect.h"
#include "decode/PanelRenderer.h"
#include "decode/WinVideoDecoder.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/ffi/ScreenFfiForward.h"
#include "deskhubp/ffi/ScreenFfiShell.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/ScreenViewer.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

constexpr const char* kStatusSeparator = " \xC2\xB7 ";
constexpr uint32_t kInitialPanelWidth = 1280;
constexpr uint32_t kInitialPanelHeight = 720;

using WinScreenViewer = deskhubp::ScreenViewer<WinVideoDecoder, WinRenderTarget>;

}

struct DHScreen : deskhubp::FfiScreenSession<WinScreenViewer> {
    DHScreen() : FfiScreenSession(deskhub::diag::ScreenClientDiagCaps{true, false}) {}

    GpuChoice gpu;
    PanelRenderer renderer;
    std::atomic<uint32_t> negotiatedFps{0};
    std::atomic<bool> negotiated{false};
};

namespace {

WinScreenViewer& EngineOf(DHScreen* s) {
    return s->engine;
}

}

DHScreen* dh_screen_start(const char* address, uint8_t sourceId, void* surface,
    const DHScreenCallbacks* callbacks) {
    if (!surface) return nullptr;

    NetAddr server{};
    if (!deskhubp::ParseScreenAddress(address, server)) return nullptr;

    auto session = std::make_unique<DHScreen>();
    session->AdoptCallbacks(callbacks);

    if (!CreateBestDevice({GpuVendor::Nvidia, GpuVendor::Intel, GpuVendor::Amd}, session->gpu))
        return nullptr;
    {
        Microsoft::WRL::ComPtr<ID3D10Multithread> mt;
        if (SUCCEEDED(session->gpu.device.As(&mt))) mt->SetMultithreadProtected(TRUE);
    }
    if (!session->renderer.InitForHwnd(session->gpu.device.Get(), surface, kInitialPanelWidth,
            kInitialPanelHeight))
        return nullptr;

    DHScreen* raw = session.get();

    deskhubp::ScreenViewerConfig cfg;
    cfg.server = server;
    cfg.sourceId = sourceId;
    cfg.screenW = uint32_t(GetSystemMetrics(SM_CXVIRTUALSCREEN));
    cfg.screenH = uint32_t(GetSystemMetrics(SM_CYVIRTUALSCREEN));
    cfg.alwaysFocused = true;
    cfg.statusSeparator = kStatusSeparator;
    cfg.displayName = deskhubp::SessionDeviceName();
    cfg.wantsAudio = deskhubp::LoadUiSettings().playAudio;
    cfg.onParams = [raw](uint32_t width, uint32_t height, uint8_t fps) {
        raw->negotiatedFps.store(fps ? fps : 60, std::memory_order_relaxed);
        raw->negotiated.store(true, std::memory_order_release);
        if (raw->callbacks.onSize) raw->callbacks.onSize(width, height, raw->callbacks.user);
    };
    cfg.onStatus = [raw](const char* compact) {
        if (raw->negotiated.load(std::memory_order_acquire) && raw->callbacks.onStatus)
            raw->callbacks.onStatus(compact, raw->callbacks.user);
    };
    raw->WireLifecycle(cfg);
    cfg.hostLabel = address ? address : "";
    cfg.onDecodeThreadStart = [] { CoInitializeEx(nullptr, COINIT_MULTITHREADED); };
    cfg.onDecodeThreadExit = [] { CoUninitialize(); };

    raw->engine.SetSurface(
        WinRenderTarget{raw->gpu.device.Get(), &raw->renderer, &raw->negotiatedFps});

    if (!raw->engine.Start(cfg)) return nullptr;

    return session.release();
}

void dh_screen_stop(DHScreen* s) {
    deskhubp::StopFfiScreenSession(s);
}

void dh_screen_set_layer(DHScreen*, void*) {}

DESKHUB_DEFINE_CLIENT_SESSION_FORWARDERS(EngineOf)

#pragma once
#include "deskhub/diag/ScreenClientDiag.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/ffi/ScreenFfi.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/ScreenViewer.h"
#include "deskhubp/system/UiSettingsStore.h"

#include <atomic>
#include <memory>

namespace deskhubp {

inline bool ParseScreenAddress(const char* address, NetAddr& server) {
    if (!address) return false;
    if (ParseNetAddr(address, server)) return true;
    LOGE("[Bridge] Invalid address: %s", address);
    return false;
}

template <class Engine>
struct FfiScreenSession {
    Engine engine;
    DHScreenCallbacks callbacks{};
    std::atomic<bool> userStop{false};
    std::atomic<bool> closedNotified{false};
    char statusBuf[256] = {};
    char reasonBuf[256] = {};

    explicit FfiScreenSession(deskhub::diag::ScreenClientDiagCaps caps = {}) : engine(caps) {}

    void AdoptCallbacks(const DHScreenCallbacks* cb) {
        if (cb) callbacks = *cb;
    }

    void NotifyClosed(const char* reason) {
        if (closedNotified.exchange(true)) return;
        if (callbacks.onClosed)
            callbacks.onClosed(reason && *reason ? reason : "connection lost", callbacks.user);
    }

    void WireLifecycle(ScreenViewerConfig& cfg) {
        cfg.onEnded = [this](const char* reason) { NotifyClosed(reason); };
        cfg.onFinished = [this](const char* reason) {
            if (!userStop.load()) NotifyClosed(reason);
        };
        cfg.onTrustAsked = [this](deskhub::TrustVerdict verdict, std::string_view fingerprint) {
            if (!callbacks.onTrustAsked) {
                engine.RejectFingerprint();
                return;
            }
            const std::string copy(fingerprint);
            callbacks.onTrustAsked(int32_t(verdict), copy.c_str(), callbacks.user);
        };
    }

    void StopQuietly() {
        userStop.store(true);
        closedNotified.store(true);
        engine.Stop();
    }
};

template <class Session, class Surface>
Session* StartFfiScreenSession(const char* address, uint8_t sourceId, void* surface,
    const DHScreenCallbacks* callbacks, uint32_t screenW, uint32_t screenH) {
    NetAddr server;
    if (!ParseScreenAddress(address, server)) return nullptr;

    auto session = std::make_unique<Session>();
    session->AdoptCallbacks(callbacks);
    Session* raw = session.get();

    ScreenViewerConfig cfg;
    cfg.server = server;
    cfg.sourceId = sourceId;
    cfg.screenW = screenW;
    cfg.screenH = screenH;
    cfg.displayName = SessionDeviceName();
    cfg.wantsAudio = LoadUiSettings().playAudio;
    cfg.onParams = [raw](uint32_t width, uint32_t height, uint8_t) {
        if (raw->callbacks.onSize) raw->callbacks.onSize(width, height, raw->callbacks.user);
    };
    cfg.onStatus = [raw](const char* compact) {
        if (raw->callbacks.onStatus) raw->callbacks.onStatus(compact, raw->callbacks.user);
    };
    raw->WireLifecycle(cfg);

    if (surface) raw->engine.SetSurface(static_cast<Surface>(surface));
    if (!raw->engine.Start(cfg)) return nullptr;
    return session.release();
}

template <class Session>
void AcceptFfiScreenKey(Session* s) {
    if (s) s->engine.AcceptFingerprint();
}

template <class Session>
void RejectFfiScreenKey(Session* s) {
    if (s) s->engine.RejectFingerprint();
}

template <class Session>
void StopFfiScreenSession(Session* s) {
    if (!s) return;
    s->StopQuietly();
    delete s;
}

}

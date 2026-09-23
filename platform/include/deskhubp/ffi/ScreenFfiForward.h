#pragma once
#include "deskhub/input/Hotkeys.h"
#include "deskhubp/ffi/ScreenFfi.h"
#include "deskhubp/ffi/FfiText.h"

#include <cstring>

#define DESKHUB_DEFINE_CLIENT_SESSION_FORWARDERS(engineOf)                          \
    void dh_screen_key(DHScreen* s, int32_t vk, int32_t scan, bool down) {          \
        if (s) engineOf(s).QueueKey(vk, scan, down);                                \
    }                                                                               \
                                                                                    \
    void dh_screen_hotkey(DHScreen* s, int32_t vk, int32_t scan, int32_t modVk,     \
        int32_t modScan) {                                                          \
        if (!s) return;                                                             \
        deskhub::DispatchHotkey(engineOf(s),                                        \
            deskhub::Hotkey{"", vk, scan, modVk, modScan});                         \
    }                                                                               \
                                                                                    \
    void dh_screen_char_tap(DHScreen* s, uint32_t codepoint) {                      \
        if (s) engineOf(s).QueueCharTap(codepoint);                                 \
    }                                                                               \
                                                                                    \
    void dh_screen_release_all_input(DHScreen* s) {                                 \
        if (s) engineOf(s).ReleaseAllInput();                                       \
    }                                                                               \
                                                                                    \
    void dh_screen_mouse_move(DHScreen* s, int32_t nx, int32_t ny) {                \
        if (s) engineOf(s).QueueMouseMoveAbs(nx, ny);                               \
    }                                                                               \
                                                                                    \
    void dh_screen_mouse_move_rel(DHScreen* s, int32_t dx, int32_t dy) {            \
        if (s) engineOf(s).QueueMouseMoveRel(dx, dy);                               \
    }                                                                               \
                                                                                    \
    void dh_screen_mouse_button(DHScreen* s, int32_t button, bool down) {           \
        if (s) engineOf(s).QueueMouseButton(button, down);                          \
    }                                                                               \
                                                                                    \
    void dh_screen_mouse_wheel(DHScreen* s, int32_t delta) {                        \
        if (s) engineOf(s).QueueMouseWheel(delta);                                  \
    }                                                                               \
                                                                                    \
    void dh_screen_mouse_wheel_notches(DHScreen* s, int32_t notches) {              \
        if (s) engineOf(s).QueueMouseWheel(notches * deskhub::kWheelDeltaPerNotch); \
    }                                                                               \
                                                                                    \
    void dh_screen_snapshot(DHScreen* s, DHScreenState* out) {                      \
        if (!out) return;                                                           \
        *out = DHScreenState{};                                                     \
        if (!s) return;                                                             \
        out->phase = DHPhase(int(engineOf(s).phase()));                             \
        out->videoWidth = engineOf(s).videoWidth();                                 \
        out->videoHeight = engineOf(s).videoHeight();                               \
        deskhubp::CopyToBuf(out->statusLine, sizeof(out->statusLine),               \
            engineOf(s).StatusLine());                                              \
        deskhubp::CopyToBuf(out->endReason, sizeof(out->endReason),                 \
            engineOf(s).EndReason());                                               \
    }                                                                               \
                                                                                    \
    void dh_screen_link_health(DHScreen* s, DHLinkHealth* out) {                    \
        if (!out) return;                                                           \
        *out = DHLinkHealth{};                                                      \
        if (!s) return;                                                             \
        const deskhub::LinkPulseView view = engineOf(s).LinkHealth();               \
        out->haveRtt = view.haveRtt;                                                \
        out->rttMs = (view.rttUs + 500) / 1000;                                     \
        out->lossPct = view.lossPct;                                                \
        out->quality = DHLinkQuality(int(view.quality));                            \
    }                                                                               \
                                                                                    \
    DHPhase dh_screen_phase(DHScreen* s) {                                          \
        if (!s) return DHPhaseIdle;                                                 \
        return DHPhase(int(engineOf(s).phase()));                                   \
    }                                                                               \
                                                                                    \
    void dh_screen_clip_offer(DHScreen* s, const char* text) {                      \
        if (s && text && *text) engineOf(s).OfferLocalClipboard(text);              \
    }                                                                               \
                                                                                    \
    int dh_screen_clip_take(DHScreen* s, char* out, int capacity) {                 \
        if (!out || capacity <= 0) return 0;                                        \
        out[0] = '\0';                                                              \
        if (!s) return 0;                                                           \
        const auto text = engineOf(s).TakeRemoteClipboard();                        \
        if (!text) return 0;                                                        \
        deskhubp::CopyToBuf(out, size_t(capacity), *text);                          \
        return int(std::strlen(out));                                               \
    }                                                                               \
                                                                                    \
    void dh_screen_accept_key(DHScreen* s) {                                        \
        deskhubp::AcceptFfiScreenKey(s);                                            \
    }                                                                               \
                                                                                    \
    void dh_screen_reject_key(DHScreen* s) {                                        \
        deskhubp::RejectFfiScreenKey(s);                                            \
    }

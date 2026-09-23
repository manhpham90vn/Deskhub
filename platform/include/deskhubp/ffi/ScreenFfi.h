#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "deskhubp/ffi/ClientFfi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DHScreen DHScreen;

typedef void (*DHStatusCallback)(const char* statusUtf8, void* user);
typedef void (*DHSizeCallback)(uint32_t width, uint32_t height, void* user);
typedef void (*DHClosedCallback)(const char* reasonUtf8, void* user);
typedef void (*DHTrustCallback)(int32_t verdict, const char* fingerprintUtf8, void* user);

typedef struct {
    DHStatusCallback onStatus;
    DHSizeCallback onSize;
    DHClosedCallback onClosed;
    DHTrustCallback onTrustAsked;
    void* user;
} DHScreenCallbacks;

DHScreen* dh_screen_start(const char* address, uint8_t sourceId, void* surface,
    const DHScreenCallbacks* callbacks, const char* passcode);

void dh_screen_stop(DHScreen* s);

void dh_screen_accept_key(DHScreen* s);

void dh_screen_reject_key(DHScreen* s);

void dh_screen_set_layer(DHScreen* s, void* layer);

void dh_screen_key(DHScreen* s, int32_t vk, int32_t scan, bool down);

void dh_screen_hotkey(DHScreen* s, int32_t vk, int32_t scan, int32_t modVk, int32_t modScan);

void dh_screen_char_tap(DHScreen* s, uint32_t codepoint);

void dh_screen_release_all_input(DHScreen* s);

void dh_screen_mouse_move(DHScreen* s, int32_t nx, int32_t ny);

void dh_screen_mouse_move_rel(DHScreen* s, int32_t dx, int32_t dy);

void dh_screen_mouse_button(DHScreen* s, int32_t button, bool down);

void dh_screen_mouse_wheel(DHScreen* s, int32_t delta);

void dh_screen_mouse_wheel_notches(DHScreen* s, int32_t notches);

void dh_screen_clip_offer(DHScreen* s, const char* text);
int dh_screen_clip_take(DHScreen* s, char* out, int capacity);

typedef struct {
    DHPhase phase;
    uint32_t videoWidth;
    uint32_t videoHeight;
    char statusLine[256];
    char endReason[256];
} DHScreenState;

void dh_screen_snapshot(DHScreen* s, DHScreenState* out);

DHPhase dh_screen_phase(DHScreen* s);
#ifdef __cplusplus
}
#endif

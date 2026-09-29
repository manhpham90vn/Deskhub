#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "deskhubp/ffi/ShareFfi.h"
#include "deskhubp/ffi/ClientFfi.h"
#include "deskhubp/ffi/ScreenFfi.h"
#include "deskhubp/ffi/SendFfi.h"
#include "deskhubp/ffi/DevicesFfi.h"
#include "deskhubp/ffi/SettingsFfi.h"
#include "deskhubp/ffi/HostProfileFfi.h"
#include "deskhubp/ffi/ClientKeyFfi.h"
#include "deskhubp/ffi/TerminalFfi.h"

#ifdef __cplusplus
extern "C" {
#endif

bool dh_has_screen_recording(void);
bool dh_request_screen_recording(void);
void dh_open_screen_recording_settings(void);
bool dh_has_accessibility(void);
bool dh_request_accessibility(void);
void dh_open_accessibility_settings(void);

#ifdef __cplusplus
}
#endif

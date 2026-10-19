#include "DeskhubBridge.h"

#include <chrono>
#include <optional>
#include <thread>

#include "Permissions.h"
#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/host/UnattendedPush.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/ControlSocket.h"
#include "secure/DaemonRegistration.h"

namespace {

constexpr int kDaemonStartAttempts = 10;
constexpr auto kDaemonStartRetry = std::chrono::milliseconds(300);

bool PushCurrentSettings(const deskhub::ui::UiSettings& settings) {
    const std::optional<deskhub::UnattendedConfig> config =
        deskhubp::BuildUnattendedConfig(settings);
    return config && SendUnattendedConfig(*config);
}

bool PushOnceTheDaemonIsUp(const deskhub::ui::UiSettings& settings) {
    for (int attempt = 0; attempt < kDaemonStartAttempts; ++attempt) {
        if (PushCurrentSettings(settings)) return true;
        std::this_thread::sleep_for(kDaemonStartRetry);
    }
    return false;
}

DHUnattendedDaemonStatus StatusOf(DaemonStatus status) {
    switch (status) {
        case DaemonStatus::Enabled: return DHUnattendedDaemonEnabled;
        case DaemonStatus::RequiresApproval: return DHUnattendedDaemonRequiresApproval;
        case DaemonStatus::NotFound: return DHUnattendedDaemonNotFound;
        case DaemonStatus::NotRegistered: break;
    }
    return DHUnattendedDaemonNotRegistered;
}

void SaveUnattended(bool on) {
    deskhub::ui::UiSettings settings = deskhubp::LoadUiSettings();
    settings.unattended = on;
    deskhubp::SaveUiSettings(settings);
}

bool SwitchOff() {
    deskhub::ui::UiSettings settings = deskhubp::LoadUiSettings();
    settings.unattended = false;
    deskhubp::SaveUiSettings(settings);
    if (UnattendedDaemonStatus() == DaemonStatus::Enabled) PushCurrentSettings(settings);
    return RegisterUnattendedDaemon(false);
}

bool SwitchOn() {
    if (!UnattendedDaemonInstalled() || !RegisterUnattendedDaemon(true)) return false;
    deskhub::ui::UiSettings settings = deskhubp::LoadUiSettings();
    settings.unattended = true;
    const DaemonStatus status = UnattendedDaemonStatus();
    if (status == DaemonStatus::RequiresApproval) {
        deskhubp::SaveUiSettings(settings);
        return true;
    }
    if (status != DaemonStatus::Enabled || !PushOnceTheDaemonIsUp(settings)) {
        SaveUnattended(false);
        return false;
    }
    deskhubp::SaveUiSettings(settings);
    return true;
}

}

bool dh_has_screen_recording(void) {
    return macperm::HasScreenRecording();
}

bool dh_request_screen_recording(void) {
    return macperm::RequestScreenRecording();
}

void dh_open_screen_recording_settings(void) {
    macperm::OpenScreenRecordingSettings();
}

bool dh_has_accessibility(void) {
    return macperm::HasAccessibility();
}

bool dh_request_accessibility(void) {
    return macperm::RequestAccessibility();
}

void dh_open_accessibility_settings(void) {
    macperm::OpenAccessibilitySettings();
}

bool dh_unattended_available(void) {
    return UnattendedDaemonInstalled();
}

DHUnattendedDaemonStatus dh_unattended_daemon_status(void) {
    return StatusOf(UnattendedDaemonStatus());
}

bool dh_unattended_enabled(void) {
    return deskhubp::LoadUiSettings().unattended;
}

bool dh_unattended_set(bool on) {
    return on ? SwitchOn() : SwitchOff();
}

bool dh_unattended_sync(void) {
    const deskhub::ui::UiSettings settings = deskhubp::LoadUiSettings();
    if (!settings.unattended) return true;
    if (UnattendedDaemonInstalled()) RegisterUnattendedDaemon(true);
    if (UnattendedDaemonStatus() != DaemonStatus::Enabled) return true;
    return PushCurrentSettings(settings);
}

void dh_unattended_open_login_items(void) {
    OpenLoginItemsSettings();
}

DHUnattendedPermissions dh_unattended_permissions(void) {
    DHUnattendedPermissions out{};
    if (!deskhubp::LoadUiSettings().unattended) return out;
    if (UnattendedDaemonStatus() != DaemonStatus::Enabled) return out;
    const std::optional<deskhub::UnattendedStatus> status = QueryUnattendedStatus();
    if (!status) return out;
    out.known = status->screen != deskhub::UnattendedPermission::Unknown ||
                status->input != deskhub::UnattendedPermission::Unknown;
    out.screenRecording = status->screen == deskhub::UnattendedPermission::Granted;
    out.accessibility = status->input == deskhub::UnattendedPermission::Granted;
    return out;
}

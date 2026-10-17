#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "UnattendedHost.h"

#include <chrono>
#include <thread>
#include <vector>

#include "MachineFolder.h"
#include "WinPaths.h"
#include "deskhub/session/host/ShareFlow.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "net/Firewall.h"
#include "secure/UnattendedMode.h"

namespace {

constexpr auto kPoll = std::chrono::milliseconds(500);
constexpr auto kDisplayRetry = std::chrono::seconds(2);
constexpr int kDisplayAttempts = 15;

std::vector<ShareSource> WaitForDisplays() {
    for (int attempt = 0; attempt < kDisplayAttempts; ++attempt) {
        std::vector<ShareSource> displays = deskhubp::ListDisplays();
        if (!displays.empty()) return displays;
        std::this_thread::sleep_for(kDisplayRetry);
    }
    return {};
}

ShareOptions UnattendedOptions(const deskhub::ui::UiSettings& settings) {
    ShareOptions options = deskhub::ShareOptionsOf(settings, false, false);
    options.audio = false;
    options.clipboardSync = false;
    if (options.deviceName.empty()) options.deviceName = deskhubp::LocalDeviceName();
    return options;
}

}

int RunUnattendedHost() {
    if (!EnterMachineFolder()) return 1;
    deskhubp::StartProcessLog();
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    SetUnattendedMode(true);
    if (!UnattendedEnabled()) {
        LOGE("[Unattended] No host key in the machine folder; access before sign-in is off.");
        return 1;
    }
    if (EnsureFirewallRule(kUnattendedFirewallRuleName, SelfExePath()))
        LOGI("[Unattended] Windows Firewall: inbound rule verified (all profiles).");
    else
        LOGW("[Unattended] Could not add the Windows Firewall rule for the unattended host.");

    const std::vector<ShareSource> displays = WaitForDisplays();
    if (displays.empty()) {
        LOGE("[Unattended] No display appeared; exiting so the service can try again.");
        return 1;
    }

    SharingHost host;
    const ShareOptions options = UnattendedOptions(deskhubp::LoadUiSettings());
    if (!host.Start(displays, options)) {
        LOGE("[Unattended] Could not start the host: %s", host.LastError().c_str());
        return 1;
    }
    LOGI("[Unattended] Sharing %zu display(s) on UDP %u before sign-in.", displays.size(),
        unsigned(options.port));
    while (host.running()) std::this_thread::sleep_for(kPoll);
    host.Stop();
    LOGI("[Unattended] Host stopped.");
    return 0;
}

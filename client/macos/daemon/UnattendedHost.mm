#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>

#include "UnattendedHost.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "deskhub/session/host/ShareFlow.h"
#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/host/UnattendedMode.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/MachineFolder.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr auto kPoll = std::chrono::milliseconds(500);
constexpr uint64_t kRestartBackoffUs = 2'000'000;

struct ConfigSnapshot {
    std::string hostKey;
    std::string settings;

    bool operator==(const ConfigSnapshot&) const = default;
};

ConfigSnapshot ReadSnapshot() {
    return ConfigSnapshot{deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName),
        deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName)};
}

bool HeldByTheApp() {
    std::error_code ignored;
    return std::filesystem::is_regular_file(deskhubp::AppDataFilePath(kUnattendedHoldFile),
        ignored);
}

deskhub::UnattendedPermission PermissionOf(bool granted) {
    return granted ? deskhub::UnattendedPermission::Granted
                   : deskhub::UnattendedPermission::Denied;
}

void PublishPermissions() {
    deskhub::UnattendedStatus status;
    status.screen = PermissionOf(CGPreflightScreenCaptureAccess());
    status.input = PermissionOf(AXIsProcessTrusted());
    deskhubp::WriteAppDataFileAtomic(kUnattendedStatusFile,
        deskhub::SerializeUnattendedStatus(status));
    LOGI("[Unattended] Screen Recording: %s. Accessibility: %s.",
        status.screen == deskhub::UnattendedPermission::Granted ? "granted" : "not granted",
        status.input == deskhub::UnattendedPermission::Granted ? "granted" : "not granted");
}

ShareOptions UnattendedOptions(const deskhub::ui::UiSettings& settings) {
    ShareOptions options = deskhub::ShareOptionsOf(settings, false, false);
    options.audio = false;
    options.clipboardSync = false;
    if (options.deviceName.empty()) options.deviceName = deskhubp::LocalDeviceName();
    return options;
}

class HostSupervisor {
public:
    void Run() {
        while (deskhubp::UnattendedEnabled(kUnattendedMachineFolder)) {
            Step();
            std::this_thread::sleep_for(kPoll);
        }
        StopHost("access before sign-in was switched off");
        LOGI("[Unattended] Exiting.");
    }

private:
    void Step() {
        if (HeldByTheApp()) {
            StopHost("the app is sharing");
            return;
        }
        if (running_ && !host_.running()) StopHost("the engine stopped");
        if (running_ && ReadSnapshot() != runningWith_) StopHost("the key or settings changed");
        if (!running_ && NowUs() - lastStopUs_ >= kRestartBackoffUs) StartHost();
    }

    void StartHost() {
        const std::vector<ShareSource> displays = deskhubp::ListDisplays();
        if (displays.empty()) {
            LOGW("[Unattended] No display yet; trying again shortly.");
            lastStopUs_ = NowUs();
            return;
        }
        runningWith_ = ReadSnapshot();
        const ShareOptions options = UnattendedOptions(deskhubp::LoadUiSettings());
        if (!host_.Start(displays, options)) {
            LOGE("[Unattended] Could not start the host: %s", host_.LastError().c_str());
            lastStopUs_ = NowUs();
            return;
        }
        running_ = true;
        LOGI("[Unattended] Sharing %zu display(s) on UDP %u before sign-in.", displays.size(),
            unsigned(options.port));
    }

    void StopHost(const char* why) {
        if (!running_) return;
        host_.Stop();
        running_ = false;
        lastStopUs_ = NowUs();
        LOGI("[Unattended] Host stopped: %s.", why);
    }

    SharingHost host_;
    bool running_ = false;
    uint64_t lastStopUs_ = 0;
    ConfigSnapshot runningWith_;
};

}

int RunUnattendedHost() {
    if (!deskhubp::EnterMachineFolder(kUnattendedMachineFolder)) {
        LOGE("[Unattended] Could not enter %s.", kUnattendedMachineFolder);
        return 1;
    }
    deskhubp::SetUnattendedMode(true);
    PublishPermissions();
    if (!deskhubp::UnattendedEnabled(kUnattendedMachineFolder)) {
        LOGE("[Unattended] No host key in the machine folder; access before sign-in is off.");
        return 0;
    }
    CFRunLoopRef mainLoop = CFRunLoopGetMain();
    std::thread supervisor([mainLoop] {
        HostSupervisor().Run();
        CFRunLoopStop(mainLoop);
    });
    CFRunLoopRun();
    supervisor.join();
    return 0;
}

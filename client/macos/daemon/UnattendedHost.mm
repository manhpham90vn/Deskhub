#import <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>

#include "UnattendedHost.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/host/UnattendedDaemon.h"
#include "deskhubp/host/UnattendedMode.h"
#include "deskhubp/host/UnattendedPush.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/MachineFolder.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr auto kPoll = std::chrono::milliseconds(500);
constexpr auto kPollLeeway = std::chrono::milliseconds(50);
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
    return std::filesystem::is_regular_file(deskhubp::AppDataFilePath(deskhubp::kUnattendedHoldFile),
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
    deskhubp::WriteAppDataFileAtomic(deskhubp::kUnattendedStatusFile,
        deskhub::SerializeUnattendedStatus(status));
    LOGI("[Unattended] Screen Recording: %s. Accessibility: %s.",
        status.screen == deskhub::UnattendedPermission::Granted ? "granted" : "not granted",
        status.input == deskhub::UnattendedPermission::Granted ? "granted" : "not granted");
}

class HostSupervisor {
public:
    bool Tick() {
        if (deskhubp::UnattendedEnabled(kUnattendedMachineFolder)) {
            Step();
            return true;
        }
        StopHost("access before sign-in was switched off");
        LOGI("[Unattended] Exiting.");
        return false;
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
        const ShareOptions options = deskhubp::UnattendedShareOptions(deskhubp::LoadUiSettings());
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
    auto supervisor = std::make_shared<HostSupervisor>();
    dispatch_source_t tick =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
    dispatch_source_set_timer(tick, DISPATCH_TIME_NOW,
        uint64_t(std::chrono::nanoseconds(kPoll).count()),
        uint64_t(std::chrono::nanoseconds(kPollLeeway).count()));
    dispatch_source_set_event_handler(tick, ^{
      if (supervisor->Tick()) return;
      dispatch_source_cancel(tick);
      CFRunLoopStop(mainLoop);
    });
    dispatch_resume(tick);
    CFRunLoopRun();
    return 0;
}

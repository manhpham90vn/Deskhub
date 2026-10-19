#include "UnattendedHost.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <thread>
#include <vector>

#include "capture/KmsDevice.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/host/UnattendedMode.h"
#include "deskhubp/host/UnattendedPush.h"
#include "deskhubp/system/MachineFolder.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr auto kPoll = std::chrono::milliseconds(500);
constexpr int kPollsPerLayoutCheck = 4;

std::atomic<bool> g_stop{false};

void OnSignal(int) {
    g_stop.store(true, std::memory_order_release);
}

bool SameLayout(const std::vector<ShareSource>& a, const std::vector<ShareSource>& b) {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
        [](const ShareSource& x, const ShareSource& y) {
            return x.targetId == y.targetId && x.x == y.x && x.y == y.y &&
                   x.width == y.width && x.height == y.height;
        });
}

void SpanDesktop(ShareOptions& options, const std::vector<ShareSource>& displays) {
    int32_t left = displays.front().x, top = displays.front().y;
    int32_t right = left, bottom = top;
    for (const ShareSource& d : displays) {
        left = std::min(left, d.x);
        top = std::min(top, d.y);
        right = std::max(right, d.x + int32_t(d.width));
        bottom = std::max(bottom, d.y + int32_t(d.height));
    }
    options.desktopX = left;
    options.desktopY = top;
    options.desktopW = uint32_t(right - left);
    options.desktopH = uint32_t(bottom - top);
}

void ServeUntilSomethingChanges(SharingHost& host, const std::vector<ShareSource>& displays) {
    int polls = 0;
    while (!g_stop.load(std::memory_order_acquire) && host.running()) {
        std::this_thread::sleep_for(kPoll);
        if (++polls % kPollsPerLayoutCheck != 0) continue;
        if (SameLayout(displays, ListKmsDisplays())) continue;
        LOGI("[Unattended] The displays changed; restarting to follow them.");
        return;
    }
}

}

int RunUnattendedHost() {
    if (!deskhubp::EnterMachineFolder(kUnattendedMachineFolder)) {
        LOGE("[Unattended] Could not enter %s.", kUnattendedMachineFolder);
        return 1;
    }
    deskhubp::SetUnattendedMode(true);
    if (!deskhubp::UnattendedEnabled(kUnattendedMachineFolder)) {
        LOGI("[Unattended] No host key in the machine folder; access before sign-in is off.");
        return 0;
    }
    std::signal(SIGTERM, OnSignal);
    std::signal(SIGINT, OnSignal);

    const std::vector<ShareSource> displays = ListKmsDisplays();
    if (displays.empty()) {
        LOGW("[Unattended] No display is lit on any /dev/dri/card*.");
        return 1;
    }
    ShareOptions options = deskhubp::UnattendedShareOptions(deskhubp::LoadUiSettings());
    SpanDesktop(options, displays);
    SharingHost host;
    if (!host.Start(displays, options)) {
        LOGE("[Unattended] Could not start the host: %s", host.LastError().c_str());
        return 1;
    }
    LOGI("[Unattended] Sharing %zu display(s) on UDP %u before sign-in.", displays.size(),
        unsigned(options.port));
    ServeUntilSomethingChanges(host, displays);
    host.Stop();
    LOGI("[Unattended] Stopped.");
    return 0;
}

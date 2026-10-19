#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/host/UnattendedDaemon.h"

#include <atomic>
#include <cstdio>

#ifdef DESKHUB_LOCAL_SOCKET

#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/net/LocalSocket.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/MachineFolder.h"
#include "deskhubp/system/UiSettingsStore.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <unistd.h>

namespace {

constexpr auto kSettle = std::chrono::milliseconds(20);
constexpr int kSettleTries = 150;

struct RestoreDirs {
    std::string appData = deskhubp::AppDataDirRef();
    std::string config = deskhubp::ConfigDirRef();

    ~RestoreDirs() {
        deskhubp::SetAppDataDir(appData);
        deskhubp::SetConfigDir(config);
    }
};

struct Recorded {
    std::mutex mutex;
    std::vector<bool> configured;
    std::vector<bool> holds;
    deskhubp::UnattendedPeerRights rights = deskhubp::UnattendedPeerRights::Configure;
};

class RunningDaemon {
public:
    explicit RunningDaemon(Recorded& recorded)
        : root_(UniqueTempDir("deskhub-daemon-")),
          socketPath_((root_ / "control.sock").string()),
          recorded_(recorded) {
        deskhubp::EnterMachineFolder((root_ / "machine").string());
        thread_ = std::thread([this] {
            deskhubp::ServeUnattendedControl(socketPath_, 0600, Hooks(), stop_);
        });
        for (int i = 0; i < kSettleTries && !std::filesystem::exists(socketPath_); ++i)
            std::this_thread::sleep_for(kSettle);
    }

    ~RunningDaemon() {
        stop_.store(true);
        thread_.join();
        unlink(socketPath_.c_str());
        std::error_code ignored;
        std::filesystem::remove_all(root_, ignored);
    }

    RunningDaemon(const RunningDaemon&) = delete;
    RunningDaemon& operator=(const RunningDaemon&) = delete;

    deskhubp::LocalSocket Send(deskhub::SecureDesktopRequest request) const {
        deskhubp::LocalSocket control = deskhubp::LocalSocket::Connect(socketPath_);
        const auto byte = uint8_t(request);
        control.WriteAll(std::span<const uint8_t>(&byte, 1));
        return control;
    }

private:
    deskhubp::UnattendedDaemonHooks Hooks() {
        deskhubp::UnattendedDaemonHooks hooks;
        hooks.rightsOf = [this](const deskhubp::LocalPeer&) {
            const std::lock_guard<std::mutex> lock(recorded_.mutex);
            return recorded_.rights;
        };
        hooks.afterConfigure = [this](bool enabled) {
            const std::lock_guard<std::mutex> lock(recorded_.mutex);
            recorded_.configured.push_back(enabled);
            return true;
        };
        hooks.holdChanged = [this](bool held) {
            const std::lock_guard<std::mutex> lock(recorded_.mutex);
            recorded_.holds.push_back(held);
        };
        hooks.status = [] {
            return deskhub::UnattendedStatus{deskhub::UnattendedPermission::Granted,
                deskhub::UnattendedPermission::Denied};
        };
        return hooks;
    }

    std::filesystem::path root_;
    std::string socketPath_;
    Recorded& recorded_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

bool ReadAck(deskhubp::LocalSocket& control) {
    uint8_t ack = 0;
    return control.ReadExactly(std::span<uint8_t>(&ack, 1)) && ack == deskhub::kSecureDesktopAck;
}

bool Configure(const RunningDaemon& daemon, const deskhub::UnattendedConfig& config) {
    deskhubp::LocalSocket control = daemon.Send(deskhub::SecureDesktopRequest::Configure);
    return control.WriteAll(deskhub::EncodeUnattendedConfig(config)) && ReadAck(control);
}

deskhub::UnattendedConfig EnabledConfig() {
    deskhub::UnattendedConfig config;
    config.enabled = true;
    config.hostKeyPem = "host key";
    config.authorizedKeys = "allowed list";
    config.settings = "fps=24\n";
    return config;
}

bool HoldMarked() {
    std::error_code ignored;
    return std::filesystem::is_regular_file(
        deskhubp::AppDataFilePath(deskhubp::kUnattendedHoldFile), ignored);
}

bool WaitUntil(const std::function<bool()>& condition) {
    for (int i = 0; i < kSettleTries; ++i) {
        if (condition()) return true;
        std::this_thread::sleep_for(kSettle);
    }
    return condition();
}

void TestConfigureWritesAndRemovesTheCopies() {
    std::printf("[daemon] a configuration is written into the machine folder, and wiped again...\n");
    const RestoreDirs restore;
    Recorded recorded;
    RunningDaemon daemon(recorded);
    Check(Configure(daemon, EnabledConfig()), "switching on is acknowledged");
    Check(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName) == "host key" &&
              deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName) == "fps=24\n",
        "the key and the settings are kept as copies");
    deskhub::UnattendedConfig off;
    Check(Configure(daemon, off), "switching off is acknowledged");
    Check(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName).empty(),
        "the copied key is gone");
    const std::lock_guard<std::mutex> lock(recorded.mutex);
    Check(recorded.configured == std::vector<bool>{true, false},
        "the platform hook hears both switches in order");
}

void TestTakeoverHoldsUntilTheAppLetsGo() {
    std::printf("[daemon] a takeover holds the unattended host off until the app hangs up...\n");
    const RestoreDirs restore;
    Recorded recorded;
    RunningDaemon daemon(recorded);
    {
        deskhubp::LocalSocket hold = daemon.Send(deskhub::SecureDesktopRequest::Takeover);
        Check(ReadAck(hold), "the takeover is acknowledged");
        {
            const std::lock_guard<std::mutex> lock(recorded.mutex);
            Check(recorded.holds == std::vector<bool>{true},
                "the host was told to step aside before the acknowledgement");
        }
        Check(HoldMarked(), "the hold marker is written for a host in another process");
    }
    Check(WaitUntil([&recorded] {
        const std::lock_guard<std::mutex> lock(recorded.mutex);
        return recorded.holds.size() == 2;
    }),
        "hanging up releases the hold");
    Check(!HoldMarked(), "and the marker is removed");
}

void TestARestrictedPeerCannotConfigure() {
    std::printf("[daemon] a program that may only hold cannot change the configuration...\n");
    const RestoreDirs restore;
    Recorded recorded;
    recorded.rights = deskhubp::UnattendedPeerRights::Hold;
    RunningDaemon daemon(recorded);
    Check(!Configure(daemon, EnabledConfig()), "the configuration is not acknowledged");
    Check(deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName).empty(),
        "and nothing is written");
    deskhubp::LocalSocket hold = daemon.Send(deskhub::SecureDesktopRequest::Takeover);
    Check(ReadAck(hold), "the same program may still take over");
}

void TestAnUntrustedPeerGetsNothing() {
    std::printf("[daemon] an untrusted program is turned away before any request...\n");
    const RestoreDirs restore;
    Recorded recorded;
    recorded.rights = deskhubp::UnattendedPeerRights::None;
    RunningDaemon daemon(recorded);
    deskhubp::LocalSocket hold = daemon.Send(deskhub::SecureDesktopRequest::Takeover);
    Check(!ReadAck(hold), "a takeover is not acknowledged");
    const std::lock_guard<std::mutex> lock(recorded.mutex);
    Check(recorded.holds.empty(), "and the host is left alone");
}

void TestStatusComesFromThePlatform() {
    std::printf("[daemon] a status request answers with what the platform reports...\n");
    const RestoreDirs restore;
    Recorded recorded;
    RunningDaemon daemon(recorded);
    deskhubp::LocalSocket control = daemon.Send(deskhub::SecureDesktopRequest::Status);
    std::array<uint8_t, deskhub::kUnattendedStatusSize> reply{};
    Check(control.ReadExactly(reply), "two status bytes come back");
    const auto status = deskhub::DecodeUnattendedStatus(reply);
    Check(status && status->screen == deskhub::UnattendedPermission::Granted &&
              status->input == deskhub::UnattendedPermission::Denied,
        "they carry the platform's answer");
}

void TestAnAgentRequestIsRefused() {
    std::printf("[daemon] a secure desktop agent request is not served off Windows...\n");
    const RestoreDirs restore;
    Recorded recorded;
    RunningDaemon daemon(recorded);
    deskhubp::LocalSocket control = daemon.Send(deskhub::SecureDesktopRequest::Agent);
    Check(!ReadAck(control), "the connection closes without an acknowledgement");
    deskhubp::LocalSocket again = daemon.Send(deskhub::SecureDesktopRequest::Status);
    std::array<uint8_t, deskhub::kUnattendedStatusSize> reply{};
    Check(again.ReadExactly(reply), "the daemon keeps serving afterwards");
}

}

void RunUnattendedDaemonTests() {
    TestConfigureWritesAndRemovesTheCopies();
    TestTakeoverHoldsUntilTheAppLetsGo();
    TestARestrictedPeerCannotConfigure();
    TestAnUntrustedPeerGetsNothing();
    TestStatusComesFromThePlatform();
    TestAnAgentRequestIsRefused();
}

#else

void RunUnattendedDaemonTests() {
    std::printf("[daemon] this platform has no Unix socket; the daemon cannot listen...\n");
    std::atomic<bool> stop{true};
    Check(!deskhubp::ServeUnattendedControl("unused", 0600, {}, stop),
        "serving fails cleanly");
}

#endif

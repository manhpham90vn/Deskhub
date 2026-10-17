#import <Foundation/Foundation.h>

#include "Daemon.h"

#include <array>
#include <atomic>
#include <csignal>
#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "LoginWindowAgent.h"
#include "PeerTrust.h"
#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/net/LocalSocket.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/MachineFolder.h"
#include "deskhubp/system/PairingTokenFile.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr int kAcceptPollMs = 500;
constexpr size_t kReadChunk = 4096;
constexpr int kMaxConcurrentClients = 16;
constexpr mode_t kSocketDirMode = 0755;
constexpr uint32_t kSocketMode = 0666;

std::atomic<bool> g_stop{false};
std::atomic<int> g_activeClients{0};

struct TakeoverHolds {
    std::mutex mutex;
    int count = 0;
};

TakeoverHolds g_holds;

void OnSignal(int) {
    g_stop.store(true, std::memory_order_release);
}

bool SendByte(deskhubp::LocalSocket& control, uint8_t byte) {
    return control.WriteAll(std::span<const uint8_t>(&byte, 1));
}

void DrainUntilClosed(deskhubp::LocalSocket& control) {
    std::array<uint8_t, 64> sink{};
    size_t got = 0;
    while (!g_stop.load(std::memory_order_acquire) && control.ReadSome(sink, got)) {
    }
}

void WriteHoldMarker(bool held) {
    if (held)
        deskhubp::WriteAppDataFileAtomic(kUnattendedHoldFile, "held\n");
    else
        deskhubp::RemoveAppDataFile(kUnattendedHoldFile);
}

void ServeTakeover(deskhubp::LocalSocket& control, const deskhubp::LocalPeer& peer) {
    {
        const std::lock_guard<std::mutex> lock(g_holds.mutex);
        ++g_holds.count;
        WriteHoldMarker(true);
    }
    LOGI("[Daemon] Deskhub (pid %d) is taking over from the unattended host.", peer.pid);
    if (SendByte(control, deskhub::kSecureDesktopAck)) DrainUntilClosed(control);
    {
        const std::lock_guard<std::mutex> lock(g_holds.mutex);
        --g_holds.count;
        if (g_holds.count == 0) WriteHoldMarker(false);
    }
    LOGI("[Daemon] Deskhub (pid %d) stopped sharing; the unattended host may return.", peer.pid);
}

void RemoveUnattendedFiles() {
    deskhubp::RemoveAppDataFile(deskhubp::kHostKeyFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kAuthorizedKeysFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kUiSettingsFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kAccessRequestsFileName);
    deskhubp::RemoveAppDataFile(deskhubp::kPairingTokensFileName);
    deskhubp::RemoveAppDataFile(kUnattendedStatusFile);
}

bool ApplyConfig(const deskhub::UnattendedConfig& config) {
    const std::lock_guard<std::mutex> lock(g_holds.mutex);
    if (!config.enabled) {
        RemoveUnattendedFiles();
        RemoveLoginWindowAgent();
        LOGI("[Daemon] Access before sign-in switched off; the machine copies are removed.");
        return true;
    }
    const bool written =
        deskhubp::WriteAppDataFileAtomic(deskhubp::kHostKeyFileName, config.hostKeyPem) &&
        deskhubp::WriteAppDataFileAtomic(deskhubp::kAuthorizedKeysFileName,
            config.authorizedKeys) &&
        deskhubp::WriteAppDataFileAtomic(deskhubp::kUiSettingsFileName, config.settings);
    if (!written) {
        LOGE("[Daemon] Could not write the unattended configuration into %s.",
            kUnattendedMachineFolder);
        return false;
    }
    if (!WriteLoginWindowAgent()) {
        LOGE("[Daemon] Could not write the login window agent into %s.", kLaunchAgentsDir);
        return false;
    }
    LOGI("[Daemon] Access before sign-in is on; the login window host picks the copies up.");
    return true;
}

void ServeConfigure(deskhubp::LocalSocket& control, const deskhub::UnattendedConfig& config,
    const deskhubp::LocalPeer& peer) {
    if (!ApplyConfig(config)) {
        LOGW("[Daemon] The configuration from pid %d was not applied.", peer.pid);
        return;
    }
    SendByte(control, deskhub::kSecureDesktopAck);
}

void ServeStatus(deskhubp::LocalSocket& control) {
    const deskhub::UnattendedStatus status =
        deskhub::ParseUnattendedStatus(deskhubp::ReadAppDataFile(kUnattendedStatusFile));
    control.WriteAll(deskhub::EncodeUnattendedStatus(status));
}

std::optional<deskhub::UnattendedAction> ReadRequest(deskhubp::LocalSocket& control,
    deskhub::UnattendedControlServer& parser) {
    std::array<uint8_t, kReadChunk> chunk{};
    size_t got = 0;
    while (control.ReadSome(chunk, got)) {
        const deskhub::UnattendedAction action =
            parser.Feed(std::span<const uint8_t>(chunk.data(), got));
        if (action != deskhub::UnattendedAction::NeedMore) return action;
    }
    return std::nullopt;
}

void ServeClient(deskhubp::LocalSocket control) {
    const std::optional<deskhubp::LocalPeer> peer = control.Peer();
    if (!peer) {
        LOGW("[Daemon] Could not identify a connecting process; closing.");
        return;
    }
    if (!TrustedAppPeer(*peer)) {
        LOGW("[Daemon] Refused pid %d (%s): it is not the signed Deskhub app.", peer->pid,
            peer->exePath.c_str());
        return;
    }
    deskhub::UnattendedControlServer parser;
    const std::optional<deskhub::UnattendedAction> action = ReadRequest(control, parser);
    if (!action) return;
    switch (*action) {
        case deskhub::UnattendedAction::Takeover: ServeTakeover(control, *peer); return;
        case deskhub::UnattendedAction::Configure:
            ServeConfigure(control, parser.config(), *peer);
            return;
        case deskhub::UnattendedAction::Status: ServeStatus(control); return;
        case deskhub::UnattendedAction::Agent:
            LOGW("[Daemon] pid %d asked for a secure desktop agent, which macOS has no use for.",
                peer->pid);
            return;
        case deskhub::UnattendedAction::NeedMore:
        case deskhub::UnattendedAction::Refuse: break;
    }
    LOGW("[Daemon] pid %d sent a request this daemon does not understand.", peer->pid);
}

void StartClientThread(deskhubp::LocalSocket client) {
    if (g_activeClients.load() >= kMaxConcurrentClients) {
        LOGW("[Daemon] Refused a connection: %d clients are already served.",
            kMaxConcurrentClients);
        return;
    }
    g_activeClients.fetch_add(1);
    std::thread([socket = std::move(client)]() mutable {
        ServeClient(std::move(socket));
        g_activeClients.fetch_sub(1);
    }).detach();
}

bool PrepareSocketDir() {
    if (mkdir(kUnattendedSocketDir, kSocketDirMode) != 0 && errno != EEXIST) return false;
    return chmod(kUnattendedSocketDir, kSocketDirMode) == 0;
}

}

int RunDaemon() {
    if (geteuid() != 0) {
        LOGE("[Daemon] DeskhubDaemon must run as root; launchd starts it from the Deskhub app "
             "once you allow Deskhub to run in the background.");
        return 1;
    }
    if (!deskhubp::EnterMachineFolder(kUnattendedMachineFolder)) {
        LOGE("[Daemon] Could not enter %s.", kUnattendedMachineFolder);
        return 1;
    }
    WriteHoldMarker(false);
    if (!PrepareSocketDir()) {
        LOGE("[Daemon] Could not prepare %s.", kUnattendedSocketDir);
        return 1;
    }
    deskhubp::LocalSocket server = deskhubp::LocalSocket::Listen(kUnattendedSocketPath, kSocketMode);
    if (!server.Valid()) {
        LOGE("[Daemon] Could not listen on %s.", kUnattendedSocketPath);
        return 1;
    }
    std::signal(SIGTERM, OnSignal);
    std::signal(SIGINT, OnSignal);
    LOGI("[Daemon] Running; waiting for Deskhub on %s.", kUnattendedSocketPath);
    while (!g_stop.load(std::memory_order_acquire)) {
        std::optional<deskhubp::LocalSocket> client = server.Accept(kAcceptPollMs);
        if (client) StartClientThread(std::move(*client));
    }
    server.Close();
    unlink(kUnattendedSocketPath);
    LOGI("[Daemon] Stopped.");
    return 0;
}

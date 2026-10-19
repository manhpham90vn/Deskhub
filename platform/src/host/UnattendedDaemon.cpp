#include "deskhubp/host/UnattendedDaemon.h"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace deskhubp {
namespace {

constexpr int kAcceptPollMs = 500;
constexpr size_t kReadChunk = 4096;
constexpr int kMaxConcurrentClients = 16;

struct DaemonState {
    UnattendedDaemonHooks hooks;
    const std::atomic<bool>* stop = nullptr;
    std::mutex mutex;
    int holds = 0;
    std::atomic<int> activeClients{0};
};

bool SendByte(LocalSocket& control, uint8_t byte) {
    return control.WriteAll(std::span<const uint8_t>(&byte, 1));
}

void DrainUntilClosed(LocalSocket& control, const std::atomic<bool>& stop) {
    std::array<uint8_t, 64> sink{};
    size_t got = 0;
    while (!stop.load(std::memory_order_acquire) && control.ReadSome(sink, got)) {
    }
}

void WriteHoldMarker(bool held) {
    if (held)
        WriteAppDataFileAtomic(kUnattendedHoldFile, "held\n");
    else
        RemoveAppDataFile(kUnattendedHoldFile);
}

void ChangeHolds(DaemonState& state, int delta) {
    const std::lock_guard<std::mutex> lock(state.mutex);
    const bool wasHeld = state.holds > 0;
    state.holds += delta;
    const bool held = state.holds > 0;
    if (held == wasHeld) return;
    WriteHoldMarker(held);
    if (state.hooks.holdChanged) state.hooks.holdChanged(held);
}

void ServeTakeover(DaemonState& state, LocalSocket& control, const LocalPeer& peer) {
    ChangeHolds(state, +1);
    LOGI("[Daemon] Deskhub (pid %d) is taking over from the unattended host.", peer.pid);
    if (SendByte(control, deskhub::kSecureDesktopAck)) DrainUntilClosed(control, *state.stop);
    ChangeHolds(state, -1);
    LOGI("[Daemon] Deskhub (pid %d) stopped sharing; the unattended host may return.", peer.pid);
}

void RemoveUnattendedCopies() {
    RemoveAppDataFile(kHostKeyFileName);
    RemoveAppDataFile(kAuthorizedKeysFileName);
    RemoveAppDataFile(kUiSettingsFileName);
    RemoveAppDataFile(kAccessRequestsFileName);
    RemoveAppDataFile(kPairingTokensFileName);
    RemoveAppDataFile(kUnattendedStatusFile);
}

bool WriteUnattendedCopies(const deskhub::UnattendedConfig& config) {
    return WriteAppDataFileAtomic(kHostKeyFileName, config.hostKeyPem) &&
           WriteAppDataFileAtomic(kAuthorizedKeysFileName, config.authorizedKeys) &&
           WriteAppDataFileAtomic(kUiSettingsFileName, config.settings);
}

bool ApplyConfig(DaemonState& state, const deskhub::UnattendedConfig& config) {
    const std::lock_guard<std::mutex> lock(state.mutex);
    if (!config.enabled) {
        RemoveUnattendedCopies();
        if (state.hooks.afterConfigure) state.hooks.afterConfigure(false);
        LOGI("[Daemon] Access before sign-in switched off; the machine copies are removed.");
        return true;
    }
    if (!WriteUnattendedCopies(config)) {
        LOGE("[Daemon] Could not write the unattended configuration into %s.",
            ConfigDir().c_str());
        return false;
    }
    if (state.hooks.afterConfigure && !state.hooks.afterConfigure(true)) return false;
    LOGI("[Daemon] Access before sign-in is on; the unattended host picks the copies up.");
    return true;
}

void ServeConfigure(DaemonState& state, LocalSocket& control,
    const deskhub::UnattendedConfig& config, const LocalPeer& peer) {
    if (!ApplyConfig(state, config)) {
        LOGW("[Daemon] The configuration from pid %d was not applied.", peer.pid);
        return;
    }
    SendByte(control, deskhub::kSecureDesktopAck);
}

void ServeStatus(const DaemonState& state, LocalSocket& control) {
    const deskhub::UnattendedStatus status =
        state.hooks.status ? state.hooks.status() : deskhub::UnattendedStatus{};
    control.WriteAll(deskhub::EncodeUnattendedStatus(status));
}

std::optional<deskhub::UnattendedAction> ReadRequest(LocalSocket& control,
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

bool Permits(UnattendedPeerRights rights, deskhub::UnattendedAction action) {
    if (action == deskhub::UnattendedAction::Configure)
        return rights == UnattendedPeerRights::Configure;
    return rights != UnattendedPeerRights::None;
}

void Dispatch(DaemonState& state, LocalSocket& control, deskhub::UnattendedAction action,
    const deskhub::UnattendedControlServer& parser, const LocalPeer& peer) {
    switch (action) {
        case deskhub::UnattendedAction::Takeover: ServeTakeover(state, control, peer); return;
        case deskhub::UnattendedAction::Configure:
            ServeConfigure(state, control, parser.config(), peer);
            return;
        case deskhub::UnattendedAction::Status: ServeStatus(state, control); return;
        case deskhub::UnattendedAction::Agent:
            LOGW("[Daemon] pid %d asked for a secure desktop agent, which only Windows has.",
                peer.pid);
            return;
        case deskhub::UnattendedAction::NeedMore:
        case deskhub::UnattendedAction::Refuse: break;
    }
    LOGW("[Daemon] pid %d sent a request this daemon does not understand.", peer.pid);
}

void ServeClient(DaemonState& state, LocalSocket control) {
    const std::optional<LocalPeer> peer = control.Peer();
    if (!peer) {
        LOGW("[Daemon] Could not identify a connecting process; closing.");
        return;
    }
    const UnattendedPeerRights rights =
        state.hooks.rightsOf ? state.hooks.rightsOf(*peer) : UnattendedPeerRights::None;
    if (rights == UnattendedPeerRights::None) {
        LOGW("[Daemon] Refused pid %d (%s): it is not a trusted Deskhub program.", peer->pid,
            peer->exePath.c_str());
        return;
    }
    deskhub::UnattendedControlServer parser;
    const std::optional<deskhub::UnattendedAction> action = ReadRequest(control, parser);
    if (!action) return;
    if (!Permits(rights, *action)) {
        LOGW("[Daemon] Refused a configuration from pid %d (uid %u): it may not change access "
             "before sign-in.",
            peer->pid, unsigned(peer->uid));
        return;
    }
    Dispatch(state, control, *action, parser, *peer);
}

void StartClientThread(const std::shared_ptr<DaemonState>& state, LocalSocket client) {
    if (state->activeClients.load() >= kMaxConcurrentClients) {
        LOGW("[Daemon] Refused a connection: %d clients are already served.",
            kMaxConcurrentClients);
        return;
    }
    state->activeClients.fetch_add(1);
    std::thread([state, socket = std::move(client)]() mutable {
        ServeClient(*state, std::move(socket));
        state->activeClients.fetch_sub(1);
    }).detach();
}

}

bool ServeUnattendedControl(const std::string& socketPath, uint32_t socketMode,
    UnattendedDaemonHooks hooks, const std::atomic<bool>& stop) {
    LocalSocket server = LocalSocket::Listen(socketPath, socketMode);
    if (!server.Valid()) {
        LOGE("[Daemon] Could not listen on %s.", socketPath.c_str());
        return false;
    }
    auto state = std::make_shared<DaemonState>();
    state->hooks = std::move(hooks);
    state->stop = &stop;
    WriteHoldMarker(false);
    LOGI("[Daemon] Running; waiting for Deskhub on %s.", socketPath.c_str());
    while (!stop.load(std::memory_order_acquire)) {
        std::optional<LocalSocket> client = server.Accept(kAcceptPollMs);
        if (client) StartClientThread(state, std::move(*client));
    }
    server.Close();
    return true;
}

}

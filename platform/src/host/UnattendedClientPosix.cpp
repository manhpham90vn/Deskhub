#include "deskhubp/host/UnattendedClient.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <system_error>
#include <utility>

#include <sys/stat.h>

#include "deskhubp/diag/Log.h"
#include "deskhubp/net/LocalSocket.h"

namespace deskhubp {
namespace {

std::string CanonicalPath(const std::string& path) {
    std::error_code ignored;
    const std::filesystem::path resolved = std::filesystem::weakly_canonical(path, ignored);
    return resolved.empty() ? path : resolved.string();
}

bool OnlyRootMayWriteBeside(const std::string& socketPath) {
    const std::string folder = std::filesystem::path(socketPath).parent_path().string();
    struct stat info{};
    if (stat(folder.c_str(), &info) != 0) return false;
    return info.st_uid == 0 && (info.st_mode & (S_IWGRP | S_IWOTH)) == 0;
}

bool RunsOneOf(const LocalPeer& peer, const std::vector<std::string>& exePaths) {
    if (exePaths.empty()) return true;
    const std::string exe = CanonicalPath(peer.exePath);
    return std::any_of(exePaths.begin(), exePaths.end(),
        [&exe](const std::string& path) { return CanonicalPath(path) == exe; });
}

bool IsTheDaemon(const LocalSocket& control, const UnattendedDaemonAddress& daemon) {
    const std::optional<LocalPeer> peer = control.Peer();
    if (!peer || peer->uid != 0) return false;
    if (!OnlyRootMayWriteBeside(daemon.socketPath)) return false;
    return RunsOneOf(*peer, daemon.daemonExePaths);
}

std::unique_ptr<LocalSocket> ConnectDaemon(const UnattendedDaemonAddress& daemon,
    deskhub::SecureDesktopRequest request) {
    auto control = std::make_unique<LocalSocket>(LocalSocket::Connect(daemon.socketPath));
    if (!control->Valid()) {
        LOGW("[Secure] The Deskhub daemon is not listening on %s.", daemon.socketPath.c_str());
        return nullptr;
    }
    if (!IsTheDaemon(*control, daemon)) {
        LOGW("[Secure] %s is not served by the Deskhub daemon running as root; not using it.",
            daemon.socketPath.c_str());
        return nullptr;
    }
    const auto byte = uint8_t(request);
    if (!control->WriteAll(std::span<const uint8_t>(&byte, 1))) return nullptr;
    return control;
}

bool AwaitAck(LocalSocket& control) {
    uint8_t ack = 0;
    return control.ReadExactly(std::span<uint8_t>(&ack, 1)) && ack == deskhub::kSecureDesktopAck;
}

}

std::shared_ptr<void> TakeoverUnattendedHost(const UnattendedDaemonAddress& daemon) {
    std::unique_ptr<LocalSocket> control =
        ConnectDaemon(daemon, deskhub::SecureDesktopRequest::Takeover);
    if (!control) return nullptr;
    if (!AwaitAck(*control)) {
        LOGW("[Secure] The daemon did not confirm the takeover of the unattended host.");
        return nullptr;
    }
    LOGI("[Secure] The unattended host, if any, has stepped aside for this share.");
    std::shared_ptr<LocalSocket> hold = std::move(control);
    return hold;
}

bool SendUnattendedConfig(const UnattendedDaemonAddress& daemon,
    const deskhub::UnattendedConfig& config) {
    const std::vector<uint8_t> payload = deskhub::EncodeUnattendedConfig(config);
    if (payload.empty()) {
        LOGE("[Secure] The unattended configuration does not fit the wire limits.");
        return false;
    }
    std::unique_ptr<LocalSocket> control =
        ConnectDaemon(daemon, deskhub::SecureDesktopRequest::Configure);
    if (!control || !control->WriteAll(payload)) return false;
    const bool accepted = AwaitAck(*control);
    if (accepted)
        LOGI("[Secure] Access before sign-in is now %s.", config.enabled ? "on" : "off");
    else
        LOGW("[Secure] The daemon did not accept the unattended configuration.");
    return accepted;
}

std::optional<deskhub::UnattendedStatus> QueryUnattendedStatus(
    const UnattendedDaemonAddress& daemon) {
    std::unique_ptr<LocalSocket> control =
        ConnectDaemon(daemon, deskhub::SecureDesktopRequest::Status);
    if (!control) return std::nullopt;
    std::array<uint8_t, deskhub::kUnattendedStatusSize> reply{};
    if (!control->ReadExactly(reply)) return std::nullopt;
    return deskhub::DecodeUnattendedStatus(reply);
}

}

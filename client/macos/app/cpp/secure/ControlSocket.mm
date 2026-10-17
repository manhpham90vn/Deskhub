#import <Foundation/Foundation.h>

#include "secure/ControlSocket.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

#include "deskhubp/diag/Log.h"
#include "deskhubp/net/LocalSocket.h"
#include "secure/UnattendedNames.h"

namespace {

std::string RealPath(const std::string& path) {
    char resolved[PATH_MAX] = {};
    if (path.empty() || !realpath(path.c_str(), resolved)) return path;
    return resolved;
}

std::string BundleContentsPath() {
    NSURL* executable = [NSBundle mainBundle].executableURL;
    if (!executable) return {};
    NSURL* contents = executable.URLByDeletingLastPathComponent.URLByDeletingLastPathComponent;
    return contents.path ? contents.path.UTF8String : "";
}

bool IsOurDaemon(const deskhubp::LocalSocket& control) {
    const std::optional<deskhubp::LocalPeer> peer = control.Peer();
    if (!peer || peer->uid != 0) return false;
    return RealPath(peer->exePath) == RealPath(UnattendedDaemonPath());
}

std::unique_ptr<deskhubp::LocalSocket> ConnectDaemon(deskhub::SecureDesktopRequest request) {
    auto control = std::make_unique<deskhubp::LocalSocket>(
        deskhubp::LocalSocket::Connect(kUnattendedSocketPath));
    if (!control->Valid()) {
        LOGW("[Secure] The Deskhub daemon is not listening on %s.", kUnattendedSocketPath);
        return nullptr;
    }
    if (!IsOurDaemon(*control)) {
        LOGW("[Secure] %s is not served by the DeskhubDaemon inside this app; not using it.",
            kUnattendedSocketPath);
        return nullptr;
    }
    const uint8_t byte = uint8_t(request);
    if (!control->WriteAll(std::span<const uint8_t>(&byte, 1))) return nullptr;
    return control;
}

bool AwaitAck(deskhubp::LocalSocket& control) {
    uint8_t ack = 0;
    return control.ReadExactly(std::span<uint8_t>(&ack, 1)) && ack == deskhub::kSecureDesktopAck;
}

}

std::string UnattendedDaemonPath() {
    const std::string contents = BundleContentsPath();
    if (contents.empty()) return {};
    return contents + "/MacOS/" + kUnattendedDaemonExe;
}

std::string UnattendedDaemonPlistPath() {
    const std::string contents = BundleContentsPath();
    if (contents.empty()) return {};
    return contents + "/Library/LaunchDaemons/" + UnattendedDaemonPlistName();
}

bool UnattendedDaemonInstalled() {
    std::error_code ignored;
    return std::filesystem::is_regular_file(UnattendedDaemonPath(), ignored) &&
           std::filesystem::is_regular_file(UnattendedDaemonPlistPath(), ignored);
}

std::shared_ptr<void> TakeoverUnattendedHost() {
    std::unique_ptr<deskhubp::LocalSocket> control =
        ConnectDaemon(deskhub::SecureDesktopRequest::Takeover);
    if (!control) return nullptr;
    if (!AwaitAck(*control)) {
        LOGW("[Secure] The daemon did not confirm the takeover of the unattended host.");
        return nullptr;
    }
    LOGI("[Secure] The unattended host, if any, has stepped aside for this share.");
    std::shared_ptr<deskhubp::LocalSocket> hold = std::move(control);
    return hold;
}

bool SendUnattendedConfig(const deskhub::UnattendedConfig& config) {
    const std::vector<uint8_t> payload = deskhub::EncodeUnattendedConfig(config);
    if (payload.empty()) {
        LOGE("[Secure] The unattended configuration does not fit the wire limits.");
        return false;
    }
    std::unique_ptr<deskhubp::LocalSocket> control =
        ConnectDaemon(deskhub::SecureDesktopRequest::Configure);
    if (!control || !control->WriteAll(payload)) return false;
    const bool accepted = AwaitAck(*control);
    if (accepted)
        LOGI("[Secure] Access before sign-in is now %s.", config.enabled ? "on" : "off");
    else
        LOGW("[Secure] The daemon did not accept the unattended configuration.");
    return accepted;
}

std::optional<deskhub::UnattendedStatus> QueryUnattendedStatus() {
    std::unique_ptr<deskhubp::LocalSocket> control =
        ConnectDaemon(deskhub::SecureDesktopRequest::Status);
    if (!control) return std::nullopt;
    std::array<uint8_t, deskhub::kUnattendedStatusSize> reply{};
    if (!control->ReadExactly(reply)) return std::nullopt;
    return deskhub::DecodeUnattendedStatus(reply);
}

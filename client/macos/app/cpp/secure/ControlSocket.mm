#import <Foundation/Foundation.h>

#include "secure/ControlSocket.h"

#include <filesystem>
#include <system_error>

#include "deskhubp/host/UnattendedClient.h"
#include "secure/UnattendedNames.h"

namespace {

std::string BundleContentsPath() {
    NSURL* executable = [NSBundle mainBundle].executableURL;
    if (!executable) return {};
    NSURL* contents = executable.URLByDeletingLastPathComponent.URLByDeletingLastPathComponent;
    return contents.path ? contents.path.UTF8String : "";
}

deskhubp::UnattendedDaemonAddress DaemonAddress() {
    return deskhubp::UnattendedDaemonAddress{kUnattendedSocketPath, {UnattendedDaemonPath()}};
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
    return deskhubp::TakeoverUnattendedHost(DaemonAddress());
}

bool SendUnattendedConfig(const deskhub::UnattendedConfig& config) {
    return deskhubp::SendUnattendedConfig(DaemonAddress(), config);
}

std::optional<deskhub::UnattendedStatus> QueryUnattendedStatus() {
    return deskhubp::QueryUnattendedStatus(DaemonAddress());
}

#pragma once
#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhub/session/host/UnattendedControl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace deskhubp {

struct UnattendedDaemonAddress {
    std::string socketPath;
    std::vector<std::string> daemonExePaths;
};

std::shared_ptr<void> TakeoverUnattendedHost(const UnattendedDaemonAddress& daemon);
bool SendUnattendedConfig(const UnattendedDaemonAddress& daemon,
    const deskhub::UnattendedConfig& config);
std::optional<deskhub::UnattendedStatus> QueryUnattendedStatus(
    const UnattendedDaemonAddress& daemon);

}

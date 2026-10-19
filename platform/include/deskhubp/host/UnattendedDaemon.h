#pragma once
#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/net/LocalSocket.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace deskhubp {

inline constexpr const char* kUnattendedHoldFile = "unattended-hold";
inline constexpr const char* kUnattendedStatusFile = "unattended-status.txt";

enum class UnattendedPeerRights : uint8_t {
    None,
    Hold,
    Configure,
};

struct UnattendedDaemonHooks {
    std::function<UnattendedPeerRights(const LocalPeer&)> rightsOf;
    std::function<bool(bool enabled)> afterConfigure;
    std::function<void(bool held)> holdChanged;
    std::function<deskhub::UnattendedStatus()> status;
};

bool ServeUnattendedControl(const std::string& socketPath, uint32_t socketMode,
    UnattendedDaemonHooks hooks, const std::atomic<bool>& stop);

}

#pragma once
#include <memory>
#include <optional>
#include <string>

#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhub/session/host/UnattendedControl.h"

std::string UnattendedDaemonPath();
std::string UnattendedDaemonPlistPath();
std::string BundledCliPath();
bool UnattendedDaemonInstalled();
std::shared_ptr<void> TakeoverUnattendedHost();
bool SendUnattendedConfig(const deskhub::UnattendedConfig& config);
std::optional<deskhub::UnattendedStatus> QueryUnattendedStatus();

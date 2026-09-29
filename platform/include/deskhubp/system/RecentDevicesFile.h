#pragma once
#include "deskhub/ui/RecentDevices.h"

#include <string_view>
#include <vector>

namespace deskhubp {

inline constexpr const char* kRecentDevicesFileName = "recent-hosts.txt";

std::vector<deskhub::ui::RecentDevice> LoadRecentDevices();
bool RememberRecentDevice(std::string_view address, std::string_view hostName);

}

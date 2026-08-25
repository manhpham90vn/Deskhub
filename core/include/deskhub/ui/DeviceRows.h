#pragma once
#include "deskhub/ui/RecentDevices.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deskhub::ui {

enum class DeviceOrigin : uint8_t {
    OnThisNetwork = 0,
    Recent = 1,
};

struct DeviceRow {
    std::string addr{};
    DeviceOrigin origin = DeviceOrigin::OnThisNetwork;
    int64_t lastConnectedUnix = 0;

    bool operator==(const DeviceRow&) const = default;
};

const char* DeviceOriginLabel(DeviceOrigin origin);

std::vector<DeviceRow> BuildDeviceRows(const std::vector<std::string>& scanned,
    const std::vector<RecentDevice>& recent);

}

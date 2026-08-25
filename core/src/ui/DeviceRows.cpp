#include "deskhub/ui/DeviceRows.h"

#include <algorithm>

#include "deskhub/ui/Strings.h"

namespace deskhub::ui {

namespace {

const RecentDevice* FindRecent(const std::vector<RecentDevice>& recent,
    const std::string& normalized) {
    for (const RecentDevice& device : recent)
        if (NormalizedDeviceAddr(device.addr) == normalized) return &device;
    return nullptr;
}

}

const char* DeviceOriginLabel(DeviceOrigin origin) {
    return origin == DeviceOrigin::OnThisNetwork ? kDeviceOnThisNetwork : kDeviceRecent;
}

std::vector<DeviceRow> BuildDeviceRows(const std::vector<std::string>& scanned,
    const std::vector<RecentDevice>& recent) {
    std::vector<DeviceRow> rows;
    std::vector<std::string> seen;
    rows.reserve(scanned.size() + recent.size());

    for (const std::string& addr : scanned) {
        const std::string normalized = NormalizedDeviceAddr(addr);
        if (normalized.empty()) continue;
        if (std::find(seen.begin(), seen.end(), normalized) != seen.end()) continue;
        seen.push_back(normalized);

        DeviceRow row;
        row.addr = addr;
        row.origin = DeviceOrigin::OnThisNetwork;
        const RecentDevice* known = FindRecent(recent, normalized);
        if (known != nullptr) row.lastConnectedUnix = known->lastConnectedUnix;
        rows.push_back(row);
    }

    for (const RecentDevice& device : recent) {
        const std::string normalized = NormalizedDeviceAddr(device.addr);
        if (normalized.empty()) continue;
        if (std::find(seen.begin(), seen.end(), normalized) != seen.end()) continue;
        seen.push_back(normalized);

        DeviceRow row;
        row.addr = device.addr;
        row.origin = DeviceOrigin::Recent;
        row.lastConnectedUnix = device.lastConnectedUnix;
        rows.push_back(row);
    }

    return rows;
}

}

#include "deskhubp/system/RecentDevicesFile.h"

#include <mutex>

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"

namespace deskhubp {

namespace {

constexpr const char* kRetiredRecentDevicesFileName = "recent-devices.txt";

std::mutex& RecentMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<deskhub::ui::RecentDevice> LoadLocked() {
    RemoveAppDataFile(kRetiredRecentDevicesFileName);
    return deskhub::ui::ParseRecentDevices(ReadAppDataFile(kRecentDevicesFileName));
}

}

std::vector<deskhub::ui::RecentDevice> LoadRecentDevices() {
    const std::lock_guard<std::mutex> lock(RecentMutex());
    return LoadLocked();
}

bool RememberRecentDevice(std::string_view address, std::string_view hostName) {
    const std::lock_guard<std::mutex> lock(RecentMutex());
    std::vector<deskhub::ui::RecentDevice> devices = LoadLocked();
    deskhub::ui::TouchRecentDevice(devices, address, NowUnixSeconds(), hostName);
    return WriteAppDataFileAtomic(kRecentDevicesFileName,
        deskhub::ui::SerializeRecentDevices(devices));
}

}

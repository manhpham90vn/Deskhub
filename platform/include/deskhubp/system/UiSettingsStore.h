#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/DeviceName.h"

namespace deskhubp {

inline constexpr const char* kUiSettingsFileName = "ui-settings.txt";

inline void SaveUiSettings(const deskhub::ui::UiSettings& settings) {
    WriteAppDataFile(kUiSettingsFileName, deskhub::ui::SerializeUiSettings(settings));
}

inline deskhub::ui::UiSettings LoadUiSettings() {
    return deskhub::ui::ParseUiSettings(ReadAppDataFile(kUiSettingsFileName));
}

inline std::string HostPasscode() {
    return LoadUiSettings().passcode;
}

inline std::string SessionDeviceName() {
    const std::string name = LoadUiSettings().deviceName;
    return name.empty() ? LocalDeviceName() : name;
}

}

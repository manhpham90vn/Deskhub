#pragma once
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/DeviceName.h"

#include <string>

namespace deskhubp {

inline constexpr const char* kUiSettingsFileName = "ui-settings.txt";

inline void SaveUiSettings(const deskhub::ui::UiSettings& settings) {
    WriteAppDataFileAtomic(kUiSettingsFileName, deskhub::ui::SerializeUiSettings(settings));
}

inline deskhub::ui::UiSettings LoadUiSettings() {
    const std::string stored = ReadAppDataFile(kUiSettingsFileName);
    const std::string kept = deskhub::ui::StripRetiredUiSettings(stored);
    if (kept != stored) WriteAppDataFileAtomic(kUiSettingsFileName, kept);
    return deskhub::ui::ParseUiSettings(kept);
}

inline std::string SessionDeviceName() {
    const std::string name = LoadUiSettings().deviceName;
    return name.empty() ? LocalDeviceName() : name;
}

}

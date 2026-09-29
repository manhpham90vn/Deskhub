#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/DeviceName.h"

#include <string_view>

namespace deskhubp {

inline constexpr const char* kUiSettingsFileName = "ui-settings.txt";

inline void SaveUiSettings(const deskhub::ui::UiSettings& settings) {
    WriteAppDataFileAtomic(kUiSettingsFileName, deskhub::ui::SerializeUiSettings(settings));
}

inline deskhub::ui::UiSettings LoadUiSettings() {
    const std::string text = ReadAppDataFile(kUiSettingsFileName);
    const deskhub::ui::UiSettings settings = deskhub::ui::ParseUiSettings(text);
    size_t position = 0;
    while (position < text.size()) {
        const size_t end = text.find('\n', position);
        const std::string_view line(text.data() + position,
            (end == std::string::npos ? text.size() : end) - position);
        const size_t equals = line.find('=');
        if (equals != std::string_view::npos) {
            const std::string key = deskhub::ui::TrimAscii(line.substr(0, equals));
            if (key == "passcode" || key == "allow_new_pairings") {
                WriteAppDataFileAtomic(kUiSettingsFileName,
                    deskhub::ui::SerializeUiSettings(settings));
                break;
            }
        }
        if (end == std::string::npos) break;
        position = end + 1;
    }
    return settings;
}

inline std::string SessionDeviceName() {
    const std::string name = LoadUiSettings().deviceName;
    return name.empty() ? LocalDeviceName() : name;
}

}

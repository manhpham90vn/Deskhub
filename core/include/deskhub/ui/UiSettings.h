#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace deskhub::ui {

struct UiSettings {
    uint32_t fps = 60;
    uint32_t bitrateMbps = 20;
    uint32_t maxDim = 1920;
    uint32_t port = kDeskhubPort;
    bool allowInput = true;
    bool clientControl = true;
    std::string deviceName{};
    std::string bindIp{};
    bool autostart = false;
    bool autoShare = false;
    bool clipboardSync = false;
    bool shareAudio = true;
    bool playAudio = true;
    bool startHidden = false;
    bool keepAwake = true;
    std::string transferDir{};

    bool operator==(const UiSettings&) const = default;
};

inline constexpr uint32_t kMaxSettingsFps = 240;
inline constexpr uint32_t kMaxSettingsBitrateMbps = 1000;
inline constexpr uint32_t kMaxSettingsDim = 16384;
inline constexpr uint32_t kMaxSettingsPort = 65535;
inline constexpr size_t kMaxSettingsPathBytes = 1024;

std::string TruncateDeviceName(std::string_view name);
std::string TruncateSettingsPath(std::string_view path);

UiSettings ParseUiSettings(std::string_view text);
std::string SerializeUiSettings(const UiSettings& settings);

}

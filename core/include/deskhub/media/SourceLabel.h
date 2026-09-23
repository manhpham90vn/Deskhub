#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace deskhub::media {

inline std::string SourceSizeLabel(uint32_t width, uint32_t height) {
    return std::to_string(width) + "x" + std::to_string(height);
}

inline std::string SourceName(std::string_view name, uint8_t sourceId) {
    if (!name.empty()) return std::string(name);
    return "Source " + std::to_string(unsigned(sourceId));
}

inline std::string SourcePickerLabel(std::string_view name, uint8_t sourceId, uint32_t width,
    uint32_t height) {
    return SourceName(name, sourceId) + " (" + SourceSizeLabel(width, height) + ")";
}

inline std::string ViewerCountLabel(uint32_t viewerCount) {
    if (!viewerCount) return std::string();
    if (viewerCount == 1) return "1 viewer";
    return std::to_string(viewerCount) + " viewers";
}

}

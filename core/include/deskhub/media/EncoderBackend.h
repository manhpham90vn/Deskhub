#pragma once
#include <span>
#include <string_view>

namespace deskhub::media {

inline constexpr std::string_view kEncoderBackendAuto = "auto";
inline constexpr std::string_view kEncoderBackendNvenc = "nvenc";
inline constexpr std::string_view kEncoderBackendMediaFoundation = "mf";
inline constexpr std::string_view kEncoderBackendVaApi = "vaapi";
inline constexpr std::string_view kEncoderBackendVideoToolbox = "videotoolbox";

std::span<const std::string_view> EncoderBackendNames();

bool IsEncoderBackendName(std::string_view name);

enum class GpuVendor { Nvidia,
    Intel,
    Amd,
    Microsoft,
    Unknown };

struct EncoderBackendOrder {
    std::span<const std::string_view> backends;
    bool measured = false;
};

EncoderBackendOrder EncoderBackendOrderFor(GpuVendor vendor);

}

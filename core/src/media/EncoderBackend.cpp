#include "deskhub/media/EncoderBackend.h"

namespace deskhub::media {
namespace {

constexpr std::string_view kNames[] = {kEncoderBackendAuto, kEncoderBackendNvenc,
    kEncoderBackendMediaFoundation, kEncoderBackendVaApi, kEncoderBackendVideoToolbox};

constexpr std::string_view kNvencFirst[] = {
    kEncoderBackendNvenc, kEncoderBackendMediaFoundation};

constexpr std::string_view kMediaFoundationFirst[] = {
    kEncoderBackendMediaFoundation, kEncoderBackendNvenc};

}

std::span<const std::string_view> EncoderBackendNames() {
    return kNames;
}

bool IsEncoderBackendName(std::string_view name) {
    for (std::string_view known : kNames)
        if (known == name) return true;
    return false;
}

EncoderBackendOrder EncoderBackendOrderFor(GpuVendor vendor) {
    switch (vendor) {
        case GpuVendor::Nvidia: return {kNvencFirst, true};
        case GpuVendor::Intel: return {kMediaFoundationFirst, true};
        case GpuVendor::Amd: return {kMediaFoundationFirst, false};
        case GpuVendor::Microsoft:
        case GpuVendor::Unknown: break;
    }
    return {kNvencFirst, false};
}

}

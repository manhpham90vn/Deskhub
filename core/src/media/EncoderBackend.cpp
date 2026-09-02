#include "deskhub/media/EncoderBackend.h"

namespace deskhub::media {
namespace {

constexpr std::string_view kNames[] = {kEncoderBackendAuto, kEncoderBackendNvenc,
    kEncoderBackendMediaFoundation, kEncoderBackendVaApi, kEncoderBackendVideoToolbox};

}

std::span<const std::string_view> EncoderBackendNames() {
    return kNames;
}

bool IsEncoderBackendName(std::string_view name) {
    for (std::string_view known : kNames)
        if (known == name) return true;
    return false;
}

}

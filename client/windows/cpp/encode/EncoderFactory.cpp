#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "encode/IVideoEncoder.h"
#include "encode/NvencEncoder.h"
#include "encode/MfEncoder.h"
#include "gpu/GpuSelect.h"

#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "deskhubp/diag/Log.h"

namespace {

using deskhub::media::EncoderRecoveryCaps;

struct Backend {
    std::string_view id;
    std::unique_ptr<IVideoEncoder> (*make)();
};

std::unique_ptr<IVideoEncoder> MakeNvenc() {
    return std::make_unique<NvencEncoder>();
}

std::unique_ptr<IVideoEncoder> MakeMediaFoundation() {
    return std::make_unique<MfEncoder>();
}

constexpr Backend kBackends[] = {
    {deskhub::media::kEncoderBackendNvenc, MakeNvenc},
    {deskhub::media::kEncoderBackendMediaFoundation, MakeMediaFoundation},
};

const Backend* FindBackend(std::string_view id) {
    for (const Backend& backend : kBackends)
        if (backend.id == id) return &backend;
    return nullptr;
}

std::string BuiltInBackends() {
    std::string list;
    for (std::string_view id : BuiltInEncoderBackends()) {
        if (!list.empty()) list += ", ";
        list += std::string(id);
    }
    return list;
}

void ReportChoice(const IVideoEncoder& encoder, std::string_view requested) {
    const EncoderRecoveryCaps caps = encoder.RecoveryCaps();
    const std::string_view id = encoder.BackendId();
    LOGI("[Encoder] measurement: backend=%.*s requested=%.*s ltr=%d intra_refresh=%d (%s)",
        int(id.size()), id.data(), int(requested.size()), requested.data(),
        caps.longTermReference ? 1 : 0, caps.intraRefresh ? 1 : 0, encoder.BackendName());
}

}

std::span<const std::string_view> BuiltInEncoderBackends() {
    static const std::vector<std::string_view> ids = [] {
        std::vector<std::string_view> names;
        for (const Backend& backend : kBackends) names.push_back(backend.id);
        return names;
    }();
    return ids;
}

std::unique_ptr<IVideoEncoder> CreateEncoder(ID3D11Device* device, const EncoderConfig& cfg,
    std::string_view backend, deskhub::media::GpuVendor vendor) {
    if (backend.empty()) backend = deskhub::media::kEncoderBackendAuto;

    if (backend != deskhub::media::kEncoderBackendAuto) {
        if (const Backend* b = FindBackend(backend)) {
            auto enc = b->make();
            if (enc->Init(device, cfg)) {
                ReportChoice(*enc, backend);
                return enc;
            }
            LOGE(
                "[Encoder] %.*s was named on the command line and would not start, so this "
                "source stops rather than measuring a different backend under its name.",
                int(backend.size()), backend.data());
            return nullptr;
        }
        LOGE("[Encoder] This build has no backend called \"%.*s\" - it has %s.",
            int(backend.size()), backend.data(), BuiltInBackends().c_str());
        return nullptr;
    }

    const deskhub::media::EncoderBackendOrder order =
        deskhub::media::EncoderBackendOrderFor(vendor);
    LOGI("[Encoder] %s adapter: trying %s first (%s)", GpuVendorName(vendor),
        std::string(order.backends.front()).c_str(),
        order.measured ? "measured on this vendor" : "no measurement on this vendor yet");

    for (std::string_view id : order.backends) {
        const Backend* b = FindBackend(id);
        if (b == nullptr) continue;
        auto enc = b->make();
        if (enc->Init(device, cfg)) {
            ReportChoice(*enc, backend);
            return enc;
        }
        LOGW("[Encoder] %.*s unavailable, trying the next backend...", int(id.size()), id.data());
    }
    LOGE("[Encoder] Failed to initialize any backend.");
    return nullptr;
}

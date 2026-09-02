#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "encode/IVideoEncoder.h"
#include "encode/NvencEncoder.h"
#include "encode/MfEncoder.h"

#include <cstdio>
#include <string>

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

std::string BuiltInBackends() {
    std::string list;
    for (const Backend& b : kBackends) {
        if (!list.empty()) list += ", ";
        list += std::string(b.id);
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

std::unique_ptr<IVideoEncoder> CreateEncoder(ID3D11Device* device, const EncoderConfig& cfg,
    std::string_view backend) {
    if (backend.empty()) backend = deskhub::media::kEncoderBackendAuto;

    if (backend != deskhub::media::kEncoderBackendAuto) {
        for (const Backend& b : kBackends) {
            if (b.id != backend) continue;
            auto enc = b.make();
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

    for (const Backend& b : kBackends) {
        auto enc = b.make();
        if (enc->Init(device, cfg)) {
            ReportChoice(*enc, backend);
            return enc;
        }
        LOGW("[Encoder] %.*s unavailable, trying the next backend...", int(b.id.size()),
            b.id.data());
    }
    LOGE("[Encoder] Failed to initialize any backend.");
    return nullptr;
}

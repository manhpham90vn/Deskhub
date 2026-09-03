#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "deskhub/media/EncoderBackend.h"
#include "deskhub/media/RecoveryPolicy.h"
#include "deskhub/media/VideoContract.h"
#include "deskhubp/diag/Log.h"

using deskhub::media::PacketHandler;
using deskhub::media::RateControl;

struct EncoderConfig : deskhub::media::EncoderConfig {
    std::wstring outputPath;
};

class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;

    virtual bool Init(ID3D11Device* device, const EncoderConfig& cfg) = 0;

    virtual bool IsOpen() const = 0;

    virtual bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) = 0;

    virtual bool SetBitrate(uint32_t bitrateBps) = 0;

    virtual bool SetFps(uint32_t fps) = 0;

    virtual void Finish() = 0;

    virtual const char* BackendName() const = 0;

    virtual std::string_view BackendId() const = 0;

    virtual deskhub::media::EncoderRecoveryCaps RecoveryCaps() const = 0;

    virtual bool MarkLongTermReference(uint32_t frameId) = 0;

    virtual bool InvalidateReference(uint32_t firstInvalidFrameId) = 0;

    virtual bool BeginIntraRefresh(uint32_t frames) = 0;
};

inline bool OpenEncoderOutput(const EncoderConfig& cfg, const char* tag, FILE*& out) {
    out = nullptr;
    if (!cfg.outputPath.empty()) {
        std::wstring path = cfg.outputPath;
        const size_t dot = path.find_last_of(L'.');
        if (dot != std::wstring::npos && path.substr(dot) == L".mp4")
            path = path.substr(0, dot) + L".h264";
        if (_wfopen_s(&out, path.c_str(), L"wb") != 0) out = nullptr;
        if (!out) LOGE("[%s] Failed to open output file.", tag);
        return out != nullptr;
    }
    if (!cfg.onPacket) {
        LOGE("[%s] No outputPath or onPacket - no output destination.", tag);
        return false;
    }
    return true;
}

std::unique_ptr<IVideoEncoder> CreateEncoder(ID3D11Device* device, const EncoderConfig& cfg,
    std::string_view backend);

static_assert(deskhub::media::VideoEncoderLike<IVideoEncoder, ID3D11Texture2D*>,
    "IVideoEncoder must match the shared encoder signature");
static_assert(deskhub::media::HotFpsEncoder<IVideoEncoder>,
    "IVideoEncoder can change fps on the fly (NVENC reconfigure; MF rebuilds the transform)");
static_assert(deskhub::media::ReferenceInvalidatingEncoder<IVideoEncoder>,
    "IVideoEncoder carries the long-term reference half of media::RecoveryPolicy");
static_assert(deskhub::media::IntraRefreshEncoder<IVideoEncoder>,
    "IVideoEncoder carries the intra-refresh half of media::RecoveryPolicy");

#pragma once
#include "encode/IVideoEncoder.h"

class MfEncoder : public IVideoEncoder {
public:
    MfEncoder();
    ~MfEncoder() override;

    bool Init(ID3D11Device* device, const EncoderConfig& cfg) override;
    bool IsOpen() const override {
        return impl_ != nullptr;
    }
    bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) override;
    bool SetBitrate(uint32_t bitrateBps) override;
    bool SetFps(uint32_t fps) override;
    void Finish() override;
    const char* BackendName() const override {
        return "Media Foundation (HW/SW auto)";
    }
    std::string_view BackendId() const override {
        return deskhub::media::kEncoderBackendMediaFoundation;
    }
    deskhub::media::EncoderRecoveryCaps RecoveryCaps() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

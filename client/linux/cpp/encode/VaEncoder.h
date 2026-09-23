#pragma once
#include <va/va.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "capture/CaptureTypes.h"

#include "deskhub/media/H264Encode.h"
#include "deskhub/media/RatePlan.h"
#include "deskhub/media/VideoContract.h"

using deskhub::media::EncoderConfig;
using deskhub::media::PacketHandler;

inline constexpr uint32_t kLog2MaxFrameNumMinus4 = 12;
inline constexpr uint32_t kLog2MaxPocLsbMinus4 = 12;
inline constexpr uint32_t kMaxRefFrames = 1;
inline constexpr uint32_t kLog2MaxMvLength = 15;

inline constexpr int kPicInitQp = 26;

class VaEncoder {
public:
    VaEncoder() = default;
    ~VaEncoder();
    VaEncoder(const VaEncoder&) = delete;
    VaEncoder& operator=(const VaEncoder&) = delete;

    bool Init(const EncoderConfig& cfg);

    bool Encode(const LinuxFrameInfo& fi, uint64_t timestampUs, bool forceKeyframe);

    bool EncodeLast(uint64_t timestampUs, bool forceKeyframe);

    bool haveSourceFrame() const {
        return haveSource_;
    }

    bool SetBitrate(uint32_t bitrateBps);

    void Finish();

    bool IsOpen() const {
        return encContext_ != VA_INVALID_ID;
    }
    const char* BackendName() const {
        return "VA-API (hardware)";
    }

private:
    bool CreateContexts();
    void BuildParameterSets();
    uint32_t TargetPercentage() const;
    uint32_t PeakBitrate() const;
    uint32_t VbvWindowMs(const deskhub::media::RatePlan& plan) const;
    int IdrQp() const;
    VASurfaceID ImportDmaBuf(const LinuxFrameInfo& fi);
    bool UploadMapped(const LinuxFrameInfo& fi);
    bool ConvertToNv12(VASurfaceID rgb);
    bool EncodeNv12(bool idr, size_t& outSize);

    VADisplay dpy_ = nullptr;
    EncoderConfig cfg_{};

    deskhub::media::AlignedEncodeSize aligned_{};

    VAEntrypoint encEntrypoint_ = VAEntrypointEncSlice;
    VAConfigID encConfig_ = VA_INVALID_ID;
    VAContextID encContext_ = VA_INVALID_ID;
    VABufferID codedBuf_ = VA_INVALID_ID;
    VASurfaceID srcNv12_ = VA_INVALID_SURFACE;
    VASurfaceID reconNv12_[2] = {VA_INVALID_SURFACE, VA_INVALID_SURFACE};

    VAConfigID vppConfig_ = VA_INVALID_ID;
    VAContextID vppContext_ = VA_INVALID_ID;

    VASurfaceID rgbSurface_ = VA_INVALID_SURFACE;
    VAImage rgbImage_{};
    bool haveRgbImage_ = false;
    uint32_t rgbFourcc_ = 0;
    uint32_t srcW_ = 0, srcH_ = 0;

    uint64_t frameCount_ = 0;
    uint32_t frameNum_ = 0;
    int32_t poc_ = 0;
    uint16_t idrPicId_ = 0;
    bool haveRef_ = false;
    uint32_t refFrameNum_ = 0;
    int32_t refPoc_ = 0;
    VASurfaceID refSurface_ = VA_INVALID_SURFACE;

    uint32_t pendingBitrate_ = 0;

    uint32_t rcMode_ = VA_RC_CQP;
    bool cqpMode_ = false;
    int qp_ = kPicInitQp;
    double logRatioEma_ = 0.0;
    bool haveRatio_ = false;
    uint64_t lastEncodeUs_ = 0;
    int lastIdrQp_ = 0;
    size_t lastIdrBytes_ = 0;

    bool packedHeaders_ = false;
    bool haveSource_ = false;
    std::vector<uint8_t> sps_, pps_;
    uint32_t spsBits_ = 0, ppsBits_ = 0;
    std::vector<uint8_t> out_;
};

static_assert(deskhub::media::VideoEncoderLike<VaEncoder, const LinuxFrameInfo&>,
    "VaEncoder must match the shared encoder signature");

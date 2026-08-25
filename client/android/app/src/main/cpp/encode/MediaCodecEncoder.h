#pragma once
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "deskhub/media/VideoContract.h"

using deskhub::media::EncoderConfig;

class MediaCodecEncoder {
public:
    MediaCodecEncoder() = default;
    ~MediaCodecEncoder();
    MediaCodecEncoder(const MediaCodecEncoder&) = delete;
    MediaCodecEncoder& operator=(const MediaCodecEncoder&) = delete;

    bool Init(const EncoderConfig& cfg);

    bool Encode(void* surfaceFedFrame, uint64_t timestampUs, bool forceKeyframe);

    bool SetBitrate(uint32_t bitrateBps);

    bool SetFps(uint32_t fps);

    void Finish();

    bool IsOpen() const {
        return codec_ != nullptr;
    }

    const char* BackendName() const {
        return "MediaCodec (input surface)";
    }

    ANativeWindow* inputWindow() const {
        return inputWindow_;
    }

private:
    void DrainLoop();
    bool SetParameter(const char* key, int32_t value);
    void ApplyPendingSyncRequest();
    void EmitAccessUnit(const uint8_t* payload, size_t size, uint64_t ptsUs);

    AMediaCodec* codec_ = nullptr;
    ANativeWindow* inputWindow_ = nullptr;
    EncoderConfig cfg_{};

    std::thread drainThread_;
    std::atomic<bool> draining_{false};
    std::atomic<bool> syncRequested_{false};

    std::vector<uint8_t> parameterSets_;
    std::vector<uint8_t> annexb_;
};

static_assert(deskhub::media::VideoEncoderLike<MediaCodecEncoder, void*>,
    "MediaCodecEncoder must match the shared encoder signature");
static_assert(deskhub::media::HotFpsEncoder<MediaCodecEncoder>,
    "the virtual display feeds the input surface, so a frame-rate step must not rebuild the codec");

#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

#include "capture/CaptureTypes.h"

class ScreenCapture {
public:
    using FrameHandler = std::function<void(const MacFrameInfo&)>;
    using AudioHandler = std::function<void(std::span<const int16_t>)>;

    ScreenCapture();
    ~ScreenCapture();
    ScreenCapture(const ScreenCapture&) = delete;
    ScreenCapture& operator=(const ScreenCapture&) = delete;

    bool Start(uint64_t targetId, const deskhub::media::CaptureOptions& opt,
        FrameHandler onFrame);
    void Stop();

    void SetAudioHandler(AudioHandler onAudio);

    void SetClientSize(uint32_t clientW, uint32_t clientH, uint32_t& outW, uint32_t& outH);

    void SetQuality(uint32_t scalePct, uint32_t fps, uint32_t& outW, uint32_t& outH);

    uint32_t TakeIdleCount();

    bool Closed() const;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

static_assert(deskhub::media::ScreenCaptureLike<ScreenCapture>,
    "ScreenCapture must match the shared capture signature");
static_assert(deskhub::media::QualityAwareCapture<ScreenCapture>,
    "ScreenCaptureKit rescales in the capture stream, so the ladder drives it directly");

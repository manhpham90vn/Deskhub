#pragma once
#include <cstddef>
#include <cstdint>

#include "deskhub/media/VideoContract.h"

class VideoSink;

class AvDecoder {
public:
    AvDecoder() = default;
    ~AvDecoder();
    AvDecoder(const AvDecoder&) = delete;
    AvDecoder& operator=(const AvDecoder&) = delete;

    bool Init(VideoSink* sink, int width, int height);
    void Shutdown();
    bool IsOpen() const {
        return ctx_ != nullptr;
    }

    bool Decode(const uint8_t* nal, size_t len, uint64_t ptsUs);

    uint32_t TakeRenderedCount();

    uint64_t lastRenderedPtsUs() const;

private:
    void* ctx_ = nullptr;
    void* packet_ = nullptr;
    void* frame_ = nullptr;
    void* hwDevice_ = nullptr;
    VideoSink* sink_ = nullptr;
};

static_assert(deskhub::media::EngineDecoder<AvDecoder, VideoSink*>,
    "AvDecoder must decode, restart in place, and bind to the sink it renders into");
static_assert(deskhub::media::RenderCountingDecoder<AvDecoder>,
    "the renderer presents asynchronously, so Decode cannot count presented frames");

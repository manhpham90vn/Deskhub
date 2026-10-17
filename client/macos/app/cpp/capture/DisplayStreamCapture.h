#pragma once
#include <cstdint>
#include <functional>

#include "capture/CaptureTypes.h"

class DisplayStreamCapture {
public:
    using FrameHandler = std::function<void(const MacFrameInfo&)>;
    using IdleHandler = std::function<void()>;

    DisplayStreamCapture() = default;
    ~DisplayStreamCapture();
    DisplayStreamCapture(const DisplayStreamCapture&) = delete;
    DisplayStreamCapture& operator=(const DisplayStreamCapture&) = delete;

    bool Start(uint32_t displayId, uint32_t width, uint32_t height, uint32_t fps,
        FrameHandler onFrame, IdleHandler onIdle);
    void Stop();

private:
    void* stream_ = nullptr;
    void* queue_ = nullptr;
};

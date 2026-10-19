#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include "capture/CaptureTypes.h"

class KmsCapture {
public:
    using FrameHandler = std::function<void(const LinuxFrameInfo&)>;

    KmsCapture();
    ~KmsCapture();
    KmsCapture(const KmsCapture&) = delete;
    KmsCapture& operator=(const KmsCapture&) = delete;

    bool Start(uint64_t targetId, const deskhub::media::CaptureOptions& opt,
        FrameHandler onFrame);
    void Stop();

    bool Closed() const;

    bool usingDmaBuf() const;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

static_assert(deskhub::media::ScreenCaptureLike<KmsCapture>,
    "KmsCapture must match the shared capture signature");
static_assert(deskhub::media::ZeroCopyCapture<KmsCapture>,
    "the KMS capture reports whether the frame stayed in GPU memory");

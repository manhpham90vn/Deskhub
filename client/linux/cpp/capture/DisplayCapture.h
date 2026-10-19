#pragma once
#include <cstdint>
#include <functional>

#include "capture/CaptureTypes.h"
#include "capture/KmsCapture.h"
#include "capture/ScreenCapture.h"
#include "deskhubp/host/UnattendedMode.h"

class DisplayCapture {
public:
    using FrameHandler = std::function<void(const LinuxFrameInfo&)>;

    bool Start(uint64_t targetId, const deskhub::media::CaptureOptions& opt,
        FrameHandler onFrame) {
        fromScanout_ = deskhubp::UnattendedMode();
        if (fromScanout_) return scanout_.Start(targetId, opt, std::move(onFrame));
        return portal_.Start(targetId, opt, std::move(onFrame));
    }

    void Stop() {
        if (fromScanout_)
            scanout_.Stop();
        else
            portal_.Stop();
    }

    bool Closed() const {
        return fromScanout_ ? scanout_.Closed() : portal_.Closed();
    }

    bool usingDmaBuf() const {
        return fromScanout_ ? scanout_.usingDmaBuf() : portal_.usingDmaBuf();
    }

private:
    bool fromScanout_ = false;
    ScreenCapture portal_;
    KmsCapture scanout_;
};

static_assert(deskhub::media::ScreenCaptureLike<DisplayCapture>,
    "DisplayCapture must match the shared capture signature");
static_assert(deskhub::media::ZeroCopyCapture<DisplayCapture>,
    "DisplayCapture reports whether the frame stayed in GPU memory");

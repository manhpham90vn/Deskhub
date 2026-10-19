#include "capture/KmsCapture.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <thread>
#include <utility>

#include "capture/KmsBlitter.h"
#include "capture/KmsDevice.h"
#include "deskhubp/diag/Log.h"

namespace {

constexpr uint32_t kMaxConsecutiveFailures = 30;
constexpr uint32_t kMinFps = 1;
constexpr uint32_t kMaxFps = 120;

std::chrono::microseconds FrameInterval(uint32_t fps) {
    return std::chrono::microseconds(1'000'000 / std::clamp(fps, kMinFps, kMaxFps));
}

}

struct KmsCapture::Impl {
    KmsCard card;
    uint32_t crtcId = 0;
    uint32_t fps = 0;
    FrameHandler onFrame;
    std::thread thread;
    std::atomic<bool> stop{false};
    std::atomic<bool> closed{false};
    std::atomic<bool> running{false};

    void Run(std::promise<bool> ready) {
        KmsBlitter blitter;
        const std::string renderNode = card.RenderNode();
        if (renderNode.empty() || !blitter.Open(renderNode)) {
            closed.store(true);
            ready.set_value(false);
            return;
        }
        running.store(true);
        ready.set_value(true);
        Loop(blitter);
        running.store(false);
    }

    void Loop(KmsBlitter& blitter) {
        const auto interval = FrameInterval(fps);
        auto next = std::chrono::steady_clock::now();
        uint32_t failures = 0;
        while (!stop.load(std::memory_order_acquire)) {
            const std::optional<KmsScanout> scanout = card.Scanout(crtcId);
            if (!scanout) {
                LOGI("[KMS] CRTC %u went dark; the capture ends.", crtcId);
                break;
            }
            failures = CaptureOnce(blitter, *scanout) ? 0 : failures + 1;
            if (failures >= kMaxConsecutiveFailures) {
                LOGE("[KMS] CRTC %u gave %u unusable frames in a row; the capture ends.", crtcId,
                    failures);
                break;
            }
            next += interval;
            const auto now = std::chrono::steady_clock::now();
            if (next < now) next = now;
            std::this_thread::sleep_until(next);
        }
        closed.store(true);
    }

    bool CaptureOnce(KmsBlitter& blitter, const KmsScanout& scanout) {
        std::optional<KmsBuffer> screen = card.Export(scanout.fbId);
        if (!screen) return false;
        std::optional<KmsBuffer> cursor;
        if (scanout.cursor) cursor = card.Export(scanout.cursor->fbId);
        const std::optional<LinuxFrameInfo> frame =
            blitter.Compose(scanout, std::move(*screen), std::move(cursor));
        if (!frame) return false;
        onFrame(*frame);
        return true;
    }
};

KmsCapture::KmsCapture() : impl_(std::make_unique<Impl>()) {}

KmsCapture::~KmsCapture() {
    Stop();
}

bool KmsCapture::Start(uint64_t targetId, const deskhub::media::CaptureOptions& opt,
    FrameHandler onFrame) {
    Stop();
    std::optional<KmsCard> card = KmsCard::Open(KmsCardOf(targetId));
    if (!card) {
        LOGE("[KMS] Cannot open /dev/dri/card%u; capturing before sign-in needs root.",
            KmsCardOf(targetId));
        return false;
    }
    impl_ = std::make_unique<Impl>();
    impl_->card = std::move(*card);
    impl_->crtcId = KmsCrtcOf(targetId);
    impl_->fps = opt.fps;
    impl_->onFrame = std::move(onFrame);
    std::promise<bool> ready;
    std::future<bool> started = ready.get_future();
    impl_->thread = std::thread([impl = impl_.get(), promise = std::move(ready)]() mutable {
        impl->Run(std::move(promise));
    });
    if (started.get()) return true;
    impl_->thread.join();
    return false;
}

void KmsCapture::Stop() {
    if (!impl_->thread.joinable()) return;
    impl_->stop.store(true, std::memory_order_release);
    impl_->thread.join();
}

bool KmsCapture::Closed() const {
    return impl_->closed.load();
}

bool KmsCapture::usingDmaBuf() const {
    return impl_->running.load();
}

#include "CcControls.h"

#include "CcShared.h"

#include <cstdint>

namespace deskhub::cc {
namespace {

inline constexpr uint32_t kRttTrendWindow = 5;
inline constexpr uint32_t kRttRisingMs = 8;
inline constexpr uint32_t kRttFallingMs = 2;

class DelayTrendControl final : public CongestionControl {
public:
    DelayTrendControl(uint32_t startBps, uint32_t minBps) : rate_(startBps, minBps) {}

    std::string_view Name() const override {
        return "delay-trend";
    }

    BitrateDecision Update(const Feedback& fb, uint32_t frameAgeMs, uint64_t nowUs) override {
        BitrateDecision d;
        const bool before = fec_.armed();
        d.fecEnabled = fec_.Update(fb.lossPct);
        d.fecToggled = d.fecEnabled != before;
        d.fecParityPerGroup = FecParityRowsFor(fb.lossPct);

        if (fb.rttMs && (minRttMs_ == 0 || fb.rttMs < minRttMs_)) minRttMs_ = fb.rttMs;
        const uint32_t queueMs = fb.rttMs > minRttMs_ ? fb.rttMs - minRttMs_ : 0;

        rttSum_ += queueMs;
        if (++samples_ > kRttTrendWindow) {
            rttSum_ -= rttSum_ / samples_;
            samples_ = kRttTrendWindow;
        }
        const uint32_t trendMs = samples_ ? uint32_t(rttSum_ / samples_) : 0;

        uint32_t want = rate_.cur();
        if (fb.lossPct >= 5 || frameAgeMs >= BitrateController::kSevereBacklogMs ||
            queueMs >= kQueueDelayHighMs) {
            want = rate_.cur() - rate_.cur() / 4;
            lastDecreaseUs_ = nowUs;
        } else if (fb.lossPct >= 2 || frameAgeMs >= BitrateController::kBacklogMs ||
                   trendMs >= kRttRisingMs) {
            want = rate_.cur() - rate_.cur() / 10;
            lastDecreaseUs_ = nowUs;
        } else if (trendMs <= kRttFallingMs && fb.lossPct == 0 &&
                   nowUs - lastDecreaseUs_ > kHoldAfterDecreaseUs) {
            want = rate_.cur() + rate_.max() / 20;
        }

        const BitrateDecision settled = rate_.Settle(want);
        d.changeBitrate = settled.changeBitrate;
        d.bitrateBps = settled.bitrateBps;
        return d;
    }

    void CommitBitrate(uint32_t bps) override {
        rate_.Commit(bps);
    }

    uint32_t bitrateBps() const override {
        return rate_.cur();
    }

    bool fecEnabled() const override {
        return fec_.armed();
    }

private:
    RateWindow rate_;
    FecArming fec_;
    uint32_t minRttMs_ = 0;
    uint64_t rttSum_ = 0;
    uint32_t samples_ = 0;
    uint64_t lastDecreaseUs_ = 0;
};

}

std::unique_ptr<CongestionControl> MakeDelayTrend(uint32_t startBps, uint32_t minBps) {
    return std::make_unique<DelayTrendControl>(startBps, minBps);
}

}

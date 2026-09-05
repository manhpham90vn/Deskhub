#include "CcControls.h"

#include "CcShared.h"

#include <cstdint>

namespace deskhub::cc {
namespace {

inline constexpr uint32_t kQueueDelayLowMs = 30;
inline constexpr uint32_t kScreamHeadroomPct = 110;

class ScreamControl final : public CongestionControl {
public:
    ScreamControl(uint32_t startBps, uint32_t minBps) : rate_(startBps, minBps) {}

    std::string_view Name() const override {
        return "scream";
    }

    BitrateDecision Update(const Feedback& fb, uint32_t frameAgeMs, uint64_t nowUs) override {
        BitrateDecision d;
        const bool before = fec_.armed();
        d.fecEnabled = fec_.Update(fb.lossPct);
        d.fecToggled = d.fecEnabled != before;
        d.fecParityPerGroup = FecParityRowsFor(fb.lossPct);

        if (fb.rttMs && (minRttMs_ == 0 || fb.rttMs < minRttMs_)) minRttMs_ = fb.rttMs;
        const uint32_t queueMs = fb.rttMs > minRttMs_ ? fb.rttMs - minRttMs_ : 0;
        const uint64_t arrivedBps = uint64_t(fb.recvBitrateKbps) * 1000;

        uint32_t want = rate_.cur();
        if (queueMs >= kQueueDelayHighMs || fb.lossPct >= 5 ||
            frameAgeMs >= BitrateController::kSevereBacklogMs) {
            want = arrivedBps ? uint32_t(arrivedBps / 2) : rate_.cur() - rate_.cur() / 4;
            lastDecreaseUs_ = nowUs;
        } else if (queueMs >= kQueueDelayLowMs || fb.lossPct >= 2 ||
                   frameAgeMs >= BitrateController::kBacklogMs) {
            want = arrivedBps ? uint32_t(arrivedBps) : rate_.cur() - rate_.cur() / 10;
            lastDecreaseUs_ = nowUs;
        } else if (nowUs - lastDecreaseUs_ > kHoldAfterDecreaseUs) {
            const uint64_t headroom = arrivedBps * kScreamHeadroomPct / 100;
            want = uint32_t(std::max<uint64_t>(headroom, rate_.cur() + rate_.max() / 20));
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
    uint64_t lastDecreaseUs_ = 0;
};

}

std::unique_ptr<CongestionControl> MakeScream(uint32_t startBps, uint32_t minBps) {
    return std::make_unique<ScreamControl>(startBps, minBps);
}

}

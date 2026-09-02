#include "CcControls.h"

#include "CcShared.h"

#include <algorithm>
#include <cstdint>
#include <memory>

namespace deskhub::cc {
namespace {

class HybridControl final : public CongestionControl {
public:
    HybridControl(uint32_t startBps, uint32_t minBps)
        : loss_(MakeAimd(startBps, minBps)), delay_(MakeDelayTrend(startBps, minBps)),
          cur_(startBps) {}

    std::string_view Name() const override {
        return "hybrid";
    }

    BitrateDecision Update(const Feedback& fb, uint32_t frameAgeMs, uint64_t nowUs) override {
        const BitrateDecision byLoss = loss_->Update(fb, frameAgeMs, nowUs);
        const BitrateDecision byDelay = delay_->Update(fb, frameAgeMs, nowUs);

        BitrateDecision d;
        d.fecEnabled = byLoss.fecEnabled || byDelay.fecEnabled;
        d.fecParityPerGroup = std::max(byLoss.fecParityPerGroup, byDelay.fecParityPerGroup);
        d.fecToggled = d.fecEnabled != fec_;
        fec_ = d.fecEnabled;

        const uint32_t want = std::min(byLoss.bitrateBps, byDelay.bitrateBps);
        const uint32_t delta = want > cur_ ? want - cur_ : cur_ - want;
        d.changeBitrate = (want != cur_) && (delta >= cur_ / (100 / kMinReportedChangePct));
        d.bitrateBps = d.changeBitrate ? want : cur_;
        return d;
    }

    void CommitBitrate(uint32_t bps) override {
        cur_ = bps;
        loss_->CommitBitrate(bps);
        delay_->CommitBitrate(bps);
    }

    uint32_t bitrateBps() const override {
        return cur_;
    }

    bool fecEnabled() const override {
        return fec_;
    }

private:
    std::unique_ptr<CongestionControl> loss_;
    std::unique_ptr<CongestionControl> delay_;
    uint32_t cur_;
    bool fec_ = true;
};

}

std::unique_ptr<CongestionControl> MakeHybrid(uint32_t startBps, uint32_t minBps) {
    return std::make_unique<HybridControl>(startBps, minBps);
}

}

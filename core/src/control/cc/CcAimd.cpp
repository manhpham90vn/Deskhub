#include "CcControls.h"

#include "CcShared.h"

#include <cstdint>

namespace deskhub::cc {
namespace {

class AimdControl final : public CongestionControl {
public:
    AimdControl(uint32_t startBps, uint32_t minBps) : inner_(startBps, minBps) {}

    std::string_view Name() const override {
        return "aimd";
    }

    BitrateDecision Update(const Feedback& fb, uint32_t frameAgeMs, uint64_t nowUs) override {
        return inner_.Update(fb, frameAgeMs, nowUs);
    }

    void CommitBitrate(uint32_t bps) override {
        inner_.CommitBitrate(bps);
    }

    uint32_t bitrateBps() const override {
        return inner_.bitrateBps();
    }

    bool fecEnabled() const override {
        return inner_.fecEnabled();
    }

private:
    BitrateController inner_;
};

}

std::unique_ptr<CongestionControl> MakeAimd(uint32_t startBps, uint32_t minBps) {
    return std::make_unique<AimdControl>(startBps, minBps);
}

}

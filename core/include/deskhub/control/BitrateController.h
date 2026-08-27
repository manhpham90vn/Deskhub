#pragma once
#include <cstdint>

#include "deskhub/protocol/Wire.h"

namespace deskhub {

struct BitrateDecision {
    uint32_t bitrateBps = 0;
    bool changeBitrate = false;
    bool fecEnabled = false;
    bool fecToggled = false;
};

class BitrateController {
public:
    static constexpr int kCleanSecondsBeforeDroppingFec = 10;
    static constexpr uint32_t kBacklogMs = 150;
    static constexpr uint32_t kSevereBacklogMs = 400;

    BitrateController(uint32_t startBps, uint32_t minBps)
        : cur_(startBps), max_(startBps), min_(minBps) {}

    BitrateDecision Update(const Feedback& fb, uint32_t frameAgeMs, uint64_t nowUs);

    void CommitBitrate(uint32_t bps) {
        cur_ = bps;
    }

    uint32_t bitrateBps() const {
        return cur_;
    }
    bool fecEnabled() const {
        return fec_;
    }

private:
    uint32_t cur_;
    uint32_t max_;
    uint32_t min_;

    uint64_t lastDecreaseUs_ = 0;
    int cleanSeconds_ = 0;
    bool fec_ = true;
};

}

#pragma once
#include "deskhub/control/CongestionControl.h"

#include <algorithm>
#include <cstdint>

namespace deskhub::cc {

inline constexpr uint32_t kQueueDelayHighMs = 100;
inline constexpr uint64_t kHoldAfterDecreaseUs = 2'000'000;
inline constexpr uint32_t kMinReportedChangePct = 2;

class FecArming {
public:
    bool Update(uint8_t lossPct) {
        if (lossPct >= 1) {
            cleanSeconds_ = 0;
            armed_ = true;
        } else if (++cleanSeconds_ >= BitrateController::kCleanSecondsBeforeDroppingFec) {
            armed_ = false;
        }
        return armed_;
    }

    bool armed() const {
        return armed_;
    }

private:
    int cleanSeconds_ = 0;
    bool armed_ = true;
};

class RateWindow {
public:
    RateWindow(uint32_t startBps, uint32_t minBps)
        : cur_(startBps), max_(startBps), min_(minBps) {}

    uint32_t Clamp(uint32_t want) const {
        return std::clamp(want, min_, max_);
    }

    BitrateDecision Settle(uint32_t want) {
        BitrateDecision d;
        const uint32_t next = Clamp(want);
        const uint32_t delta = next > cur_ ? next - cur_ : cur_ - next;
        d.changeBitrate = (next != cur_) && (delta >= cur_ / (100 / kMinReportedChangePct));
        d.bitrateBps = d.changeBitrate ? next : cur_;
        return d;
    }

    void Commit(uint32_t bps) {
        cur_ = bps;
    }

    uint32_t cur() const {
        return cur_;
    }
    uint32_t max() const {
        return max_;
    }

private:
    uint32_t cur_;
    uint32_t max_;
    uint32_t min_;
};

}

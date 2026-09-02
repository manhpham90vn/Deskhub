#pragma once
#include <cstdint>

#include "deskhub/control/ClockOffsetEstimator.h"

namespace deskhub {

class VideoPacer {
public:
    static constexpr uint64_t kDefaultLeadUs = 33'000;
    static constexpr uint64_t kResyncThresholdUs = 250'000;
    static constexpr uint64_t kStreamJumpUs = 2'000'000;
    static constexpr uint64_t kMinLeadUs = 8'000;
    static constexpr uint64_t kMaxLeadUs = 120'000;
    static constexpr uint64_t kJitterLeadMultiple = 3;
    static constexpr uint32_t kJitterFilterShift = 4;

    explicit VideoPacer(uint64_t leadUs = kDefaultLeadUs)
        : offset_(MakeClockOffsetEstimator(kDefaultClockOffset)), leadUs_(leadUs), baseLeadUs_(leadUs) {}

    bool SetClockOffset(std::string_view name) {
        std::unique_ptr<ClockOffsetEstimator> made = MakeClockOffsetEstimator(name);
        if (!made) return false;
        offset_ = std::move(made);
        return true;
    }

    std::string_view clockOffset() const {
        return offset_->Name();
    }

    void ObserveArrival(uint64_t ptsUs, uint64_t nowUs);

    void SetAdaptiveLead(bool on) {
        adaptive_ = on;
        if (!on) leadUs_ = baseLeadUs_;
    }

    void SetDisplayIntervalUs(uint64_t intervalUs) {
        displayIntervalUs_ = intervalUs;
    }

    uint64_t leadUs() const {
        return leadUs_;
    }

    uint64_t jitterUs() const {
        return jitterUs_;
    }

    bool ready() const {
        return offset_->ready();
    }

    int64_t DesiredTimebaseUs(uint64_t nowUs) const;

    uint64_t DisplayTimeUs(uint64_t ptsUs, uint64_t nowUs) const;

    bool NeedsResync(int64_t currentTimebaseUs, uint64_t nowUs) const;

    void Reset() {
        offset_->Reset();
        jitterUs_ = 0;
        lastRawUs_ = 0;
        haveRaw_ = false;
        if (adaptive_) leadUs_ = baseLeadUs_;
    }

private:
    std::unique_ptr<ClockOffsetEstimator> offset_;
    uint64_t leadUs_;
    uint64_t baseLeadUs_;
    uint64_t displayIntervalUs_ = 0;
    uint64_t jitterUs_ = 0;
    int64_t lastRawUs_ = 0;
    bool haveRaw_ = false;
    bool adaptive_ = false;
};

}

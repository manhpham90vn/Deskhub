#include "ClockEstimators.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace deskhub::clock {
namespace {

inline constexpr size_t kTrendSamples = 256;

class TrendlineEstimator final : public ClockOffsetEstimator {
public:
    std::string_view Name() const override {
        return "trendline";
    }

    void AddSample(uint64_t hostPtsUs, uint64_t localUs) override {
        const int64_t raw = int64_t(localUs) - int64_t(hostPtsUs);
        lastRaw_ = raw;

        if (!ready_) {
            ready_ = true;
            firstLocalUs_ = localUs;
            lowest_ = raw;
        }

        const double seconds = double(localUs - firstLocalUs_) / 1e6;
        if (count_ < kTrendSamples) ++count_;
        const double weight = 1.0 / double(count_);
        sumT_ += (seconds - sumT_) * weight;
        sumY_ += (double(raw) - sumY_) * weight;
        sumTT_ += (seconds * seconds - sumTT_) * weight;
        sumTY_ += (seconds * double(raw) - sumTY_) * weight;

        const double varT = sumTT_ - sumT_ * sumT_;
        slopePerSecond_ = varT > 1e-9 ? (sumTY_ - sumT_ * sumY_) / varT : 0.0;

        const double drift = slopePerSecond_ * seconds;
        const int64_t detrended = raw - int64_t(drift);
        lowest_ = std::min(lowest_, detrended);
        lastSeconds_ = seconds;
    }

    bool ready() const override {
        return ready_;
    }

    int64_t LatencyUs(uint64_t netFloorUs) const override {
        if (!ready_) return -1;
        const int64_t detrended = lastRaw_ - int64_t(slopePerSecond_ * lastSeconds_);
        const int64_t excess = std::max<int64_t>(0, detrended - lowest_);
        return excess + int64_t(netFloorUs);
    }

    int64_t floorUs() const override {
        return lowest_ + int64_t(slopePerSecond_ * lastSeconds_);
    }

    void Reset() override {
        ready_ = false;
        count_ = 0;
        sumT_ = sumY_ = sumTT_ = sumTY_ = 0.0;
        slopePerSecond_ = 0.0;
        lastSeconds_ = 0.0;
        lastRaw_ = 0;
        lowest_ = 0;
        firstLocalUs_ = 0;
    }

private:
    bool ready_ = false;
    size_t count_ = 0;
    double sumT_ = 0.0, sumY_ = 0.0, sumTT_ = 0.0, sumTY_ = 0.0;
    double slopePerSecond_ = 0.0;
    double lastSeconds_ = 0.0;
    int64_t lastRaw_ = 0;
    int64_t lowest_ = 0;
    uint64_t firstLocalUs_ = 0;
};

}

std::unique_ptr<ClockOffsetEstimator> MakeTrendline() {
    return std::make_unique<TrendlineEstimator>();
}

}

#include "ClockEstimators.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace deskhub::clock {
namespace {

inline constexpr int64_t kKalmanMeasurementNoise = 4000;
inline constexpr int64_t kKalmanProcessNoise = 4;

class KalmanEstimator final : public ClockOffsetEstimator {
public:
    std::string_view Name() const override {
        return "kalman";
    }

    void AddSample(uint64_t hostPtsUs, uint64_t localUs) override {
        const int64_t raw = int64_t(localUs) - int64_t(hostPtsUs);
        lastRaw_ = raw;

        if (!ready_) {
            ready_ = true;
            estimate_ = raw;
            variance_ = kKalmanMeasurementNoise;
            lowest_ = raw;
            return;
        }

        variance_ += kKalmanProcessNoise;
        const int64_t gainNumerator = variance_;
        const int64_t gainDenominator = variance_ + kKalmanMeasurementNoise;
        const int64_t innovation = raw - estimate_;
        estimate_ += innovation * gainNumerator / gainDenominator;
        variance_ = variance_ * kKalmanMeasurementNoise / gainDenominator;

        lowest_ = std::min(lowest_, estimate_);
    }

    bool ready() const override {
        return ready_;
    }

    int64_t LatencyUs(uint64_t netFloorUs) const override {
        if (!ready_) return -1;
        const int64_t excess = std::max<int64_t>(0, lastRaw_ - lowest_);
        return excess + int64_t(netFloorUs);
    }

    int64_t floorUs() const override {
        return lowest_;
    }

    void Reset() override {
        ready_ = false;
        estimate_ = 0;
        variance_ = 0;
        lowest_ = 0;
        lastRaw_ = 0;
    }

private:
    bool ready_ = false;
    int64_t estimate_ = 0;
    int64_t variance_ = 0;
    int64_t lowest_ = 0;
    int64_t lastRaw_ = 0;
};

}

std::unique_ptr<ClockOffsetEstimator> MakeKalman() {
    return std::make_unique<KalmanEstimator>();
}

}

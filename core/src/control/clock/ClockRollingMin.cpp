#include "ClockEstimators.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace deskhub::clock {
namespace {

class RollingMinEstimator final : public ClockOffsetEstimator {
public:
    std::string_view Name() const override {
        return "rolling-min";
    }

    void AddSample(uint64_t hostPtsUs, uint64_t localUs) override {
        inner_.AddSample(hostPtsUs, localUs);
    }

    bool ready() const override {
        return inner_.ready();
    }

    int64_t LatencyUs(uint64_t netFloorUs) const override {
        return inner_.LatencyUs(netFloorUs);
    }

    int64_t floorUs() const override {
        return inner_.floorUs();
    }

    void Reset() override {
        inner_.Reset();
    }

private:
    ClockOffset inner_;
};

}

std::unique_ptr<ClockOffsetEstimator> MakeRollingMin() {
    return std::make_unique<RollingMinEstimator>();
}

}

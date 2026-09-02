#pragma once
#include "deskhub/control/ClockOffset.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace deskhub {

class ClockOffsetEstimator {
public:
    virtual ~ClockOffsetEstimator() = default;

    virtual std::string_view Name() const = 0;

    virtual void AddSample(uint64_t hostPtsUs, uint64_t localUs) = 0;

    virtual bool ready() const = 0;

    virtual int64_t LatencyUs(uint64_t netFloorUs = 0) const = 0;

    virtual int64_t floorUs() const = 0;

    virtual void Reset() = 0;
};

inline constexpr std::string_view kDefaultClockOffset = "rolling-min";

std::unique_ptr<ClockOffsetEstimator> MakeClockOffsetEstimator(std::string_view name);

std::span<const std::string_view> ClockOffsetEstimatorNames();

bool IsClockOffsetEstimatorName(std::string_view name);

}

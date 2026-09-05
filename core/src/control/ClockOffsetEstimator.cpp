#include "deskhub/control/ClockOffsetEstimator.h"

#include "clock/ClockEstimators.h"

namespace deskhub {
namespace {

constexpr std::string_view kNames[] = {"rolling-min", "trendline", "kalman"};

}

std::unique_ptr<ClockOffsetEstimator> MakeClockOffsetEstimator(std::string_view name) {
    if (name == "rolling-min") return clock::MakeRollingMin();
    if (name == "trendline") return clock::MakeTrendline();
    if (name == "kalman") return clock::MakeKalman();
    return nullptr;
}

std::span<const std::string_view> ClockOffsetEstimatorNames() {
    return kNames;
}

bool IsClockOffsetEstimatorName(std::string_view name) {
    for (std::string_view known : kNames)
        if (known == name) return true;
    return false;
}

}

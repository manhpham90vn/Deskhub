#pragma once
#include "deskhub/control/ClockOffsetEstimator.h"

#include <memory>

namespace deskhub::clock {

std::unique_ptr<ClockOffsetEstimator> MakeRollingMin();

std::unique_ptr<ClockOffsetEstimator> MakeTrendline();

std::unique_ptr<ClockOffsetEstimator> MakeKalman();

}

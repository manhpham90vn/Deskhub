#pragma once
#include "deskhub/control/CongestionControl.h"

#include <cstdint>
#include <memory>

namespace deskhub::cc {

std::unique_ptr<CongestionControl> MakeAimd(uint32_t startBps, uint32_t minBps);

std::unique_ptr<CongestionControl> MakeDelayTrend(uint32_t startBps, uint32_t minBps);

std::unique_ptr<CongestionControl> MakeScream(uint32_t startBps, uint32_t minBps);

std::unique_ptr<CongestionControl> MakeHybrid(uint32_t startBps, uint32_t minBps);

}

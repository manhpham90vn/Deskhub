#include "deskhub/control/CongestionControl.h"

#include "cc/CcControls.h"

namespace deskhub {
namespace {

constexpr std::string_view kNames[] = {"aimd", "delay-trend", "scream", "hybrid"};

}

std::unique_ptr<CongestionControl> MakeCongestionControl(std::string_view name,
    uint32_t startBps, uint32_t minBps) {
    if (name == "aimd") return cc::MakeAimd(startBps, minBps);
    if (name == "delay-trend") return cc::MakeDelayTrend(startBps, minBps);
    if (name == "scream") return cc::MakeScream(startBps, minBps);
    if (name == "hybrid") return cc::MakeHybrid(startBps, minBps);
    return nullptr;
}

std::span<const std::string_view> CongestionControlNames() {
    return kNames;
}

bool IsCongestionControlName(std::string_view name) {
    for (std::string_view known : kNames)
        if (known == name) return true;
    return false;
}

}

#pragma once
#include "deskhub/control/BitrateController.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace deskhub {

class CongestionControl {
public:
    virtual ~CongestionControl() = default;

    virtual std::string_view Name() const = 0;

    virtual BitrateDecision Update(const Feedback& fb, uint32_t frameAgeMs, uint64_t nowUs) = 0;

    virtual void CommitBitrate(uint32_t bps) = 0;

    virtual uint32_t bitrateBps() const = 0;

    virtual bool fecEnabled() const = 0;
};

inline constexpr std::string_view kDefaultCongestionControl = "aimd";

std::unique_ptr<CongestionControl> MakeCongestionControl(std::string_view name,
    uint32_t startBps, uint32_t minBps);

std::span<const std::string_view> CongestionControlNames();

bool IsCongestionControlName(std::string_view name);

}

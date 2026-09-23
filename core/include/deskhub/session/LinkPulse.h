#pragma once
#include "deskhub/protocol/Wire.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace deskhub {

inline constexpr uint64_t kLinkPingIntervalUs = 1'000'000;
inline constexpr uint64_t kLinkStallAfterUs = 5'000'000;
inline constexpr uint64_t kLinkWatchStepUs = 250'000;
inline constexpr size_t kLinkPingWindow = 10;

class LinkPulse {
public:
    void Reset();
    void Tick(uint64_t nowUs);
    bool PingDue(uint64_t nowUs) const;
    PingPong MakePing(uint64_t nowUs);
    void OnPong(const PingPong& pong, uint64_t nowUs);
    bool Stalled(uint64_t nowUs) const;

private:
    struct SentPing {
        uint32_t pingId = 0;
        uint64_t sentUs = 0;
        bool answered = false;
    };

    std::array<SentPing, kLinkPingWindow> sent_{};
    uint32_t nextPingId_ = 1;
    uint64_t lastTickUs_ = 0;
    uint64_t lastPingUs_ = 0;
    uint64_t lastPongUs_ = 0;
    bool answeredOnce_ = false;
};

}

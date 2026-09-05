#pragma once
#include <cstdint>

namespace deskhub {

inline constexpr uint32_t kKeepalivesPerIdleTimeout = 3;
inline constexpr uint64_t kMinKeepaliveIntervalUs = 1'000'000;

uint64_t KeepaliveIntervalUs(uint64_t idleTimeoutMs);
bool KeepaliveDue(uint64_t nowUs, uint64_t lastSentUs, uint64_t intervalUs);

inline constexpr uint64_t kReconnectFirstDelayUs = 500'000;
inline constexpr uint64_t kReconnectMaxDelayUs = 5'000'000;
inline constexpr uint64_t kViewerReattachGraceUs = 60'000'000;

uint64_t ReconnectDelayUs(uint32_t attempt, uint32_t jitter);
bool ReconnectStillWorthTrying(uint64_t sinceLossUs, uint64_t graceUs);

}

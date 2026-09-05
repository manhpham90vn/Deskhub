#include "deskhub/session/LinkRecovery.h"

namespace deskhub {

uint64_t KeepaliveIntervalUs(uint64_t idleTimeoutMs) {
    const uint64_t idleUs = idleTimeoutMs * 1000;
    if (idleUs == 0) return kMinKeepaliveIntervalUs;

    uint64_t interval = idleUs / kKeepalivesPerIdleTimeout;
    if (interval < kMinKeepaliveIntervalUs) interval = kMinKeepaliveIntervalUs;

    const uint64_t half = idleUs / 2;
    if (half != 0 && interval > half) interval = half;
    return interval == 0 ? 1 : interval;
}

bool KeepaliveDue(uint64_t nowUs, uint64_t lastSentUs, uint64_t intervalUs) {
    if (nowUs <= lastSentUs) return false;
    return nowUs - lastSentUs >= intervalUs;
}

uint64_t ReconnectDelayUs(uint32_t attempt, uint32_t jitter) {
    uint64_t delay = kReconnectFirstDelayUs;
    for (uint32_t doubled = 0; doubled < attempt && delay < kReconnectMaxDelayUs; ++doubled)
        delay *= 2;
    if (delay > kReconnectMaxDelayUs) delay = kReconnectMaxDelayUs;

    const uint64_t floor = delay / 2;
    const uint64_t spread = delay - floor;
    return floor + (uint64_t(jitter) * spread) / 0xFFFFFFFFull;
}

bool ReconnectStillWorthTrying(uint64_t sinceLossUs, uint64_t graceUs) {
    return sinceLossUs < graceUs;
}

}

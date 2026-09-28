#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/auth/FailureLimiter.h"

#include <cstdio>

void RunAuthFailureLimiterTests() {
    std::printf("[auth] failed signatures pause one key and source for a bounded time...\n");
    deskhub::AuthFailureLimiter limiter;
    deskhub::Fingerprint key{};
    key.bytes.fill(0x11);
    deskhub::Fingerprint otherKey{};
    otherKey.bytes.fill(0x22);
    constexpr uint32_t ip = 0x7F000001;
    constexpr uint64_t startUs = 1'000'000;

    for (size_t i = 0; i + 1 < deskhub::kAuthFailureLimit; ++i) {
        limiter.RecordFailure(key, ip, startUs + i);
        Check(limiter.Allow(key, ip, startUs + i),
            "a small number of mistakes does not block the key");
    }
    const uint64_t blockedAtUs = startUs + deskhub::kAuthFailureLimit;
    limiter.RecordFailure(key, ip, blockedAtUs);
    Check(!limiter.Allow(key, ip, blockedAtUs), "repeated bad signatures trigger a pause");
    Check(limiter.Allow(otherKey, ip, blockedAtUs), "another key at the same IP stays usable");
    Check(limiter.Allow(key, ip + 1, blockedAtUs), "the same key at another IP stays usable");
    Check(!limiter.Allow(key, ip, blockedAtUs + deskhub::kAuthFailureCooldownUs - 1),
        "the pause lasts through its final microsecond");
    Check(limiter.Allow(key, ip, blockedAtUs + deskhub::kAuthFailureCooldownUs),
        "the key can retry after the pause");

    limiter.RecordFailure(key, ip, blockedAtUs + deskhub::kAuthFailureCooldownUs);
    Check(limiter.Allow(key, ip, blockedAtUs + deskhub::kAuthFailureCooldownUs),
        "one later mistake does not immediately block the key again");
    limiter.RecordSuccess(key, ip);
    for (size_t i = 0; i + 1 < deskhub::kAuthFailureLimit; ++i)
        limiter.RecordFailure(key, ip, blockedAtUs + deskhub::kAuthFailureCooldownUs + i + 1);
    Check(limiter.Allow(key, ip, blockedAtUs + deskhub::kAuthFailureCooldownUs + 2),
        "a successful authentication clears previous failures");

    limiter.Clear();
    limiter.RecordFailure(key, ip, startUs);
    limiter.RecordFailure(key, ip, startUs + deskhub::kAuthFailureWindowUs + 1);
    Check(limiter.Allow(key, ip, startUs + deskhub::kAuthFailureWindowUs + 1),
        "failures outside the window do not accumulate");

    limiter.Clear();
    for (size_t i = 0; i < deskhub::kAuthFailureSlots; ++i) {
        deskhub::Fingerprint slotKey{};
        if (i == 0)
            slotKey = key;
        else
            slotKey.bytes[0] = uint8_t(i + 1);
        for (size_t failure = 0; failure < deskhub::kAuthFailureLimit; ++failure)
            limiter.RecordFailure(slotKey, ip, startUs + i);
    }
    Check(!limiter.Allow(key, ip, startUs + deskhub::kAuthFailureSlots),
        "a remembered blocked key remains blocked while the table is full");
    limiter.RecordFailure(otherKey, ip, startUs + deskhub::kAuthFailureSlots);
    Check(limiter.Allow(key, ip, startUs + deskhub::kAuthFailureSlots),
        "the oldest entry is evicted when the fixed-size table fills");
}

#pragma once
#include "deskhub/net/TrustStore.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace deskhub {

inline constexpr size_t kAuthFailureLimit = 3;
inline constexpr uint64_t kAuthFailureWindowUs = 60'000'000;
inline constexpr uint64_t kAuthFailureCooldownUs = 10'000'000;
inline constexpr size_t kAuthFailureSlots = 64;

class AuthFailureLimiter {
public:
    bool Allow(const Fingerprint& key, uint32_t ip, uint64_t nowUs) const;
    void RecordFailure(const Fingerprint& key, uint32_t ip, uint64_t nowUs);
    void RecordSuccess(const Fingerprint& key, uint32_t ip);
    void Clear();

private:
    struct Entry {
        Fingerprint key{};
        uint32_t ip = 0;
        size_t failures = 0;
        uint64_t lastFailureUs = 0;
        uint64_t blockedUntilUs = 0;
        bool occupied = false;
    };

    std::array<Entry, kAuthFailureSlots> entries_{};
};

}

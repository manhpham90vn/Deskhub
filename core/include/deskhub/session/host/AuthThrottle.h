#pragma once
#include <algorithm>
#include <cstdint>

namespace deskhub {

inline constexpr uint32_t kMaxPasscodeAttempts = 3;
inline constexpr uint64_t kPasscodeLockoutUs = 30'000'000;
inline constexpr uint64_t kMaxPasscodeLockoutUs = 3'600'000'000;
inline constexpr uint32_t kMaxLockoutDoublings = 16;

class AuthThrottle {
public:
    bool Locked(uint64_t nowUs) const {
        return nowUs < lockUntilUs_;
    }

    void RecordFailure(uint64_t nowUs) {
        if (++wrongAttempts_ < kMaxPasscodeAttempts) return;
        wrongAttempts_ = 0;
        lockUntilUs_ = nowUs + NextLockoutUs();
        if (lockouts_ < kMaxLockoutDoublings) ++lockouts_;
    }

    void RecordSuccess() {
        wrongAttempts_ = 0;
        lockouts_ = 0;
    }

private:
    uint64_t NextLockoutUs() const {
        const uint32_t doublings = std::min(lockouts_, kMaxLockoutDoublings);
        return std::min(kPasscodeLockoutUs << doublings, kMaxPasscodeLockoutUs);
    }

    uint32_t wrongAttempts_ = 0;
    uint32_t lockouts_ = 0;
    uint64_t lockUntilUs_ = 0;
};

}

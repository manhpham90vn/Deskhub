#pragma once
#include <cstdint>

namespace deskhub {

inline constexpr uint32_t kMaxPasscodeAttempts = 3;
inline constexpr uint64_t kPasscodeLockoutUs = 30'000'000;

class AuthThrottle {
public:
    bool Locked(uint64_t nowUs) const {
        return nowUs < lockUntilUs_;
    }

    void RecordFailure(uint64_t nowUs) {
        if (++wrongAttempts_ < kMaxPasscodeAttempts) return;
        wrongAttempts_ = 0;
        lockUntilUs_ = nowUs + kPasscodeLockoutUs;
    }

    void RecordSuccess() {
        wrongAttempts_ = 0;
    }

private:
    uint32_t wrongAttempts_ = 0;
    uint64_t lockUntilUs_ = 0;
};

}

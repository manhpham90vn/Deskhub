#pragma once
#include <cstdint>

namespace deskhub::ui {

inline constexpr uint32_t kAutoShareProbeMs = 500;
inline constexpr uint32_t kAutoShareGiveUpMs = 30000;

enum class AutoShareStep : uint8_t {
    KeepWaiting,
    ShareNow,
    GiveUpWaiting,
};

AutoShareStep NextAutoShareStep(bool displaysReady, uint32_t waitedMs, uint32_t probeMs,
    uint32_t giveUpMs);

class AutoShareGate {
public:
    AutoShareGate() = default;
    AutoShareGate(uint32_t probeMs, uint32_t giveUpMs);

    AutoShareStep Advance(bool displaysReady);

    uint32_t ProbeMs() const {
        return probeMs_;
    }
    uint32_t WaitedMs() const {
        return waitedMs_;
    }
    bool Decided() const {
        return decided_;
    }

private:
    AutoShareStep Decide(AutoShareStep step);

    uint32_t probeMs_ = kAutoShareProbeMs;
    uint32_t giveUpMs_ = kAutoShareGiveUpMs;
    uint32_t waitedMs_ = 0;
    bool decided_ = false;
    AutoShareStep decision_ = AutoShareStep::KeepWaiting;
};

}

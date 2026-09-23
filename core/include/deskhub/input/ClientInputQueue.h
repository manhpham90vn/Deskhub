#pragma once
#include "deskhub/input/PointerMap.h"
#include "deskhub/protocol/Wire.h"

#include <atomic>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

namespace deskhub {

inline constexpr uint64_t kTapHoldUs = 50'000;

class ClientInputQueue {
public:
    void Key(int32_t vk, int32_t scan, bool down, uint64_t nowUs);

    void KeyTap(int32_t vk, int32_t scan, uint64_t nowUs);

    void KeyChord(int32_t modVk, int32_t modScan, int32_t vk, int32_t scan, uint64_t nowUs);

    bool CharTap(uint32_t codepoint, uint64_t nowUs);

    void MouseMoveAbsolute(int32_t nx, int32_t ny, uint64_t nowUs);

    void MouseMoveRelative(int32_t dx, int32_t dy, uint64_t nowUs);

    void MouseButtonEvent(int32_t button, bool down, uint64_t nowUs);

    void MouseWheel(int32_t delta, uint64_t nowUs);

    void ReleaseAll(uint64_t nowUs);

    void Drain(uint64_t nowUs, std::vector<InputEvent>& out);

    bool wantsFocus() const {
        return wantFocus_.load(std::memory_order_acquire);
    }

private:
    void PushLocked(const InputEvent& e);
    static InputEvent KeyEvent(int32_t vk, int32_t scan, bool down, uint64_t nowUs);

    mutable std::mutex mutex_;
    std::vector<InputEvent> ready_;
    std::vector<std::pair<uint64_t, InputEvent>> delayed_;
    std::map<int32_t, int32_t> keysDown_;
    std::set<int32_t> buttonsDown_;
    std::atomic<bool> wantFocus_{false};
};

}

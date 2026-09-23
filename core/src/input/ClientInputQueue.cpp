#include "deskhub/input/ClientInputQueue.h"

#include "deskhub/input/KeyMap.h"
#include "deskhub/input/Set1Scancodes.h"

namespace deskhub {

InputEvent ClientInputQueue::KeyEvent(int32_t vk, int32_t scan, bool down, uint64_t nowUs) {
    InputEvent e;
    e.type = InputType::Key;
    e.timestampUs = nowUs;
    e.a = vk;
    e.b = NeedsVirtualKeyInjection(vk) ? 0 : scan;
    e.state = down ? 1 : 0;
    return e;
}

void ClientInputQueue::PushLocked(const InputEvent& e) {
    ready_.push_back(e);
    wantFocus_.store(true, std::memory_order_release);
}

void ClientInputQueue::Key(int32_t vk, int32_t scan, bool down, uint64_t nowUs) {
    const InputEvent e = KeyEvent(vk, scan, down, nowUs);
    std::lock_guard<std::mutex> lk(mutex_);
    if (down)
        keysDown_[vk] = scan;
    else
        keysDown_.erase(vk);
    PushLocked(e);
}

void ClientInputQueue::KeyTap(int32_t vk, int32_t scan, uint64_t nowUs) {
    std::lock_guard<std::mutex> lk(mutex_);
    PushLocked(KeyEvent(vk, scan, true, nowUs));
    delayed_.emplace_back(nowUs + kTapHoldUs, KeyEvent(vk, scan, false, nowUs));
}

void ClientInputQueue::KeyChord(int32_t modVk, int32_t modScan, int32_t vk, int32_t scan,
    uint64_t nowUs) {
    std::lock_guard<std::mutex> lk(mutex_);
    PushLocked(KeyEvent(modVk, modScan, true, nowUs));
    PushLocked(KeyEvent(vk, scan, true, nowUs));
    delayed_.emplace_back(nowUs + kTapHoldUs, KeyEvent(vk, scan, false, nowUs));
    delayed_.emplace_back(nowUs + kTapHoldUs, KeyEvent(modVk, modScan, false, nowUs));
}

bool ClientInputQueue::CharTap(uint32_t codepoint, uint64_t nowUs) {
    const auto chord = CharToKeyChord(codepoint);
    if (!chord) return false;

    std::lock_guard<std::mutex> lk(mutex_);
    if (chord->shift) PushLocked(KeyEvent(kVkShift, 0, true, nowUs));
    PushLocked(KeyEvent(chord->vk, 0, true, nowUs));
    if (chord->shift) PushLocked(KeyEvent(kVkShift, 0, false, nowUs));
    delayed_.emplace_back(nowUs + kTapHoldUs, KeyEvent(chord->vk, 0, false, nowUs));
    return true;
}

void ClientInputQueue::MouseMoveAbsolute(int32_t nx, int32_t ny, uint64_t nowUs) {
    InputEvent e;
    e.type = InputType::MouseMove;
    e.timestampUs = nowUs;
    e.a = ClampAbsCoord(nx);
    e.b = ClampAbsCoord(ny);
    e.absolute = 1;
    std::lock_guard<std::mutex> lk(mutex_);
    PushLocked(e);
}

void ClientInputQueue::MouseMoveRelative(int32_t dx, int32_t dy, uint64_t nowUs) {
    if (dx == 0 && dy == 0) return;
    InputEvent e;
    e.type = InputType::MouseMove;
    e.timestampUs = nowUs;
    e.a = dx;
    e.b = dy;
    e.absolute = 0;
    std::lock_guard<std::mutex> lk(mutex_);
    PushLocked(e);
}

void ClientInputQueue::MouseButtonEvent(int32_t button, bool down, uint64_t nowUs) {
    InputEvent e;
    e.type = InputType::MouseButton;
    e.timestampUs = nowUs;
    e.a = button;
    e.state = down ? 1 : 0;
    std::lock_guard<std::mutex> lk(mutex_);
    if (down)
        buttonsDown_.insert(button);
    else
        buttonsDown_.erase(button);
    PushLocked(e);
}

void ClientInputQueue::MouseWheel(int32_t delta, uint64_t nowUs) {
    if (delta == 0) return;
    InputEvent e;
    e.type = InputType::MouseWheel;
    e.timestampUs = nowUs;
    e.a = 0;
    e.b = delta;
    std::lock_guard<std::mutex> lk(mutex_);
    PushLocked(e);
}

void ClientInputQueue::ReleaseAll(uint64_t nowUs) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& [vk, scan] : keysDown_) PushLocked(KeyEvent(vk, scan, false, nowUs));
    keysDown_.clear();

    for (int32_t button : buttonsDown_) {
        InputEvent e;
        e.type = InputType::MouseButton;
        e.timestampUs = nowUs;
        e.a = button;
        e.state = 0;
        PushLocked(e);
    }
    buttonsDown_.clear();
}

void ClientInputQueue::Drain(uint64_t nowUs, std::vector<InputEvent>& out) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto it = delayed_.begin(); it != delayed_.end();) {
        if (it->first <= nowUs) {
            ready_.push_back(it->second);
            it = delayed_.erase(it);
        } else {
            ++it;
        }
    }
    out.clear();
    out.swap(ready_);
}

}

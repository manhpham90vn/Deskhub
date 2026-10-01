#pragma once
#include "deskhub/input/PressedInputTracker.h"
#include "deskhub/input/Set1Scancodes.h"
#include "deskhub/protocol/Wire.h"

#include <cstdint>

namespace deskhub {

template <class Backend, class NativeKey>
class InputApplier {
public:
    uint64_t applied() const {
        return held_.applied();
    }
    uint64_t skipped() const {
        return held_.skipped();
    }

protected:
    Backend& backend() {
        return static_cast<Backend&>(*this);
    }

    bool DispatchInput(const InputEvent& e, bool localActive) {
        const InputGate gate = held_.Gate(localActive);
        if (!gate.allow) {
            if (gate.justSuppressed) {
                backend().OnLocalUserTookOver();
                backend().ReleaseAll();
            }
            return ReleaseEvenWhileSuppressed(e);
        }
        if (gate.justResumed) backend().OnLocalUserIdle();

        switch (e.type) {
            case InputType::Key:
                backend().SendKey(SidedModifier(e.a, e.b), e.b, e.state != 0);
                break;
            case InputType::MouseMove:
                if (e.absolute)
                    backend().SendMoveAbsolute(e.a, e.b);
                else
                    backend().SendMoveRelative(e.a, e.b);
                break;
            case InputType::MouseButton:
                backend().SendButton(MouseButton(e.a), e.state != 0);
                break;
            case InputType::MouseWheel:
                backend().SendWheel(e.b);
                break;
            default:
                return false;
        }

        held_.CountApplied();
        return true;
    }

    bool ReleaseEvenWhileSuppressed(const InputEvent& e) {
        if (e.state != 0) return false;
        if (e.type == InputType::Key) {
            const int32_t vk = SidedModifier(e.a, e.b);
            if (held_.FindKey(vk) == nullptr) return false;
            backend().SendKey(vk, e.b, false);
            return true;
        }
        if (e.type == InputType::MouseButton) {
            const MouseButton button = MouseButton(e.a);
            if (!held_.ButtonIsDown(button)) return false;
            backend().SendButton(button, false);
            return true;
        }
        return false;
    }

    void ReleaseAllHeld() {
        for (const auto& key : held_.TakeHeldKeys()) backend().ReleaseKey(key.id, key.native);
        for (MouseButton button : held_.TakeHeldButtons()) backend().SendButton(button, false);
    }

    PressedInputTracker<NativeKey> held_;
};

}

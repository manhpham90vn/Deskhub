#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace deskhub {

struct InputGate {
    bool allow = false;
    bool justSuppressed = false;
    bool justResumed = false;
};

template <class NativeKey>
class PressedInputTracker {
public:
    using KeyMapType = std::map<int32_t, NativeKey>;

    struct HeldKey {
        int32_t id = 0;
        NativeKey native{};
    };

    InputGate Gate(bool localActive) {
        InputGate g;
        if (localActive) {
            g.justSuppressed = !suppressed_;
            suppressed_ = true;
            ++skipped_;
            return g;
        }
        g.allow = true;
        g.justResumed = suppressed_;
        suppressed_ = false;
        return g;
    }

    void CountApplied() {
        ++applied_;
    }

    void SetKey(int32_t id, NativeKey native, bool down) {
        if (down)
            keys_[id] = native;
        else
            keys_.erase(id);
    }

    const NativeKey* FindKey(int32_t id) const {
        const auto it = keys_.find(id);
        return it == keys_.end() ? nullptr : &it->second;
    }

    void SetButton(MouseButton button, bool down) {
        if (down)
            buttons_.insert(button);
        else
            buttons_.erase(button);
    }

    bool ButtonIsDown(MouseButton button) const {
        return buttons_.count(button) != 0;
    }

    const KeyMapType& heldKeys() const {
        return keys_;
    }
    const std::set<MouseButton>& heldButtons() const {
        return buttons_;
    }

    std::vector<HeldKey> TakeHeldKeys() {
        std::vector<HeldKey> out;
        out.reserve(keys_.size());
        for (const auto& [id, native] : keys_) out.push_back(HeldKey{id, native});
        keys_.clear();
        return out;
    }

    std::vector<MouseButton> TakeHeldButtons() {
        const std::vector<MouseButton> out(buttons_.begin(), buttons_.end());
        buttons_.clear();
        return out;
    }

    bool nothingHeld() const {
        return keys_.empty() && buttons_.empty();
    }
    size_t heldKeyCount() const {
        return keys_.size();
    }
    size_t heldButtonCount() const {
        return buttons_.size();
    }

    uint64_t applied() const {
        return applied_;
    }
    uint64_t skipped() const {
        return skipped_;
    }

private:
    KeyMapType keys_;
    std::set<MouseButton> buttons_;
    bool suppressed_ = false;
    uint64_t applied_ = 0;
    uint64_t skipped_ = 0;
};

}

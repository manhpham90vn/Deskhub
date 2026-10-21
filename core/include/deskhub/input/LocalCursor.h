#pragma once
#include "deskhub/input/VirtualKeys.h"
#include "deskhub/ui/Strings.h"

namespace deskhub {

inline constexpr int32_t kViewerCursorToggleVk = kVkF1 + 8;
inline constexpr int32_t kViewerFullScreenToggleVk = kVkF1 + 10;

inline bool IsViewerShortcutVk(int32_t vk) {
    return vk == kViewerCursorToggleVk || vk == kViewerFullScreenToggleVk;
}

class LocalCursorState {
public:
    void Toggle() {
        hidden_ = !hidden_;
    }

    bool HidesOverVideo(bool overVideo) const {
        return hidden_ && overVideo;
    }

    const char* ButtonLabel() const {
        return CursorButtonLabel(hidden_);
    }

    static const char* CursorButtonLabel(bool hidden) {
        return hidden ? ui::kShowMyCursorButton : ui::kHideMyCursorButton;
    }

private:
    bool hidden_ = true;
};

}

#pragma once
#include "deskhub/ui/Strings.h"

namespace deskhub::media {

class AudioMuteState {
public:
    void Toggle() {
        muted_ = !muted_;
    }

    bool muted() const {
        return muted_;
    }

    const char* ButtonLabel() const {
        return SoundButtonLabel(muted_);
    }

    static const char* SoundButtonLabel(bool muted) {
        return muted ? ui::kUnmuteSoundButton : ui::kMuteSoundButton;
    }

private:
    bool muted_ = false;
};

}

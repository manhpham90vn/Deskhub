#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/media/AudioMute.h"

#include <cstdio>
#include <string>

using deskhub::media::AudioMuteState;
namespace ui = deskhub::ui;

namespace {

void TestStartsAudible() {
    std::printf("[mute] a new viewer plays the host's sound...\n");
    const AudioMuteState st;
    Check(!st.muted(), "sound starts on");
    Check(std::string(st.ButtonLabel()) == ui::kMuteSoundButton, "audible: offers to mute");
}

void TestToggleFlipsAndLabelsFollow() {
    std::printf("[mute] the toggle flips the sound and the button says what it will do...\n");
    AudioMuteState st;
    st.Toggle();
    Check(st.muted(), "one toggle mutes");
    Check(std::string(st.ButtonLabel()) == ui::kUnmuteSoundButton, "muted: offers to unmute");
    st.Toggle();
    Check(!st.muted(), "a second toggle brings the sound back");
    Check(std::string(st.ButtonLabel()) == ui::kMuteSoundButton, "and offers to mute again");
    Check(std::string(AudioMuteState::SoundButtonLabel(true)) == ui::kUnmuteSoundButton,
        "the static label agrees with the instance");
}

}

void RunAudioMuteTests() {
    TestStartsAudible();
    TestToggleFlipsAndLabelsFollow();
}

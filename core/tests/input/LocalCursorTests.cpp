#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/input/LocalCursor.h"
#include "deskhub/input/Set1Scancodes.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

void TestStartsHiddenOverTheVideo() {
    std::printf("[cursor] a new viewer shows only the host's pointer over the picture...\n");
    const LocalCursorState st;
    Check(st.HidesOverVideo(true), "the local pointer starts hidden over the video");
    Check(!st.HidesOverVideo(false), "but never over the bars or the window chrome");
}

void TestToggleFlipsAndLabelsFollow() {
    std::printf("[cursor] the toggle flips visibility and the button says what it will do...\n");
    LocalCursorState st;
    Check(std::string(st.ButtonLabel()) == ui::kShowMyCursorButton, "hidden: offers to show");
    st.Toggle();
    Check(!st.HidesOverVideo(true), "after one toggle the pointer is shown");
    Check(std::string(st.ButtonLabel()) == ui::kHideMyCursorButton, "shown: offers to hide");
    st.Toggle();
    Check(st.HidesOverVideo(true), "a second toggle hides it again");
    Check(std::string(LocalCursorState::CursorButtonLabel(false)) == ui::kHideMyCursorButton,
        "the static label agrees with the instance");
}

void TestShortcutKeysAreCanonical() {
    std::printf("[cursor] every platform agrees on the viewer's own shortcut keys...\n");
    Check(kViewerCursorToggleVk == kVkF1 + 8, "the cursor toggle is F9");
    Check(kViewerFullScreenToggleVk == kVkF1 + 10, "full screen is F11");
    Check(IsViewerShortcutVk(kViewerCursorToggleVk) && IsViewerShortcutVk(kViewerFullScreenToggleVk),
        "both are kept by the viewer");
    Check(!IsViewerShortcutVk(kVkF1) && !IsViewerShortcutVk(kVkEscape),
        "other keys still reach the host");
    Check(VkToSet1Scancode(kViewerCursorToggleVk) != 0 &&
              VkToSet1Scancode(kViewerFullScreenToggleVk) != 0,
        "both have real scancodes, so they could be forwarded if they were not consumed");
    Check(std::string(ui::kShowMyCursorButton).find("F9") != std::string::npos &&
              std::string(ui::kHideMyCursorButton).find("F9") != std::string::npos &&
              std::string(ui::kFullScreenButton).find("F11") != std::string::npos,
        "every button names its key");
}

}

void RunLocalCursorTests() {
    TestStartsHiddenOverTheVideo();
    TestToggleFlipsAndLabelsFollow();
    TestShortcutKeysAreCanonical();
}

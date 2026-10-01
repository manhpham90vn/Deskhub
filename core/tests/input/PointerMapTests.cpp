#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/input/InputApplier.h"
#include "deskhub/input/PointerMap.h"
#include "deskhub/media/ViewFit.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

void TestClamp() {
    std::printf("[pointer] out-of-range normalized coordinates clamp to the axis...\n");
    Check(ClampAbsCoord(-1) == 0, "negative clamps to 0");
    Check(ClampAbsCoord(0) == 0, "0 passes through");
    Check(ClampAbsCoord(kAbsCoordMax) == kAbsCoordMax, "the max passes through");
    Check(ClampAbsCoord(kAbsCoordMax + 1) == kAbsCoordMax, "over the max clamps down");
}

void TestPixelMapping() {
    std::printf("[pointer] normalized maps onto a pixel count and back...\n");
    Check(AbsCoordToPixel(0, 100, 1920) == 100, "0 lands on the first pixel");
    Check(AbsCoordToPixel(kAbsCoordMax, 100, 1920) == 100 + 1919,
        "the max lands on the last pixel, not one past it");
    Check(AbsCoordToPixel(kAbsCoordMax / 2, 0, 1920) == 959, "the midpoint lands mid-screen");
    Check(AbsCoordToPixel(30000, 0, 0) == 0, "an empty screen collapses to the origin");
    Check(AbsCoordToPixel(30000, 7, 1) == 7, "a one-pixel screen has only the origin to land on");
    Check(AbsCoordToPixel(-5, 40, 800) == 40, "a negative input clamps before mapping");

    Check(AxisToAbsCoord(100, 100, 1920) == 0, "the first pixel normalizes to 0");
    Check(AxisToAbsCoord(100 + 1919, 100, 1920) == kAbsCoordMax, "the last pixel saturates");
    Check(AxisToAbsCoord(50, 100, 1920) == 0, "left of the screen clamps to 0");
    Check(AxisToAbsCoord(500, 0, 0) == 0, "an empty screen normalizes to 0");

    const double mid = AbsCoordToAxis(kAbsCoordMax / 2, 0.0, 100.0);
    Check(mid > 49.9 && mid < 50.1, "the floating-point form agrees with the integer one");
}

void TestPixelMappingRoundTripsWithNormalizeAxis() {
    std::printf("[pointer] a tap and an injected pixel agree on which pixel they mean...\n");
    constexpr int64_t kOrigin = 0;
    constexpr int64_t kPixels = 1920;
    for (int64_t px : {int64_t(0), int64_t(1), int64_t(959), int64_t(1918), int64_t(1919)}) {
        const int32_t n = AxisToAbsCoord(px, kOrigin, kPixels);
        Check(AbsCoordToPixel(n, kOrigin, kPixels) == px,
            "every pixel survives the round trip through the wire coordinate");
    }

    Check(AxisToAbsCoord(1919, kOrigin, kPixels) == NormalizeAxis(1919.0, 1920.0),
        "the injector and the viewer normalize the same pixel identically");
    Check(AxisToAbsCoord(0, kOrigin, kPixels) == NormalizeAxis(0.0, 1920.0),
        "including the first one");
}

void TestX11ButtonMapping() {
    std::printf("[pointer] the X11 button numbers a viewer sees map onto the wire buttons...\n");
    MouseButton b = MouseButton::Left;
    Check(X11ButtonToMouseButton(kX11ButtonLeft, b) && b == MouseButton::Left, "1 is left");
    Check(X11ButtonToMouseButton(kX11ButtonMiddle, b) && b == MouseButton::Middle,
        "2 is middle, not right \xE2\x80\x94 X11 orders them differently from the wire");
    Check(X11ButtonToMouseButton(kX11ButtonRight, b) && b == MouseButton::Right, "3 is right");
    Check(X11ButtonToMouseButton(kX11ButtonBack, b) && b == MouseButton::X1, "8 is back");
    Check(X11ButtonToMouseButton(kX11ButtonForward, b) && b == MouseButton::X2, "9 is forward");
    Check(!X11ButtonToMouseButton(4, b), "the scroll pseudo-buttons are not buttons");
    Check(!X11ButtonToMouseButton(0, b), "and neither is nothing");
}

void TestWheelNotches() {
    std::printf("[pointer] a wheel delta becomes whole notches, carrying what is left...\n");
    int32_t carry = 0;
    Check(TakeWheelNotches(0, carry) == 0 && carry == 0, "no movement is no notches");
    Check(TakeWheelNotches(kWheelDeltaPerNotch, carry) == 1, "one detent is one notch");
    Check(TakeWheelNotches(-kWheelDeltaPerNotch, carry) == -1, "and the other way round");
    Check(TakeWheelNotches(3 * kWheelDeltaPerNotch, carry) == 3, "three detents are three notches");

    carry = 0;
    int32_t scrolled = 0;
    for (int i = 0; i < 12; ++i) scrolled += TakeWheelNotches(10, carry);
    Check(scrolled == 1 && carry == 0,
        "twelve smooth-scroll ticks of a twelfth add up to one notch, not twelve");
    Check(TakeWheelNotches(40, carry) == 0 && carry == 40, "a partial detent waits for the rest");
    Check(TakeWheelNotches(-30, carry) == 0 && carry == -30,
        "turning back drops what was carried the other way");
    Check(TakeWheelNotches(-90, carry) == -1 && carry == 0, "and scrolls once a whole notch is in");
}

void TestTouchScrollAccumulates() {
    std::printf("[pointer] a touch drag turns into whole notches and keeps the remainder...\n");
    double carry = 0;

    Check(TakeScrollNotches(kTouchPointsPerNotch / 4, carry) == 0,
        "a short drag is not a notch yet");
    Check(TakeScrollNotches(kTouchPointsPerNotch / 4, carry) == 0, "nor is the next one");
    Check(TakeScrollNotches(kTouchPointsPerNotch / 2, carry) == 1,
        "but together they cross one notch");
    Check(carry < 1e-9 && carry > -1e-9, "and nothing is left over");

    Check(TakeScrollNotches(3 * kTouchPointsPerNotch, carry) == 3,
        "a long drag emits every notch it crossed, so a flick does not lose scroll");

    carry = 0;
    Check(TakeScrollNotches(-kTouchPointsPerNotch, carry) == -1, "dragging back scrolls back");

    carry = 0;
    Check(TakeScrollNotches(kTouchPointsPerNotch * 1.5, carry) == 1, "1.5 notches emits one");
    Check(TakeScrollNotches(kTouchPointsPerNotch * 0.5, carry) == 1,
        "and the half that was carried completes the next one");

    carry = 0;
    TakeScrollNotches(kTouchPointsPerNotch * 0.9, carry);
    Check(TakeScrollNotches(-kTouchPointsPerNotch * 1.8, carry) == 0,
        "reversing mid-drag cancels the pending notch instead of firing it");
}

struct FakeInjector : InputApplier<FakeInjector, uint16_t> {
    std::vector<std::string> calls;
    bool localActive = false;

    bool Apply(const InputEvent& e) {
        return DispatchInput(e, localActive);
    }

    void OnLocalUserTookOver() {
        calls.push_back("suppress");
    }
    void OnLocalUserIdle() {
        calls.push_back("resume");
    }
    void ReleaseAll() {
        calls.push_back("release");
    }
    void SendKey(int32_t vk, int32_t native, bool down) {
        held_.SetKey(vk, uint16_t(native), down);
        calls.push_back(std::string("key:") + std::to_string(vk) + (down ? ":down" : ":up"));
    }
    void SendButton(MouseButton button, bool down) {
        held_.SetButton(button, down);
        calls.push_back(down ? "btn:down" : "btn:up");
    }
    void SendMoveAbsolute(int32_t, int32_t) {
        calls.push_back("move:abs");
    }
    void SendMoveRelative(int32_t, int32_t) {
        calls.push_back("move:rel");
    }
    void SendWheel(int32_t delta) {
        calls.push_back(std::string("wheel:") + std::to_string(delta));
    }
};

InputEvent Key(int32_t vk, bool down) {
    InputEvent e;
    e.type = InputType::Key;
    e.a = vk;
    e.b = 0x1E;
    e.state = down ? 1 : 0;
    return e;
}

InputEvent Move(bool absolute) {
    InputEvent e;
    e.type = InputType::MouseMove;
    e.absolute = absolute ? 1 : 0;
    return e;
}

InputEvent Wheel(int32_t delta) {
    InputEvent e;
    e.type = InputType::MouseWheel;
    e.b = delta;
    return e;
}

InputEvent Button(bool down) {
    InputEvent e;
    e.type = InputType::MouseButton;
    e.a = int32_t(MouseButton::Left);
    e.state = down ? 1 : 0;
    return e;
}

void TestDispatch() {
    std::printf("[applier] every event type reaches the matching backend call...\n");
    FakeInjector inj;

    Check(inj.Apply(Key('A', true)), "a key down is applied");
    Check(inj.Apply(Move(true)), "an absolute move is applied");
    Check(inj.Apply(Move(false)), "a relative move is applied");
    Check(inj.Apply(Wheel(240)), "a wheel event is applied");
    Check(inj.Apply(Button(true)), "a button down is applied");
    Check(inj.Apply(Button(false)), "a button up is applied");

    const std::vector<std::string> want{
        "key:65:down", "move:abs", "move:rel", "wheel:240", "btn:down", "btn:up"};
    Check(inj.calls == want, "the calls arrive in order with no extras");
    Check(inj.applied() == 6, "each applied event is counted");
    Check(inj.skipped() == 0, "and nothing was skipped");

    InputEvent junk;
    junk.type = InputType(0x7F);
    Check(!inj.Apply(junk), "an unknown event type is refused");
    Check(inj.applied() == 6, "and never counted as applied");
}

void TestHostWinsReleasesHeldInput() {
    std::printf("[applier] the local user taking over releases what the remote still holds...\n");
    FakeInjector inj;
    Check(inj.Apply(Key('A', true)), "remote holds a key");

    inj.calls.clear();
    inj.localActive = true;
    Check(!inj.Apply(Key('B', true)), "the next remote event is refused");
    Check(inj.calls == std::vector<std::string>({"suppress", "release"}),
        "the backend is told once and asked to drop everything it holds");

    inj.calls.clear();
    Check(!inj.Apply(Key('C', true)), "still refused while the local user is active");
    Check(inj.calls.empty(), "and the transition is not reported twice");
    Check(inj.skipped() == 2, "both refused events are counted as skipped");

    inj.localActive = false;
    Check(inj.Apply(Key('D', true)), "input resumes once the local user goes idle");
    Check(inj.calls == std::vector<std::string>({"resume", "key:68:down"}),
        "the resume is reported once, then the event goes through");
}

void TestTakeoverNeverStrandsAHeldKey() {
    std::printf("[applier] letting go is never suppressed, so no key is left stuck down...\n");
    FakeInjector inj;
    Check(inj.Apply(Key('A', true)), "remote presses a key while it may");
    Check(inj.Apply(Button(true)), "and holds a mouse button too");

    inj.localActive = true;
    inj.calls.clear();
    Check(inj.Apply(Key('A', false)),
        "the release still goes through the moment the local user takes over");
    Check(inj.calls == std::vector<std::string>({"suppress", "release", "key:65:up"}),
        "the takeover is reported, then the key is actually let go");

    inj.calls.clear();
    Check(!inj.Apply(Key('B', true)), "a fresh press stays refused: the host still wins");
    Check(inj.calls.empty(), "and nothing reaches the machine");

    inj.calls.clear();
    Check(inj.Apply(Button(false)), "a button the remote still holds is released as well");
    Check(inj.calls == std::vector<std::string>({"btn:up"}), "with no takeover noise repeated");

    inj.calls.clear();
    Check(!inj.Apply(Key('Z', false)), "a release for a key nobody holds does nothing");
    Check(inj.calls.empty(), "so a stray release never fabricates input");
}

}

namespace {

void TestLineScrollAlwaysMovesAtLeastOneNotch() {
    std::printf("[pmap] a coarse line-scroll tick always moves at least one notch...\n");
    Check(ScrollNotchesFromLines(0.0) == 0, "no movement, no notch");
    Check(ScrollNotchesFromLines(0.2) == 1, "a tiny positive tick still scrolls one notch");
    Check(ScrollNotchesFromLines(-0.2) == -1, "and a tiny negative one, downward");
    Check(ScrollNotchesFromLines(2.6) == 3, "bigger ticks round to the nearest notch");
    Check(ScrollNotchesFromLines(-2.6) == -3, "in both directions");
}

}

void RunPointerMapTests() {
    TestClamp();
    TestPixelMapping();
    TestPixelMappingRoundTripsWithNormalizeAxis();
    TestX11ButtonMapping();
    TestWheelNotches();
    TestTouchScrollAccumulates();
    TestDispatch();
    TestHostWinsReleasesHeldInput();
    TestTakeoverNeverStrandsAHeldKey();
    TestLineScrollAlwaysMovesAtLeastOneNotch();
}

#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/input/ClientInputQueue.h"
#include "deskhub/input/KeyMap.h"

#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

constexpr uint64_t kT0 = 1'000'000;

bool IsKey(const InputEvent& e, int32_t vk, int32_t scan, bool down) {
    return e.type == InputType::Key && e.a == vk && e.b == scan &&
           e.state == (down ? 1 : 0);
}

bool IsButton(const InputEvent& e, int32_t button, bool down) {
    return e.type == InputType::MouseButton && e.a == button && e.state == (down ? 1 : 0);
}

std::vector<InputEvent> Drain(ClientInputQueue& q, uint64_t nowUs) {
    std::vector<InputEvent> out;
    q.Drain(nowUs, out);
    return out;
}

void TestDrainIsReadAndClear() {
    std::printf("[cinput] Drain() hands over everything queued and empties the queue...\n");
    ClientInputQueue q;
    Check(Drain(q, kT0).empty(), "a fresh queue drains to nothing");
    Check(!q.wantsFocus(), "and asks for no focus");

    q.Key(0x41, 30, true, kT0);
    q.Key(0x41, 30, false, kT0 + 1);
    const auto batch = Drain(q, kT0 + 2);
    Check(batch.size() == 2, "both events came out in order");
    Check(IsKey(batch[0], 0x41, 30, true) && IsKey(batch[1], 0x41, 30, false), "down then up");
    Check(batch[0].timestampUs == kT0 && batch[1].timestampUs == kT0 + 1,
        "each event keeps the timestamp it was queued with");
    Check(Drain(q, kT0 + 3).empty(), "draining again yields nothing");

    std::vector<InputEvent> reused{InputEvent{}, InputEvent{}};
    q.Key(0x42, 48, true, kT0);
    q.Drain(kT0, reused);
    Check(reused.size() == 1, "Drain() clears the caller's vector instead of appending to it");
}

void TestFocusFlag() {
    std::printf("[cinput] any queued input asks the session for focus...\n");
    ClientInputQueue q;
    q.MouseMoveRelative(0, 0, kT0);
    Check(!q.wantsFocus(), "a move that was dropped as a no-op does not ask for focus");
    q.MouseWheel(0, kT0);
    Check(!q.wantsFocus(), "neither does a zero wheel delta");

    q.MouseMoveRelative(3, 4, kT0);
    Check(q.wantsFocus(), "a real event does");
    Drain(q, kT0);
    Check(q.wantsFocus(), "and the flag is sticky across a drain");
}

void TestAbsoluteClamping() {
    std::printf("[cinput] absolute coordinates are clamped into 0..65535...\n");
    ClientInputQueue q;
    q.MouseMoveAbsolute(-5, 70'000, kT0);
    q.MouseMoveAbsolute(0, 65'535, kT0);
    q.MouseMoveAbsolute(12'345, 54'321, kT0);
    const auto batch = Drain(q, kT0);
    Check(batch.size() == 3, "three moves");
    Check(batch[0].a == 0 && batch[0].b == kAbsCoordMax, "out of range clamps to both ends");
    Check(batch[1].a == 0 && batch[1].b == kAbsCoordMax, "the exact bounds pass through");
    Check(batch[2].a == 12'345 && batch[2].b == 54'321, "an in-range point is untouched");
    for (const auto& e : batch)
        Check(e.type == InputType::MouseMove && e.absolute == 1, "all marked absolute");
}

void TestRelativeAndWheelSkipNoOps() {
    std::printf("[cinput] a zero relative move or wheel notch is not sent at all...\n");
    ClientInputQueue q;
    q.MouseMoveRelative(0, 0, kT0);
    q.MouseWheel(0, kT0);
    Check(Drain(q, kT0).empty(), "nothing queued");

    q.MouseMoveRelative(-2, 0, kT0);
    q.MouseWheel(-120, kT0);
    const auto batch = Drain(q, kT0);
    Check(batch.size() == 2, "a non-zero move and wheel do go out");
    Check(batch[0].a == -2 && batch[0].b == 0 && batch[0].absolute == 0, "relative move");
    Check(batch[1].type == InputType::MouseWheel && batch[1].a == 0 && batch[1].b == -120,
        "the wheel delta travels in b");
}

void TestHeldKeysAndButtonsAreReleased() {
    std::printf("[cinput] ReleaseAll() lets go of exactly what is still held...\n");
    ClientInputQueue q;
    q.Key(0x11, 29, true, kT0);
    q.Key(0x41, 30, true, kT0);
    q.Key(0x41, 30, false, kT0);
    q.MouseButtonEvent(1, true, kT0);
    q.MouseButtonEvent(2, true, kT0);
    q.MouseButtonEvent(2, false, kT0);
    Drain(q, kT0);

    q.ReleaseAll(kT0 + 500);
    const auto batch = Drain(q, kT0 + 500);
    Check(batch.size() == 2, "one key and one button were still down");
    Check(IsKey(batch[0], 0x11, 29, false), "the held key is released with its scancode");
    Check(IsButton(batch[1], 1, false), "and the held button too");
    for (const auto& e : batch)
        Check(e.timestampUs == kT0 + 500, "release events are stamped at release time");

    q.ReleaseAll(kT0 + 600);
    Check(Drain(q, kT0 + 600).empty(), "a second release has nothing left to do");
}

void TestRepeatedPressDoesNotDoubleRelease() {
    std::printf("[cinput] holding a key across repeats still releases it once...\n");
    ClientInputQueue q;
    q.Key(0x41, 30, true, kT0);
    q.Key(0x41, 30, true, kT0 + 1);
    q.Key(0x41, 30, true, kT0 + 2);
    q.MouseButtonEvent(1, true, kT0);
    q.MouseButtonEvent(1, true, kT0);
    Drain(q, kT0 + 3);

    q.ReleaseAll(kT0 + 10);
    const auto batch = Drain(q, kT0 + 10);
    Check(batch.size() == 2, "one key release and one button release, not three and two");
}

void TestKeyTapHoldsThenReleases() {
    std::printf("[cinput] a tap sends down now and up once the hold has elapsed...\n");
    ClientInputQueue q;
    q.KeyTap(0x41, 30, kT0);

    auto batch = Drain(q, kT0);
    Check(batch.size() == 1 && IsKey(batch[0], 0x41, 30, true), "only the press is ready");

    batch = Drain(q, kT0 + kTapHoldUs - 1);
    Check(batch.empty(), "one microsecond early is still early");

    batch = Drain(q, kT0 + kTapHoldUs);
    Check(batch.size() == 1 && IsKey(batch[0], 0x41, 30, false), "then the release lands");

    Check(Drain(q, kT0 + 10 * kTapHoldUs).empty(), "a matured event is not delivered twice");
}

void TestKeyChordOrder() {
    std::printf("[cinput] a chord presses modifier then key, and unwinds in reverse...\n");
    ClientInputQueue q;
    q.KeyChord(0x11, 29, 0x43, 46, kT0);

    auto batch = Drain(q, kT0);
    Check(batch.size() == 2, "both presses go out together");
    Check(IsKey(batch[0], 0x11, 29, true), "modifier down first");
    Check(IsKey(batch[1], 0x43, 46, true), "then the key");

    batch = Drain(q, kT0 + kTapHoldUs);
    Check(batch.size() == 2, "both releases arrive together");
    Check(IsKey(batch[0], 0x43, 46, false), "key up first");
    Check(IsKey(batch[1], 0x11, 29, false), "then the modifier — the reverse of the press order");
}

void TestCharTap() {
    std::printf("[cinput] a character becomes the keystrokes that produce it...\n");
    ClientInputQueue q;
    Check(q.CharTap('a', kT0), "'a' is mappable");
    auto batch = Drain(q, kT0);
    Check(batch.size() == 1 && IsKey(batch[0], 'A', 0, true), "lowercase needs no shift");
    batch = Drain(q, kT0 + kTapHoldUs);
    Check(batch.size() == 1 && IsKey(batch[0], 'A', 0, false), "and is released after the hold");

    ClientInputQueue s;
    Check(s.CharTap('A', kT0), "'A' is mappable");
    batch = Drain(s, kT0);
    Check(batch.size() == 3, "uppercase wraps the key in a shift press and release");
    Check(IsKey(batch[0], kVkShift, 0, true), "shift down");
    Check(IsKey(batch[1], 'A', 0, true), "key down");
    Check(IsKey(batch[2], kVkShift, 0, false), "shift up");
    batch = Drain(s, kT0 + kTapHoldUs);
    Check(batch.size() == 1 && IsKey(batch[0], 'A', 0, false), "the key release is the delayed one");

    ClientInputQueue u;
    Check(!u.CharTap(0x4E2D, kT0), "a character with no keystroke is rejected");
    Check(Drain(u, kT0).empty(), "and queues nothing");
    Check(!u.wantsFocus(), "and does not ask for focus");
}

void TestDelayedEventsSurviveOutOfOrderDrains() {
    std::printf("[cinput] several taps in flight mature independently...\n");
    ClientInputQueue q;
    q.KeyTap(0x41, 30, kT0);
    q.KeyTap(0x42, 48, kT0 + 20'000);
    Drain(q, kT0);

    auto batch = Drain(q, kT0 + kTapHoldUs);
    Check(batch.size() == 1 && IsKey(batch[0], 0x41, 30, false), "only the first has matured");

    batch = Drain(q, kT0 + 20'000 + kTapHoldUs);
    Check(batch.size() == 1 && IsKey(batch[0], 0x42, 48, false), "then the second");
}

void TestTapsAreNotTrackedAsHeld() {
    std::printf("[cinput] taps release themselves, so ReleaseAll() has nothing to undo...\n");
    ClientInputQueue q;
    q.KeyTap(0x41, 30, kT0);
    q.KeyChord(0x11, 29, 0x43, 46, kT0);
    Drain(q, kT0);

    q.ReleaseAll(kT0 + 1);
    Check(Drain(q, kT0 + 1).empty(), "a tap is not a held key");
}

void TestPauseIsSentWithoutAScancode() {
    std::printf(
        "[cinput] Pause travels as a virtual key, so the host does not hear "
        "NumLock...\n");
    ClientInputQueue q;

    q.Key(kVkPause, 0x45, true, kT0);
    q.Key(kVkPause, 0x45, false, kT0 + 1);
    const auto batch = Drain(q, kT0 + 2);
    Check(batch.size() == 2, "both halves of the press come out");
    Check(IsKey(batch[0], kVkPause, 0, true) && IsKey(batch[1], kVkPause, 0, false),
        "a scancode a viewer guessed for Pause is dropped before it reaches the host");

    q.KeyTap(kVkPause, 0x1D, kT0);
    const auto tap = Drain(q, kT0);
    Check(tap.size() == 1 && IsKey(tap[0], kVkPause, 0, true),
        "the same holds for a tap from the on-screen key bar");
    const auto release = Drain(q, kT0 + kTapHoldUs);
    Check(release.size() == 1 && IsKey(release[0], kVkPause, 0, false),
        "and for its delayed release, so the key does not stay down");

    q.Key('A', 30, true, kT0);
    const auto ordinary = Drain(q, kT0);
    Check(ordinary.size() == 1 && IsKey(ordinary[0], 'A', 30, true),
        "every other key keeps the scancode the viewer measured");
}

}

void RunClientInputQueueTests() {
    TestDrainIsReadAndClear();
    TestFocusFlag();
    TestAbsoluteClamping();
    TestRelativeAndWheelSkipNoOps();
    TestHeldKeysAndButtonsAreReleased();
    TestRepeatedPressDoesNotDoubleRelease();
    TestKeyTapHoldsThenReleases();
    TestKeyChordOrder();
    TestCharTap();
    TestDelayedEventsSurviveOutOfOrderDrains();
    TestTapsAreNotTrackedAsHeld();
    TestPauseIsSentWithoutAScancode();
}

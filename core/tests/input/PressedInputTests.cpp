#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/input/InputApplier.h"
#include "deskhub/input/PressedInputTracker.h"

#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

using Tracker = PressedInputTracker<uint16_t>;

struct FakeInjector : InputApplier<FakeInjector, uint16_t> {
    std::vector<uint16_t> releasedKeys;
    std::vector<MouseButton> releasedButtons;

    void Hold(int32_t vk, uint16_t native, MouseButton button) {
        held_.SetKey(vk, native, true);
        held_.SetButton(button, true);
    }

    void Release() {
        ReleaseAllHeld();
    }

    void ReleaseKey(int32_t, uint16_t native) {
        releasedKeys.push_back(native);
    }

    void SendButton(MouseButton button, bool down) {
        if (!down) releasedButtons.push_back(button);
    }
};

void TestReleaseAllHeldDrainsThroughTheBackend() {
    std::printf("[held] the shared release helper lifts every held key and button...\n");
    FakeInjector fake;
    fake.Hold(0x41, 30, MouseButton::Left);
    fake.Hold(0x42, 48, MouseButton::Right);

    fake.Release();
    Check(fake.releasedKeys.size() == 2, "both keys were released via the backend hook");
    Check(fake.releasedButtons.size() == 2, "and both buttons");

    fake.releasedKeys.clear();
    fake.releasedButtons.clear();
    fake.Release();
    Check(fake.releasedKeys.empty() && fake.releasedButtons.empty(),
        "a second call has nothing left to release");
}

void TestGateLatch() {
    std::printf("[held] the host-wins gate reports each transition exactly once...\n");
    Tracker t;

    InputGate g = t.Gate(false);
    Check(g.allow && !g.justSuppressed && !g.justResumed, "idle host: input passes, no edges");

    g = t.Gate(true);
    Check(!g.allow && g.justSuppressed, "the first blocked event reports the transition");
    g = t.Gate(true);
    Check(!g.allow && !g.justSuppressed, "the second one does not report it again");
    g = t.Gate(true);
    Check(!g.allow && !g.justSuppressed, "nor the third");

    g = t.Gate(false);
    Check(g.allow && g.justResumed, "going idle reports resumption once");
    g = t.Gate(false);
    Check(g.allow && !g.justResumed, "and not again");
}

void TestGateCounters() {
    std::printf("[held] applied/skipped count what actually happened...\n");
    Tracker t;
    Check(t.applied() == 0 && t.skipped() == 0, "a fresh tracker has counted nothing");

    for (int i = 0; i < 3; ++i)
        if (t.Gate(false).allow) t.CountApplied();
    Check(t.applied() == 3 && t.skipped() == 0, "three events through an idle host");

    for (int i = 0; i < 5; ++i)
        if (t.Gate(true).allow) t.CountApplied();
    Check(t.skipped() == 5, "every blocked event is counted, not just the first");
    Check(t.applied() == 3, "and none of them counts as applied");
}

void TestKeyBookkeeping() {
    std::printf("[held] keys remember the native code they went down with...\n");
    Tracker t;
    Check(t.FindKey(0x41) == nullptr, "a key that was never pressed is not held");

    t.SetKey(0x41, 30, true);
    t.SetKey(0x42, 48, true);
    Check(t.heldKeyCount() == 2, "two keys held");
    Check(t.FindKey(0x41) && *t.FindKey(0x41) == 30, "'A' remembers evdev 30");
    Check(t.FindKey(0x42) && *t.FindKey(0x42) == 48, "'B' remembers evdev 48");

    t.SetKey(0x41, 30, false);
    Check(t.FindKey(0x41) == nullptr, "releasing drops it");
    Check(t.heldKeyCount() == 1, "the other key is untouched");

    t.SetKey(0x42, 99, true);
    Check(*t.FindKey(0x42) == 99, "a repeat press updates the native code");
    Check(t.heldKeyCount() == 1, "and does not double-count");

    t.SetKey(0x43, 0, false);
    Check(t.heldKeyCount() == 1, "releasing a key that was never down is harmless");
}

void TestButtonBookkeeping() {
    std::printf("[held] buttons are a set, so a repeated press is not a second button...\n");
    Tracker t;
    Check(!t.ButtonIsDown(MouseButton::Left), "nothing down to start");

    t.SetButton(MouseButton::Left, true);
    t.SetButton(MouseButton::Right, true);
    t.SetButton(MouseButton::Left, true);
    Check(t.heldButtonCount() == 2, "left twice is still one held button");
    Check(t.ButtonIsDown(MouseButton::Left) && t.ButtonIsDown(MouseButton::Right), "both down");
    Check(!t.ButtonIsDown(MouseButton::Middle), "middle is not");

    t.SetButton(MouseButton::Left, false);
    Check(!t.ButtonIsDown(MouseButton::Left) && t.heldButtonCount() == 1, "left released");
    t.SetButton(MouseButton::Middle, false);
    Check(t.heldButtonCount() == 1, "releasing an unheld button is harmless");
}

void TestTakeHeldClearsBeforeTheCallerSendsUpEvents() {
    std::printf("[held] Take*() hands out a snapshot and clears, so send-up cannot re-enter...\n");
    Tracker t;
    t.SetKey(0x41, 30, true);
    t.SetKey(0x11, 29, true);
    t.SetButton(MouseButton::Left, true);
    t.SetButton(MouseButton::X2, true);
    Check(!t.nothingHeld(), "four things held");

    const std::vector<Tracker::HeldKey> keys = t.TakeHeldKeys();
    Check(keys.size() == 2, "both keys came out");
    Check(t.heldKeyCount() == 0, "and the tracker no longer holds them");

    bool sawA = false, sawCtrl = false;
    for (const auto& k : keys) {
        if (k.id == 0x41 && k.native == 30) sawA = true;
        if (k.id == 0x11 && k.native == 29) sawCtrl = true;
        t.SetKey(k.id, k.native, false);
    }
    Check(sawA && sawCtrl, "each held key carries its id and native code");
    Check(t.heldKeyCount() == 0, "releasing from the snapshot stays a no-op");

    const std::vector<MouseButton> buttons = t.TakeHeldButtons();
    Check(buttons.size() == 2, "both buttons came out");
    for (MouseButton b : buttons) t.SetButton(b, false);
    Check(t.nothingHeld(), "nothing is held after a full release");

    Check(t.TakeHeldKeys().empty() && t.TakeHeldButtons().empty(),
        "taking again from an empty tracker yields nothing");
}

void TestReleaseDoesNotTouchTheGate() {
    std::printf("[held] releasing everything leaves the suppression latch alone...\n");
    Tracker t;
    t.SetKey(0x41, 30, true);
    t.Gate(true);
    t.TakeHeldKeys();
    t.TakeHeldButtons();
    Check(t.Gate(false).justResumed, "and then it does");
}

void TestWindowsStyleScanKeyedTracker() {
    std::printf("[held] the same tracker works keyed by scancode with a vk payload...\n");
    PressedInputTracker<int32_t> t;
    t.SetKey(0x1E, 0x41, true);
    t.SetKey(0x30, 0x42, true);
    Check(*t.FindKey(0x1E) == 0x41, "scan 0x1E holds vk 'A'");

    const auto held = t.TakeHeldKeys();
    Check(held.size() == 2, "both scancodes come back");
    Check(t.nothingHeld(), "and the tracker is empty");
}

}

void RunPressedInputTests() {
    TestGateLatch();
    TestGateCounters();
    TestKeyBookkeeping();
    TestButtonBookkeeping();
    TestTakeHeldClearsBeforeTheCallerSendsUpEvents();
    TestReleaseDoesNotTouchTheGate();
    TestWindowsStyleScanKeyedTracker();
    TestReleaseAllHeldDrainsThroughTheBackend();
}

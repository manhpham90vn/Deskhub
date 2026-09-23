#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/AuthThrottle.h"

#include <cstdio>

using namespace deskhub;

namespace {

constexpr uint64_t kStartUs = 1'000'000;

void TestThreeFailuresLockTheDoor() {
    std::printf("[throttle] three wrong guesses shut the door for the lockout window...\n");
    AuthThrottle throttle;
    Check(!throttle.Locked(kStartUs), "a fresh throttle is open");

    throttle.RecordFailure(kStartUs);
    throttle.RecordFailure(kStartUs + 1);
    Check(!throttle.Locked(kStartUs + 2), "two wrong guesses do not lock it yet");

    throttle.RecordFailure(kStartUs + 2);
    Check(throttle.Locked(kStartUs + 3), "the third one does");
    Check(throttle.Locked(kStartUs + 2 + kPasscodeLockoutUs - 1),
        "and it stays shut for the whole window");
    Check(!throttle.Locked(kStartUs + 2 + kPasscodeLockoutUs),
        "then opens again on its own");
}

void TestTheCountRestartsAfterALockout() {
    std::printf("[throttle] a served lockout does not shorten the next one...\n");
    AuthThrottle throttle;
    for (uint32_t i = 0; i < kMaxPasscodeAttempts; ++i) throttle.RecordFailure(kStartUs);
    const uint64_t reopenedUs = kStartUs + kPasscodeLockoutUs;
    Check(!throttle.Locked(reopenedUs), "the door reopened");

    throttle.RecordFailure(reopenedUs);
    Check(!throttle.Locked(reopenedUs + 1),
        "and one more wrong guess after it is only the first of a new count");

    throttle.RecordFailure(reopenedUs + 1);
    throttle.RecordFailure(reopenedUs + 2);
    Check(throttle.Locked(reopenedUs + 3), "but a full run of wrong guesses locks it again");
}

void TestGuessingWhileLockedPushesTheReopeningOut() {
    std::printf("[throttle] guessing into a locked door only extends the lockout...\n");
    AuthThrottle throttle;
    for (uint32_t i = 0; i < kMaxPasscodeAttempts; ++i) throttle.RecordFailure(kStartUs);
    const uint64_t midLockUs = kStartUs + kPasscodeLockoutUs / 2;
    for (uint32_t i = 0; i < kMaxPasscodeAttempts; ++i) throttle.RecordFailure(midLockUs);
    Check(throttle.Locked(kStartUs + kPasscodeLockoutUs),
        "the door does not reopen at the original time");
    Check(throttle.Locked(midLockUs + 2 * kPasscodeLockoutUs - 1),
        "it stays shut for the doubled window counted from the last run of guesses");
    Check(!throttle.Locked(midLockUs + 2 * kPasscodeLockoutUs), "and reopens after it");
}

void TestALockoutOutlivesASuccess() {
    std::printf("[throttle] a lockout in force is not cut short by a success...\n");
    AuthThrottle throttle;
    for (uint32_t i = 0; i < kMaxPasscodeAttempts; ++i) throttle.RecordFailure(kStartUs);
    throttle.RecordSuccess();
    Check(throttle.Locked(kStartUs + kPasscodeLockoutUs - 1),
        "the window still has to be served in full");
    Check(!throttle.Locked(kStartUs + kPasscodeLockoutUs), "and still ends on time");
}

void LockOnce(AuthThrottle& throttle, uint64_t nowUs) {
    for (uint32_t i = 0; i < kMaxPasscodeAttempts; ++i) throttle.RecordFailure(nowUs);
}

void TestEachLockoutDoublesTheNext() {
    std::printf("[throttle] every lockout in a row is twice as long as the one before...\n");
    AuthThrottle throttle;
    uint64_t nowUs = kStartUs;
    uint64_t expectedUs = kPasscodeLockoutUs;
    for (int round = 0; round < 4; ++round) {
        LockOnce(throttle, nowUs);
        Check(throttle.Locked(nowUs + expectedUs - 1), "the door stays shut for the whole window");
        Check(!throttle.Locked(nowUs + expectedUs), "and opens exactly when it ends");
        nowUs += expectedUs;
        expectedUs *= 2;
    }
}

void TestTheLockoutStopsGrowingAtTheCeiling() {
    std::printf("[throttle] a determined guesser is held to one window per hour at most...\n");
    AuthThrottle throttle;
    uint64_t nowUs = kStartUs;
    for (int round = 0; round < 40; ++round) {
        LockOnce(throttle, nowUs);
        nowUs += kMaxPasscodeLockoutUs;
    }
    LockOnce(throttle, nowUs);
    Check(throttle.Locked(nowUs + kMaxPasscodeLockoutUs - 1),
        "a long run of lockouts ends at the ceiling");
    Check(!throttle.Locked(nowUs + kMaxPasscodeLockoutUs), "and never grows past it");
}

void TestSuccessResetsTheEscalation() {
    std::printf("[throttle] a correct passcode brings the lockout back to its first length...\n");
    AuthThrottle throttle;
    LockOnce(throttle, kStartUs);
    LockOnce(throttle, kStartUs + kPasscodeLockoutUs);
    throttle.RecordSuccess();

    const uint64_t laterUs = kStartUs + 10 * kPasscodeLockoutUs;
    LockOnce(throttle, laterUs);
    Check(!throttle.Locked(laterUs + kPasscodeLockoutUs),
        "the next lockout is the short one again");
}

void TestSuccessResetsTheCount() {
    std::printf("[throttle] a correct passcode wipes the slate...\n");
    AuthThrottle throttle;
    throttle.RecordFailure(kStartUs);
    throttle.RecordFailure(kStartUs + 1);
    throttle.RecordSuccess();

    throttle.RecordFailure(kStartUs + 2);
    throttle.RecordFailure(kStartUs + 3);
    Check(!throttle.Locked(kStartUs + 4), "two guesses after a success do not lock");

    throttle.RecordFailure(kStartUs + 4);
    Check(throttle.Locked(kStartUs + 5), "three in a row still do");
}

}

void RunAuthThrottleTests() {
    TestThreeFailuresLockTheDoor();
    TestTheCountRestartsAfterALockout();
    TestGuessingWhileLockedPushesTheReopeningOut();
    TestALockoutOutlivesASuccess();
    TestSuccessResetsTheCount();
    TestEachLockoutDoublesTheNext();
    TestTheLockoutStopsGrowingAtTheCeiling();
    TestSuccessResetsTheEscalation();
}

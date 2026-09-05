#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/LinkRecovery.h"
#include "deskhub/session/TerminalSession.h"

#include <cstdio>

using namespace deskhub;

namespace {

void TestKeepaliveAlwaysBeatsTheIdleTimeout() {
    std::printf("[link] a keepalive always lands well before the link times out...\n");
    const uint64_t timeouts[] = {30'000, 10'000, 5'000, 1'000, 100, 1};
    for (uint64_t ms : timeouts) {
        const uint64_t interval = KeepaliveIntervalUs(ms);
        Check(interval > 0, "the interval is a real delay, never zero");
        Check(interval * 2 <= ms * 1000 || ms * 1000 < 2,
            "and at most half the idle timeout, so one lost packet is survivable");
    }
    Check(KeepaliveIntervalUs(30'000) == 10'000'000,
        "the 30 s link Deskhub uses pings every 10 s");
    Check(KeepaliveIntervalUs(0) == kMinKeepaliveIntervalUs,
        "a caller with no timeout to divide still gets a usable interval");
}

void TestKeepaliveOnlyFiresOnceItIsDue() {
    std::printf("[link] a keepalive waits its turn and does not fire twice...\n");
    const uint64_t interval = KeepaliveIntervalUs(30'000);
    Check(!KeepaliveDue(0, 0, interval), "nothing is due the instant the clock starts");
    Check(!KeepaliveDue(interval - 1, 0, interval), "one microsecond early is still early");
    Check(KeepaliveDue(interval, 0, interval), "on the interval it is due");
    Check(KeepaliveDue(interval * 3, 0, interval), "and stays due until something is sent");
    Check(!KeepaliveDue(interval, interval, interval),
        "sending it resets the wait rather than repeating");
    Check(!KeepaliveDue(5, 10, interval),
        "a clock that jumped backwards does not spray keepalives");
}

void TestReconnectBacksOffAndStops() {
    std::printf("[link] reconnect backs off, then gives up before the shell is gone...\n");
    constexpr uint32_t kNoJitter = 0;
    constexpr uint32_t kFullJitter = 0xFFFFFFFF;

    Check(ReconnectDelayUs(0, kFullJitter) == kReconnectFirstDelayUs,
        "the first retry is quick");
    Check(ReconnectDelayUs(1, kFullJitter) > ReconnectDelayUs(0, kFullJitter),
        "each retry waits longer than the last");
    Check(ReconnectDelayUs(2, kFullJitter) > ReconnectDelayUs(1, kFullJitter),
        "and keeps growing while it is small");

    uint64_t previous = 0;
    for (uint32_t attempt = 0; attempt < 64; ++attempt) {
        const uint64_t delay = ReconnectDelayUs(attempt, kFullJitter);
        Check(delay >= previous, "the backoff never shrinks");
        Check(delay <= kReconnectMaxDelayUs, "and never grows past the cap");
        previous = delay;
    }
    Check(ReconnectDelayUs(63, kFullJitter) == kReconnectMaxDelayUs,
        "a long outage settles at the cap instead of overflowing");

    Check(ReconnectDelayUs(4, kNoJitter) * 2 == ReconnectDelayUs(4, kFullJitter),
        "the jitter spreads a retry over the top half of its own backoff window");
    Check(ReconnectDelayUs(63, kNoJitter) >= kReconnectMaxDelayUs / 2,
        "and never lets a retry come sooner than half the nominal wait");

    uint64_t spread = 0;
    uint64_t seen[8] = {};
    for (uint32_t slice = 0; slice < 8; ++slice) {
        seen[slice] = ReconnectDelayUs(10, uint32_t((uint64_t(slice) << 29)));
        if (slice && seen[slice] != seen[slice - 1]) ++spread;
    }
    Check(spread == 7,
        "viewers that lost the same host at the same moment draw different waits, so they "
        "do not come back in lockstep");
}

void TestReconnectGivesUpWithTheHostsGrace() {
    std::printf("[link] retrying stops exactly when the host has dropped the shell...\n");
    const uint64_t grace = kTerminalReattachGraceUs;
    Check(ReconnectStillWorthTrying(0, grace), "a fresh loss is worth retrying");
    Check(ReconnectStillWorthTrying(grace - 1, grace), "so is one just inside the grace window");
    Check(!ReconnectStillWorthTrying(grace, grace),
        "at the grace deadline the shell is gone, so retrying is pointless");
    Check(!ReconnectStillWorthTrying(grace * 10, grace), "and it never comes back later");

    uint64_t elapsed = 0;
    uint32_t attempts = 0;
    while (ReconnectStillWorthTrying(elapsed, grace)) {
        elapsed += ReconnectDelayUs(attempts, 0xFFFFFFFF);
        ++attempts;
        Check(attempts < 1000, "the retry loop terminates rather than spinning");
    }
    Check(attempts > 5, "the window still allows a decent number of tries");
}

}

void RunLinkRecoveryTests() {
    TestKeepaliveAlwaysBeatsTheIdleTimeout();
    TestKeepaliveOnlyFiresOnceItIsDue();
    TestReconnectBacksOffAndStops();
    TestReconnectGivesUpWithTheHostsGrace();
}

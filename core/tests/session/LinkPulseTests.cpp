#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/LinkPulse.h"

#include <cstdio>

using namespace deskhub;

namespace {

void TestPingsGoOutOncePerSecond() {
    std::printf("[pulse] pings go out once a second, starting immediately...\n");
    LinkPulse pulse;
    Check(pulse.PingDue(5), "the first ping is due the moment the link is up");
    const PingPong first = pulse.MakePing(5);
    Check(!pulse.PingDue(5 + kLinkPingIntervalUs - 1), "one microsecond early is still early");
    Check(pulse.PingDue(5 + kLinkPingIntervalUs), "on the interval the next one is due");
    const PingPong second = pulse.MakePing(5 + kLinkPingIntervalUs);
    Check(second.pingId == first.pingId + 1, "every ping carries a fresh id");
    Check(second.sendTimeUs == 5 + kLinkPingIntervalUs, "and stamps when it left");
}

void TestAPongMustMatchWhatWasSent() {
    std::printf("[pulse] a pong that does not match a ping we sent is ignored...\n");
    LinkPulse pulse;
    const uint64_t now = 2'000'000;
    const PingPong sent = pulse.MakePing(now);

    pulse.OnPong(PingPong{sent.pingId + 500, sent.sendTimeUs}, now + 1'000);
    pulse.OnPong(PingPong{sent.pingId, sent.sendTimeUs + 1}, now + 1'000);
    pulse.OnPong(sent, now - 1);
    Check(!pulse.Stalled(now + kLinkStallAfterUs * 3),
        "a stranger id, a doctored timestamp or a pong from the past never counts as an answer");

    pulse.OnPong(sent, now + 1'000);
    Check(pulse.Stalled(now + 1'000 + kLinkStallAfterUs + 1),
        "the genuine pong counts, so silence after it reads as a stall");
    pulse.OnPong(sent, now + 2'000'000);
    Check(pulse.Stalled(now + 1'000 + kLinkStallAfterUs + 1),
        "and answering the same ping twice does not buy more time");
}

void TestAStalledLinkNeedsAFirstPong() {
    std::printf("[pulse] silence only reads as a stall once the host has answered before...\n");
    LinkPulse pulse;
    uint64_t now = 4'000'000;
    const PingPong first = pulse.MakePing(now);
    Check(!pulse.Stalled(now + kLinkStallAfterUs * 3),
        "a host that never answers pings is never called stalled");

    pulse.OnPong(first, now + 50'000);
    now += 50'000;
    Check(!pulse.Stalled(now + kLinkStallAfterUs), "silence inside the window is fine");
    Check(pulse.Stalled(now + kLinkStallAfterUs + 1), "past the window the link has stalled");

    pulse.Reset();
    Check(!pulse.Stalled(now + kLinkStallAfterUs * 2),
        "a fresh connection starts with a clean slate");
}

void TestAFrozenLoopDoesNotBlameThePeer() {
    std::printf("[pulse] time the loop spent not running is not counted as silence...\n");
    LinkPulse pulse;
    uint64_t now = 9'000'000;
    const PingPong first = pulse.MakePing(now);
    now += 40'000;
    pulse.OnPong(first, now);
    pulse.Tick(now);

    const uint64_t frozenUs = 4'000'000;
    now += frozenUs;
    pulse.Tick(now);
    Check(!pulse.Stalled(now + kLinkStallAfterUs - frozenUs),
        "a turn of the loop that took four seconds is four seconds we were not listening");
    Check(pulse.Stalled(now + kLinkStallAfterUs),
        "past the window the loop really did watch, the link has still stalled");

    LinkPulse steady;
    now = 9'000'000;
    const PingPong only = steady.MakePing(now);
    steady.OnPong(only, now);
    for (uint64_t spent = 0; spent <= kLinkStallAfterUs; spent += kLinkWatchStepUs)
        steady.Tick(now + spent);
    Check(steady.Stalled(now + kLinkStallAfterUs + 1),
        "a loop that kept its turns short forgives nothing");
}

}

void RunLinkPulseTests() {
    TestPingsGoOutOncePerSecond();
    TestAPongMustMatchWhatWasSent();
    TestAStalledLinkNeedsAFirstPong();
    TestAFrozenLoopDoesNotBlameThePeer();
}

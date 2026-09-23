#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/LinkPulse.h"
#include "deskhub/ui/Strings.h"

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

void TestRttComesFromTheEchoedPing() {
    std::printf("[pulse] the pong's echo turns into a smoothed round-trip time...\n");
    LinkPulse pulse;
    uint64_t now = 1'000'000;
    const PingPong first = pulse.MakePing(now);
    Check(!pulse.View(now).haveRtt, "before any pong there is no reading");
    Check(pulse.View(now).quality == LinkQuality::Unknown, "so the quality is unknown");

    Check(pulse.OnPong(first, now + 40'000), "the first pong is accepted");
    LinkPulseView view = pulse.View(now + 40'000);
    Check(view.haveRtt, "one pong is enough for a reading");
    Check(view.rttUs == 40'000, "the first sample is taken as-is");

    now += kLinkPingIntervalUs;
    const PingPong second = pulse.MakePing(now);
    Check(pulse.OnPong(second, now + 8'000), "the next pong is accepted too");
    view = pulse.View(now + 8'000);
    Check(view.rttUs == 36'000, "later samples are smoothed instead of jumping");
    Check(view.quality == LinkQuality::Good, "a fast, lossless link reads as good");
}

void TestAPongMustMatchWhatWasSent() {
    std::printf("[pulse] a pong that does not match a ping we sent is ignored...\n");
    LinkPulse pulse;
    const uint64_t now = 2'000'000;
    const PingPong sent = pulse.MakePing(now);

    Check(!pulse.OnPong(PingPong{sent.pingId + 500, sent.sendTimeUs}, now + 1'000),
        "an id we never sent is ignored");
    Check(!pulse.OnPong(PingPong{sent.pingId, sent.sendTimeUs + 1}, now + 1'000),
        "a doctored timestamp is ignored");
    Check(!pulse.OnPong(sent, now - 1), "a pong from before the ping left is ignored");
    Check(pulse.OnPong(sent, now + 1'000), "the genuine pong still lands");
    Check(!pulse.OnPong(sent, now + 2'000), "and answering twice does not count twice");
    Check(pulse.View(now + 2'000).rttUs == 1'000, "only the first answer set the reading");
}

void TestUnansweredPingsCountAsLoss() {
    std::printf("[pulse] a ping nobody answered becomes loss, but not right away...\n");
    LinkPulse pulse;
    uint64_t now = 3'000'000;
    pulse.MakePing(now);
    now += kLinkPingIntervalUs;
    const PingPong answered = pulse.MakePing(now);
    Check(pulse.OnPong(answered, now + 100'000), "the second ping gets its pong");

    LinkPulseView view = pulse.View(now + 100'000);
    Check(view.lossPct == 0, "the first ping is still in flight, not lost");

    view = pulse.View(now + kLinkPingLostAfterUs);
    Check(view.lossPct == 50, "once its window passes, one of two pings counts as lost");
    Check(view.quality == LinkQuality::Poor, "that much loss reads as poor");
}

void TestQualityBandsAreStable() {
    std::printf("[pulse] the quality bands sit where the thresholds say...\n");
    Check(ClassifyLinkQuality(false, 0, 0) == LinkQuality::Unknown,
        "no reading means no verdict");
    Check(ClassifyLinkQuality(true, kLinkGoodMaxRttUs - 1, 0) == LinkQuality::Good,
        "fast and lossless is good");
    Check(ClassifyLinkQuality(true, kLinkGoodMaxRttUs, 0) == LinkQuality::Fair,
        "at the good ceiling it turns fair");
    Check(ClassifyLinkQuality(true, 10'000, 1) == LinkQuality::Fair,
        "any loss at all costs the good rating");
    Check(ClassifyLinkQuality(true, kLinkFairMaxRttUs - 1, kLinkFairMaxLossPct) ==
              LinkQuality::Fair,
        "the fair band stretches to its own ceilings");
    Check(ClassifyLinkQuality(true, kLinkFairMaxRttUs, 0) == LinkQuality::Poor,
        "past the fair ceiling it is poor however clean");
    Check(ClassifyLinkQuality(true, 10'000, kLinkFairMaxLossPct + 1) == LinkQuality::Poor,
        "and heavy loss is poor however fast");
}

void TestAStalledLinkNeedsAFirstPong() {
    std::printf("[pulse] silence only reads as a stall once the host has answered before...\n");
    LinkPulse pulse;
    uint64_t now = 4'000'000;
    const PingPong first = pulse.MakePing(now);
    Check(!pulse.Stalled(now + kLinkStallAfterUs * 3),
        "a host that never answers pings is never called stalled");

    Check(pulse.OnPong(first, now + 50'000), "then the host answers once");
    now += 50'000;
    Check(!pulse.Stalled(now + kLinkStallAfterUs), "silence inside the window is fine");
    Check(pulse.Stalled(now + kLinkStallAfterUs + 1), "past the window the link has stalled");
    Check(pulse.View(now + kLinkStallAfterUs + 1).quality == LinkQuality::Poor,
        "and a stalled link reads as poor");

    pulse.Reset();
    Check(!pulse.Stalled(now + kLinkStallAfterUs * 2),
        "a fresh connection starts with a clean slate");
    Check(pulse.View(now).quality == LinkQuality::Unknown, "and no reading");
}

void TestAFrozenLoopDoesNotBlameThePeer() {
    std::printf("[pulse] time the loop spent not running is not counted as silence...\n");
    LinkPulse pulse;
    uint64_t now = 9'000'000;
    const PingPong first = pulse.MakePing(now);
    now += 40'000;
    Check(pulse.OnPong(first, now), "the host answers once, so silence starts being measured");
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
    Check(steady.OnPong(only, now), "a second link answers once and then goes quiet");
    for (uint64_t spent = 0; spent <= kLinkStallAfterUs; spent += kLinkWatchStepUs)
        steady.Tick(now + spent);
    Check(steady.Stalled(now + kLinkStallAfterUs + 1),
        "a loop that kept its turns short forgives nothing");
}

void TestTheReadingsTurnIntoWords() {
    std::printf("[pulse] the readings turn into the words the status bar shows...\n");
    Check(std::string(ui::LinkQualityText(LinkQuality::Good)) == ui::kLinkQualityGood,
        "good has its word");
    Check(std::string(ui::LinkQualityText(LinkQuality::Fair)) == ui::kLinkQualityFair,
        "fair has its word");
    Check(std::string(ui::LinkQualityText(LinkQuality::Poor)) == ui::kLinkQualityPoor,
        "poor has its word");
    Check(std::string(ui::LinkQualityText(LinkQuality::Unknown)) == ui::kLinkNoReading,
        "no reading shows a dash, not a guess");
}

}

void RunLinkPulseTests() {
    TestPingsGoOutOncePerSecond();
    TestRttComesFromTheEchoedPing();
    TestAPongMustMatchWhatWasSent();
    TestUnansweredPingsCountAsLoss();
    TestQualityBandsAreStable();
    TestAStalledLinkNeedsAFirstPong();
    TestAFrozenLoopDoesNotBlameThePeer();
    TestTheReadingsTurnIntoWords();
}

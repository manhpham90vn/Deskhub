#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transport/AudioJitterBuffer.h"

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

using namespace deskhub;

namespace {

void Push(AudioJitterBuffer& buf, uint32_t seq, uint8_t marker) {
    const std::vector<uint8_t> payload(8, marker);
    AudioPacketView v;
    v.hdr.seq = seq;
    v.hdr.timestampUs = uint64_t(seq) * kAudioFrameUs;
    v.payload = payload;
    buf.Push(v);
}

uint8_t MarkerOf(const AudioJitterBuffer::Frame& f) {
    return f.payload.empty() ? 0 : f.payload.front();
}

void TestPrefillHoldsBeforePlaying() {
    std::printf("[audio] nothing plays until the buffer holds its target delay...\n");
    AudioJitterBuffer buf;
    Check(buf.targetDelayMs() == kDefaultAudioDelayMs, "60 ms of delay is three 20 ms frames");

    Push(buf, 100, 0xA1);
    Check(!buf.Pop().has_value(), "one frame is not enough to start");
    Push(buf, 101, 0xA2);
    Check(!buf.Pop().has_value(), "two frames are not enough to start");
    Push(buf, 102, 0xA3);

    const auto first = buf.Pop();
    Check(first && first->seq == 100 && MarkerOf(*first) == 0xA1,
        "the third frame starts play-out at the oldest packet");
    Check(buf.playing(), "the buffer reports itself playing");
}

void TestReorderingIsUndone() {
    std::printf("[audio] packets that arrive out of order play in order...\n");
    AudioJitterBuffer buf;
    Push(buf, 2, 0xB2);
    Push(buf, 0, 0xB0);
    Push(buf, 1, 0xB1);

    for (uint32_t expected = 0; expected < 3; ++expected) {
        const auto f = buf.Pop();
        Check(f && f->seq == expected && !f->concealed && MarkerOf(*f) == uint8_t(0xB0 + expected),
            "reordered frames come back in sequence");
    }
    Check(buf.stats().framesPlayed == 3, "every reordered frame was played, none concealed");
}

void TestDuplicatesAndLatecomers() {
    std::printf("[audio] duplicates and packets that arrive after their turn are dropped...\n");
    AudioJitterBuffer buf;
    Push(buf, 10, 0xC0);
    Push(buf, 10, 0xC0);
    Check(buf.stats().framesDuplicate == 1, "a repeated sequence number is counted once");

    Push(buf, 11, 0xC1);
    Push(buf, 12, 0xC2);
    Check(buf.Pop().has_value() && buf.Pop().has_value(), "play-out starts normally");

    Push(buf, 10, 0xC0);
    Check(buf.stats().framesLate == 1, "a frame whose turn has passed is refused");
    Check(buf.buffered() == 1, "the late frame did not join the queue");
}

void TestGapIsConcealed() {
    std::printf("[audio] a lost frame comes back as concealment, not as silence...\n");
    AudioJitterBuffer buf;
    Push(buf, 0, 0xD0);
    Push(buf, 1, 0xD1);
    Push(buf, 3, 0xD3);

    Check(buf.Pop()->seq == 0, "the first frame plays");
    Check(buf.Pop()->seq == 1, "the second frame plays");

    const auto lost = buf.Pop();
    Check(lost && lost->seq == 2 && lost->concealed && lost->payload.empty(),
        "the missing frame is reported as concealed so the decoder can run PLC");

    const auto after = buf.Pop();
    Check(after && after->seq == 3 && !after->concealed && MarkerOf(*after) == 0xD3,
        "play-out carries on past the hole");
    Check(buf.stats().framesConcealed == 1 && buf.stats().framesPlayed == 3,
        "one concealment, three real frames");
}

void TestUnderrunRebuffers() {
    std::printf("[audio] an empty buffer stops play-out and refills before starting again...\n");
    AudioJitterBuffer buf;
    for (uint32_t seq = 0; seq < 3; ++seq) Push(buf, seq, uint8_t(0xE0 + seq));
    for (uint32_t seq = 0; seq < 3; ++seq) Check(buf.Pop()->seq == seq, "the burst plays out");

    Check(!buf.Pop().has_value(), "an empty buffer plays nothing");
    Check(!buf.playing(), "the buffer stops calling itself playing");
    Check(buf.stats().underruns == 1, "the underrun is counted");

    Push(buf, 3, 0xE3);
    Check(!buf.Pop().has_value(), "one frame does not restart play-out on its own");
    Push(buf, 4, 0xE4);
    Push(buf, 5, 0xE5);
    Check(buf.Pop()->seq == 3, "play-out restarts once the delay is refilled");
}

void TestLatencyIsBounded() {
    std::printf("[audio] a burst cannot grow the buffer without bound...\n");
    AudioJitterBuffer buf;
    for (uint32_t seq = 0; seq < 200; ++seq) Push(buf, seq, uint8_t(seq));

    Check(buf.buffered() <= 6, "the queue stays near the target delay");
    Check(buf.stats().framesDropped > 0, "the frames beyond it were dropped, not queued");

    const auto f = buf.Pop();
    Check(f && !f->concealed, "what is left still plays");
}

void TestResyncOnSequenceJump() {
    std::printf("[audio] a new stream resets the buffer instead of concealing thousands...\n");
    AudioJitterBuffer buf;
    for (uint32_t seq = 0; seq < 3; ++seq) Push(buf, seq, uint8_t(seq));
    Check(buf.Pop().has_value(), "the first stream plays");

    Push(buf, 100000, 0xF0);
    Check(buf.stats().resyncs == 1, "the jump is treated as a new stream");
    Check(!buf.playing() && buf.buffered() == 1, "the buffer starts filling again from scratch");

    Push(buf, 100001, 0xF1);
    Push(buf, 100002, 0xF2);
    const auto f = buf.Pop();
    Check(f && f->seq == 100000 && MarkerOf(*f) == 0xF0, "play-out resumes on the new stream");
    Check(buf.stats().framesConcealed == 0, "not one concealed frame was invented");
}

void TestDelayIsClamped() {
    std::printf("[audio] the requested delay is clamped to something playable...\n");
    AudioJitterBuffer tight(0);
    Check(tight.targetDelayMs() == kAudioFrameMs, "zero delay still buffers one frame");

    AudioJitterBuffer loose(10'000);
    Check(loose.targetDelayMs() == 500, "an absurd delay is capped at half a second");

    AudioJitterBuffer chosen(100);
    Check(chosen.targetDelayMs() == 100, "a sensible delay is honoured exactly");
}

void TestEmptyPayloadIgnored() {
    std::printf("[audio] a packet with no payload never enters the queue...\n");
    AudioJitterBuffer buf;
    AudioPacketView v;
    v.hdr.seq = 1;
    buf.Push(v);
    Check(buf.buffered() == 0 && buf.stats().framesReceived == 0,
        "an empty audio packet is not a frame");
}

void PushAt(AudioJitterBuffer& buf, uint32_t seq, uint64_t arrivedUs) {
    const std::vector<uint8_t> payload(8, uint8_t(seq));
    AudioPacketView v;
    v.hdr.seq = seq;
    v.hdr.timestampUs = uint64_t(seq) * kAudioFrameUs;
    v.payload = payload;
    buf.Push(v, arrivedUs);
}

uint64_t FeedLink(AudioJitterBuffer& buf, uint32_t frames, uint64_t jitterUs, uint32_t seed) {
    std::vector<std::pair<uint64_t, uint32_t>> arrivals;
    arrivals.reserve(frames);
    uint32_t state = seed;
    for (uint32_t seq = 0; seq < frames; ++seq) {
        state = state * 1664525u + 1013904223u;
        const uint64_t wobble = jitterUs ? (state >> 16) % (jitterUs * 2) : 0;
        arrivals.emplace_back(1'000'000 + uint64_t(seq) * kAudioFrameUs + wobble, seq);
    }
    std::sort(arrivals.begin(), arrivals.end());

    size_t next = 0;
    uint64_t nowUs = 1'000'000;
    for (uint32_t step = 0; step < frames + 20; ++step, nowUs += kAudioFrameUs) {
        while (next < arrivals.size() && arrivals[next].first <= nowUs) {
            PushAt(buf, arrivals[next].second, arrivals[next].first);
            ++next;
        }
        buf.Pop();
    }
    return nowUs;
}

void TestFixedDelayIgnoresTheLinkItIsOn() {
    std::printf("[audio] a fixed target delay costs the same on any link...\n");

    AudioJitterBuffer quiet(kDefaultAudioDelayMs);
    AudioJitterBuffer rough(kDefaultAudioDelayMs);
    FeedLink(quiet, 400, 0, 1);
    FeedLink(rough, 400, 30'000, 2);

    Check(quiet.targetDelayMs() == rough.targetDelayMs(),
        "without adaptation the buffer holds the same delay whether the link wobbles or not");
    Check(rough.jitterMs() > quiet.jitterMs(),
        "even though it has measured that one of them wobbles far more");
}

void TestAdaptiveTargetFollowsMeasuredJitter() {
    std::printf("[audio] an adaptive target buys delay only where the link needs it...\n");

    AudioJitterBuffer quiet(kDefaultAudioDelayMs);
    AudioJitterBuffer rough(kDefaultAudioDelayMs);
    quiet.SetAdaptiveTarget(true);
    rough.SetAdaptiveTarget(true);
    Check(quiet.adaptiveTarget(), "the switch is readable");

    FeedLink(quiet, 400, 0, 1);
    FeedLink(rough, 400, 30'000, 2);

    if (quiet.targetDelayMs() >= rough.targetDelayMs())
        std::printf("[audio]   quiet held %u ms, rough held %u ms\n", quiet.targetDelayMs(),
            rough.targetDelayMs());
    Check(quiet.targetDelayMs() < rough.targetDelayMs(),
        "a steady link is given back the delay a wobbly one has to keep");
    Check(quiet.targetDelayMs() < kDefaultAudioDelayMs,
        "and a link with no jitter at all holds less than the fixed default");

    const AudioJitterBuffer::Stats& roughStats = rough.stats();
    Check(roughStats.underruns + roughStats.framesConcealed <= 400 / 4,
        "while the wobbly link still plays out without falling apart - the trade is delay "
        "against gaps, so both halves have to be reported together");
}

struct AudioPoint {
    uint32_t targetMs = 0;
    uint32_t heldMs = 0;
    uint64_t gaps = 0;
    double gapsPerMinute = 0.0;
};

AudioPoint RunAudioPoint(uint32_t targetMs, bool adaptive, uint64_t jitterUs, uint32_t frames) {
    AudioJitterBuffer buf(targetMs);
    buf.SetAdaptiveTarget(adaptive);
    FeedLink(buf, frames, jitterUs, 11);

    const AudioJitterBuffer::Stats& s = buf.stats();
    AudioPoint point;
    point.targetMs = targetMs;
    point.heldMs = buf.targetDelayMs();
    point.gaps = s.underruns + s.framesConcealed;
    const double minutes = double(frames) * double(kAudioFrameMs) / 60'000.0;
    point.gapsPerMinute = minutes > 0.0 ? double(point.gaps) / minutes : 0.0;
    return point;
}

void TestDelayAgainstGapsIsACurve() {
    std::printf("[audio] the delay/gap trade is a curve, so the sweep prints one...\n");
    std::printf("[csv] target_ms,adaptive,jitter_ms,held_ms,gaps,gaps_per_min\n");

    constexpr uint32_t kFrames = 1500;
    const uint32_t targets[] = {20, 40, 60, 80, 120, 200};
    const uint64_t jitters[] = {0, 15'000, 40'000};

    uint64_t worstQuiet = 0;
    uint64_t worstRough = 0;
    for (uint64_t jitterUs : jitters) {
        uint64_t previousGaps = ~uint64_t(0);
        for (uint32_t targetMs : targets) {
            const AudioPoint p = RunAudioPoint(targetMs, false, jitterUs, kFrames);
            std::printf("[csv] %u,0,%llu,%u,%llu,%.1f\n", p.targetMs,
                (unsigned long long)(jitterUs / 1000), p.heldMs,
                (unsigned long long)p.gaps, p.gapsPerMinute);
            Check(p.gaps <= previousGaps || previousGaps == ~uint64_t(0),
                "buying more delay never costs more gaps - if it did, the curve would not be "
                "a trade at all");
            previousGaps = p.gaps;
            if (targetMs == kDefaultAudioDelayMs) {
                if (jitterUs == 0) worstQuiet = p.gaps;
                if (jitterUs == 40'000) worstRough = p.gaps;
            }
        }

        const AudioPoint adaptive = RunAudioPoint(kDefaultAudioDelayMs, true, jitterUs, kFrames);
        std::printf("[csv] %u,1,%llu,%u,%llu,%.1f\n", adaptive.targetMs,
            (unsigned long long)(jitterUs / 1000), adaptive.heldMs,
            (unsigned long long)adaptive.gaps, adaptive.gapsPerMinute);
    }

    Check(worstRough >= worstQuiet,
        "at the shipping 60 ms target a wobbly link costs at least as many gaps as a steady "
        "one, which is what makes the fixed target the wrong answer on both");

    const AudioPoint fixedOnQuiet = RunAudioPoint(kDefaultAudioDelayMs, false, 0, kFrames);
    const AudioPoint adaptiveOnQuiet = RunAudioPoint(kDefaultAudioDelayMs, true, 0, kFrames);
    Check(adaptiveOnQuiet.heldMs < fixedOnQuiet.heldMs &&
              adaptiveOnQuiet.gaps <= fixedOnQuiet.gaps,
        "on a steady link the adaptive target sits strictly below the fixed one on the curve: "
        "less delay for no more gaps, which is the only kind of win worth taking");

    const AudioPoint fixedUnderJitter = RunAudioPoint(kDefaultAudioDelayMs, false, 40'000,
        kFrames);
    const AudioPoint adaptiveUnderJitter = RunAudioPoint(kDefaultAudioDelayMs, true, 40'000,
        kFrames);
    std::printf(
        "[audio]   at 40 ms jitter: fixed held %u ms for %llu gaps, adaptive held "
        "%u ms for %llu gaps\n",
        fixedUnderJitter.heldMs, (unsigned long long)fixedUnderJitter.gaps,
        adaptiveUnderJitter.heldMs, (unsigned long long)adaptiveUnderJitter.gaps);
    Check(adaptiveUnderJitter.heldMs <= fixedUnderJitter.heldMs,
        "under jitter the adaptive target still does not ask for more delay than the fixed "
        "one - the measured cost of adaptation shows up as gaps, not as latency, and the "
        "curve above is what says whether that trade is worth taking");
}

}

void RunAudioJitterBufferTests() {
    TestPrefillHoldsBeforePlaying();
    TestReorderingIsUndone();
    TestDuplicatesAndLatecomers();
    TestGapIsConcealed();
    TestUnderrunRebuffers();
    TestLatencyIsBounded();
    TestResyncOnSequenceJump();
    TestDelayIsClamped();
    TestEmptyPayloadIgnored();
    TestFixedDelayIgnoresTheLinkItIsOn();
    TestAdaptiveTargetFollowsMeasuredJitter();
    TestDelayAgainstGapsIsACurve();
}

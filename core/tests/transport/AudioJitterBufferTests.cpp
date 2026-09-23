#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transport/AudioJitterBuffer.h"

#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

void Push(AudioJitterBuffer& buf, uint32_t seq, uint8_t marker) {
    const std::vector<uint8_t> payload(8, marker);
    AudioPacketView v;
    v.hdr.seq = seq;
    v.hdr.timestampUs = uint64_t(seq) * kAudioFrameMs * 1000;
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
    Check(buf.buffered() == 1, "the buffer starts filling again from scratch");

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
}

#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace deskhub;

namespace {

void TestInOrder() {
    std::printf("[reasm] in-order delivery...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto frames = MakeFrames(60, 30);
    uint64_t now = 1'000'000;
    size_t popped = 0;
    for (const auto& f : frames) {
        for (const auto& d : Packetize(pk, f, now)) Feed(ra, d, now);
        while (auto out = ra.PopReady(now)) {
            Check(popped < frames.size() && SameFrame(*out, frames[popped]),
                "output frame == input frame (in-order)");
            ++popped;
        }
        now += 16'667;
    }
    Check(popped == frames.size(), "got all 60 frames (in-order)");
    Check(ra.stats().framesDropped == 0 && !ra.TakeLossEvent(), "no loss (in-order)");
}

void TestReorder() {
    std::printf("[reasm] shuffled order within a 2-frame window...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto frames = MakeFrames(40, 20);
    uint64_t now = 1'000'000;
    size_t popped = 0;
    for (size_t i = 0; i < frames.size(); i += 2) {
        std::vector<Datagram> batch = Packetize(pk, frames[i], now);
        if (i + 1 < frames.size()) {
            auto more = Packetize(pk, frames[i + 1], now + 16'667);
            batch.insert(batch.end(), std::make_move_iterator(more.begin()),
                std::make_move_iterator(more.end()));
        }
        for (size_t k = batch.size(); k > 1; --k)
            std::swap(batch[k - 1], batch[Rnd() % k]);
        for (const auto& d : batch) Feed(ra, d, now);
        while (auto out = ra.PopReady(now)) {
            Check(SameFrame(*out, frames[popped]), "output frame in correct order (reorder)");
            ++popped;
        }
        now += 2 * 16'667;
    }
    Check(popped == frames.size(), "got all 40 frames (reorder)");
    Check(ra.stats().framesDropped == 0 && !ra.TakeLossEvent(), "no loss (reorder)");
}

void TestDropPacket() {
    std::printf("[reasm] drop 1 packet -> drop that frame, keep decoding, report the bad ref...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    std::vector<uint32_t> invalidated;
    ra.onReferenceLost = [&](uint32_t frameId) { invalidated.push_back(frameId); };
    std::vector<TestFrame> frames;
    for (uint32_t i = 0; i < 20; ++i) {
        TestFrame f{i, (i % 10) == 0, {}};
        f.nal.resize(5 * kMaxVideoPayload - 100);
        for (auto& b : f.nal) b = uint8_t(Rnd());
        frames.push_back(std::move(f));
    }
    uint64_t now = 1'000'000;
    std::vector<uint32_t> got;
    bool sawLoss = false;
    for (const auto& f : frames) {
        auto pkts = Packetize(pk, f, now);
        if (f.id == 5) pkts.erase(pkts.begin() + 2);
        for (const auto& d : pkts) Feed(ra, d, now);
        while (auto out = ra.PopReady(now)) got.push_back(out->frameId);
        sawLoss = sawLoss || ra.TakeLossEvent();
        now += 16'667;
    }
    std::vector<uint32_t> want;
    for (uint32_t i = 0; i < 20; ++i)
        if (i != 5) want.push_back(i);
    Check(got == want, "only the incomplete frame is lost, the rest still reach the decoder");
    Check(sawLoss, "loss event occurred after dropping frame");
    Check(ra.stats().framesDropped == 1 && ra.stats().packetsLost == 1, "drop/lost stats");
    Check(ra.stats().framesSkipped == 0, "no complete frame is thrown away after a loss");
    Check(invalidated == std::vector<uint32_t>{5}, "the dropped frame is reported as a bad ref");
}

void TestStallTimeoutFollowsRtt() {
    std::printf("[reasm] stall window stretches to cover a retransmit round trip...\n");
    const uint64_t frameInterval = 16'667;
    const uint64_t pacedWindow = 2 * frameInterval;
    const uint64_t rttUs = 100'000;

    auto framesDroppedAfter = [&](uint64_t rtt, uint64_t waitUs) {
        Packetizer pk;
        pk.SetSessionId(42);
        Reassembler ra(frameInterval);
        ra.SetRttUs(rtt);
        const TestFrame idr = MakeIdrFrame(0, 4);
        uint64_t now = 1'000'000;
        for (const auto& d : Packetize(pk, idr, now)) Feed(ra, d, now);
        Check(ra.PopReady(now).has_value(), "IDR opens the stream");

        TestFrame next{1, false, {}};
        next.nal.resize(4 * kMaxVideoPayload - 50);
        for (auto& b : next.nal) b = uint8_t(Rnd());
        now += frameInterval;
        auto pkts = Packetize(pk, next, now);
        pkts.pop_back();
        for (const auto& d : pkts) Feed(ra, d, now);

        ra.PopReady(now + waitUs);
        return ra.stats().framesDropped;
    };

    const uint64_t beyondPaced = pacedWindow + frameInterval;
    Check(framesDroppedAfter(0, beyondPaced) == 1,
        "with no RTT measured the paced window still applies");
    Check(framesDroppedAfter(rttUs, beyondPaced) == 0,
        "a long RTT holds the frame long enough for a retransmit");
    Check(framesDroppedAfter(rttUs, rttUs * 2) == 1, "the stretched window is still bounded");
}

void TestDuplicates() {
    std::printf("[reasm] duplicate packets...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto frames = MakeFrames(20, 10);
    uint64_t now = 1'000'000;
    size_t popped = 0;
    for (const auto& f : frames) {
        const auto pkts = Packetize(pk, f, now);
        for (const auto& d : pkts) Feed(ra, d, now);
        for (const auto& d : pkts) Feed(ra, d, now);
        while (auto out = ra.PopReady(now)) {
            Check(SameFrame(*out, frames[popped]), "output frame correct despite duplicate packets");
            ++popped;
        }
        now += 16'667;
    }
    Check(popped == frames.size(), "got all frames (duplicate)");
    Check(ra.stats().framesDropped == 0, "no drop (duplicate)");
}

void TestJoinMidStream() {
    std::printf("[reasm] join mid-stream -> wait for first IDR...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto frames = MakeFrames(16, 10);
    uint64_t now = 1'000'000;
    std::vector<uint32_t> got;
    for (size_t i = 1; i < frames.size(); ++i) {
        Check(ra.WaitingForIdr() == (got.empty()), "WaitingForIdr before first IDR");
        for (const auto& d : Packetize(pk, frames[i], now)) Feed(ra, d, now);
        while (auto out = ra.PopReady(now)) got.push_back(out->frameId);
        now += 16'667;
    }
    std::vector<uint32_t> want{10, 11, 12, 13, 14, 15};
    Check(got == want, "only emits from IDR (join mid-stream)");
    Check(ra.stats().framesSkipped == 9, "9 frames before IDR swallowed");
    Check(!ra.TakeLossEvent(), "swallowing while waiting for IDR is not loss");
}

void TestHeadTimeout() {
    std::printf("[reasm] frame missing a piece past the stall window -> drop only that frame...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    std::vector<TestFrame> frames;
    for (uint32_t i = 0; i < 4; ++i) {
        TestFrame f{i, i == 0 || i == 3, {}};
        f.nal.resize(3 * kMaxVideoPayload);
        for (auto& b : f.nal) b = uint8_t(Rnd());
        frames.push_back(std::move(f));
    }
    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, frames[0], now)) Feed(ra, d, now);
    auto out = ra.PopReady(now);
    Check(out && out->frameId == 0, "frame 0 emitted normally");

    auto pkts1 = Packetize(pk, frames[1], now);
    pkts1.pop_back();
    for (const auto& d : pkts1) Feed(ra, d, now);
    for (const auto& d : Packetize(pk, frames[2], now)) Feed(ra, d, now);
    Check(!ra.PopReady(now).has_value(), "not dropped yet while still within deadline");

    now += 40'000;
    out = ra.PopReady(now);
    Check(out && out->frameId == 2, "frame 2 follows the gap straight through to the decoder");
    Check(ra.stats().framesDropped == 1, "only the incomplete frame counts as dropped");
    Check(ra.TakeLossEvent(), "loss event after timeout");

    for (const auto& d : Packetize(pk, frames[3], now)) Feed(ra, d, now);
    out = ra.PopReady(now);
    Check(out && out->frameId == 3, "the keyframe that repairs the reference still arrives");
}

void TestNackPlanning() {
    std::printf("[reasm] NACK planning: hold, rate-limit, deadline, clamp...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);

    auto f = MakeIdrFrame(0, 3);
    auto pkts = Packetize(pk, f, 1'000'000);
    const uint64_t t0 = 1'000'000;
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i != 1) Feed(ra, pkts[i], t0);

    uint16_t out[8];
    uint32_t fid = 0xFFFF;
    Check(ra.PlanNack(t0, 5000, fid, out) == 0, "no NACK before hold time");
    size_t n = ra.PlanNack(t0 + 2000, 5000, fid, out);
    Check(n == 1 && out[0] == 1 && fid == 0, "NACK lists the missing packet after hold");
    Check(ra.PlanNack(t0 + 2000, 5000, fid, out) == 0, "no re-NACK within the interval");
    Check(ra.PlanNack(t0 + 12000, 5000, fid, out) == 1, "re-NACK allowed after the interval");

    Feed(ra, pkts[1], t0 + 12000);
    auto got = ra.PopReady(t0 + 12000);
    Check(got && got->frameId == 0, "frame completes once the packet arrives");
    Check(ra.PlanNack(t0 + 20000, 5000, fid, out) == 0, "nothing to NACK once delivered");

    {
        Reassembler r2(16'667);
        auto g = MakeIdrFrame(0, 5);
        auto gp = Packetize(pk, g, 2'000'000);
        Feed(r2, gp[0], 2'000'000);
        Feed(r2, gp[4], 2'000'000);
        uint16_t small[2];
        uint32_t id = 0;
        Check(r2.PlanNack(2'002'000, 0, id, std::span<uint16_t>(small, 2)) == 2,
            "NACK clamps to out span size");
    }
    {
        Reassembler r3(16'667);
        auto g = MakeIdrFrame(0, 3);
        auto gp = Packetize(pk, g, 3'000'000);
        Feed(r3, gp[0], 3'000'000);
        uint16_t o[8];
        uint32_t id = 0;
        Check(r3.PlanNack(3'040'000, 0, id, o) == 2, "stalled tail is NACKed for repair");
        Check(r3.PlanNack(3'600'000, 0, id, o) == 0, "no NACK for a frame past the hard timeout");
    }
    {
        Reassembler r4(16'667);
        auto g = MakeIdrFrame(0, 5);
        auto gp = Packetize(pk, g, 4'000'000);
        Feed(r4, gp[0], 4'000'000);
        Feed(r4, gp[1], 4'005'000);
        uint16_t o[8];
        uint32_t id = 0;
        Check(r4.PlanNack(4'006'000, 0, id, o) == 0,
            "in-flight tail with no gaps is not NACKed");
    }
}

TestFrame MakePFrame(uint32_t id, size_t pkts) {
    TestFrame f{id, false, {}};
    f.nal.resize(pkts * kMaxVideoPayload - 100);
    for (auto& b : f.nal) b = uint8_t(Rnd());
    return f;
}

void TestOvertakenDrop() {
    std::printf("[reasm] head incomplete + 2 newer complete -> Overtaken...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    std::vector<Reassembler::DropReason> reasons;
    ra.onFrameDrop = [&](const Reassembler::FrameDropInfo& d) { reasons.push_back(d.reason); };

    const uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    Check(ra.PopReady(now).has_value(), "IDR frame 0 delivered");

    auto p1 = Packetize(pk, MakePFrame(1, 2), now);
    p1.pop_back();
    for (const auto& d : p1) Feed(ra, d, now);
    for (uint32_t id = 2; id <= 3; ++id)
        for (const auto& d : Packetize(pk, MakePFrame(id, 2), now)) Feed(ra, d, now);

    while (ra.PopReady(now)) {}
    Check(std::count(reasons.begin(), reasons.end(), Reassembler::DropReason::Overtaken) >= 1,
        "incomplete head dropped as Overtaken");
}

void TestEvictedDrop() {
    std::printf("[reasm] pending queue full -> oldest Evicted...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    std::vector<Reassembler::DropReason> reasons;
    ra.onFrameDrop = [&](const Reassembler::FrameDropInfo& d) { reasons.push_back(d.reason); };

    const uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);
    for (uint32_t id = 1; id <= 9; ++id) {
        auto p = Packetize(pk, MakePFrame(id, 2), now);
        p.pop_back();
        for (const auto& d : p) Feed(ra, d, now);
    }
    Check(std::count(reasons.begin(), reasons.end(), Reassembler::DropReason::Evicted) >= 1,
        "oldest pending frame evicted when the queue overflows");
}

void TestLatePacketAccounting() {
    std::printf("[reasm] packet arriving after its frame was dropped -> counted late...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    auto p1 = Packetize(pk, MakePFrame(1, 2), now);
    Datagram late = p1.back();
    p1.pop_back();
    for (const auto& d : p1) Feed(ra, d, now);

    now += 40'000;
    ra.PopReady(now);
    Check(ra.stats().framesDropped == 1, "frame 1 dropped on timeout");

    Feed(ra, late, now + 5'000);
    Check(ra.stats().latePackets == 1, "the straggler is counted as late, not lost");
}

void TestMaxGap() {
    std::printf("[reasm] TakeMaxGapMs reports the longest inter-packet gap...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    auto pkts = Packetize(pk, MakeIdrFrame(0, 2), 1'000'000);
    Feed(ra, pkts[0], 1'000'000);
    Feed(ra, pkts[1], 1'150'000);
    Check(ra.TakeMaxGapMs() == 150, "gap measured in ms");
    Check(ra.TakeMaxGapMs() == 0, "read-and-clear: second read is 0");
}

void TestPktCountMismatch() {
    std::printf("[reasm] packet with a mismatched pktCount is ignored...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    auto f = MakeIdrFrame(0, 3);
    auto pkts = Packetize(pk, f, 1'000'000);
    Feed(ra, pkts[0], 1'000'000);

    Datagram forged = pkts[1];
    forged[kCommonHeaderSize + 14] = 0;
    forged[kCommonHeaderSize + 15] = 5;
    Feed(ra, forged, 1'000'000);

    Feed(ra, pkts[1], 1'000'000);
    Feed(ra, pkts[2], 1'000'000);
    auto out = ra.PopReady(1'000'000);
    Check(out.has_value() && SameFrame(*out, f),
        "mismatched-pktCount packet ignored, frame still completes");
}

void TestLossRunBucketsAndDropInfo() {
    std::printf("[reasm] burst loss -> lossRuns buckets + FrameDropInfo autopsy...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    Reassembler::FrameDropInfo info{};
    int drops = 0;
    ra.onFrameDrop = [&](const Reassembler::FrameDropInfo& d) {
        info = d;
        ++drops;
    };

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    const TestFrame f = MakePFrame(1, 40);
    const auto pkts = Packetize(pk, f, now);
    for (size_t i = 0; i < pkts.size(); ++i) {
        const bool dropIt = (i >= 3 && i <= 7) || (i >= 20 && i <= 29) || i == 39;
        if (!dropIt) Feed(ra, pkts[i], now);
    }
    now += 40'000;
    Check(!ra.PopReady(now).has_value(), "incomplete frame dropped, nothing emitted");
    Check(drops == 1 && info.reason == Reassembler::DropReason::Timeout, "dropped once on timeout");
    Check(info.missing == 16 && info.firstMissing == 3 && info.lastMissing == 39,
        "autopsy pinpoints the missing spans (tail hole included)");
    Check(info.total == 40 && info.bytesGot > 0, "autopsy carries totals");

    const auto& st = ra.stats();
    Check(st.lossRuns[3] == 1, "run of 5 lands in the 4..7 bucket");
    Check(st.lossRuns[4] == 1, "run of 10 lands in the 8..15 bucket");
    Check(st.lossRuns[0] == 1, "run of 1 (the tail packet) lands in the first bucket");
    Check(st.lossRunMax == 10, "longest run recorded");
}

void TestLongLossRunBuckets() {
    std::printf("[reasm] very long bursts land in the top lossRuns buckets...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    const auto f1 = Packetize(pk, MakePFrame(1, 30), now);
    for (size_t i = 0; i < f1.size(); ++i)
        if (i < 2 || i > 21) Feed(ra, f1[i], now);

    const auto f2 = Packetize(pk, MakePFrame(2, 40), now);
    Feed(ra, f2[0], now);
    Feed(ra, f2[39], now);

    now += 40'000;
    Check(!ra.PopReady(now).has_value(), "both mutilated frames time out");

    const auto& st = ra.stats();
    Check(st.lossRuns[5] == 1, "a run of 20 lands in the 16..31 bucket");
    Check(st.lossRuns[6] == 1, "a run of 38 lands in the 32+ bucket");
    Check(st.lossRunMax == 38, "and becomes the longest run ever");
}

void TestWireLossSeesHolesThatRepairsHide() {
    std::printf("[reasm] a hole a later arrival fills still counts as absent on the wire...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    const auto pkts = Packetize(pk, MakePFrame(1, 12), now);
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i != 4 && i != 5) Feed(ra, pkts[i], now);
    Feed(ra, pkts[4], now + 1'000);
    Feed(ra, pkts[5], now + 1'000);

    now += 2'000;
    Check(ra.PopReady(now).has_value(), "the frame completes once the hole is filled");

    const auto& st = ra.stats();
    Check(st.packetsLost == 0, "nothing is lost by the old counter - the frame was never dropped");
    Check(st.packetsEverAbsent == 2, "but two packets were absent when the wire was watched");
    Check(st.packetsNeverArrived == 0, "both of them did arrive in the end");
    Check(st.packetsReordered == 2, "and with no NACK sent for them, that is reordering");
    Check(st.absentRuns[1] == 1 && st.absentRunMax == 2,
        "the two sit next to each other, so the wire saw one run of 2");
    uint64_t oldRuns = 0;
    for (uint64_t bucket : st.lossRuns) oldRuns += bucket;
    Check(oldRuns == 0, "the old run histogram stays empty, which is the whole point");
}

void TestWireLossFilesAFecRepairAsRepaired() {
    std::printf("[reasm] a hole FEC patches is absent on the wire, and filed under fec...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    pk.SetFecEnabled(true);
    Reassembler ra(16'667);

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    const auto pkts = Packetize(pk, MakePFrame(1, 16), now);
    size_t dataSeen = 0;
    for (const auto& d : pkts) {
        if (!IsFec(d) && dataSeen++ == 2) continue;
        Feed(ra, d, now);
    }

    Check(ra.PopReady(now).has_value(), "FEC rebuilds the missing packet and the frame completes");

    const auto& st = ra.stats();
    Check(st.packetsLost == 0, "the old counter sees nothing, because nothing was dropped");
    Check(st.packetsEverAbsent == 1, "the wire counter sees the one hole FEC hid");
    Check(st.packetsRepairedByFec == 1, "and files it under fec");
    Check(st.packetsNeverArrived == 0 && st.packetsRepairedAfterNack == 0 &&
              st.packetsReordered == 0,
        "the other three bins stay empty");
}

void TestWireLossFilesANackRepairSeparately() {
    std::printf("[reasm] a packet asked for by name is filed under nack, not reorder...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    ra.SetNackEnabled(true);
    ra.SetRttUs(5'000);

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    const auto pkts = Packetize(pk, MakePFrame(1, 12), now);
    Datagram held;
    for (size_t i = 0; i < pkts.size(); ++i) {
        if (i == 9) {
            held = pkts[i];
            continue;
        }
        Feed(ra, pkts[i], now);
    }

    uint32_t frameId = 0;
    uint16_t wanted[8] = {};
    now += 3'000;
    const size_t asked = ra.PlanNack(now, 5'000, frameId, std::span<uint16_t>(wanted, 8));
    Check(asked == 1 && wanted[0] == 9, "the reassembler asks for exactly the held packet");

    Feed(ra, held, now + 1'000);
    Check(ra.PopReady(now + 1'000).has_value(), "the retransmit completes the frame");

    const auto& st = ra.stats();
    Check(st.packetsEverAbsent == 1, "one packet was absent on the wire");
    Check(st.packetsRepairedAfterNack == 1, "and it is filed under nack, since we asked for it");
    Check(st.packetsReordered == 0, "not under reordering, which would understate the loss");
}

void TestWireLossCountsTailHolesOnce() {
    std::printf("[reasm] a lost tail is absent exactly once, not twice...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);

    uint64_t now = 1'000'000;
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) Feed(ra, d, now);
    ra.PopReady(now);

    const auto pkts = Packetize(pk, MakePFrame(1, 10), now);
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i < 7) Feed(ra, pkts[i], now);

    now += 400'000;
    Check(!ra.PopReady(now).has_value(), "the mutilated frame times out");

    const auto& st = ra.stats();
    Check(st.packetsEverAbsent == 3, "three tail packets never showed up");
    Check(st.packetsNeverArrived == 3, "and all three are filed as gone");
    Check(st.packetsRepairedByFec == 0 && st.packetsRepairedAfterNack == 0 &&
              st.packetsReordered == 0,
        "nothing repaired them");
    Check(st.absentRuns[2] == 1 && st.absentRunMax == 3, "the three form one run of 3");
}

size_t RunBucket(size_t run) {
    if (run <= 3) return run - 1;
    if (run < 8) return 3;
    if (run < 16) return 4;
    if (run < 32) return 5;
    return 6;
}

void TestWireLossRecoversTheLossThatWasInjected() {
    std::printf("[reasm] the wire counter rebuilds a known Gilbert-Elliott process...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);

    LossSource loss(LossKind::GilbertElliott, 0.05, 4.0, 0xC0FFEE);

    size_t truthAbsent = 0;
    uint64_t truthRuns[7] = {};
    uint64_t now = 1'000'000;
    constexpr size_t kFrames = 400;

    for (uint32_t id = 0; id < kFrames; ++id) {
        const bool idr = id == 0;
        const TestFrame frame = idr ? MakeIdrFrame(id, 20) : MakePFrame(id, 20);
        const auto pkts = Packetize(pk, frame, now);

        std::vector<bool> dropped(pkts.size(), false);
        size_t survivors = 0;
        for (size_t i = 0; i < pkts.size(); ++i) {
            dropped[i] = !idr && loss.Drop();
            if (dropped[i]) continue;
            Feed(ra, pkts[i], now);
            ++survivors;
        }

        if (survivors > 0) {
            size_t run = 0;
            for (size_t i = 0; i <= dropped.size(); ++i) {
                if (i < dropped.size() && dropped[i]) {
                    ++truthAbsent;
                    ++run;
                    continue;
                }
                if (run) ++truthRuns[RunBucket(run)];
                run = 0;
            }
        }

        now += 16'667;
        while (ra.PopReady(now)) {
        }
    }

    now += 40 * 16'667;
    while (ra.PopReady(now)) {
    }

    const auto& st = ra.stats();
    Check(truthAbsent > 0, "the injected process really did drop packets, or this proves nothing");
    Check(st.packetsEverAbsent == truthAbsent,
        "every packet the link swallowed is one the wire counter saw, and no more");
    bool runsMatch = true;
    for (size_t i = 0; i < 7; ++i)
        if (st.absentRuns[i] != truthRuns[i]) runsMatch = false;
    if (!runsMatch) {
        std::printf("      injected:");
        for (uint64_t v : truthRuns) std::printf(" %llu", (unsigned long long)v);
        std::printf("\n      measured:");
        for (uint64_t v : st.absentRuns) std::printf(" %llu", (unsigned long long)v);
        std::printf("\n");
    }
    Check(runsMatch, "and the burst histogram it reports is the one that was injected");
    Check(st.packetsLost == truthAbsent,
        "with nothing repairing anything the old counter agrees, which is the control: the two "
        "only part company once FEC or a NACK puts a packet back");
}

void TestSlowIdrAssembly() {
    std::printf("[reasm] large IDR trickling in past 2 frame intervals still completes...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto f = MakeIdrFrame(0, 12);
    const auto pkts = Packetize(pk, f, 1'000'000);
    uint64_t now = 1'000'000;
    size_t fed = 0;
    for (const auto& d : pkts) {
        Feed(ra, d, now);
        if (++fed < pkts.size()) {
            Check(!ra.PopReady(now).has_value(), "no drop while packets keep arriving");
            now += 10'000;
        }
    }
    auto out = ra.PopReady(now);
    Check(out.has_value() && SameFrame(*out, f), "slow IDR assembled intact");
    Check(ra.stats().framesDropped == 0 && !ra.TakeLossEvent(), "no loss during slow assembly");
}

void TestIdrHeadNotOvertaken() {
    std::printf("[reasm] incomplete IDR head survives newer complete frames...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto f = MakeIdrFrame(0, 4);
    auto pkts = Packetize(pk, f, 1'000'000);
    const Datagram tail = pkts.back();
    pkts.pop_back();
    const uint64_t now = 1'000'000;
    for (const auto& d : pkts) Feed(ra, d, now);
    for (uint32_t id = 1; id <= 3; ++id)
        for (const auto& d : Packetize(pk, MakePFrame(id, 2), now)) Feed(ra, d, now);
    Check(!ra.PopReady(now).has_value(), "IDR head not overtaken by complete P-frames");
    Check(ra.stats().framesDropped == 0, "nothing dropped while IDR is in flight");
    Feed(ra, tail, now + 20'000);
    std::vector<uint32_t> got;
    while (auto out = ra.PopReady(now + 20'000)) got.push_back(out->frameId);
    Check(got == std::vector<uint32_t>({0, 1, 2, 3}),
        "IDR then queued P-frames emitted in order");
}

void TestTrickleHardCap() {
    std::printf("[reasm] frame kept alive by duplicates still dies at the hard timeout...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    Reassembler ra(16'667);
    const auto f = MakeIdrFrame(0, 3);
    const auto pkts = Packetize(pk, f, 1'000'000);
    uint64_t now = 1'000'000;
    Feed(ra, pkts[0], now);
    Feed(ra, pkts[1], now);
    int drops = 0;
    ra.onFrameDrop = [&](const Reassembler::FrameDropInfo&) { ++drops; };
    while (now < 1'000'000 + 30 * 16'667 + 20'000) {
        now += 10'000;
        Feed(ra, pkts[0], now);
        ra.PopReady(now);
    }
    Check(drops == 1, "trickling frame dropped exactly once at the hard cap");
    Check(ra.stats().framesDropped == 1, "hard-capped frame counted as dropped");
}

void TestPacketizerEdges() {
    std::printf("[reasm] Packetizer edge inputs -> 0 packets...\n");
    Packetizer pk;
    pk.SetSessionId(42);
    size_t sent = 0;
    auto count = [&](std::span<const uint8_t>) { ++sent; };
    Check(pk.SendFrame({}, 1, 1, false, count) == 0 && sent == 0, "empty frame -> 0 packets");
    const uint8_t one = 0xAB;
    Check(pk.SendFrame(std::span<const uint8_t>(&one, 1), 1, 1, false, nullptr) == 0,
        "missing send callback -> 0");
}

}

void RunReassemblerTests() {
    TestInOrder();
    TestReorder();
    TestDropPacket();
    TestStallTimeoutFollowsRtt();
    TestDuplicates();
    TestJoinMidStream();
    TestHeadTimeout();
    TestNackPlanning();
    TestOvertakenDrop();
    TestEvictedDrop();
    TestLatePacketAccounting();
    TestMaxGap();
    TestPktCountMismatch();
    TestLossRunBucketsAndDropInfo();
    TestLongLossRunBuckets();
    TestWireLossSeesHolesThatRepairsHide();
    TestWireLossFilesAFecRepairAsRepaired();
    TestWireLossFilesANackRepairSeparately();
    TestWireLossCountsTailHolesOnce();
    TestWireLossRecoversTheLossThatWasInjected();
    TestSlowIdrAssembly();
    TestIdrHeadNotOvertaken();
    TestTrickleHardCap();
    TestPacketizerEdges();
}

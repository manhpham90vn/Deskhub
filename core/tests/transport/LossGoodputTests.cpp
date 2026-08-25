#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"

#include <cstdio>
#include <map>
#include <vector>

using namespace deskhub;

namespace {

constexpr uint64_t kFrameIntervalUs = 16'667;
constexpr uint64_t kOneWayDelayUs = 20'000;
constexpr uint64_t kKeyframeLatencyUs = 120'000;
constexpr size_t kFrameCount = 400;
constexpr size_t kTailBurstPackets = 6;
constexpr uint32_t kFramesBetweenLosses = 50;
constexpr size_t kDeltaFramePackets = 8;
constexpr size_t kKeyframePackets = 40;
constexpr uint64_t kAcceptableStallUs = 4 * kFrameIntervalUs;
constexpr double kRequiredGoodputPct = 99.0;

struct LinkOutcome {
    size_t framesFullyReceived = 0;
    size_t framesDelivered = 0;
    size_t lossEvents = 0;
    uint64_t longestStallUs = 0;

    double goodputPct() const {
        return framesFullyReceived
                   ? 100.0 * double(framesDelivered) / double(framesFullyReceived)
                   : 0.0;
    }
};

std::vector<uint8_t> FramePayload(size_t packets) {
    std::vector<uint8_t> nal(packets * kMaxVideoPayload - 64);
    for (auto& b : nal) b = uint8_t(Rnd());
    return nal;
}

bool TailIsDropped(uint32_t frameId, bool idr) {
    return !idr && frameId != 0 && frameId % kFramesBetweenLosses == 0;
}

LinkOutcome RunTailLossLink() {
    Packetizer host;
    host.SetSessionId(7);
    Reassembler viewer(kFrameIntervalUs);
    viewer.SetRttUs(2 * kOneWayDelayUs);

    LinkOutcome outcome;
    std::multimap<uint64_t, Datagram> inFlight;
    uint64_t simNowUs = 0;
    uint64_t keyframeDueAtUs = 0;
    bool keyframePending = false;
    uint64_t lastDeliveryUs = 0;
    bool everDelivered = false;

    viewer.onReferenceLost = [&](uint32_t) {
        ++outcome.lossEvents;
        if (keyframePending) return;
        keyframePending = true;
        keyframeDueAtUs = simNowUs + kOneWayDelayUs + kKeyframeLatencyUs;
    };

    for (uint32_t frameId = 0; frameId < kFrameCount; ++frameId) {
        const uint64_t sendUs = uint64_t(frameId) * kFrameIntervalUs;
        simNowUs = sendUs;

        const bool idr = frameId == 0 || (keyframePending && sendUs >= keyframeDueAtUs);
        if (idr) keyframePending = false;

        const std::vector<uint8_t> nal =
            FramePayload(idr ? kKeyframePackets : kDeltaFramePackets);
        std::vector<Datagram> wire;
        host.SendFrame(nal, frameId, sendUs, idr, [&](std::span<const uint8_t> d) {
            wire.emplace_back(d.begin(), d.end());
        });

        if (TailIsDropped(frameId, idr))
            wire.resize(wire.size() > kTailBurstPackets ? wire.size() - kTailBurstPackets : 0);
        else
            ++outcome.framesFullyReceived;

        for (const Datagram& d : wire) inFlight.emplace(sendUs + kOneWayDelayUs, d);

        simNowUs = sendUs + kFrameIntervalUs;
        while (!inFlight.empty() && inFlight.begin()->first <= simNowUs) {
            const auto it = inFlight.begin();
            Feed(viewer, it->second, it->first);
            inFlight.erase(it);
        }

        while (auto out = viewer.PopReady(simNowUs)) {
            ++outcome.framesDelivered;
            const uint64_t sinceLastUs = simNowUs - lastDeliveryUs;
            if (everDelivered && sinceLastUs > outcome.longestStallUs)
                outcome.longestStallUs = sinceLastUs;
            lastDeliveryUs = simNowUs;
            everDelivered = true;
        }
    }
    return outcome;
}

void TestTailLossCostsOnlyTheFramesItTookOut() {
    std::printf("[goodput] tail loss must cost only the frames it actually took out...\n");
    const LinkOutcome outcome = RunTailLossLink();

    Check(outcome.lossEvents >= kFrameCount / kFramesBetweenLosses - 1,
        "the simulated link really does lose frames, or the gate is measuring nothing");
    Check(outcome.framesFullyReceived > kFrameCount / 2,
        "and it still delivers most frames intact, or the gate is measuring a dead link");

    if (outcome.goodputPct() < kRequiredGoodputPct)
        std::printf("[goodput]   %zu of %zu intact frames reached the decoder (%.1f%%)\n",
            outcome.framesDelivered, outcome.framesFullyReceived, outcome.goodputPct());
    Check(outcome.goodputPct() >= kRequiredGoodputPct,
        "every frame whose packets all arrived reaches the decoder, not only those after an IDR");

    if (outcome.longestStallUs > kAcceptableStallUs)
        std::printf("[goodput]   longest gap between delivered frames: %llu ms\n",
            (unsigned long long)(outcome.longestStallUs / 1000));
    Check(outcome.longestStallUs <= kAcceptableStallUs,
        "and no gap long enough to read as a frozen picture");
}

}

void RunLossGoodputTests() {
    TestTailLossCostsOnlyTheFramesItTookOut();
}

#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/control/BitrateController.h"
#include "deskhub/control/LinkStats.h"
#include "deskhub/transport/FecScheme.h"
#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"
#include "deskhub/transport/RetransmitCache.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <functional>
#include <iterator>
#include <map>
#include <string_view>
#include <vector>

using namespace deskhub;

namespace {

constexpr uint64_t kFrameIntervalUs = 16'667;
constexpr uint64_t kOneWayDelayUs = 20'000;
constexpr uint64_t kNearOneWayDelayUs = 2'000;
constexpr uint64_t kKeyframeLatencyUs = 120'000;
constexpr size_t kFrameCount = 400;
constexpr size_t kArmingFrameCount = 1800;
constexpr uint64_t kFeedbackIntervalUs = 1'000'000;
constexpr uint32_t kStartBitrateBps = 20'000'000;
constexpr uint32_t kMinBitrateBps = 1'000'000;
constexpr size_t kTailBurstPackets = 6;
constexpr uint32_t kFramesBetweenLosses = 50;
constexpr size_t kDeltaFramePackets = 8;
constexpr size_t kKeyframePackets = 40;
constexpr uint64_t kAcceptableStallUs = 4 * kFrameIntervalUs;
constexpr double kRequiredGoodputPct = 99.0;
constexpr uint64_t kSweepSeed = 0x5DEECE66D;
constexpr double kSweepBurstPackets = 4.0;
constexpr double kMeasuredLossRate = 0.001;
constexpr double kMeasuredBurstPackets = 1.0;

const char* KindName(LossKind kind) {
    return kind == LossKind::Uniform ? "uniform" : "gilbert-elliott";
}

struct LinkOutcome {
    size_t framesSent = 0;
    size_t framesFullyReceived = 0;
    size_t framesDamaged = 0;
    size_t framesRescued = 0;
    size_t framesDelivered = 0;
    size_t lossEvents = 0;
    size_t keyframeRequests = 0;
    size_t packetsSent = 0;
    size_t packetsDropped = 0;
    size_t parityPacketsSent = 0;
    size_t feedbackTicks = 0;
    size_t ticksArmed = 0;
    size_t nackRequests = 0;
    size_t nackPacketsAsked = 0;
    size_t nackPacketsServed = 0;
    size_t nackPacketsDelivered = 0;
    uint64_t longestStallUs = 0;

    double goodputPct() const {
        return framesFullyReceived
                   ? 100.0 * double(framesDelivered) / double(framesFullyReceived)
                   : 0.0;
    }

    double rescuePct() const {
        return framesDamaged ? 100.0 * double(framesRescued) / double(framesDamaged) : 0.0;
    }

    double deliveredPct() const {
        return framesSent ? 100.0 * double(framesDelivered) / double(framesSent) : 0.0;
    }

    double overheadPct() const {
        const size_t data = packetsSent > parityPacketsSent ? packetsSent - parityPacketsSent : 0;
        return data ? 100.0 * double(parityPacketsSent) / double(data) : 0.0;
    }

    double dropPct() const {
        return packetsSent ? 100.0 * double(packetsDropped) / double(packetsSent) : 0.0;
    }

    double armedPct() const {
        return feedbackTicks ? 100.0 * double(ticksArmed) / double(feedbackTicks) : 0.0;
    }

    double idrPerMinute() const {
        const double seconds = double(framesSent) * double(kFrameIntervalUs) / 1e6;
        return seconds > 0.0 ? double(keyframeRequests) * 60.0 / seconds : 0.0;
    }
};

struct LinkConfig {
    std::string_view scheme = kDefaultFecScheme;
    bool fec = false;
    size_t groups = 0;
    bool armFromFeedback = false;
    bool nack = false;
    size_t parity = 1;
    uint64_t oneWayDelayUs = kOneWayDelayUs;
    size_t frames = kFrameCount;
    size_t overtakenLimit = 0;
};

struct NackRequest {
    uint32_t frameId = 0;
    std::vector<uint16_t> indices{};
};

using DropFn = std::function<bool(uint32_t frameId, bool idr, const Datagram& packet)>;

std::vector<uint8_t> FramePayload(size_t packets) {
    std::vector<uint8_t> nal(packets * kMaxVideoPayload - 64);
    for (auto& b : nal) b = uint8_t(Rnd());
    return nal;
}

LinkOutcome RunLink(const LinkConfig& config, const DropFn& drop) {
    Packetizer host;
    host.SetSessionId(7);
    host.SetFecEnabled(config.fec);
    host.SetFecScheme(config.scheme);
    host.SetFecParityPerGroup(config.parity);
    host.SetFecGroups(config.groups);

    Reassembler viewer(kFrameIntervalUs);
    viewer.SetFecScheme(config.scheme);
    viewer.SetFecParityPerGroup(config.parity);
    viewer.SetRttUs(2 * config.oneWayDelayUs);
    viewer.SetNackEnabled(config.nack);
    if (config.overtakenLimit) viewer.SetOvertakenLimit(config.overtakenLimit);

    BitrateController rate(kStartBitrateBps, kMinBitrateBps);
    LinkStats link(0);
    RetransmitCache retx;
    std::multimap<uint64_t, NackRequest> nacksInFlight;
    bool fecArmed = config.fec;
    uint64_t nextFeedbackUs = kFeedbackIntervalUs;
    uint64_t deliveredBytes = 0;

    LinkOutcome outcome;
    std::vector<bool> damaged(config.frames, false);
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
        ++outcome.keyframeRequests;
        keyframeDueAtUs = simNowUs + config.oneWayDelayUs + kKeyframeLatencyUs;
    };

    for (uint32_t frameId = 0; frameId < config.frames; ++frameId) {
        const uint64_t sendUs = uint64_t(frameId) * kFrameIntervalUs;
        simNowUs = sendUs;
        host.SetFecEnabled(fecArmed);

        const bool idr = frameId == 0 || (keyframePending && sendUs >= keyframeDueAtUs);
        if (idr) keyframePending = false;

        const std::vector<uint8_t> nal =
            FramePayload(idr ? kKeyframePackets : kDeltaFramePackets);
        std::vector<Datagram> wire;
        host.SendFrame(nal, frameId, sendUs, idr, [&](std::span<const uint8_t> d) {
            wire.emplace_back(d.begin(), d.end());
            if (config.nack) retx.Store(d);
        });
        ++outcome.framesSent;

        while (!nacksInFlight.empty() && nacksInFlight.begin()->first <= sendUs) {
            const auto ask = nacksInFlight.begin();
            for (uint16_t index : ask->second.indices) {
                const std::span<const uint8_t> held = retx.Find(ask->second.frameId, index);
                if (held.empty()) continue;
                ++outcome.nackPacketsServed;
                const Datagram again(held.begin(), held.end());
                if (drop(ask->second.frameId, false, again)) continue;
                ++outcome.nackPacketsDelivered;
                inFlight.emplace(ask->first + config.oneWayDelayUs, again);
            }
            nacksInFlight.erase(ask);
        }

        bool lostData = false;
        for (const Datagram& d : wire) {
            ++outcome.packetsSent;
            if (IsFec(d)) ++outcome.parityPacketsSent;
            if (drop(frameId, idr, d)) {
                ++outcome.packetsDropped;
                if (!IsFec(d)) lostData = true;
                continue;
            }
            inFlight.emplace(sendUs + config.oneWayDelayUs, d);
        }
        if (lostData) {
            damaged[frameId] = true;
            ++outcome.framesDamaged;
        } else {
            ++outcome.framesFullyReceived;
        }

        simNowUs = sendUs + kFrameIntervalUs;
        while (!inFlight.empty() && inFlight.begin()->first <= simNowUs) {
            const auto it = inFlight.begin();
            deliveredBytes += it->second.size();
            Feed(viewer, it->second, it->first);
            inFlight.erase(it);
        }

        if (config.nack) {
            uint16_t wanted[kMaxNackIndices];
            uint32_t nackFrameId = 0;
            const size_t n = viewer.PlanNack(simNowUs, 2 * config.oneWayDelayUs, nackFrameId,
                std::span<uint16_t>(wanted, kMaxNackIndices));
            if (n) {
                ++outcome.nackRequests;
                outcome.nackPacketsAsked += n;
                NackRequest ask;
                ask.frameId = nackFrameId;
                ask.indices.assign(wanted, wanted + n);
                nacksInFlight.emplace(simNowUs + config.oneWayDelayUs, std::move(ask));
            }
        }

        while (simNowUs >= nextFeedbackUs) {
            const LinkWindow w = link.Close(viewer.stats(), deliveredBytes, 0, nextFeedbackUs);
            const Feedback fb = MakeFeedback(w, 2 * uint32_t(config.oneWayDelayUs));
            const BitrateDecision decision = rate.Update(fb, 0, nextFeedbackUs);
            if (config.armFromFeedback) fecArmed = decision.fecEnabled;
            ++outcome.feedbackTicks;
            if (fecArmed) ++outcome.ticksArmed;
            deliveredBytes = 0;
            nextFeedbackUs += kFeedbackIntervalUs;
        }

        while (auto out = viewer.PopReady(simNowUs)) {
            ++outcome.framesDelivered;
            if (out->frameId < damaged.size() && damaged[out->frameId]) ++outcome.framesRescued;
            const uint64_t sinceLastUs = simNowUs - lastDeliveryUs;
            if (everDelivered && sinceLastUs > outcome.longestStallUs)
                outcome.longestStallUs = sinceLastUs;
            lastDeliveryUs = simNowUs;
            everDelivered = true;
        }
    }
    return outcome;
}

LinkOutcome RunTailLossLink() {
    size_t indexInFrame = 0;
    uint32_t lastFrameId = 0xFFFFFFFF;
    size_t wireCount = 0;
    return RunLink({}, [&](uint32_t frameId, bool idr, const Datagram&) {
        if (frameId != lastFrameId) {
            lastFrameId = frameId;
            indexInFrame = 0;
            wireCount = idr ? kKeyframePackets : kDeltaFramePackets;
        }
        const size_t at = indexInFrame++;
        const bool lossy = !idr && frameId != 0 && frameId % kFramesBetweenLosses == 0;
        return lossy && at + kTailBurstPackets >= wireCount;
    });
}

void PrintCsvHeader() {
    std::printf(
        "[csv] scheme,fec,armed_from_feedback,groups,model,loss_pct,burst_pkts,seed,"
        "frames_sent,frames_intact,frames_damaged,frames_rescued,rescue_pct,parity_rows,parity_sent,"
        "overhead_pct,armed_pct,nack_requests,nack_asked,nack_served,nack_delivered,"
        "frames_delivered,"
        "delivered_pct,idr_requests,idr_per_min,pkt_drop_pct,longest_stall_ms,"
        "nack,rtt_ms,overtaken_limit\n");
}

void PrintCsvRow(const LinkConfig& config, LossKind kind, double lossRate,
    double burstPackets, const LinkOutcome& outcome) {
    std::printf(
        "[csv] %.*s,%d,%d,%zu,%s,%.2f,%.1f,%llu,%zu,%zu,%zu,%zu,%.2f,%zu,%zu,%.1f,%.1f,%zu,%zu,"
        "%zu,"
        "%zu,%zu,%.2f,%zu,%.2f,%.2f,%llu,%d,%llu,%zu\n",
        int(config.scheme.size()), config.scheme.data(), config.fec ? 1 : 0,
        config.armFromFeedback ? 1 : 0, config.groups, KindName(kind), lossRate * 100.0,
        kind == LossKind::GilbertElliott ? burstPackets : 1.0,
        (unsigned long long)kSweepSeed, outcome.framesSent, outcome.framesFullyReceived,
        outcome.framesDamaged, outcome.framesRescued, outcome.rescuePct(),
        config.parity, outcome.parityPacketsSent, outcome.overheadPct(), outcome.armedPct(), outcome.nackRequests,
        outcome.nackPacketsAsked, outcome.nackPacketsServed, outcome.nackPacketsDelivered,
        outcome.framesDelivered,
        outcome.deliveredPct(), outcome.keyframeRequests, outcome.idrPerMinute(),
        outcome.dropPct(), (unsigned long long)(outcome.longestStallUs / 1000),
        config.nack ? 1 : 0, (unsigned long long)(2 * config.oneWayDelayUs / 1000),
        config.overtakenLimit);
}

void PrintNackCsvHeader() {
    std::printf(
        "[csv] repair,rtt_ms,hold_frames,loss_pct,burst_pkts,frames_damaged,frames_rescued,"
        "rescue_pct,parity_sent,overhead_pct,nack_requests,nack_served,delivered_pct,"
        "idr_per_min,longest_stall_ms\n");
}

void PrintNackCsvRow(const char* repair, const LinkConfig& config, double lossRate,
    double burstPackets, const LinkOutcome& outcome) {
    std::printf("[csv] %s,%llu,%zu,%.2f,%.1f,%zu,%zu,%.2f,%zu,%.1f,%zu,%zu,%.2f,%.2f,%llu\n",
        repair, (unsigned long long)(2 * config.oneWayDelayUs / 1000), config.overtakenLimit,
        lossRate * 100.0, burstPackets, outcome.framesDamaged, outcome.framesRescued,
        outcome.rescuePct(), outcome.parityPacketsSent, outcome.overheadPct(),
        outcome.nackRequests, outcome.nackPacketsServed, outcome.deliveredPct(),
        outcome.idrPerMinute(), (unsigned long long)(outcome.longestStallUs / 1000));
}

LinkOutcome RunPoint(const LinkConfig& config, LossKind kind, double lossRate,
    double burstPackets) {
    LossSource loss(kind, lossRate, burstPackets, kSweepSeed);
    return RunLink(config, [&](uint32_t, bool, const Datagram&) { return loss.Drop(); });
}

LinkOutcome RunSweepPoint(const LinkConfig& config, LossKind kind, double lossRate,
    double burstPackets = kSweepBurstPackets) {
    const LinkOutcome outcome = RunPoint(config, kind, lossRate, burstPackets);
    PrintCsvRow(config, kind, lossRate, burstPackets, outcome);
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

void TestBurstLossIsNotUniformLoss() {
    std::printf(
        "[goodput] a burst model must rank differently from uniform, or it is not "
        "modelling bursts...\n");
    PrintCsvHeader();

    const LinkConfig derived{.fec = true};
    const LinkOutcome uniform = RunSweepPoint(derived, LossKind::Uniform, 0.05);
    const LinkOutcome bursty = RunSweepPoint(derived, LossKind::GilbertElliott, 0.05);

    Check(uniform.packetsDropped > 0 && bursty.packetsDropped > 0,
        "both models actually drop packets at 5%, or the sweep is measuring a clean link");
    if (bursty.rescuePct() >= uniform.rescuePct())
        std::printf(
            "[goodput]   uniform rescued %.1f%% of damaged frames, "
            "Gilbert-Elliott rescued %.1f%%\n",
            uniform.rescuePct(), bursty.rescuePct());
    Check(bursty.rescuePct() < uniform.rescuePct(),
        "one parity per group rescues scattered loss and not clustered loss - if both models "
        "score the same, the burst model is not modelling bursts");
}

void TestInterleaveDepthIsWorthSweeping() {
    std::printf(
        "[goodput] interleave depth changes what a burst costs, so it is a "
        "parameter...\n");

    const LinkConfig shallow{.fec = true};
    const LinkConfig deep{.fec = true, .groups = kDeltaFramePackets};
    const LinkOutcome atDerivedDepth = RunSweepPoint(shallow, LossKind::GilbertElliott, 0.05);
    const LinkOutcome atFullDepth = RunSweepPoint(deep, LossKind::GilbertElliott, 0.05);

    if (atFullDepth.rescuePct() <= atDerivedDepth.rescuePct())
        std::printf("[goodput]   derived depth rescued %.1f%%, full depth rescued %.1f%%\n",
            atDerivedDepth.rescuePct(), atFullDepth.rescuePct());
    Check(atFullDepth.rescuePct() > atDerivedDepth.rescuePct(),
        "spreading a delta frame over more groups survives bursts the derived depth cannot");
}

void TestArmingPolicyDecidesWhetherFecRunsAtAll() {
    std::printf(
        "[goodput] the arming policy, not the scheme, decides whether FEC ever "
        "runs...\n");

    const LinkConfig always{.fec = true, .frames = kArmingFrameCount};
    const LinkConfig asShipped{
        .fec = true, .armFromFeedback = true, .frames = kArmingFrameCount};

    const LinkOutcome fixedAtMeasured =
        RunSweepPoint(always, LossKind::GilbertElliott, kMeasuredLossRate, kMeasuredBurstPackets);
    const LinkOutcome armedAtMeasured = RunSweepPoint(asShipped, LossKind::GilbertElliott,
        kMeasuredLossRate, kMeasuredBurstPackets);

    Check(fixedAtMeasured.parityPacketsSent > 0 && fixedAtMeasured.framesDamaged > 0,
        "the measured operating point still damages frames, so there is something to rescue");
    Check(armedAtMeasured.armedPct() < 50.0,
        "at the loss rate and burst length real home WiFi showed, one lost packet in a second "
        "rounds to 0% and the host stands FEC down for most of the run");
    Check(armedAtMeasured.parityPacketsSent < fixedAtMeasured.parityPacketsSent,
        "so less parity reaches the wire than any scheme comparison assumes");

    const LinkOutcome armedUnderRealLoss =
        RunSweepPoint(asShipped, LossKind::GilbertElliott, 0.05);
    Check(armedUnderRealLoss.armedPct() > 50.0,
        "at 5% the same policy does arm and stays armed - it is the low-loss regime it is "
        "blind to, not loss in general");
}

void TestNackOnlyIsBoundedByHowLongAFrameIsHeld() {
    std::printf(
        "[goodput] whether NACK-only works at all is set by RTT against the "
        "overtaken rule...\n");

    const LinkConfig farNack{
        .nack = true, .oneWayDelayUs = kOneWayDelayUs, .frames = kArmingFrameCount};
    const LinkConfig nearNack{
        .nack = true, .oneWayDelayUs = kNearOneWayDelayUs, .frames = kArmingFrameCount};
    const LinkConfig nearBare{.oneWayDelayUs = kNearOneWayDelayUs, .frames = kArmingFrameCount};

    const LinkOutcome far =
        RunSweepPoint(farNack, LossKind::GilbertElliott, kMeasuredLossRate, kMeasuredBurstPackets);
    const LinkOutcome near = RunSweepPoint(nearNack, LossKind::GilbertElliott, kMeasuredLossRate,
        kMeasuredBurstPackets);
    const LinkOutcome nearWithout = RunSweepPoint(nearBare, LossKind::GilbertElliott,
        kMeasuredLossRate, kMeasuredBurstPackets);

    Check(far.nackPacketsServed == far.nackPacketsAsked && far.nackPacketsAsked > 0,
        "the host serves every index the viewer asks for, so the retransmit path itself works");
    Check(far.framesRescued == 0,
        "yet at 40 ms round trip not one frame is saved: PopReady drops an incomplete frame "
        "once two newer ones are complete, which at 60 fps is about 33 ms - sooner than the "
        "repair can arrive");
    Check(near.framesRescued > nearWithout.framesRescued,
        "shorten the round trip and the same retransmit path starts rescuing frames, so the "
        "binding constraint is the holding rule against RTT, not the retransmit");

    const LinkConfig fecOnly{.fec = true, .frames = kArmingFrameCount};
    const LinkConfig both{.fec = true, .nack = true, .frames = kArmingFrameCount};
    const LinkOutcome parity = RunSweepPoint(fecOnly, LossKind::GilbertElliott, kMeasuredLossRate,
        kMeasuredBurstPackets);
    const LinkOutcome parityAndNack =
        RunSweepPoint(both, LossKind::GilbertElliott, kMeasuredLossRate, kMeasuredBurstPackets);

    Check(parityAndNack.nackRequests == 0 && parityAndNack.framesRescued == parity.framesRescued,
        "with parity armed no frame stays incomplete long enough to be nacked at all, so FEC "
        "and NACK never actually compete for the same repair");
}

void TestFullSchemeSweep() {
    std::printf("[goodput] the Phase 3 sweep: scheme x parity x depth x loss x burst x rtt...\n");
    PrintCsvHeader();

    const size_t depths[] = {0, 4, kDeltaFramePackets};
    const double lossRates[] = {kMeasuredLossRate, 0.01, 0.05};
    const double bursts[] = {kMeasuredBurstPackets, kSweepBurstPackets};
    const uint64_t delays[] = {kNearOneWayDelayUs, kOneWayDelayUs};

    size_t points = 0;
    bool sweptAMultiRowScheme = false;
    size_t bestRescuedByOneRow = 0;
    size_t bestRescuedByManyRows = 0;

    for (std::string_view scheme : FecSchemesUnderTest()) {
        const std::unique_ptr<FecScheme> probe = MakeFecScheme(scheme);
        if (!probe) continue;

        for (size_t parity = 1; parity <= 3; ++parity) {
            if (!probe->SetParityPerGroup(parity)) continue;
            sweptAMultiRowScheme = sweptAMultiRowScheme || parity > 1;
            for (size_t depth : depths)
                for (double lossRate : lossRates)
                    for (double burst : bursts)
                        for (uint64_t delay : delays) {
                            const LinkConfig config{.scheme = scheme,
                                .fec = true,
                                .groups = depth,
                                .parity = parity,
                                .oneWayDelayUs = delay};
                            const LinkOutcome outcome = RunSweepPoint(config,
                                LossKind::GilbertElliott, lossRate, burst);
                            ++points;

                            Check(outcome.framesDelivered > 0,
                                "every point in the sweep still delivers a picture");
                            Check(outcome.parityPacketsSent > 0,
                                "and every point actually put parity on the wire, so it is "
                                "measuring the scheme rather than a disarmed one");

                            if (parity == 1)
                                bestRescuedByOneRow =
                                    std::max(bestRescuedByOneRow, outcome.framesRescued);
                            else
                                bestRescuedByManyRows =
                                    std::max(bestRescuedByManyRows, outcome.framesRescued);
                        }
        }
    }

    const size_t gridPointsPerParityRow =
        std::size(depths) * std::size(lossRates) * std::size(bursts) * std::size(delays);
    Check(points >= gridPointsPerParityRow * FecSchemesUnderTest().size(),
        "the sweep covers the grid rather than a couple of corners");
    if (sweptAMultiRowScheme)
        Check(bestRescuedByManyRows > bestRescuedByOneRow,
            "somewhere in the grid more parity rows rescue more frames than one row ever can - if "
            "they never did, Reed-Solomon would have nothing to offer over XOR");

    const LinkConfig shipping{.fec = true};
    const LinkConfig deepest{.fec = true, .groups = kDeltaFramePackets};
    const LinkOutcome asShipped =
        RunSweepPoint(shipping, LossKind::GilbertElliott, 0.05, kSweepBurstPackets);
    const LinkOutcome atFullDepth =
        RunSweepPoint(deepest, LossKind::GilbertElliott, 0.05, kSweepBurstPackets);

    Check(atFullDepth.framesRescued > asShipped.framesRescued,
        "spreading a frame over more groups does rescue more of it");
    Check(atFullDepth.overheadPct() > asShipped.overheadPct() * 3,
        "but it costs more than three times the parity to do it - at one group per packet "
        "every packet carries its own parity, which is duplication rather than coding, and "
        "no rescue rate is worth reading without the overhead beside it");
}

void TestDepthSweepOnXorAlone() {
    std::printf(
        "[goodput] sweep before writing: what depth alone buys on the XOR already "
        "shipping...\n");

    size_t bestAtDerived = 0;
    size_t bestAtDepth = 0;
    for (double lossRate : {kMeasuredLossRate, 0.01, 0.05}) {
        const LinkConfig derived{.fec = true};
        const LinkConfig spread{.fec = true, .groups = kDeltaFramePackets};
        bestAtDerived += RunSweepPoint(derived, LossKind::GilbertElliott, lossRate,
            kSweepBurstPackets)
                             .framesRescued;
        bestAtDepth += RunSweepPoint(spread, LossKind::GilbertElliott, lossRate,
            kSweepBurstPackets)
                           .framesRescued;
    }

    if (bestAtDepth <= bestAtDerived)
        std::printf("[goodput]   derived depth rescued %zu, full depth rescued %zu\n",
            bestAtDerived, bestAtDepth);
    Check(bestAtDepth > bestAtDerived,
        "the parameter already in the shipping scheme is worth sweeping before any new "
        "implementation is written - that is what 'sweep before writing' means");
}

void TestNackAndHybridSweep() {
    std::printf(
        "[goodput] the NACK sweep: repair mode x round trip x how long a frame is held...\n");
    PrintNackCsvHeader();

    struct Mode {
        const char* name;
        bool fec;
        bool nack;
    };
    const Mode modes[] = {{"fec-only", true, false}, {"nack-only", false, true},
        {"fec+nack", true, true}};
    const uint64_t oneWayDelays[] = {2'000, 10'000, 20'000, 40'000};
    const size_t holdLimits[] = {0, 8, 30};
    const double lossRates[] = {0.01, 0.05};

    size_t points = 0;
    LinkOutcome nackShortHold{};
    LinkOutcome nackLongHold{};
    LinkOutcome fecOnlyFar{};
    LinkOutcome nearShortHold{};
    LinkOutcome nearLongHold{};

    for (const Mode& mode : modes)
        for (uint64_t delay : oneWayDelays)
            for (size_t hold : holdLimits)
                for (double lossRate : lossRates) {
                    const LinkConfig config{.fec = mode.fec,
                        .nack = mode.nack,
                        .oneWayDelayUs = delay,
                        .overtakenLimit = hold};
                    const LinkOutcome outcome = RunPoint(config, LossKind::GilbertElliott,
                        lossRate, kSweepBurstPackets);
                    PrintNackCsvRow(mode.name, config, lossRate, kSweepBurstPackets, outcome);
                    ++points;

                    Check(outcome.framesDelivered > 0,
                        "every point in the NACK sweep still delivers a picture");

                    if (lossRate != 0.05) continue;
                    const bool far = delay == 20'000;
                    const bool near = delay == 2'000;
                    if (mode.nack && !mode.fec && far && hold == 0) nackShortHold = outcome;
                    if (mode.nack && !mode.fec && far && hold == 8) nackLongHold = outcome;
                    if (mode.fec && !mode.nack && far) fecOnlyFar = outcome;
                    if (mode.nack && !mode.fec && near && hold == 0) nearShortHold = outcome;
                    if (mode.nack && !mode.fec && near && hold == 8) nearLongHold = outcome;
                }

    Check(points == std::size(modes) * std::size(oneWayDelays) * std::size(holdLimits) *
                        std::size(lossRates),
        "the sweep covers the grid rather than a couple of corners");

    Check(nackLongHold.framesRescued > nackShortHold.framesRescued * 2,
        "holding an incomplete frame long enough for the repair to arrive is what decides "
        "whether NACK works at all - at a 40 ms round trip the shipping rule of two newer "
        "frames throws the frame away before its own repair lands");

    Check(nackLongHold.idrPerMinute() < fecOnlyFar.idrPerMinute() &&
              nackLongHold.parityPacketsSent == 0,
        "and once it is held long enough, NACK alone beats parity on the objective function "
        "while spending no parity at all - the earlier 'NACK-only is a dead end' reading was "
        "taken at 0.1 % loss with the hold rule fixed at two, which measured the rule");

    Check(nearShortHold.framesRescued == nearLongHold.framesRescued,
        "at a 4 ms round trip the repair already arrives inside two frames, so the longer "
        "hold changes nothing - the win is a function of RTT, which is what makes this a "
        "hold rule to derive rather than a constant to raise");
}

void TestSweepCoversEverySchemeAndModel() {
    std::printf("[goodput] the sweep runs every scheme in the matrix under both models...\n");
    for (std::string_view scheme : FecSchemesUnderTest())
        for (size_t groups : {size_t(0), kDeltaFramePackets})
            for (double lossRate : {0.01, 0.05}) {
                const LinkConfig config{.scheme = scheme, .fec = true, .groups = groups};
                const LinkOutcome uniform = RunSweepPoint(config, LossKind::Uniform, lossRate);
                const LinkOutcome bursty =
                    RunSweepPoint(config, LossKind::GilbertElliott, lossRate);
                Check(uniform.framesDelivered > 0 && bursty.framesDelivered > 0,
                    "every point in the sweep still delivers a picture");
            }
}

}

void RunLossGoodputTests() {
    TestTailLossCostsOnlyTheFramesItTookOut();
    TestBurstLossIsNotUniformLoss();
    TestInterleaveDepthIsWorthSweeping();
    TestArmingPolicyDecidesWhetherFecRunsAtAll();
    TestNackOnlyIsBoundedByHowLongAFrameIsHeld();
    TestSweepCoversEverySchemeAndModel();
    TestDepthSweepOnXorAlone();
    TestFullSchemeSweep();
    TestNackAndHybridSweep();
}

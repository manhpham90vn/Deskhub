#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/host/ViewerFeedback.h"

#include <cstdio>
#include <memory>
#include <vector>

using namespace deskhub;

namespace {

constexpr uint32_t kStartBps = 20'000'000;
constexpr uint32_t kMinBps = 1'000'000;
constexpr uint64_t kT0 = 5'000'000;

void GiveConnectedSession(SourcePipelineState& st) {
    ScreenHostCallbacks cb;
    cb.send = [](std::span<const uint8_t>) {};
    cb.randomBytes = TestRandomBytes;
    st.session = std::make_unique<ScreenHostSession>(cb, StreamParams{1920, 1080, 60, kStartBps});
    st.session->SetPasscode(kTestPasscode);

    uint8_t buf[kMaxDatagram];
    Hello h{};
    h.passcode = kTestPasscode;
    h.clientId = 0x1234;
    h.codecMask = kCodecMaskH264;
    h.maxWidth = 1920;
    h.maxHeight = 1080;
    const size_t n = BuildHello(buf, h);
    st.session->HandlePacket(std::span<const uint8_t>(buf, n), kT0, kTestViewer);
}

Feedback CleanLink() {
    Feedback fb{};
    fb.rttMs = 8;
    fb.lossPct = 0;
    fb.recvBitrateKbps = 19'000;
    return fb;
}

Feedback LossyLink(uint8_t lossPct) {
    Feedback fb = CleanLink();
    fb.lossPct = lossPct;
    fb.recvBitrateKbps = 6'000;
    return fb;
}

struct Recorder {
    std::vector<uint32_t> bitratesOffered;
    bool encoderAccepts = true;
    int qualityCalls = 0;
    StreamSize sizeToReport{1280, 720};

    FeedbackHooks Hooks() {
        FeedbackHooks h;
        h.setEncoderBitrate = [this](uint32_t bps) {
            bitratesOffered.push_back(bps);
            return encoderAccepts;
        };
        h.applyQualityStep = [this](const QualityStep&, const QualityStep&) {
            ++qualityCalls;
            return sizeToReport;
        };
        return h;
    }
};

void TestFeedbackIsMirroredForTheUi() {
    std::printf("[hostfb] the numbers the UI shows come straight off the wire...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;

    Check(!st.haveFeedback.load(), "nothing reported before the first FEEDBACK");

    Feedback fb = CleanLink();
    fb.rttMs = 42;
    fb.lossPct = 3;
    fb.recvBitrateKbps = 12'345;
    ApplyFeedback(st, fb, kT0, r.Hooks());

    Check(st.haveFeedback.load(), "now there is something to show");
    Check(st.uiRttMs.load() == 42, "RTT mirrored");
    Check(st.uiLossPct.load() == 3, "loss mirrored");
    Check(st.uiRecvKbps.load() == 12'345, "receive rate mirrored");
}

void TestEncoderRefusalLeavesTheBudgetAlone() {
    std::printf("[hostfb] if the encoder refuses a new bitrate, nothing is committed...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;
    r.encoderAccepts = false;

    uint64_t now = kT0;
    FeedbackOutcome out;
    for (int i = 0; i < 12 && r.bitratesOffered.empty(); ++i) {
        now += 1'000'000;
        out = ApplyFeedback(st, LossyLink(20), now, r.Hooks());
    }
    Check(!r.bitratesOffered.empty(), "sustained loss did ask the encoder to slow down");
    Check(!out.bitrateChanged, "but the refusal is reported as no change");
    Check(st.rate.bitrateBps() == kStartBps, "the controller keeps its old budget");
    Check(st.curBitrateBps.load() == kStartBps, "and so does the state the UI reads");
}

void TestAcceptedBitrateIsCommittedOnce() {
    std::printf("[hostfb] an accepted bitrate is committed to controller and state together...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;

    uint64_t now = kT0;
    FeedbackOutcome out;
    for (int i = 0; i < 12 && !out.bitrateChanged; ++i) {
        now += 1'000'000;
        out = ApplyFeedback(st, LossyLink(20), now, r.Hooks());
    }
    Check(out.bitrateChanged, "the drop went through");
    Check(out.bitrateBps < out.previousBitrateBps, "and it really was a reduction");
    Check(st.rate.bitrateBps() == out.bitrateBps, "the controller committed it");
    Check(st.curBitrateBps.load() == out.bitrateBps, "the state carries the same number");
    Check(out.previousBitrateBps == kStartBps, "the outcome reports what it was before");
}

void TestFecFollowsLoss() {
    std::printf("[hostfb] FEC is armed up front and reported only on the edge...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;
    Check(st.wantFec.load(), "armed to begin with");

    FeedbackOutcome out = ApplyFeedback(st, LossyLink(10), kT0 + 1'000'000, r.Hooks());
    Check(!out.fecToggled, "loss on an already-armed link is not an edge");
    Check(st.wantFec.load(), "and the state agrees");
}

void TestQualityStepIsAppliedThroughTheHook() {
    std::printf("[hostfb] a quality rung change goes out through the platform hook...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;
    r.sizeToReport = StreamSize{960, 540};
    st.ladder = std::make_unique<QualityLadder>(uint16_t(1920), uint16_t(1080), uint8_t(60));
    st.step = st.ladder->current();
    const QualityStep top = st.step;

    uint64_t now = kT0;
    FeedbackOutcome out;
    for (int i = 0; i < 40 && !out.qualityChanged; ++i) {
        now += 1'000'000;
        out = ApplyFeedback(st, LossyLink(30), now, r.Hooks());
    }
    Check(out.qualityChanged, "sustained loss eventually drops a rung");
    Check(r.qualityCalls == 1, "the hook ran exactly once for that change");
    Check(out.previousStep.scalePct == top.scalePct && out.previousStep.fps == top.fps,
        "the outcome reports the rung it came from");
    Check(st.step.scalePct != top.scalePct || st.step.fps != top.fps, "and the state moved");
    Check(st.curFps.load() == st.step.fps, "curFps follows the new rung");
    Check(out.size.width == 960 && out.size.height == 540,
        "the size the platform reported back is what the caller logs");
    Check(st.qualityChanged.load(), "the recv loop is told to re-offer");
}

void TestSenderBacklogWalksTheLadderDownOnACleanLink() {
    std::printf("[hostfb] a clean link that the sender cannot keep up with still steps down...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;
    st.ladder = std::make_unique<QualityLadder>(uint16_t(1920), uint16_t(1080), uint8_t(60));
    st.step = st.ladder->current();
    Check(st.step.fps == 60, "the ladder starts at the top rung");

    uint64_t now = kT0;
    int seconds = 0;
    for (; seconds < 30 && st.step.fps == 60; ++seconds) {
        now += 1'000'000;
        st.frameAgeMs.Add(4'000);
        ApplyFeedback(st, CleanLink(), now, r.Hooks());
    }

    Check(st.step.fps < 60, "a sender backlog alone drops the frame rate the encoder is asked for");
    Check(st.curFps.load() == st.step.fps, "curFps carries it to the capture side");
    Check(r.qualityCalls == 1, "and the platform hook ran so the encoder can be re-capped");
    Check(seconds <= 6, "and it gets there within a few seconds, not tens of them");
    Check(st.uiLossPct.load() == 0, "all of that happened with the viewer reporting no loss");
}

void TestNoLadderMeansNoQualityWork() {
    std::printf("[hostfb] before a client negotiates there is no ladder to walk...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Recorder r;

    uint64_t now = kT0;
    for (int i = 0; i < 20; ++i) {
        now += 1'000'000;
        const FeedbackOutcome out = ApplyFeedback(st, LossyLink(30), now, r.Hooks());
        Check(!out.qualityChanged, "no rung change is reported");
    }
    Check(r.qualityCalls == 0, "and the platform hook is never called");
}

void TestNackRepliesOnlyForKnownPackets() {
    std::printf("[hostfb] a NACK is answered from the cache, and only with a peer...\n");
    SourcePipelineState st(kStartBps, kMinBps);

    std::vector<std::vector<uint8_t>> sent;
    auto capture = [&sent](std::span<const uint8_t> d) { sent.emplace_back(d.begin(), d.end()); };

    Packetizer pk;
    pk.SetSessionId(0x1234);
    const TestFrame frame = MakeIdrFrame(7, 3);
    std::vector<Datagram> pkts = Packetize(pk, frame, kT0);
    for (const auto& d : pkts) st.retxCache.Store(d);

    const uint16_t wanted[] = {0, 1};
    RespondToNack(st, 7, wanted, capture);
    Check(sent.empty(), "with nobody connected nothing is retransmitted");

    GiveConnectedSession(st);
    RespondToNack(st, 7, wanted, capture);
    Check(sent.size() == 2, "both requested packets came back");

    sent.clear();
    const uint16_t missing[] = {900};
    RespondToNack(st, 7, missing, capture);
    Check(sent.empty(), "an index the cache never saw produces nothing");

    sent.clear();
    RespondToNack(st, 999, wanted, capture);
    Check(sent.empty(), "nor does an unknown frame");
}

void TestForgetViewersClearsRetransmitState() {
    std::printf("[hostfb] when the last viewer leaves, the retransmit cache goes with it...\n");
    SourcePipelineState st(kStartBps, kMinBps);
    Packetizer pk;
    pk.SetSessionId(0x1234);
    for (const auto& d : Packetize(pk, MakeIdrFrame(1, 2), kT0)) st.retxCache.Store(d);
    GiveConnectedSession(st);

    ForgetViewers(st);

    std::vector<std::vector<uint8_t>> sent;
    const uint16_t wanted[] = {0};
    RespondToNack(st, 1, wanted, [&sent](std::span<const uint8_t> d) {
        sent.emplace_back(d.begin(), d.end());
    });
    Check(sent.empty(), "and the cache no longer answers for the old session");
}

}

void RunViewerFeedbackTests() {
    TestFeedbackIsMirroredForTheUi();
    TestEncoderRefusalLeavesTheBudgetAlone();
    TestAcceptedBitrateIsCommittedOnce();
    TestFecFollowsLoss();
    TestQualityStepIsAppliedThroughTheHook();
    TestNoLadderMeansNoQualityWork();
    TestSenderBacklogWalksTheLadderDownOnACleanLink();
    TestNackRepliesOnlyForKnownPackets();
    TestForgetViewersClearsRetransmitState();
}

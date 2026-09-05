#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/control/StreamSize.h"
#include "deskhub/session/host/SourcePipelineState.h"
#include "deskhub/transport/Packetizer.h"
#include "deskhubp/host/HostNetLoop.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using deskhub::BuildHello;
using deskhub::Feedback;
using deskhub::Hello;
using deskhub::InputEvent;
using deskhub::kMaxDatagram;
using deskhub::Packetizer;
using deskhub::ScreenHostCallbacks;
using deskhub::ScreenHostSession;
using deskhub::SourcePipelineState;
using deskhub::StreamParams;
using deskhub::StreamSize;
using deskhubp::MakeScreenHostCallbacks;
using deskhubp::ScreenHostSessionHooks;

namespace {

constexpr uint32_t kStartBps = 20'000'000;
constexpr uint32_t kMinBps = 1'000'000;

struct Recorder {
    std::vector<std::vector<uint8_t>> sent;
    std::vector<std::vector<uint8_t>> sentToPeer;
    std::vector<InputEvent> inputs;
    int releases = 0;
    int retargets = 0;
    int bitrateCalls = 0;
    int qualityCalls = 0;
    StreamSize target{1280, 720};

    ScreenHostSessionHooks Hooks() {
        ScreenHostSessionHooks h;
        h.fps = 60;
        h.send = [this](std::span<const uint8_t> d) { sent.emplace_back(d.begin(), d.end()); };
        h.sendToRequester = [this](std::span<const uint8_t> d) {
            sentToPeer.emplace_back(d.begin(), d.end());
        };
        h.retarget = [this] {
            ++retargets;
            return target;
        };
        h.applyInput = [this](const InputEvent& e) { inputs.push_back(e); };
        h.releaseInput = [this] { ++releases; };
        h.setEncoderBitrate = [this](uint32_t) {
            ++bitrateCalls;
            return true;
        };
        h.applyQualityStep = [this](const deskhub::QualityStep&, const deskhub::QualityStep&) {
            ++qualityCalls;
            return target;
        };
        return h;
    }
};

std::unique_ptr<SourcePipelineState> MakeSource(const char* name) {
    auto st = std::make_unique<SourcePipelineState>(kStartBps, kMinBps);
    st->name = name;
    st->sourceId = 1;
    st->srcW.store(1920);
    st->srcH.store(1080);
    return st;
}

void GiveConnectedSession(SourcePipelineState& st) {
    ScreenHostCallbacks cb;
    cb.send = [](std::span<const uint8_t>) {};
    cb.randomBytes = [](std::span<uint8_t> out) {
        for (size_t i = 0; i < out.size(); ++i) out[i] = uint8_t(i + 1);
        return true;
    };
    st.session = std::make_unique<ScreenHostSession>(cb, StreamParams{1920, 1080, 60, kStartBps});
    st.session->SetPasscode(kTestPasscode);

    uint8_t buf[kMaxDatagram];
    Hello hello{};
    hello.clientId = 0x1234;
    hello.codecMask = deskhub::kCodecMaskH264;
    hello.maxWidth = 1920;
    hello.maxHeight = 1080;
    hello.passcode = kTestPasscode;
    const size_t n = BuildHello(buf, hello);
    st.session->HandlePacket(std::span<const uint8_t>(buf, n), 0, 0xC0A80001'0000ULL);
    Check(st.session->viewerCount() == 1, "the rig's viewer really is admitted");
}

Hello ClientHello(uint16_t w, uint16_t h) {
    Hello hello{};
    hello.clientId = 0x1234;
    hello.codecMask = deskhub::kCodecMaskH264;
    hello.maxWidth = w;
    hello.maxHeight = h;
    hello.desiredFps = 60;
    return hello;
}

void TestOutgoingBytesReachTheSocket() {
    std::printf("[hostcb] what the session wants to send is what the socket gets...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    const uint8_t bytes[] = {1, 2, 3, 4};
    cb.send(bytes);
    Check(rec.sent.size() == 1, "one send in, one send out");
    Check(rec.sent[0] == std::vector<uint8_t>{1, 2, 3, 4}, "with the bytes unchanged");
}

void TestAMissingHookIsNotACrash() {
    std::printf("[hostcb] a client that wires up no hooks still gets a usable session...\n");
    auto st = MakeSource("Display 1");
    const auto cb = MakeScreenHostCallbacks(*st, ScreenHostSessionHooks{});

    const uint8_t bytes[] = {9};
    cb.send(bytes);
    cb.onInput(InputEvent{});
    cb.onFocus(false);
    cb.onStart();
    cb.onKeyframeRequest(deskhub::KeyframeReason::Unknown);
    cb.onNack(1, std::span<const uint16_t>{});
    cb.onHello(ClientHello(1280, 720));
    cb.onFeedback(Feedback{});
    cb.onDisconnect();
    Check(true, "every callback tolerates the hook it depends on being absent");
}

void TestSessionIdsComeFromRealRandomness() {
    std::printf("[hostcb] the session id a host hands out is not guessable...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    uint8_t buf[16];
    for (uint8_t& b : buf) b = 0;
    Check(cb.randomBytes(buf), "the platform RNG answers");

    bool allZero = true;
    for (uint8_t b : buf) {
        if (b) allZero = false;
    }
    Check(!allZero, "and it actually wrote something, rather than leaving zeros");
}

void TestTheFirstHelloBuildsTheOffer() {
    std::printf("[hostcb] the first HELLO decides the size and fps the host will send...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    cb.onHello(ClientHello(1280, 720));

    Check(rec.retargets == 1, "the client size is resolved against the capture");
    Check(st->cliW == 1280 && st->cliH == 720, "and the client's screen is remembered");
    Check(st->ladder != nullptr, "a quality ladder is built for this client");
    Check(st->offer.width == 1280 && st->offer.height == 720,
        "the offer matches what the ladder was built from");
    Check(st->offer.fps != 0, "and it names an fps, so the client is not left guessing");
    Check(st->offer.bitrateBps == kStartBps, "the offer starts at the configured bitrate");
}

void TestAHelloWeCannotServeIsDeclined() {
    std::printf("[hostcb] a client we cannot size a stream for gets no offer at all...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    rec.target = StreamSize{0, 0};
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    cb.onHello(ClientHello(0, 0));
    Check(st->ladder == nullptr, "no ladder is built from a zero size");
    Check(st->offer.width == 0 && st->offer.height == 0,
        "and no offer is invented, so the client is rejected instead of sent garbage");
}

void TestStartAndKeyframeRequestsBothOweAnIdr() {
    std::printf("[hostcb] the first frame after START, or after a request, is a keyframe...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    Check(!st->forceIdr.load(), "nothing is owed before the client asks");
    cb.onStart();
    Check(st->forceIdr.load(), "START owes a keyframe, or the client decodes nothing");

    st->forceIdr.store(false);
    cb.onKeyframeRequest(deskhub::KeyframeReason::Loss);
    Check(st->forceIdr.load(), "an explicit request owes one too");

    char line[deskhub::diag::SourceDiag::kKeyframeReqBufBytes];
    const char* summary = st->diag.FormatKeyframeRequests(line, sizeof(line), "Display 1");
    Check(summary != nullptr && std::strstr(summary, "loss=1") != nullptr,
        "and the host records why it was asked, because a keyframe the viewer's decode queue "
        "wanted and one a lost packet forced score differently against any FEC scheme");
}

void TestInputIsHandedToTheInjector() {
    std::printf("[hostcb] a remote key press reaches the injector unchanged...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    InputEvent e{};
    e.type = deskhub::InputType::Key;
    e.a = 'A';
    e.b = 0x1E;
    e.state = 1;
    cb.onInput(e);

    Check(rec.inputs.size() == 1, "the event is delivered");
    Check(rec.inputs[0].type == deskhub::InputType::Key && rec.inputs[0].a == 'A' &&
              rec.inputs[0].b == 0x1E && rec.inputs[0].state == 1,
        "with its key, scancode and up/down state intact");
}

void TestLosingFocusLetsGoOfEveryKey() {
    std::printf("[hostcb] a viewer that alt-tabs away does not leave keys stuck down...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    cb.onFocus(true);
    Check(rec.releases == 0, "gaining focus releases nothing");

    cb.onFocus(false);
    Check(rec.releases == 1, "losing it releases everything the viewer was holding");
}

void TestADepartingClientIsForgottenCompletely() {
    std::printf("[hostcb] when the last viewer leaves, nothing of it is kept around...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    Packetizer pk;
    pk.SetSessionId(0x1234);
    uint8_t nal[64] = {};
    pk.SendFrame(nal, 7, 0, true, [&st](std::span<const uint8_t> d) { st->retxCache.Store(d); });
    Check(!st->retxCache.Find(7, 0).empty(), "the cache had something to forget");

    cb.onDisconnect();
    Check(st->retxCache.Find(7, 0).empty(),
        "the retransmit cache is dropped, so no stale frame can be resent into a new session");

    cb.onControllerChange(0);
    Check(rec.releases == 1, "and the keys the controlling viewer held are released on the host");
}

void TestANackForAnUnknownPeerSendsNothing() {
    std::printf("[hostcb] a NACK from nowhere does not make the host send anything...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());

    const uint16_t indices[] = {0, 1, 2};
    cb.onNack(7, indices);
    Check(rec.sentToPeer.empty(), "with no viewer connected there is nobody to retransmit to");

    GiveConnectedSession(*st);
    cb.onNack(7, indices);
    Check(rec.sentToPeer.empty(), "and an empty retransmit cache has nothing to resend");
}

void TestFeedbackDrivesTheEncoder() {
    std::printf("[hostcb] the link report the client sends actually moves the encoder...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());
    cb.onHello(ClientHello(1280, 720));

    Feedback fb{};
    fb.rttMs = 40;
    fb.lossPct = 20;
    fb.recvBitrateKbps = 3'000;
    cb.onFeedback(fb);

    Check(st->rate->bitrateBps() < kStartBps,
        "a lossy link pulls the bitrate down instead of insisting on the original");
    Check(rec.bitrateCalls >= 1, "and the encoder is told about it");
    Check(st->wantFec.load(), "20% loss keeps FEC on");
}

void TestAHealthyLinkIsLeftAlone() {
    std::printf("[hostcb] a clean link is not disturbed by the first report...\n");
    auto st = MakeSource("Display 1");
    Recorder rec;
    const auto cb = MakeScreenHostCallbacks(*st, rec.Hooks());
    cb.onHello(ClientHello(1280, 720));

    Feedback fb{};
    fb.rttMs = 5;
    fb.lossPct = 0;
    fb.recvBitrateKbps = 19'000;
    cb.onFeedback(fb);

    Check(st->rate->bitrateBps() == kStartBps, "no loss, no reason to change the bitrate");
    Check(st->wantFec.load(), "and one clean second is not yet proof FEC can be dropped");
    Check(st->uiRttMs.load() == 5, "the UI still learns the measured round trip");
    Check(st->haveFeedback.load(), "and knows the numbers are real, not placeholders");
}

void TestEachSourceKeepsItsOwnState() {
    std::printf("[hostcb] two shared displays never write into each other's state...\n");
    auto a = MakeSource("Display 1");
    auto b = MakeSource("Display 2");
    Recorder recA, recB;
    const auto cbA = MakeScreenHostCallbacks(*a, recA.Hooks());
    const auto cbB = MakeScreenHostCallbacks(*b, recB.Hooks());

    cbA.onStart();
    Check(a->forceIdr.load(), "the source that was started owes a keyframe");
    Check(!b->forceIdr.load(), "the other one is untouched");

    cbB.onHello(ClientHello(800, 600));
    Check(b->cliW == 800 && a->cliW == 0, "and a HELLO only reaches the source it was for");
    Check(recA.retargets == 0 && recB.retargets == 1, "the hooks stay with their own source");
}

}

void RunScreenHostCallbackTests() {
    TestOutgoingBytesReachTheSocket();
    TestAMissingHookIsNotACrash();
    TestSessionIdsComeFromRealRandomness();
    TestTheFirstHelloBuildsTheOffer();
    TestAHelloWeCannotServeIsDeclined();
    TestStartAndKeyframeRequestsBothOweAnIdr();
    TestInputIsHandedToTheInjector();
    TestLosingFocusLetsGoOfEveryKey();
    TestADepartingClientIsForgottenCompletely();
    TestANackForAnUnknownPeerSendsNothing();
    TestFeedbackDrivesTheEncoder();
    TestAHealthyLinkIsLeftAlone();
    TestEachSourceKeepsItsOwnState();
}

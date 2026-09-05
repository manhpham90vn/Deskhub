#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/client/ScreenClient.h"
#include "deskhub/session/host/ScreenHostSession.h"
#include "deskhub/transport/Packetizer.h"

#include <cstdio>
#include <deque>
#include <optional>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

ScreenClientConfig TestPumpCfg(uint32_t clientId, uint16_t maxWidth, uint16_t maxHeight,
    uint8_t sourceId, uint8_t desiredFps) {
    ScreenClientConfig cfg{clientId, maxWidth, maxHeight, sourceId, desiredFps};
    cfg.passcode = kTestPasscode;
    return cfg;
}

struct Rig {
    std::deque<Datagram> toHost, toClient;

    diag::ScreenClientDiag diag;
    std::vector<Reassembler::Frame> frames;
    std::vector<std::pair<uint32_t, Datagram>> audio;
    std::vector<std::string> logs;
    std::string status;
    std::string ended;
    NegotiatedParams params{};
    int paramsCalls = 0;
    int reconfigCalls = 0;
    uint32_t renderedToReport = 0;
    int64_t latency = -1;

    ScreenClientCallbacks Callbacks() {
        ScreenClientCallbacks cb;
        cb.send = [this](std::span<const uint8_t> d) { toHost.emplace_back(d.begin(), d.end()); };
        cb.onFrame = [this](Reassembler::Frame&& f) { frames.push_back(std::move(f)); };
        cb.onAudioPacket = [this](const AudioPacketView& v) {
            audio.emplace_back(v.hdr.seq, Datagram(v.payload.begin(), v.payload.end()));
        };
        cb.onParams = [this](const NegotiatedParams& p, bool reconfigured) {
            params = p;
            ++paramsCalls;
            if (reconfigured) ++reconfigCalls;
        };
        cb.onEnded = [this](const char* reason, ScreenSessionEnd) { ended = reason ? reason : ""; };
        cb.takeRenderedCount = [this] {
            const uint32_t n = renderedToReport;
            renderedToReport = 0;
            return n;
        };
        cb.latencyUs = [this] { return latency; };
        cb.onStatus = [this](const char* s) { status = s ? s : ""; };
        cb.localTime = [] { return std::string("12:34:56"); };
        cb.log = [this](bool, const char* line) { logs.emplace_back(line ? line : ""); };
        return cb;
    }

    bool LoggedContaining(const char* needle) const {
        for (const auto& l : logs)
            if (l.find(needle) != std::string::npos) return true;
        return false;
    }
};

size_t CountToHost(const std::deque<Datagram>& q, MsgType type) {
    size_t n = 0;
    for (const auto& d : q) {
        const auto h = ParseCommonHeader(d);
        if (h && h->type == type) ++n;
    }
    return n;
}

struct Connected {
    Rig rig;
    ScreenClient pump;
    ScreenHostSession host;
    uint64_t now = 10'000'000;

    Connected(ScreenHostCallbacks hcb, StreamParams offer)
        : pump(rig.Callbacks(), rig.diag), host(hcb, offer) {}
};

void Exchange(Rig& r, ScreenClient& pump, ScreenHostSession& host, uint64_t now) {
    for (int guard = 0; guard < 8; ++guard) {
        if (r.toHost.empty() && r.toClient.empty()) break;
        while (!r.toHost.empty()) {
            auto d = std::move(r.toHost.front());
            r.toHost.pop_front();
            host.HandlePacket(d, now, kTestViewer);
        }
        while (!r.toClient.empty()) {
            auto d = std::move(r.toClient.front());
            r.toClient.pop_front();
            pump.OnDatagram(d, now);
        }
    }
}

void TestHandshakeAndParams() {
    std::printf("[pump] Start() negotiates and reports the agreed parameters...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    ScreenClientConfig cfg = TestPumpCfg(0x11223344, 2560, 1440, 0, 60);
    cfg.displayName = "Anh's laptop";
    pump.Start(cfg, now);
    Check(CountToHost(r.toHost, MsgType::Hello) == 1, "Start() puts a HELLO on the wire");
    const auto sentHello = ParseHello(PayloadOf(r.toHost.front()));
    Check(sentHello && sentHello->clientName == "Anh's laptop",
        "and the HELLO carries the configured display name");

    Exchange(r, pump, host, now);
    Check(r.paramsCalls == 1 && r.reconfigCalls == 0, "onParams fires once, not as a reconfig");
    Check(r.params.width == 1920 && r.params.height == 1080 && r.params.fps == 60,
        "the negotiated size and rate are handed to the client");
    Check(host.state() == ScreenHostSession::State::Streaming, "the host went streaming");
    Check(r.LoggedContaining("Negotiation done"), "and it was logged");
}

void TestVideoReachesTheFrameSink() {
    std::printf("[pump] reassembled frames are handed to the decoder, IDRs cancel requests...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    const TestFrame idr = MakeIdrFrame(0, 4);
    for (const auto& d : Packetize(pk, idr, now)) pump.OnDatagram(d, now);

    now += 20'000;
    pump.PollFrames(now);
    Check(r.frames.size() == 1, "the frame came out whole");
    Check(SameFrame(r.frames[0], idr), "and byte-identical to what the host sent");
    Check(r.frames[0].idr, "marked as an IDR");
    Check(pump.streaming(), "video traffic moves the session to streaming");
}

void TestKeyframeRequestsAreLoggedOnce() {
    std::printf("[pump] a keyframe request is logged on the edge, not on every repeat...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    std::optional<deskhub::KeyframeReason> hostSawReason;
    hcb.onKeyframeRequest = [&](deskhub::KeyframeReason reason) { hostSawReason = reason; };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) pump.OnDatagram(d, now);
    Check(pump.streaming(), "video traffic is what makes a keyframe request sendable");
    r.logs.clear();

    pump.RequestKeyframe(diag::KeyframeReason::DecFail, now);
    const size_t afterFirst = r.logs.size();
    Check(afterFirst == 1, "the first request is logged");
    Check(r.LoggedContaining("dec_fail"), "with its reason");

    pump.RequestKeyframe(diag::KeyframeReason::QOverflow, now + 1000);
    Check(r.logs.size() == afterFirst, "a second request while one is pending stays quiet");

    r.toHost.clear();
    now += 300'000;
    pump.Tick(now);
    Exchange(r, pump, host, now);
    Check(hostSawReason.has_value(), "the host did receive the request");
    Check(hostSawReason == deskhub::KeyframeReason::DecFail,
        "and it arrives labelled with what armed it, not with the reason that came second "
        "while the first was still pending - the host counts these to tell a keyframe the "
        "link cost from one the viewer's own decode queue asked for");
}

void TestReportRunsOncePerWindow() {
    std::printf("[pump] the status line and FEEDBACK go out once a second...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    Feedback got{};
    int feedbacks = 0;
    hcb.onFeedback = [&](const Feedback& f) {
        got = f;
        ++feedbacks;
    };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 4), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);
    r.renderedToReport = 60;
    r.latency = 25'000;

    Check(r.status.empty(), "no status before the first window closes");
    now += 400'000;
    Check(pump.Tick(now), "mid-window tick keeps the session alive");
    Check(r.status.empty(), "and reports nothing");

    r.toHost.clear();
    now += 700'000;
    Check(pump.Tick(now), "the window closes");
    Check(!r.status.empty(), "a compact status line reached the UI");
    Check(CountToHost(r.toHost, MsgType::Feedback) == 1, "exactly one FEEDBACK went out");

    Exchange(r, pump, host, now);
    Check(feedbacks == 1 && got.recvBitrateKbps > 0,
        "the host got it, carrying the bytes actually received");

    Check(r.status.find("  ") != std::string::npos,
        "the default separator is two spaces");
}

void TestStatusSeparatorIsConfigurable() {
    std::printf("[pump] the UI can ask for its own separator in the status line...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    ScreenClientConfig cfg{1, 1920, 1080, 0, 60};
    cfg.passcode = kTestPasscode;
    cfg.statusSeparator = " | ";
    pump.Start(cfg, now);
    Exchange(r, pump, host, now);

    now += 1'100'000;
    Check(pump.Tick(now), "the window closes");
    Check(r.status.find(" | ") != std::string::npos, "the chosen separator is used");
    Check(r.status.find("fps") != std::string::npos, "and the fields are still there");
}

void TestDisconnectEndsTheLoop() {
    std::printf("[pump] a host BYE ends the pump and names the reason...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);
    Check(r.ended.empty(), "still connected");

    uint8_t bye[kCommonHeaderSize];
    const size_t n = BuildBye(bye, host.sessionId());
    Check(n > 0, "built a BYE");
    pump.OnDatagram(std::span<const uint8_t>(bye, n), now);

    Check(!r.ended.empty(), "onEnded reported a reason");
    Check(!pump.Tick(now), "and Tick() tells the caller to stop looping");
}

void TestLoopBusyWarning() {
    std::printf("[pump] a slow loop iteration is counted and warned about...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), 10'000'000);
    r.logs.clear();

    pump.CountLoopBusy(10'000'000, 10'001'000);
    Check(r.logs.empty(), "a 1 ms iteration is unremarkable");

    pump.CountLoopBusy(10'000'000, 10'000'000 + (kLoopStallWarnMs + 10) * 1000);
    Check(r.LoggedContaining("recv_stall"), "a long one is reported");
}

void TestStrayTrafficIsIgnored() {
    std::printf("[pump] video for another session is dropped, not reassembled...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);

    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer stray;
    stray.SetSessionId(host.sessionId() ^ 0xFFFF);
    for (const auto& d : Packetize(stray, MakeIdrFrame(0, 2), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now + 20'000);
    Check(r.frames.empty(), "nothing was handed to the decoder");

    const uint8_t garbage[3] = {1, 2, 3};
    pump.OnDatagram(garbage, now);
    Check(r.frames.empty(), "a runt datagram is harmless too");
}

ScreenHostCallbacks HostSendTo(Rig& r) {
    ScreenHostCallbacks hcb;
    hcb.send = [&r](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    return hcb;
}

void TestReconfigIsAppliedMidStream() {
    std::printf("[pump] a host RECONFIG updates the params and the reassembler...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostSession host(HostSendTo(r), StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    uint8_t msg[64];
    Reconfig early{1280, 720, 10'000'000, 30};
    size_t n = BuildReconfig(msg, host.sessionId(), early);
    pump.OnDatagram(std::span<const uint8_t>(msg, n), now);
    Check(r.reconfigCalls == 1, "a reconfig before any video still reaches the client");
    Check(r.params.width == 1280 && r.params.fps == 30, "and carries the new mode");

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 4), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);

    Reconfig later{1920, 1080, 20'000'000, 60};
    n = BuildReconfig(msg, host.sessionId(), later);
    pump.OnDatagram(std::span<const uint8_t>(msg, n), now);
    Check(r.reconfigCalls == 2, "a mid-stream reconfig fires onParams again");
    Check(r.params.width == 1920 && r.params.fps == 60, "with the restored mode");
    Check(r.LoggedContaining("Host reconfigured"), "and it was logged");
}

void TestOfferWithoutFpsFallsBackToDefault() {
    std::printf("[pump] a host that offers no fps still gets a working reassembler...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostSession host(HostSendTo(r), StreamParams{1920, 1080, 0, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    const TestFrame idr = MakeIdrFrame(0, 3);
    for (const auto& d : Packetize(pk, idr, now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);
    Check(r.frames.size() == 1 && SameFrame(r.frames[0], idr),
        "frames still reassemble at the default fps");

    uint8_t msg[64];
    const size_t n = BuildReconfig(msg, host.sessionId(), Reconfig{0, 0, 0, 0});
    pump.OnDatagram(std::span<const uint8_t>(msg, n), now);
    Check(r.reconfigCalls == 1, "an all-zero reconfig is delivered and changes nothing");
}

void TestFecRecoversALostPacket() {
    std::printf("[pump] a parity packet fills the hole a lost datagram left...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostSession host(HostSendTo(r), StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    pk.SetFecEnabled(true);
    Check(pk.fecEnabled(), "FEC can be switched on");
    Check(pk.sessionId() == host.sessionId(), "the packetizer carries the session id");

    const TestFrame idr = MakeIdrFrame(0, 4);
    const auto pkts = Packetize(pk, idr, now);
    bool sawFec = false;
    for (const auto& d : pkts) sawFec = sawFec || IsFec(d);
    Check(sawFec, "an FEC-enabled packetizer emits parity");

    const size_t lost = NthDataPacket(pkts, 1);
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i != lost) pump.OnDatagram(pkts[i], now);

    pump.PollFrames(now);
    Check(r.frames.size() == 1, "the frame completed without the lost datagram");
    Check(SameFrame(r.frames[0], idr), "and is byte-identical after recovery");
}

void TestNackGoesOutForAHole() {
    std::printf("[pump] with NACKs on, a hole is reported back to the host...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostCallbacks hcb = HostSendTo(r);
    uint32_t nackFrame = 0;
    std::vector<uint16_t> nackIdx;
    hcb.onNack = [&](uint32_t frameId, std::span<const uint16_t> idx) {
        nackFrame = frameId;
        nackIdx.assign(idx.begin(), idx.end());
    };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.PlanNacks(now);
    Check(r.toHost.empty(), "NACKs stay off unless the config asks for them");

    ScreenClientConfig cfg{1, 1920, 1080, 0, 60};
    cfg.passcode = kTestPasscode;
    cfg.sendNacks = true;
    pump.Start(cfg, now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);

    const auto pkts = Packetize(pk, MakeIdrFrame(1, 4), now);
    const size_t lost = NthDataPacket(pkts, 2);
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i != lost) pump.OnDatagram(pkts[i], now);

    r.toHost.clear();
    pump.PlanNacks(now + 5'000);
    Check(CountToHost(r.toHost, MsgType::Nack) == 1, "with NACKs on, the hole goes out");

    Exchange(r, pump, host, now + 5'000);
    Check(nackFrame == 1, "the host learned which frame");
    Check(nackIdx.size() == 1 && nackIdx[0] == 2, "and exactly which packet is missing");
}

void TestLossAsksForAKeyframe() {
    std::printf("[pump] a dropped frame is logged and answered with a keyframe request...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostSession host(HostSendTo(r), StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);
    r.logs.clear();

    const auto pkts = Packetize(pk, MakeIdrFrame(1, 4), now);
    const size_t lost = NthDataPacket(pkts, 1);
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i != lost) pump.OnDatagram(pkts[i], now);

    now += 50'000;
    pump.PollFrames(now);
    Check(r.LoggedContaining("evt=frame_drop"), "the drop itself is a diagnostic line");
    Check(r.LoggedContaining("reason=loss"), "and the keyframe request names loss");

    pump.PollFrames(now + 1'000);
    Check(r.frames.size() == 1, "still only the first frame came out");

    const TestFrame recovery = MakeIdrFrame(2, 2);
    for (const auto& d : Packetize(pk, recovery, now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);
    Check(r.frames.size() == 2 && r.frames[1].idr, "the next IDR unblocks the stream");
    Check(r.LoggedContaining("evt=idr_rx"), "and its arrival closes the request log");
}

void TestLossRunsLineIsPrinted() {
    std::printf("[pump] with logLossRuns on, the run histogram is printed at the window...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostSession host(HostSendTo(r), StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    ScreenClientConfig cfg{1, 1920, 1080, 0, 60};
    cfg.passcode = kTestPasscode;
    cfg.logLossRuns = true;
    pump.Start(cfg, now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);

    const auto pkts = Packetize(pk, MakeIdrFrame(1, 4), now);
    const size_t lost = NthDataPacket(pkts, 1);
    for (size_t i = 0; i < pkts.size(); ++i)
        if (i != lost) pump.OnDatagram(pkts[i], now);
    pump.PollFrames(now + 50'000);

    now += 1'100'000;
    pump.Tick(now);
    Check(r.LoggedContaining("loss runs:"), "the histogram line is printed");
    Check(r.LoggedContaining("longest ever"), "with the worst run ever seen");
}

void TestFocusInputByeAndRtt() {
    std::printf("[pump] focus, input, RTT and BYE all pass through to the session...\n");
    Rig r;
    ScreenClient pump(r.Callbacks(), r.diag);
    ScreenHostCallbacks hcb = HostSendTo(r);
    bool focused = false;
    int inputs = 0;
    bool hostEnded = false;
    hcb.onFocus = [&](bool on) { focused = on; };
    hcb.onInput = [&](const InputEvent&) { ++inputs; };
    hcb.onDisconnect = [&] { hostEnded = true; };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    uint64_t now = 10'000'000;
    pump.Start(TestPumpCfg(1, 1920, 1080, 0, 60), now);
    Exchange(r, pump, host, now);

    Packetizer pk;
    pk.SetSessionId(host.sessionId());
    for (const auto& d : Packetize(pk, MakeIdrFrame(0, 2), now)) pump.OnDatagram(d, now);
    pump.PollFrames(now);
    Check(pump.streaming(), "streaming after the first frame");

    pump.SetFocused(true);
    InputEvent e;
    e.type = InputType::Key;
    e.a = 65;
    e.state = 1;
    pump.QueueInput(e);
    now += 60'000;
    pump.Tick(now);
    Exchange(r, pump, host, now);
    Check(focused, "the focus change reached the host");
    Check(inputs == 1, "so did the key press");

    Check(pump.lastRttUs() == 0, "no RTT sample yet");
    now += 1'100'000;
    pump.Tick(now);
    while (!r.toHost.empty()) {
        auto d = std::move(r.toHost.front());
        r.toHost.pop_front();
        host.HandlePacket(d, now, kTestViewer);
    }
    const uint64_t pongAt = now + 30'000;
    while (!r.toClient.empty()) {
        auto d = std::move(r.toClient.front());
        r.toClient.pop_front();
        pump.OnDatagram(d, pongAt);
    }
    Check(pump.lastRttUs() == 30'000, "the pong round trip is measured");

    pump.SendBye();
    Exchange(r, pump, host, pongAt);
    Check(hostEnded, "the BYE told the host we left");
}

}

void TestAudioOnlyFlowsWhenAskedFor() {
    std::printf("[pump] audio reaches the speaker only for a viewer that asked for it...\n");

    auto run = [](bool wantsAudio) {
        struct Result {
            size_t packets = 0;
            uint32_t seq = 0;
            uint8_t firstByte = 0;
            bool helloAskedForAudio = false;
            size_t audioViewers = 0;
        } out;

        Rig r;
        ScreenClient pump(r.Callbacks(), r.diag);

        ScreenHostCallbacks hcb;
        hcb.send = [&](std::span<const uint8_t> d) { r.toClient.emplace_back(d.begin(), d.end()); };
        hcb.randomBytes = TestRandomBytes;
        ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
        host.SetPasscode(kTestPasscode);

        const uint64_t now = 10'000'000;
        ScreenClientConfig cfg = TestPumpCfg(2, 1920, 1080, 0, 60);
        cfg.wantsAudio = wantsAudio;
        pump.Start(cfg, now);

        const auto hello = ParseHello(PayloadOf(r.toHost.front()));
        out.helloAskedForAudio = hello && (hello->features & kClientWantsAudio) != 0;
        Exchange(r, pump, host, now);

        uint64_t addrs[kMaxViewersPerHost];
        out.audioViewers = host.SnapshotAudioViewerAddrs(addrs);

        const std::vector<uint8_t> opus = {0xFC, 0x11, 0x22, 0x33};
        uint8_t buf[kMaxDatagram];
        const AudioHeader ah{42, 1'234'000};
        const size_t n = BuildAudioPacket(buf, host.sessionId(), ah, opus);
        pump.OnDatagram(std::span<const uint8_t>(buf, n), now);

        out.packets = r.audio.size();
        if (out.packets) {
            out.seq = r.audio.front().first;
            out.firstByte = r.audio.front().second.front();
        }
        return out;
    };

    const auto asked = run(true);
    Check(asked.helloAskedForAudio, "the HELLO says the viewer wants sound");
    Check(asked.audioViewers == 1, "the host counts it among the viewers to send audio to");
    Check(asked.packets == 1 && asked.seq == 42 && asked.firstByte == 0xFC,
        "the Opus frame arrives untouched, sequence and all");

    const auto silent = run(false);
    Check(!silent.helloAskedForAudio, "a viewer that wants no sound says nothing");
    Check(silent.audioViewers == 0, "the host has nobody to send audio to");
    Check(silent.packets == 0, "and an audio packet that arrives anyway is ignored");
}

void RunScreenClientTests() {
    TestHandshakeAndParams();
    TestVideoReachesTheFrameSink();
    TestKeyframeRequestsAreLoggedOnce();
    TestReportRunsOncePerWindow();
    TestStatusSeparatorIsConfigurable();
    TestDisconnectEndsTheLoop();
    TestLoopBusyWarning();
    TestStrayTrafficIsIgnored();
    TestReconfigIsAppliedMidStream();
    TestOfferWithoutFpsFallsBackToDefault();
    TestFecRecoversALostPacket();
    TestNackGoesOutForAHole();
    TestLossAsksForAKeyframe();
    TestLossRunsLineIsPrinted();
    TestFocusInputByeAndRtt();
    TestAudioOnlyFlowsWhenAskedFor();
}

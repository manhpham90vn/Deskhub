#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/session/client/ScreenClientSession.h"
#include "deskhub/session/host/ScreenHostSession.h"

#include <cstdio>
#include <deque>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

struct WirePair {
    std::deque<Datagram> toHost, toClient;
};

void TestSessions() {
    std::printf("[session] handshake ScreenHostSession <-> ScreenClientSession...\n");
    WirePair w;
    uint64_t now = 10'000'000;

    bool hostStarted = false, hostKeyframeReq = false, hostDisconnected = false;
    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { w.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    hcb.onStart = [&] { hostStarted = true; };
    hcb.onKeyframeRequest = [&](KeyframeReason) { hostKeyframeReq = true; };
    hcb.onDisconnect = [&] { hostDisconnected = true; };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    bool cliReady = false;
    uint32_t cliRtt = 0;
    std::string cliDead;
    NegotiatedParams np{};
    ScreenClientSessionCallbacks ccb;
    ccb.send = [&](std::span<const uint8_t> d) { w.toHost.emplace_back(d.begin(), d.end()); };
    ccb.onReady = [&](const NegotiatedParams& p) { cliReady = true; np = p; };
    ccb.onRtt = [&](uint32_t r) { cliRtt = r; };
    ccb.onDisconnect = [&](const char* r, ScreenSessionEnd) { cliDead = r; };
    ScreenClientSession cli(ccb);

    auto pump = [&] {
        for (int guard = 0; guard < 8; ++guard) {
            if (w.toHost.empty() && w.toClient.empty()) break;
            while (!w.toHost.empty()) {
                auto d = std::move(w.toHost.front());
                w.toHost.pop_front();
                host.HandlePacket(d, now, kTestViewer);
            }
            while (!w.toClient.empty()) {
                auto d = std::move(w.toClient.front());
                w.toClient.pop_front();
                cli.HandlePacket(d, now);
            }
        }
    };

    cli.Start(Hello{0x11223344, kCodecMaskH264, 2560, 1440, 60, 0, 0, kTestPasscode}, now);
    w.toHost.clear();
    now += 600'000;
    cli.Tick(now);
    pump();
    Check(cliReady, "onReady after HELLO_ACK (via retry)");
    Check(np.width == 1920 && np.height == 1080 && np.fps == 60, "negotiated parameters");
    Check(host.state() == ScreenHostSession::State::Streaming, "host STREAMING after START");
    Check(hostStarted, "onStart was called (force IDR to open)");
    Check(cli.sessionId() == host.sessionId() && cli.sessionId() != 0, "sessionId matches");

    cli.NotifyVideoPacket(now);
    Check(cli.state() == ScreenClientSession::State::Streaming, "client STREAMING when video present");

    now += 1'100'000;
    cli.Tick(now);
    host.Tick(now);
    const uint64_t pingSent = now;
    now += 30'000;
    pump();
    Check(cliRtt == uint32_t(now - pingSent), "RTT = simulated round-trip delay");

    cli.RequestKeyframe(KeyframeReason::Loss);
    now += 260'000;
    cli.Tick(now);
    pump();
    Check(hostKeyframeReq, "REQUEST_KEYFRAME reaches host");

    {
        constexpr uint64_t kSecondViewer = kTestViewer + 0x1'0000ULL;
        WirePair w2;
        std::string otherDead;
        NegotiatedParams otherParams{};
        ScreenClientSessionCallbacks c2 = ccb;
        c2.send = [&](std::span<const uint8_t> d) { w2.toHost.emplace_back(d.begin(), d.end()); };
        c2.onReady = [&](const NegotiatedParams& p) { otherParams = p; };
        c2.onDisconnect = [&](const char* r, ScreenSessionEnd) { otherDead = r; };
        ScreenClientSession other(c2);
        other.Start(Hello{0x55667788, kCodecMaskH264, 1280, 720, 30, 0, 0, kTestPasscode}, now);
        while (!w2.toHost.empty()) {
            auto d = std::move(w2.toHost.front());
            w2.toHost.pop_front();
            const size_t before = w.toClient.size();
            host.HandlePacket(d, now, kSecondViewer);
            while (w.toClient.size() > before) {
                other.HandlePacket(w.toClient.back(), now);
                w.toClient.pop_back();
            }
        }
        Check(otherDead.empty(), "a second viewer is welcome, not refused");
        Check(host.viewerCount() == 2, "the host counts both");
        Check(other.sessionId() == host.sessionId(), "and both ride the same session");
        Check(otherParams.width == np.width && otherParams.height == np.height,
            "the newcomer gets the stream the first viewer negotiated");
        Check(host.state() == ScreenHostSession::State::Streaming, "existing session unaffected");

        uint8_t bye[kMaxDatagram];
        const size_t byeLen = BuildBye(bye, host.sessionId());
        host.HandlePacket(std::span<const uint8_t>(bye, byeLen), now, kSecondViewer);
        Check(host.viewerCount() == 1 && host.state() == ScreenHostSession::State::Streaming,
            "and when it leaves the first viewer keeps streaming");
        w.toClient.clear();
    }

    cli.SendBye();
    pump();
    Check(hostDisconnected && host.state() == ScreenHostSession::State::Idle, "BYE -> host IDLE");

    hostDisconnected = false;
    ScreenClientSession cli2(ccb);
    cliDead.clear();
    cli2.Start(Hello{0x99AA0001, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode}, now);
    while (!w.toHost.empty()) {
        host.HandlePacket(w.toHost.front(), now, kTestViewer);
        w.toHost.pop_front();
    }
    w.toClient.clear();
    Check(host.state() != ScreenHostSession::State::Idle, "second session established");
    now += 11'000'000;
    host.Tick(now);
    Check(hostDisconnected && host.state() == ScreenHostSession::State::Idle, "timeout -> host IDLE");
    cli2.Tick(now);
    Check(!cliDead.empty(), "client gives up when host goes silent");
}

void TestHostKicksOneViewer() {
    std::printf("[session] the host can kick a single viewer by address...\n");
    WirePair w;
    const uint64_t now = 10'000'000;

    std::vector<std::pair<uint64_t, Datagram>> targeted;
    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { w.toClient.emplace_back(d.begin(), d.end()); };
    hcb.sendTo = [&](uint64_t addr, std::span<const uint8_t> d) {
        targeted.emplace_back(addr, Datagram(d.begin(), d.end()));
    };
    hcb.randomBytes = TestRandomBytes;
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    std::string cliDead;
    ScreenClientSessionCallbacks ccb;
    ccb.send = [&](std::span<const uint8_t> d) { w.toHost.emplace_back(d.begin(), d.end()); };
    ccb.onDisconnect = [&](const char* r, ScreenSessionEnd) { cliDead = r; };
    ScreenClientSession cli(ccb);

    auto pump = [&] {
        for (int guard = 0; guard < 8; ++guard) {
            if (w.toHost.empty() && w.toClient.empty()) break;
            while (!w.toHost.empty()) {
                auto d = std::move(w.toHost.front());
                w.toHost.pop_front();
                host.HandlePacket(d, now, kTestViewer);
            }
            while (!w.toClient.empty()) {
                auto d = std::move(w.toClient.front());
                w.toClient.pop_front();
                cli.HandlePacket(d, now);
            }
        }
    };

    cli.Start(Hello{0x1, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode}, now);
    pump();
    Check(host.viewerCount() == 1, "one viewer is connected");

    Check(!host.KickViewer(kTestViewer + 1), "an unknown address cannot be kicked");
    Check(host.viewerCount() == 1, "and the real viewer is untouched by that");

    Check(host.KickViewer(kTestViewer), "the connected viewer can be kicked");
    Check(host.viewerCount() == 0, "it is gone from the viewer table");
    Check(targeted.size() == 1 && targeted[0].first == kTestViewer,
        "a farewell went to exactly that address");
    const auto header = ParseCommonHeader(targeted[0].second);
    Check(header.has_value() && header->type == MsgType::Bye, "and it really is a BYE");

    cli.HandlePacket(targeted[0].second, now);
    Check(!cliDead.empty(), "the kicked viewer shuts its session down");
}

void TestSessionsNackInvalidate() {
    std::printf("[session] NACK / INVALIDATE_REF routing + pre-stream gating...\n");
    WirePair w;
    uint64_t now = 10'000'000;

    uint32_t nackFrame = 0, invFrame = 0;
    std::vector<uint16_t> nackIdx;
    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { w.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    hcb.onNack = [&](uint32_t fid, std::span<const uint16_t> idx) {
        nackFrame = fid;
        nackIdx.assign(idx.begin(), idx.end());
    };
    hcb.onInvalidateRef = [&](uint32_t fid) { invFrame = fid; };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    ScreenClientSessionCallbacks ccb;
    ccb.send = [&](std::span<const uint8_t> d) { w.toHost.emplace_back(d.begin(), d.end()); };
    ScreenClientSession cli(ccb);

    auto pump = [&] {
        for (int guard = 0; guard < 8; ++guard) {
            if (w.toHost.empty() && w.toClient.empty()) break;
            while (!w.toHost.empty()) {
                auto d = std::move(w.toHost.front());
                w.toHost.pop_front();
                host.HandlePacket(d, now, kTestViewer);
            }
            while (!w.toClient.empty()) {
                auto d = std::move(w.toClient.front());
                w.toClient.pop_front();
                cli.HandlePacket(d, now);
            }
        }
    };

    cli.Start(Hello{0x1, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode}, now);
    pump();
    cli.NotifyVideoPacket(now);
    Check(host.state() == ScreenHostSession::State::Streaming &&
              cli.state() == ScreenClientSession::State::Streaming,
        "both sides streaming");

    const uint16_t idx[] = {1, 4, 7};
    cli.SendNack(0xABCD, idx);
    cli.SendInvalidateRef(0x1234);
    pump();
    Check(nackFrame == 0xABCD && nackIdx.size() == 3 && nackIdx[0] == 1 && nackIdx[2] == 7,
        "host routed NACK to onNack with the right indices");
    Check(invFrame == 0x1234, "host routed INVALIDATE_REF to onInvalidateRef");

    ScreenClientSession idle(ccb);
    const size_t before = w.toHost.size();
    idle.SendNack(1, idx);
    idle.SendInvalidateRef(1);
    Check(w.toHost.size() == before, "SendNack/SendInvalidateRef ignored before STREAMING");
}

void TestReconfigFocusFeedback() {
    std::printf("[session] RECONFIG / SET_FOCUS / FEEDBACK routing...\n");
    WirePair w;
    uint64_t now = 10'000'000;

    bool focus = false, gotFocusFalse = false;
    Feedback lastFb{};
    bool gotFb = false;
    ScreenHostCallbacks hcb;
    hcb.send = [&](std::span<const uint8_t> d) { w.toClient.emplace_back(d.begin(), d.end()); };
    hcb.randomBytes = TestRandomBytes;
    hcb.onFocus = [&](bool on) { focus = on; if (!on) gotFocusFalse = true; };
    hcb.onFeedback = [&](const Feedback& fb) { lastFb = fb; gotFb = true; };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    bool reconfigured = false;
    NegotiatedParams rp{};
    ScreenClientSessionCallbacks ccb;
    ccb.send = [&](std::span<const uint8_t> d) { w.toHost.emplace_back(d.begin(), d.end()); };
    ccb.onReconfig = [&](const NegotiatedParams& p) { reconfigured = true; rp = p; };
    ScreenClientSession cli(ccb);

    auto pump = [&] {
        for (int guard = 0; guard < 8; ++guard) {
            if (w.toHost.empty() && w.toClient.empty()) break;
            while (!w.toHost.empty()) {
                auto d = std::move(w.toHost.front());
                w.toHost.pop_front();
                host.HandlePacket(d, now, kTestViewer);
            }
            while (!w.toClient.empty()) {
                auto d = std::move(w.toClient.front());
                w.toClient.pop_front();
                cli.HandlePacket(d, now);
            }
        }
    };

    cli.Start(Hello{0x1, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode}, now);
    pump();
    cli.NotifyVideoPacket(now);

    uint8_t buf[kMaxDatagram];
    size_t n = BuildReconfig(buf, cli.sessionId(), Reconfig{1280, 720, 8'000'000});
    cli.HandlePacket(std::span<const uint8_t>(buf, n), now);
    Check(reconfigured && rp.width == 1280 && rp.height == 720 && rp.bitrateBps == 8'000'000,
        "RECONFIG updates params and fires onReconfig");
    reconfigured = false;
    n = BuildReconfig(buf, cli.sessionId(), Reconfig{0, 0, 0});
    cli.HandlePacket(std::span<const uint8_t>(buf, n), now);
    Check(reconfigured && rp.width == 1280 && rp.height == 720,
        "RECONFIG with zero size keeps the previous dimensions");

    cli.SetFocused(true);
    cli.Tick(now);
    pump();
    Check(focus, "SET_FOCUS(true) reaches host onFocus");
    cli.SetFocused(false);
    now += 100'000;
    cli.Tick(now);
    pump();
    Check(gotFocusFalse, "SET_FOCUS(false) reaches host onFocus");

    cli.SendFeedback(Feedback{2, 7, 25, 9000});
    pump();
    Check(gotFb && lastFb.lossPct == 7 && lastFb.rttMs == 25, "FEEDBACK reaches host onFeedback");
}

size_t CountType(const std::deque<Datagram>& q, MsgType t) {
    size_t n = 0;
    for (const auto& d : q) {
        const auto h = ParseCommonHeader(d);
        if (h && h->type == t) ++n;
    }
    return n;
}

InputEvent SessionKey(int32_t vk, bool down) {
    InputEvent e;
    e.type = InputType::Key;
    e.a = vk;
    e.b = 0x1E;
    e.state = down ? 1 : 0;
    return e;
}

struct Rig {
    WirePair w;
    uint64_t now = 10'000'000;
    int startCalls = 0, readyCalls = 0, nackCalls = 0;
    bool hostDisconnected = false;
    uint32_t lastRtt = 0;
    std::vector<InputEvent> hostInput;
    std::string cliDead;
    ScreenHostSession host;
    ScreenClientSession cli;

    std::vector<std::string> hostClipboard;
    std::vector<std::string> cliClipboard;

    Rig() : host(HostCb(), StreamParams{1920, 1080, 60, 20'000'000}), cli(CliCb()) {
        host.SetPasscode(kTestPasscode);
    }

    ScreenHostCallbacks HostCb() {
        ScreenHostCallbacks cb;
        cb.send = [this](std::span<const uint8_t> d) { w.toClient.emplace_back(d.begin(), d.end()); };
        cb.randomBytes = TestRandomBytes;
        cb.onStart = [this] { ++startCalls; };
        cb.onDisconnect = [this] { hostDisconnected = true; };
        cb.onInput = [this](const InputEvent& e) { hostInput.push_back(e); };
        cb.onNack = [this](uint32_t, std::span<const uint16_t>) { ++nackCalls; };
        cb.onClipboardText = [this](std::string_view t) { hostClipboard.emplace_back(t); };
        return cb;
    }
    ScreenClientSessionCallbacks CliCb() {
        ScreenClientSessionCallbacks cb;
        cb.send = [this](std::span<const uint8_t> d) { w.toHost.emplace_back(d.begin(), d.end()); };
        cb.onReady = [this](const NegotiatedParams&) { ++readyCalls; };
        cb.onRtt = [this](uint32_t r) { lastRtt = r; };
        cb.onDisconnect = [this](const char* r, ScreenSessionEnd) { cliDead = r; };
        cb.onClipboardText = [this](std::string_view t) { cliClipboard.emplace_back(t); };
        return cb;
    }
    void Pump() {
        for (int guard = 0; guard < 8; ++guard) {
            if (w.toHost.empty() && w.toClient.empty()) break;
            while (!w.toHost.empty()) {
                auto d = std::move(w.toHost.front());
                w.toHost.pop_front();
                host.HandlePacket(d, now, kTestViewer);
            }
            while (!w.toClient.empty()) {
                auto d = std::move(w.toClient.front());
                w.toClient.pop_front();
                cli.HandlePacket(d, now);
            }
        }
    }
    void Handshake(uint32_t clientId = 0x1) {
        cli.Start(Hello{clientId, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode}, now);
        Pump();
        cli.NotifyVideoPacket(now);
    }
};

void TestHandshakeDuplicates() {
    std::printf("[session] duplicate HELLO_ACK / re-HELLO / repeated START are idempotent...\n");
    Rig r;
    r.Handshake();
    Check(r.startCalls == 1 && r.readyCalls == 1, "handshake reached STREAMING once");

    uint8_t buf[kMaxDatagram];
    HelloAck dup{};
    dup.sessionId = 0xDEAD;
    dup.codec = Codec::H264;
    dup.width = 640;
    dup.height = 480;
    dup.fps = 30;
    size_t n = BuildHelloAck(buf, dup);
    Check(r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now),
        "duplicate HELLO_ACK accepted as valid traffic");
    Check(r.cli.sessionId() != 0xDEAD && r.cli.params().width == 1920 && r.readyCalls == 1,
        "duplicate HELLO_ACK doesn't rebuild the session");

    const uint32_t sid = r.host.sessionId();
    r.w.toClient.clear();
    n = BuildHello(buf, Hello{0x1, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode});
    Check(r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer),
        "re-HELLO from the same client accepted");
    Check(r.host.sessionId() == sid && r.host.state() == ScreenHostSession::State::Streaming,
        "re-HELLO keeps the existing session");
    Check(CountType(r.w.toClient, MsgType::HelloAck) == 1, "re-HELLO answered with another ACK");
    const auto ack = ParseHelloAck(PayloadOf(r.w.toClient.front()));
    Check(ack && ack->sessionId == sid, "re-sent ACK carries the same sessionId");

    n = BuildStart(buf, sid);
    r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer);
    Check(r.startCalls == 1, "repeated START doesn't re-fire onStart");
}

void TestClientDeathPaths() {
    std::printf("[session] BYE from host + mid-session timeout kill the client...\n");
    {
        Rig r;
        r.Handshake();
        uint8_t buf[kMaxDatagram];
        const size_t n = BuildBye(buf, r.cli.sessionId());
        r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now);
        Check(r.cli.state() == ScreenClientSession::State::Dead && !r.cliDead.empty(),
            "BYE from host kills the client session");
    }
    {
        Rig r;
        r.Handshake();
        r.now += kSessionTimeoutUs + 1'000'000;
        r.cli.Tick(r.now);
        Check(r.cli.state() == ScreenClientSession::State::Dead && !r.cliDead.empty(),
            "silent host -> client dies on session timeout");
    }
}

void TestRejectCodecMismatch() {
    std::printf("[session] HELLO without H.264 -> rejected at handshake...\n");
    Rig r;
    r.cli.Start(Hello{0x2, uint16_t(0), 1920, 1080, 60, 0, 0, kTestPasscode}, r.now);
    r.Pump();
    Check(!r.cliDead.empty(), "client without H.264 refused at handshake");
    Check(r.host.state() == ScreenHostSession::State::Idle, "host stays IDLE after the codec reject");
}

void TestPasscodeGate() {
    std::printf("[session] 4-digit passcode: wrong rejected, right admitted, lockout...\n");
    {
        Rig r;
        r.host.SetPasscode("0417");
        Hello h{0x2, kCodecMaskH264, 1920, 1080, 60, 0};
        h.passcode = "1111";
        r.cli.Start(h, r.now);
        r.Pump();
        Check(r.cliDead.find("passcode") != std::string::npos, "wrong passcode is told why");
        Check(r.cli.rejectReason() == RejectReason::WrongPasscode,
            "the reject reason names the passcode");
        Check(r.host.state() == ScreenHostSession::State::Idle && r.host.viewerCount() == 0,
            "host stays idle after a wrong passcode");
    }
    {
        Rig r;
        r.host.SetPasscode("0417");
        Hello h{0x2, kCodecMaskH264, 1920, 1080, 60, 0};
        h.passcode = "0417";
        r.cli.Start(h, r.now);
        r.Pump();
        Check(r.readyCalls == 1 && r.host.viewerCount() == 1, "the right passcode is admitted");
    }
    {
        Rig r;
        r.host.SetPasscode("");
        Hello h{0x2, kCodecMaskH264, 1920, 1080, 60, 0};
        h.passcode = "9999";
        r.cli.Start(h, r.now);
        r.Pump();
        Check(r.readyCalls == 0 && r.host.viewerCount() == 0,
            "a host with no passcode set admits nobody");
        Check(r.cli.rejectReason() == RejectReason::WrongPasscode,
            "and the viewer is told why rather than left hanging");
    }
    {
        Rig r;
        r.host.SetPasscode("");
        r.cli.Start(Hello{0x2, kCodecMaskH264, 1920, 1080, 60, 0}, r.now);
        r.Pump();
        Check(r.readyCalls == 0, "not even a viewer that sends no passcode either");
    }
    {
        Rig r;
        r.host.SetPasscode("0417");
        uint8_t buf[kMaxDatagram];
        Hello bad{0x9, kCodecMaskH264, 1920, 1080, 60, 0};
        bad.passcode = "0000";
        for (uint32_t i = 0; i < kMaxPasscodeAttempts; ++i) {
            const size_t n = BuildHello(buf, bad);
            r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer);
        }
        r.w.toClient.clear();

        Hello good = bad;
        good.passcode = "0417";
        size_t n = BuildHello(buf, good);
        r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer);
        Check(r.w.toClient.empty() && r.host.viewerCount() == 0,
            "locked out after repeated wrong guesses, even with the right code");

        r.now += kPasscodeLockoutUs + 1;
        n = BuildHello(buf, good);
        r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer);
        Check(r.host.viewerCount() == 1, "the lockout expires and the right code works again");
    }
}

void RunHandshakeAgainstBrokenRng(ScreenHostCallbacks hcb, const char* what) {
    WirePair w;
    uint64_t now = 10'000'000;
    hcb.send = [&](std::span<const uint8_t> d) { w.toClient.emplace_back(d.begin(), d.end()); };
    ScreenHostSession host(hcb, StreamParams{1920, 1080, 60, 20'000'000});
    host.SetPasscode(kTestPasscode);

    std::string cliDead;
    ScreenClientSessionCallbacks ccb;
    ccb.send = [&](std::span<const uint8_t> d) { w.toHost.emplace_back(d.begin(), d.end()); };
    ccb.onDisconnect = [&](const char* r, ScreenSessionEnd) { cliDead = r; };
    ScreenClientSession cli(ccb);

    cli.Start(Hello{0x3, kCodecMaskH264, 1920, 1080, 60, 0, 0, kTestPasscode}, now);
    while (!w.toHost.empty()) {
        auto d = std::move(w.toHost.front());
        w.toHost.pop_front();
        host.HandlePacket(d, now, kTestViewer);
    }
    while (!w.toClient.empty()) {
        auto d = std::move(w.toClient.front());
        w.toClient.pop_front();
        cli.HandlePacket(d, now);
    }

    Check(host.state() == ScreenHostSession::State::Idle, what);
    Check(cli.state() == ScreenClientSession::State::Dead, "and the client is told to go away");
    Check(cliDead.find("rejected") != std::string::npos, "with a reject, not a timeout");
    Check(cli.rejectReason() == RejectReason::None,
        "the generic reject carries no specific reason");
}

void TestRejectWhenNoSessionIdCanBeMade() {
    std::printf("[session] a host whose RNG fails refuses the client cleanly...\n");
    ScreenHostCallbacks failing;
    failing.randomBytes = [](std::span<uint8_t>) { return false; };
    RunHandshakeAgainstBrokenRng(failing, "a failed RNG leaves the host IDLE");

    ScreenHostCallbacks missing;
    missing.randomBytes = nullptr;
    RunHandshakeAgainstBrokenRng(missing, "a host with no RNG at all also stays IDLE");
}

void TestUnknownMessagesAreIgnored() {
    std::printf("[session] messages meant for the other side fall through safely...\n");
    Rig r;
    r.Handshake();

    uint8_t buf[kMaxDatagram];
    HelloAck ack{};
    ack.sessionId = r.host.sessionId();
    ack.codec = Codec::H264;
    size_t n = BuildHelloAck(buf, ack);
    Check(!r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer),
        "a HELLO_ACK sent at a host is refused");
    Check(r.host.state() == ScreenHostSession::State::Streaming, "and changes nothing");

    n = BuildHello(buf, Hello{0x9, kCodecMaskH264, 640, 480, 30, 0, 0, kTestPasscode});
    Check(!r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now),
        "a HELLO sent at a client is refused");
    Check(r.cli.state() == ScreenClientSession::State::Streaming, "and changes nothing");
}

void TestIdleClientTickIsInert() {
    std::printf("[session] a client that never started stays silent...\n");
    std::deque<Datagram> sent;
    ScreenClientSessionCallbacks cb;
    cb.send = [&](std::span<const uint8_t> d) { sent.emplace_back(d.begin(), d.end()); };
    ScreenClientSession cli(cb);
    cli.Tick(10'000'000);
    cli.Tick(30'000'000);
    Check(sent.empty(), "no pings, no hellos before Start()");
    Check(cli.state() == ScreenClientSession::State::Idle, "and the state stays IDLE");
}

void TestHostInputStats() {
    std::printf("[session] the host exposes the input counters for its status line...\n");
    Rig r;
    r.Handshake();
    Check(r.host.inputStats().applied == 0, "no input applied yet");

    r.cli.QueueInput(SessionKey(0x41, true));
    r.cli.QueueInput(SessionKey(0x41, false));
    r.now += 20'000;
    r.cli.Tick(r.now);
    r.Pump();
    Check(r.host.inputStats().applied == 2, "both key events are counted as applied");
    Check(r.host.inputStats().lost == 0, "nothing was lost on a clean wire");
}

void TestInputThroughSession() {
    std::printf("[session] input flows client -> host, deduped, gated on STREAMING...\n");
    Rig r;
    r.cli.QueueInput(SessionKey('A', true));
    r.Handshake();
    r.cli.Tick(r.now);
    r.Pump();
    Check(r.hostInput.empty(), "input queued before STREAMING is dropped");

    r.cli.QueueInput(SessionKey('B', true));
    r.cli.QueueInput(SessionKey('B', false));
    r.now += 20'000;
    r.cli.Tick(r.now);
    for (int i = 0; i < 3; ++i) {
        r.now += kInputRepeatIntervalUs;
        r.cli.Tick(r.now);
    }
    r.Pump();
    Check(r.hostInput.size() == 2 && r.hostInput[0].a == 'B' && r.hostInput[0].state == 1 &&
              r.hostInput[1].a == 'B' && r.hostInput[1].state == 0,
        "input events reach host exactly once, in order");
}

void TestStraySessionIdIgnored() {
    std::printf("[session] packets with a stray sessionId are ignored on both sides...\n");
    Rig r;
    r.Handshake();
    uint8_t buf[kMaxDatagram];
    const uint32_t bad = r.cli.sessionId() ^ 0x55AA55AA;

    size_t n = BuildPong(buf, bad, PingPong{9, 1});
    Check(!r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now) && r.lastRtt == 0,
        "stray PONG rejected, RTT untouched");
    n = BuildReconfig(buf, bad, Reconfig{320, 200, 1'000'000});
    Check(!r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now) &&
              r.cli.params().width == 1920,
        "stray RECONFIG ignored, params untouched");
    n = BuildBye(buf, bad);
    Check(!r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now) &&
              r.cli.state() == ScreenClientSession::State::Streaming,
        "stray BYE doesn't kill the session");

    r.w.toClient.clear();
    n = BuildPing(buf, bad, PingPong{1, 1});
    Check(!r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer) && r.w.toClient.empty(),
        "stray PING rejected, no PONG sent");
    const uint16_t idx[] = {1};
    n = BuildNack(buf, bad, 7, idx);
    Check(!r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer) && r.nackCalls == 0,
        "stray NACK rejected, onNack not called");
    const InputEvent ev = SessionKey('Z', true);
    n = BuildInputEvents(buf, bad, 0, std::span<const InputEvent>(&ev, 1));
    Check(!r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer) && r.hostInput.empty(),
        "stray INPUT_EVENT rejected, nothing injected");
}

void TestFocusRepeatsAndKeyframeCancel() {
    std::printf("[session] SET_FOCUS repeat quota + CancelKeyframeRequest...\n");
    Rig r;
    r.Handshake();
    r.w.toHost.clear();

    r.cli.SetFocused(true);
    for (int i = 0; i < 10; ++i) {
        r.now += kFocusRetryUs;
        r.cli.Tick(r.now);
    }
    Check(CountType(r.w.toHost, MsgType::SetFocus) == size_t(kFocusRepeats),
        "SET_FOCUS sent exactly kFocusRepeats times");

    r.w.toHost.clear();
    r.cli.SetFocused(true);
    r.now += kFocusRetryUs;
    r.cli.Tick(r.now);
    Check(CountType(r.w.toHost, MsgType::SetFocus) == 0, "SetFocused(same value) doesn't resend");

    r.w.toHost.clear();
    r.cli.RequestKeyframe(KeyframeReason::Loss);
    r.cli.Tick(r.now);
    r.now += kKeyframeRetryUs;
    r.cli.Tick(r.now);
    Check(CountType(r.w.toHost, MsgType::RequestKeyframe) == 2,
        "REQUEST_KEYFRAME repeats while wanted");
    r.cli.CancelKeyframeRequest();
    r.w.toHost.clear();
    r.now += kKeyframeRetryUs;
    r.cli.Tick(r.now);
    Check(CountType(r.w.toHost, MsgType::RequestKeyframe) == 0,
        "CancelKeyframeRequest stops the retries");
}

void TestSessionsSurviveGarbage() {
    std::printf("[session] 500 garbage datagrams -> both sides unaffected...\n");
    Rig r;
    r.Handshake();
    const uint32_t sid = r.host.sessionId();
    for (int i = 0; i < 500; ++i) {
        Datagram d(Rnd() % 1300, 0);
        for (auto& b : d) b = uint8_t(Rnd());
        r.host.HandlePacket(d, r.now, kTestViewer);
        r.cli.HandlePacket(d, r.now);
    }
    Check(r.host.state() == ScreenHostSession::State::Streaming && r.host.sessionId() == sid,
        "host unaffected by garbage datagrams");
    Check(r.cli.state() == ScreenClientSession::State::Streaming && r.cliDead.empty(),
        "client unaffected by garbage datagrams");
}

void TestClipboardThroughSession() {
    std::printf("[session] clipboard flows only when the host enables it...\n");
    {
        Rig r;
        r.host.SetClipboardEnabled(true);
        r.Handshake();
        r.w.toHost.clear();

        r.cli.QueueClipboard("shared text");
        r.now += 20'000;
        r.cli.Tick(r.now);
        Check(CountType(r.w.toHost, MsgType::Clipboard) > 0, "the viewer sends its clipboard");
        r.Pump();
        Check(r.hostClipboard.size() == 1 && r.hostClipboard[0] == "shared text",
            "the host applies it exactly once");

        for (uint32_t i = 0; i < 1 + kClipboardResendCount; ++i) {
            r.now += kClipboardResendIntervalUs;
            r.cli.Tick(r.now);
        }
        r.Pump();
        Check(r.hostClipboard.size() == 1, "the redundant resends never re-apply");

        uint8_t buf[kMaxDatagram];
        const std::string down = "from the host";
        ClipboardChunkView chunk;
        chunk.revision = 1;
        chunk.chunkIndex = 0;
        chunk.chunkCount = 1;
        chunk.payload = std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(down.data()), down.size());
        const size_t n = BuildClipboardChunk(buf, r.host.sessionId(), chunk);
        Check(r.cli.HandlePacket(std::span<const uint8_t>(buf, n), r.now),
            "a clipboard datagram from the host is valid");
        Check(r.cliClipboard.size() == 1 && r.cliClipboard[0] == down,
            "and the viewer applies it");
    }
    {
        Rig r;
        r.Handshake();
        r.cli.QueueClipboard("blocked");
        r.now += 20'000;
        r.cli.Tick(r.now);
        r.Pump();
        Check(r.hostClipboard.empty(), "with the toggle off, the host drops clipboard packets");
    }
}

void TestInputAlwaysFlows() {
    std::printf("[session] input is always shared, no opt-in needed...\n");
    {
        Rig r;
        r.Handshake();
        r.w.toHost.clear();
        for (int i = 0; i < 5; ++i)
            r.cli.QueueInput(InputEvent{InputType::Key, uint64_t(i), 65, 30, 1, 0});
        r.now += 20'000;
        r.cli.Tick(r.now);
        Check(CountType(r.w.toHost, MsgType::InputEvent) > 0, "the client sends input by default");
    }
    {
        Rig r;
        r.Handshake();
        uint8_t buf[kMaxDatagram];
        const InputEvent ev{InputType::Key, 1, 65, 30, 1, 0};
        const size_t n = BuildInputEvents(buf, r.cli.sessionId(), 0,
            std::span<const InputEvent>(&ev, 1));
        Check(r.host.HandlePacket(std::span<const uint8_t>(buf, n), r.now, kTestViewer), "the packet is valid");
        Check(r.hostInput.size() == 1, "and it reaches the injector");
    }
}

}

void RunScreenSessionTests() {
    TestSessions();
    TestHostKicksOneViewer();
    TestSessionsNackInvalidate();
    TestReconfigFocusFeedback();
    TestHandshakeDuplicates();
    TestRejectWhenNoSessionIdCanBeMade();
    TestUnknownMessagesAreIgnored();
    TestIdleClientTickIsInert();
    TestHostInputStats();
    TestClientDeathPaths();
    TestRejectCodecMismatch();
    TestPasscodeGate();
    TestInputThroughSession();
    TestClipboardThroughSession();
    TestStraySessionIdIgnored();
    TestFocusRepeatsAndKeyframeCancel();
    TestInputAlwaysFlows();
    TestSessionsSurviveGarbage();
}

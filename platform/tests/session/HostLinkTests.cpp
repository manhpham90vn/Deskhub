#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/session/LinkPulse.h"
#include "deskhub/session/host/Beacon.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/client/HostLink.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <atomic>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint16_t kLinkTestPort = 47845;
constexpr const char* kLinkPasscode = "0417";

bool WaitUntil(const std::function<bool()>& done, int millis) {
    for (int i = 0; i < millis; ++i) {
        if (done()) return true;
        SleepUs(1000);
    }
    return done();
}

std::vector<uint8_t> TerminalProbe() {
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildTermClose(message, 7));
    return message;
}

struct LinkHostRig {
    deskhubp::SessionTransport sock{};
    deskhub::Beacon beacon{};
    std::thread pump{};
    std::atomic<bool> stop{false};
    std::atomic<bool> answerPings{true};
    std::atomic<int> terminalSeen{0};

    ~LinkHostRig() {
        Shutdown();
    }

    bool Start(const deskhubp::HostIdentity& identity) {
        sock.SetRecvTimeout(1);
        deskhubp::QuicSettings settings;
        settings.certPemPath = identity.certPath;
        settings.keyPemPath = identity.keyPath;
        if (!sock.Listen(settings, kLinkTestPort, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = identity;
        auth.SetPasscode(deskhubp::LoadOrCreateAuthSalt(), kLinkPasscode);
        auth.allowNewPairings = true;
        sock.SetHostAuth(std::move(auth), deskhubp::TransportAuthCallbacks{});

        stop.store(false, std::memory_order_release);
        pump = std::thread([this] {
            uint8_t buf[deskhub::kMaxRecordSize];
            uint8_t reply[deskhub::kMaxDatagram];
            while (!stop.load(std::memory_order_acquire)) {
                NetAddr from;
                const int n = sock.RecvFrom(buf, sizeof(buf), from);
                if (n <= 0) continue;
                const std::span<const uint8_t> message(buf, size_t(n));
                const auto header = deskhub::ParseCommonHeader(message);
                if (!header) continue;
                if (header->chan == deskhub::Chan::Terminal) {
                    terminalSeen.fetch_add(1, std::memory_order_relaxed);
                    sock.SendRecordOn(from, deskhubp::kQuicControlStream, message);
                    continue;
                }
                if (!answerPings.load(std::memory_order_acquire)) continue;
                if (const size_t rn = beacon.Reply(reply, message, sock.Authenticated(from)); rn)
                    sock.SendTo(from, reply, rn);
            }
        });
        return true;
    }

    void Shutdown() {
        if (pump.joinable()) {
            stop.store(true, std::memory_order_release);
            pump.join();
        }
        sock.Close();
    }
};

deskhubp::HostLinkConfig LinkConfig(const char* passcode) {
    deskhubp::HostLinkConfig config;
    const std::string endpoint = std::string("127.0.0.1:") + std::to_string(kLinkTestPort);
    ParseNetAddr(endpoint, config.host);
    config.hostLabel = endpoint;
    config.passcode = passcode;
    config.clientName = "link-test-client";
    config.authTimeoutMs = 5000;
    return config;
}

void TestALinkAdmitsOnceAndRoutesByChannel() {
    std::printf("[hostlink] one handshake carries every channel to its own queue...\n");
    const ForgottenHost fresh(std::string("127.0.0.1:") + std::to_string(kLinkTestPort));
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLink link;
    const std::shared_ptr<deskhubp::HostLinkChannel> terminal =
        link.Open({deskhub::Chan::Terminal});
    const std::shared_ptr<deskhubp::HostLinkChannel> files = link.Open({deskhub::Chan::File});

    std::atomic<int> readyCalls{0};
    deskhubp::HostLinkCallbacks hooks;
    hooks.onReady = [&readyCalls](bool) { readyCalls.fetch_add(1, std::memory_order_relaxed); };
    Check(link.Start(LinkConfig(kLinkPasscode), std::move(hooks)), "the link starts");

    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "the link is admitted inside the deadline");
    Check(readyCalls.load(std::memory_order_relaxed) == 1, "and says so exactly once");
    Check(deskhubp::CheckTrustedHost(link.Config().hostLabel, identity.fingerprint) ==
              deskhub::TrustVerdict::Trusted,
        "a proved passcode pins the host key");

    const std::vector<uint8_t> ping = TerminalProbe();
    Check(link.SendRecordOn(deskhubp::kQuicControlStream, ping), "a terminal record goes out");
    Check(WaitUntil([&terminal] { return terminal->Pending() > 0; }, 10000),
        "the echo lands inside the deadline");
    const auto echoed = terminal->Poll();
    Check(echoed.has_value() && echoed->size() == ping.size(),
        "the terminal channel carries the echo whole");
    Check(files->Pending() == 0, "and the file channel never sees it");
    Check(!terminal->Poll().has_value(), "a drained queue answers empty");

    link.Stop();
    Check(!link.Running(), "a stopped link is not running");
    host.Shutdown();
}

void TestALinkStopsOnAChangedKeyUntilAccepted() {
    std::printf("[hostlink] a changed host key parks the link until someone decides...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig(kLinkPasscode);
    deskhub::Fingerprint stale;
    stale.bytes.fill(0x77);
    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", stale, NowUnixSeconds()),
        "another machine's key is on record");

    std::mutex askedMutex;
    std::string asked;
    const auto askedCopy = [&askedMutex, &asked] {
        const std::lock_guard<std::mutex> lock(askedMutex);
        return asked;
    };
    deskhubp::HostLink link;
    deskhubp::HostLinkCallbacks hooks;
    hooks.onTrustAsked = [&askedMutex, &asked](deskhub::TrustVerdict,
                             std::string_view fingerprint) {
        const std::lock_guard<std::mutex> lock(askedMutex);
        asked.assign(fingerprint);
    };
    Check(link.Start(config, std::move(hooks)), "the link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Deciding; }, 10000),
        "the link parks on Deciding");
    Check(WaitUntil([&askedCopy] { return !askedCopy().empty(); }, 10000),
        "and asks inside the deadline");
    Check(askedCopy() == deskhub::FormatFingerprint(identity.fingerprint),
        "and shows the host's real fingerprint");
    Check(link.Message() == deskhub::ui::kTrustChangedBody, "with the shared warning text");

    link.AcceptFingerprint();
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "accepting the key lets the link through");
    Check(deskhubp::CheckTrustedHost(config.hostLabel, identity.fingerprint) ==
              deskhub::TrustVerdict::Trusted,
        "and the new key replaces the stale one");
    link.Stop();

    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", stale, NowUnixSeconds()),
        "the stale key is planted again");
    deskhubp::HostLink refuser;
    Check(refuser.Start(config, deskhubp::HostLinkCallbacks{}), "a second link starts");
    Check(WaitUntil([&refuser] {
        return refuser.State() == deskhubp::HostLinkState::Deciding;
    },
              10000),
        "it parks on Deciding too");
    refuser.RejectFingerprint();
    Check(WaitUntil([&refuser] { return refuser.Settled(); }, 10000),
        "rejecting settles the link");
    Check(refuser.State() == deskhubp::HostLinkState::Ended, "as ended, not failed");
    Check(refuser.Message() == deskhub::ui::kTrustReject, "with the shared rejection text");
    refuser.Stop();

    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", identity.fingerprint,
              NowUnixSeconds()),
        "the true key is restored for the tests that follow");
    host.Shutdown();
}

void TestALinkReportsARefusal() {
    std::printf("[hostlink] a wrong passcode comes back as a refusal, not a hang...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLink link;
    Check(link.Start(LinkConfig("9999"), deskhubp::HostLinkCallbacks{}), "the link starts");
    Check(WaitUntil([&link] { return link.Settled(); }, 10000),
        "the link settles inside the deadline");
    Check(link.State() == deskhubp::HostLinkState::Refused, "as refused");
    Check(link.AuthCode() == deskhub::AuthResultCode::WrongPasscode, "for the stated reason");
    Check(!link.Message().empty(), "with something to show the user");
    link.Stop();
    host.Shutdown();
}

void TestALinkRecoversAndSaysItResumed() {
    std::printf("[hostlink] a dropped link redials on its own and says it resumed...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    auto host = std::make_unique<LinkHostRig>();
    Check(host->Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig(kLinkPasscode);
    config.recoverLink = true;
    config.recoverGraceUs = uint64_t{30} * 1000 * 1000;

    std::atomic<bool> resumedSeen{false};
    std::atomic<bool> lostSeen{false};
    deskhubp::HostLink link;
    deskhubp::HostLinkCallbacks hooks;
    hooks.onReady = [&resumedSeen](bool resumed) {
        if (resumed) resumedSeen.store(true, std::memory_order_release);
    };
    hooks.onLinkLost = [&lostSeen] { lostSeen.store(true, std::memory_order_release); };
    Check(link.Start(config, std::move(hooks)), "the link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "the link is admitted");

    host->Shutdown();
    host = std::make_unique<LinkHostRig>();
    Check(host->Start(identity), "the host comes back on the same port");

    Check(WaitUntil([&resumedSeen] { return resumedSeen.load(std::memory_order_acquire); },
              20000),
        "the link redials and resumes inside the deadline");
    Check(lostSeen.load(std::memory_order_acquire), "after having said the link was lost");
    Check(link.State() == deskhubp::HostLinkState::Ready, "and is ready again");

    link.Stop();
    host->Shutdown();
}

void TestTheLinkTakesItsOwnPulse() {
    std::printf("[hostlink] the link pings on its own and reads out rtt and quality...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    std::atomic<bool> pulseSeen{false};
    deskhubp::HostLink link;
    deskhubp::HostLinkCallbacks hooks;
    hooks.onPulse = [&pulseSeen](const deskhub::LinkPulseView& view) {
        if (view.haveRtt) pulseSeen.store(true, std::memory_order_release);
    };
    Check(link.Start(LinkConfig(kLinkPasscode), std::move(hooks)), "the link starts");
    Check(WaitUntil([&pulseSeen] { return pulseSeen.load(std::memory_order_acquire); }, 10000),
        "a pong turns into a reading inside the deadline");

    const deskhub::LinkPulseView view = link.Pulse();
    Check(view.haveRtt, "the accessor hands out the same reading");
    Check(view.quality != deskhub::LinkQuality::Unknown, "and a verdict on the quality");
    Check(view.rttUs < 1'000'000, "loopback reads as under a second");

    link.Stop();
    host.Shutdown();
}

void TestARequestedRedialResumes() {
    std::printf("[hostlink] asking for a redial drops the link and brings it back...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig(kLinkPasscode);
    config.recoverLink = true;
    config.recoverGraceUs = uint64_t{30} * 1000 * 1000;

    std::atomic<bool> lostSeen{false};
    std::atomic<bool> resumedSeen{false};
    deskhubp::HostLink link;
    deskhubp::HostLinkCallbacks hooks;
    hooks.onLinkLost = [&lostSeen] { lostSeen.store(true, std::memory_order_release); };
    hooks.onReady = [&resumedSeen](bool resumed) {
        if (resumed) resumedSeen.store(true, std::memory_order_release);
    };
    Check(link.Start(config, std::move(hooks)), "the link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "the link is admitted");

    link.RequestRedial();
    Check(WaitUntil([&lostSeen] { return lostSeen.load(std::memory_order_acquire); }, 10000),
        "the redial request drops the connection");
    Check(WaitUntil([&resumedSeen] { return resumedSeen.load(std::memory_order_acquire); },
              20000),
        "and the link comes back on its own");
    Check(link.State() == deskhubp::HostLinkState::Ready, "ready again");

    link.Stop();
    host.Shutdown();
}

void TestAHostThatStopsAnsweringPingsReadsAsLost() {
    std::printf("[hostlink] a host that goes silent on pings is treated as lost...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig(kLinkPasscode);
    config.recoverLink = true;
    config.recoverGraceUs = uint64_t{30} * 1000 * 1000;

    std::atomic<bool> pulseSeen{false};
    std::atomic<bool> lostSeen{false};
    std::atomic<bool> resumedSeen{false};
    deskhubp::HostLink link;
    deskhubp::HostLinkCallbacks hooks;
    hooks.onPulse = [&pulseSeen](const deskhub::LinkPulseView& view) {
        if (view.haveRtt) pulseSeen.store(true, std::memory_order_release);
    };
    hooks.onLinkLost = [&lostSeen] { lostSeen.store(true, std::memory_order_release); };
    hooks.onReady = [&resumedSeen](bool resumed) {
        if (resumed) resumedSeen.store(true, std::memory_order_release);
    };
    Check(link.Start(config, std::move(hooks)), "the link starts");
    Check(WaitUntil([&pulseSeen] { return pulseSeen.load(std::memory_order_acquire); }, 10000),
        "pongs are flowing first");

    host.answerPings.store(false, std::memory_order_release);
    Check(WaitUntil([&lostSeen] { return lostSeen.load(std::memory_order_acquire); }, 15000),
        "the silence is called out as a lost link");

    host.answerPings.store(true, std::memory_order_release);
    Check(WaitUntil([&resumedSeen] { return resumedSeen.load(std::memory_order_acquire); },
              20000),
        "and answering again brings the link back");

    link.Stop();
    host.Shutdown();
}

}

void RunHostLinkTests() {
    TestALinkAdmitsOnceAndRoutesByChannel();
    TestALinkStopsOnAChangedKeyUntilAccepted();
    TestALinkReportsARefusal();
    TestALinkRecoversAndSaysItResumed();
    TestTheLinkTakesItsOwnPulse();
    TestARequestedRedialResumes();
    TestAHostThatStopsAnsweringPingsReadsAsLost();
}

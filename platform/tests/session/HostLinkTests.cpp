#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/session/host/SourceListResponder.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/HostProfiles.h"
#include "deskhubp/client/HostLink.h"
#include "deskhubp/client/SourceQuery.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/ClientIdentity.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/Random.h"
#include "deskhubp/system/TrustStoreFile.h"

#include <atomic>
#include <array>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint16_t kLinkTestPort = 47845;

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
    deskhub::SourceListResponder sourceList{};
    std::thread pump{};
    std::atomic<bool> stop{false};
    std::atomic<bool> answerPings{true};
    std::atomic<int> terminalSeen{0};
    std::atomic<int> pongsSent{0};

    ~LinkHostRig() {
        Shutdown();
    }

    bool Start(const deskhubp::HostIdentity& identity) {
        const auto client = deskhubp::LoadOrCreateClientIdentity();
        if (!client.Valid() ||
            !GrantClientKey(client))
            return false;
        sock.SetRecvTimeout(1);
        deskhubp::QuicSettings settings;
        settings.certPemPath = identity.certPath;
        settings.keyPemPath = identity.keyPath;
        if (!sock.Listen(settings, kLinkTestPort, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = identity;
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
                const size_t rn = sourceList.Reply(reply, message, sock.Authenticated(from));
                if (!rn) continue;
                sock.SendTo(from, reply, rn);
                if (header->type == deskhub::MsgType::Ping)
                    pongsSent.fetch_add(1, std::memory_order_relaxed);
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

deskhubp::HostLinkConfig LinkConfig() {
    deskhubp::HostLinkConfig config;
    const std::string endpoint = std::string("127.0.0.1:") + std::to_string(kLinkTestPort);
    ParseNetAddr(endpoint, config.host);
    config.hostLabel = endpoint;
    config.clientName = "link-test-client";
    config.authTimeoutMs = 5000;
    return config;
}

void TestALinkAdmitsOnceAndRoutesByChannel() {
    std::printf("[hostlink] one handshake carries every channel to its own queue...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    const deskhubp::HostLinkConfig config = LinkConfig();
    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1",
              identity.fingerprint, NowUnixSeconds()),
        "the host key is pinned before connecting");

    deskhubp::HostLink link;
    const std::shared_ptr<deskhubp::HostLinkChannel> terminal =
        link.Open({deskhub::Chan::Terminal});
    const std::shared_ptr<deskhubp::HostLinkChannel> files = link.Open({deskhub::Chan::File});

    std::atomic<int> readyCalls{0};
    deskhubp::HostLinkCallbacks hooks;
    hooks.onReady = [&readyCalls](bool) { readyCalls.fetch_add(1, std::memory_order_relaxed); };
    Check(link.Start(config, std::move(hooks)), "the link starts");

    Check(WaitUntil(
              [&link, &readyCalls] {
                  return link.State() == deskhubp::HostLinkState::Ready &&
                         readyCalls.load(std::memory_order_relaxed) > 0;
              },
              10000),
        "the link is admitted inside the deadline");
    Check(readyCalls.load(std::memory_order_relaxed) == 1, "and says so exactly once");
    Check(deskhubp::CheckTrustedHost(LinkConfig().hostLabel, identity.fingerprint) ==
              deskhub::TrustVerdict::Trusted,
        "the configured host key remains pinned");

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

void TestALinkRejectsUnknownAndChangedHostKeys() {
    std::printf("[hostlink] unknown and changed host keys are rejected before auth...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig();
    deskhubp::ForgetTrustedHost(config.hostLabel);
    deskhubp::HostLink unknown;
    Check(unknown.Start(config, deskhubp::HostLinkCallbacks{}), "an unconfigured link starts");
    Check(WaitUntil([&unknown] { return unknown.Settled(); }, 10000),
        "the unknown host key is rejected");
    Check(unknown.State() == deskhubp::HostLinkState::Failed,
        "unknown host key fails without a trust prompt");
    unknown.Stop();

    deskhub::Fingerprint stale;
    stale.bytes.fill(0x77);
    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", stale, NowUnixSeconds()),
        "another machine's key is on record");
    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the changed-key link starts");
    Check(WaitUntil([&link] { return link.Settled(); }, 10000),
        "the changed host key is rejected");
    Check(link.State() == deskhubp::HostLinkState::Failed,
        "changed host key fails without a trust prompt");
    Check(deskhubp::CheckTrustedHost(config.hostLabel, stale) ==
              deskhub::TrustVerdict::Trusted,
        "the saved pin is not silently changed");
    link.Stop();

    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", identity.fingerprint,
              NowUnixSeconds()),
        "the true key is restored for the tests that follow");
    host.Shutdown();
}

void TestALinkReportsARefusal() {
    std::printf("[hostlink] an unlisted client key is refused without approval...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    const auto client = deskhubp::LoadOrCreateClientIdentity();
    Check(deskhubp::ForgetAuthorizedClient(client.fingerprint), "the client key is revoked");
    const auto config = LinkConfig();
    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1",
              identity.fingerprint, NowUnixSeconds()),
        "the host key is pinned separately");

    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the link starts");
    Check(WaitUntil([&link] { return link.Settled(); }, 10000),
        "the link settles inside the deadline");
    Check(link.State() == deskhubp::HostLinkState::Refused, "as refused");
    Check(link.Message() == deskhub::ui::AuthRefusalText(deskhub::AuthResultCode::NotPaired),
        "because the client key is not authorized, and the user is told so");
    link.Stop();
    host.Shutdown();
}

void TestALinkUsesTheSelectedClientIdentity() {
    std::array<uint8_t, 8> suffix{};
    if (!RandomBytes(suffix.data(), suffix.size())) {
        Check(false, "the selected identity test has a unique directory");
        return;
    }
    std::string name = "deskhub-selected-key-";
    constexpr char digits[] = "0123456789abcdef";
    for (uint8_t byte : suffix) {
        name += digits[byte >> 4];
        name += digits[byte & 15];
    }
    const auto dir = std::filesystem::temp_directory_path() / name;
    const std::string previous = deskhubp::AppDataDirRef();
    deskhubp::SetAppDataDir(dir.string());

    const auto identity = deskhubp::LoadOrCreateHostIdentity("selected-key-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host starts with the default client key authorized");
    const auto fallback = deskhubp::LoadClientIdentity();
    const auto selected = deskhubp::GenerateClientIdentity("phone");
    Check(selected.Valid() && selected.fingerprint != fallback.fingerprint,
        "the selected client key differs from the default");
    Check(GrantClientKey(selected),
        "the host authorizes the selected client key");
    Check(deskhubp::ForgetAuthorizedClient(fallback.fingerprint),
        "the default key is not authorized for this connection");

    auto config = LinkConfig();
    Check(deskhubp::RememberTrustedHostProfile(config.hostLabel, "selected-key-host",
              identity.fingerprint, "phone", NowUnixSeconds()),
        "the host profile pins its key and selects the phone identity");
    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the selected-key link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "the key selected by the host profile authenticates the connection");
    link.Stop();
    host.Shutdown();

    deskhubp::SetAppDataDir(previous);
    std::error_code error;
    std::filesystem::remove_all(dir, error);
}

void TestALinkRecoversAndSaysItResumed() {
    std::printf("[hostlink] a dropped link redials on its own and says it resumed...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    auto host = std::make_unique<LinkHostRig>();
    Check(host->Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig();
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

void TestTheLinkPingsOnItsOwn() {
    std::printf("[hostlink] the link keeps pinging the host on its own...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLink link;
    Check(link.Start(LinkConfig(), deskhubp::HostLinkCallbacks{}), "the link starts");
    Check(WaitUntil([&host] { return host.pongsSent.load(std::memory_order_relaxed) >= 2; },
              10000),
        "pings keep arriving and the host answers each one");

    link.Stop();
    host.Shutdown();
}

void TestARequestedRedialResumes() {
    std::printf("[hostlink] asking for a redial drops the link and brings it back...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig();
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

    deskhubp::HostLinkConfig config = LinkConfig();
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
    Check(WaitUntil([&host] { return host.pongsSent.load(std::memory_order_relaxed) > 0; }, 10000),
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

void TestANewHostKeyIsSavedOnlyWhenAsked() {
    std::printf("[hostlink] a first-time host key is saved only when the user accepts it...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("link-test-host");
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    deskhubp::HostLinkConfig config = LinkConfig();
    deskhubp::ForgetTrustedHost(config.hostLabel);

    SourceQueryReply reply;
    Check(!QuerySources(config.host, reply), "a query to an unknown host is refused");
    Check(reply.unknownHostKey == identity.fingerprint,
        "and hands back the fingerprint the user has to confirm");
    Check(reply.failure == deskhub::ui::kAuthUntrustedHost, "with the reason to show");
    Check(deskhubp::CheckTrustedHost(config.hostLabel, identity.fingerprint) ==
              deskhub::TrustVerdict::Unknown,
        "nothing is saved while the user has not answered");

    config.acceptNewHostKey = true;
    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the accepting link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "the accepted host is connected");
    link.Stop();
    const auto saved = deskhubp::LoadTrustStore().Find(config.hostLabel);
    Check(saved && saved->fingerprint == identity.fingerprint,
        "the key the host proved is the one saved");
    Check(saved && deskhub::ui::IsValidHostAlias(saved->label),
        "under a name the user can edit later");

    deskhub::Fingerprint stale;
    stale.bytes.fill(0x55);
    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", stale, NowUnixSeconds()),
        "a different key is now on record");
    deskhubp::HostLink changed;
    Check(changed.Start(config, deskhubp::HostLinkCallbacks{}), "the link starts again");
    Check(WaitUntil([&changed] { return changed.Settled(); }, 10000), "and settles");
    Check(changed.State() == deskhubp::HostLinkState::Failed &&
              changed.Message() == deskhub::ui::kAuthHostKeyChanged,
        "accepting new keys never overrides a key that changed");
    changed.Stop();

    Check(deskhubp::RememberTrustedHost(config.hostLabel, "127.0.0.1", identity.fingerprint,
              NowUnixSeconds()),
        "the true key is restored for the tests that follow");
    host.Shutdown();
}

void RunHostLinkTests() {
    TestALinkAdmitsOnceAndRoutesByChannel();
    TestALinkRejectsUnknownAndChangedHostKeys();
    TestANewHostKeyIsSavedOnlyWhenAsked();
    TestALinkReportsARefusal();
    TestALinkUsesTheSelectedClientIdentity();
    TestALinkRecoversAndSaysItResumed();
    TestTheLinkPingsOnItsOwn();
    TestARequestedRedialResumes();
    TestAHostThatStopsAnsweringPingsReadsAsLost();
}

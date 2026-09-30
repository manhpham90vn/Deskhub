#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhub/session/host/SourceListResponder.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/HostProfiles.h"
#include "deskhubp/client/HostLink.h"
#include "deskhubp/client/HostProfiles.h"
#include "deskhubp/client/SourceQuery.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/host/PairingInvite.h"
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
        const auto client = deskhubp::LoadOrCreateHostIdentity();
        if (!client.Valid() ||
            !GrantClientKey(client))
            return false;
        sock.SetRecvTimeout(1);
        deskhubp::QuicSettings settings;
        settings.certPem = deskhubp::TransportCertificatePem(identity);
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
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    const deskhubp::HostLinkConfig config = LinkConfig();
    Check(deskhubp::RememberTrustedHost(identity.fingerprint, "link-test-host",
              config.hostLabel, NowUnixSeconds()),
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
    Check(deskhubp::CheckTrustedHost(identity.fingerprint) == deskhub::TrustVerdict::Trusted,
        "the configured host key remains pinned");
    const auto touched = deskhubp::LoadTrustStore().Find(identity.fingerprint);
    Check(touched && touched->endpoint == config.hostLabel,
        "and the address it answered from is remembered");

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

void TestALinkRejectsAnUnknownHostKey() {
    std::printf("[hostlink] an unknown host key is rejected before auth...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");

    deskhubp::HostLinkConfig config = LinkConfig();
    deskhubp::ForgetTrustedHost(identity.fingerprint);
    deskhubp::HostLink unknown;
    Check(unknown.Start(config, deskhubp::HostLinkCallbacks{}), "an unconfigured link starts");
    Check(WaitUntil([&unknown] { return unknown.Settled(); }, 10000),
        "the unknown host key is rejected");
    Check(unknown.State() == deskhubp::HostLinkState::Failed &&
              unknown.Message() == deskhub::ui::kAuthUntrustedHost,
        "unknown host key fails without a trust prompt");
    unknown.Stop();

    deskhub::Fingerprint stale;
    stale.bytes.fill(0x77);
    Check(deskhubp::RememberTrustedHost(stale, "someone-else", config.hostLabel, NowUnixSeconds()),
        "another machine's key is on record for the same address");
    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the link starts again");
    Check(WaitUntil([&link] { return link.Settled(); }, 10000), "and settles");
    Check(link.State() == deskhubp::HostLinkState::Failed,
        "a key trusted for some other machine does not vouch for this one");
    Check(deskhubp::CheckTrustedHost(stale) == deskhub::TrustVerdict::Trusted,
        "the saved pin is not silently changed");
    link.Stop();
    deskhubp::ForgetTrustedHost(stale);

    Check(deskhubp::RememberTrustedHost(identity.fingerprint, "link-test-host", config.hostLabel,
              NowUnixSeconds()),
        "the true key is restored for the tests that follow");
    host.Shutdown();
}

void TestALinkWaitsForApprovalAndGetsIn() {
    std::printf("[hostlink] an unlisted client waits, and is let in the moment the owner approves...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    Check(deskhubp::ForgetAuthorizedClient(identity.fingerprint), "the client key is revoked");
    deskhubp::RemoveAppDataFile(deskhubp::kAccessRequestsFileName);
    auto config = LinkConfig();
    Check(deskhubp::RememberTrustedHost(identity.fingerprint, "link-test-host", config.hostLabel,
              NowUnixSeconds()),
        "the host key is pinned separately");

    std::atomic<int> waitingSeen{0};
    deskhubp::HostLinkCallbacks hooks;
    hooks.onState = [&waitingSeen](deskhubp::HostLinkState state, std::string_view) {
        if (state == deskhubp::HostLinkState::AwaitingApproval) waitingSeen.fetch_add(1);
    };
    deskhubp::HostLink link;
    Check(link.Start(config, std::move(hooks)), "the link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::AwaitingApproval; },
              10000),
        "the link says it is waiting for approval");
    Check(link.Message() == deskhub::ui::AwaitingApprovalLine(config.hostLabel),
        "and tells the user who has to approve");
    Check(!link.Settled(), "waiting is not a final state");
    Check(WaitUntil(
              [] {
                  const auto requests = deskhubp::ListAccessRequests();
                  return requests && requests->size() == 1;
              },
              5000),
        "the host recorded the request");
    Check(waitingSeen.load() == 1, "the waiting state is announced once, not on every redial");

    Check(deskhubp::ApproveAccessRequest(identity.fingerprint), "the owner approves");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 15000),
        "the link redials on its own and is admitted");
    link.Stop();

    Check(deskhubp::ForgetAuthorizedClient(identity.fingerprint), "the client key is revoked again");
    deskhubp::HostLinkConfig impatient = config;
    impatient.approvalWaitUs = 1'500'000;
    deskhubp::HostLink shortWait;
    Check(shortWait.Start(impatient, deskhubp::HostLinkCallbacks{}), "a link with a short wait starts");
    Check(WaitUntil([&shortWait] { return shortWait.Settled(); }, 15000),
        "it gives up when the wait runs out");
    Check(shortWait.State() == deskhubp::HostLinkState::Refused &&
              shortWait.Message() ==
                  deskhub::ui::AuthRefusalText(deskhub::AuthResultCode::AwaitingApproval),
        "and explains that nobody approved in time");
    shortWait.Stop();
    Check(deskhubp::DenyAccessRequest(identity.fingerprint), "the owner can also deny the request");
    Check(GrantClientKey(identity), "the client key is allowed again for the tests that follow");
    host.Shutdown();
}

void TestAnInviteAdmitsWithoutADialog() {
    std::printf("[hostlink] a QR invite pins the host and lets the client in at once...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    Check(deskhubp::ForgetAuthorizedClient(identity.fingerprint), "the client key is not allowed");
    Check(deskhubp::ForgetTrustedHost(identity.fingerprint), "and the host is not trusted");

    const std::string invite = deskhubp::BuildPairingInvite(kLinkTestPort, "127.0.0.1", "link-host");
    const auto parsed = deskhub::ParsePairingInvite(invite);
    Check(parsed.has_value(), "the host made an invite");
    if (!parsed) return;

    auto config = LinkConfig();
    config.expectedHostKey = parsed->hostKey;
    config.pairingToken.assign(parsed->token.begin(), parsed->token.end());
    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the invited link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "it is admitted with no dialog and no approval");
    Check(deskhubp::CheckTrustedHost(identity.fingerprint) == deskhub::TrustVerdict::Trusted,
        "the host is pinned from the invite");
    Check(deskhubp::IsClientKeyAuthorized(identity.publicKey),
        "and the client key was added on the host side");
    link.Stop();

    deskhubp::HostLinkConfig wrong = LinkConfig();
    wrong.expectedHostKey = parsed->hostKey;
    wrong.expectedHostKey->bytes[0] ^= 0xFF;
    wrong.pairingToken = config.pairingToken;
    deskhubp::HostLink mismatch;
    Check(mismatch.Start(wrong, deskhubp::HostLinkCallbacks{}), "a link with a foreign invite starts");
    Check(WaitUntil([&mismatch] { return mismatch.Settled(); }, 10000), "and settles");
    Check(mismatch.State() == deskhubp::HostLinkState::Failed &&
              mismatch.Message() == deskhub::ui::kInviteHostMismatch,
        "a host that is not the one the invite names is refused before any token is sent");
    mismatch.Stop();
    host.Shutdown();
}

void TestALinkRecoversAndSaysItResumed() {
    std::printf("[hostlink] a dropped link redials on its own and says it resumed...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
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
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
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
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
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
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
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

void TestAnImpostorOnTheKnownAddressIsAnotherMachine() {
    std::printf("[hostlink] a different key at a known address is a stranger, not the host...\n");
    const deskhubp::HostIdentity real = deskhubp::LoadOrCreateHostIdentity();
    const std::string savedKey = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
    const deskhubp::HostLinkConfig config = LinkConfig();
    Check(deskhubp::RememberTrustedHost(real.fingerprint, "link-test-host", config.hostLabel,
              NowUnixSeconds()),
        "the real host key is trusted and last seen at this address");

    ForgetHostIdentity();
    const deskhubp::HostIdentity impostor = deskhubp::LoadOrCreateHostIdentity();
    Check(impostor.Valid() && impostor.fingerprint != real.fingerprint,
        "an impostor makes its own key");
    LinkHostRig host;
    Check(host.Start(impostor), "and listens on the same IP and port");

    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the client dials the known address");
    Check(WaitUntil([&link] { return link.Settled(); }, 10000), "the link settles");
    Check(link.State() == deskhubp::HostLinkState::Failed &&
              link.Message() == deskhub::ui::kAuthUntrustedHost,
        "a matching address does not stand in for the trusted key");
    Check(deskhubp::CheckTrustedHost(real.fingerprint) == deskhub::TrustVerdict::Trusted,
        "and the real host stays trusted");
    Check(!deskhubp::PreviousOwnerWarningFor(config.hostLabel, impostor.fingerprint).empty(),
        "the New host dialog would warn that this address used to belong to the real host");
    link.Stop();
    host.Shutdown();

    deskhubp::HostLinkConfig accepting = config;
    accepting.acceptNewHostKey = true;
    LinkHostRig again;
    Check(again.Start(impostor), "the impostor listens again");
    deskhubp::HostLink eager;
    Check(eager.Start(accepting, deskhubp::HostLinkCallbacks{}),
        "a client that accepts new host keys dials it");
    Check(WaitUntil([&eager] { return eager.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "accepting a new key trusts the stranger as a machine of its own");
    Check(deskhubp::CheckTrustedHost(real.fingerprint) == deskhub::TrustVerdict::Trusted &&
              deskhubp::CheckTrustedHost(impostor.fingerprint) == deskhub::TrustVerdict::Trusted,
        "both machines are now trusted, neither replaced the other");
    eager.Stop();
    again.Shutdown();
    deskhubp::ForgetTrustedHost(impostor.fingerprint);

    Check(deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, savedKey) &&
              deskhubp::LoadHostIdentity().fingerprint == real.fingerprint,
        "the real identity is restored for the tests that follow");
}

void TestANewHostKeyIsSavedOnlyWhenAsked() {
    std::printf("[hostlink] a first-time host key is saved only when the user accepts it...\n");
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
    LinkHostRig host;
    Check(host.Start(identity), "the host rig listens");
    deskhubp::HostLinkConfig config = LinkConfig();
    deskhubp::ForgetTrustedHost(identity.fingerprint);

    SourceQueryReply reply;
    Check(!QuerySources(config.host, reply), "a query to an unknown host is refused");
    Check(reply.unknownHostKey == identity.fingerprint,
        "and hands back the fingerprint the user has to confirm");
    Check(reply.failure == deskhub::ui::kAuthUntrustedHost, "with the reason to show");
    Check(reply.failureKind == SourceQueryFailure::UntrustedHost, "under the right kind");
    Check(deskhubp::CheckTrustedHost(identity.fingerprint) == deskhub::TrustVerdict::Unknown,
        "nothing is saved while the user has not answered");

    config.acceptNewHostKey = true;
    deskhubp::HostLink link;
    Check(link.Start(config, deskhubp::HostLinkCallbacks{}), "the accepting link starts");
    Check(WaitUntil([&link] { return link.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "the accepted host is connected");
    link.Stop();
    const auto saved = deskhubp::LoadTrustStore().Find(identity.fingerprint);
    Check(saved && saved->endpoint == config.hostLabel,
        "the key the host proved is the one saved, with the address it answered from");
    Check(saved && deskhub::ui::IsValidHostAlias(saved->label),
        "under a name the user can edit later");

    Check(deskhubp::TouchTrustedHost(identity.fingerprint, "10.0.0.99:47777", NowUnixSeconds()),
        "the host is now remembered at some other address");
    deskhubp::HostLink moved;
    Check(moved.Start(LinkConfig(), deskhubp::HostLinkCallbacks{}), "the link dials it here anyway");
    Check(WaitUntil([&moved] { return moved.State() == deskhubp::HostLinkState::Ready; }, 10000),
        "trust follows the key, so a host that changed address connects without a prompt");
    moved.Stop();
    const auto back = deskhubp::LoadTrustStore().Find(identity.fingerprint);
    Check(back && back->endpoint == config.hostLabel, "and its new address is written down");
    host.Shutdown();
}

}

void RunHostLinkTests() {
    TestALinkAdmitsOnceAndRoutesByChannel();
    TestALinkRejectsAnUnknownHostKey();
    TestAnImpostorOnTheKnownAddressIsAnotherMachine();
    TestANewHostKeyIsSavedOnlyWhenAsked();
    TestALinkWaitsForApprovalAndGetsIn();
    TestAnInviteAdmitsWithoutADialog();
    TestALinkRecoversAndSaysItResumed();
    TestTheLinkPingsOnItsOwn();
    TestARequestedRedialResumes();
    TestAHostThatStopsAnsweringPingsReadsAsLost();
}

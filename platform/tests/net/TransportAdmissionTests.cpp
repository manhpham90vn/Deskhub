#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/PairingTokenFile.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr uint16_t kAdmissionPort = 47867;
constexpr uint32_t kAuthTimeoutMs = 4000;
constexpr int kSettleMillis = 5000;
constexpr int kQuietMillis = 300;

struct SavedState {
    std::string key{};
    std::string authorizedKeys{};

    SavedState() {
        key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
        authorizedKeys = deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName);
        RevokeAllClientKeys();
    }

    ~SavedState() {
        RevokeAllClientKeys();
        if (!key.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, key);
        if (authorizedKeys.empty())
            deskhubp::RemoveAppDataFile(deskhubp::kAuthorizedKeysFileName);
        else
            deskhubp::WriteAppDataFile(deskhubp::kAuthorizedKeysFileName, authorizedKeys);
    }
};

struct Machines {
    deskhubp::HostIdentity viewer{};
    deskhubp::HostIdentity impostor{};
    deskhubp::HostIdentity host{};

    bool Make() {
        ForgetHostIdentity();
        viewer = deskhubp::LoadOrCreateHostIdentity();
        ForgetHostIdentity();
        impostor = deskhubp::LoadOrCreateHostIdentity();
        ForgetHostIdentity();
        host = deskhubp::LoadOrCreateHostIdentity();
        return viewer.Valid() && impostor.Valid() && host.Valid() &&
               !(viewer.fingerprint == host.fingerprint) &&
               !(viewer.fingerprint == impostor.fingerprint);
    }
};

bool WaitUntil(const std::function<bool()>& done, int millis) {
    for (int i = 0; i < millis; ++i) {
        if (done()) return true;
        SleepUs(1000);
    }
    return done();
}

struct AdmissionRig {
    Machines machines{};
    deskhubp::SessionTransport host{};
    deskhubp::SessionTransport viewer{};
    NetAddr target{0x7F000001u, kAdmissionPort};
    std::atomic<uint64_t> admitted{0};
    std::atomic<int> delivered{0};
    std::atomic<bool> stop{false};
    std::thread pump{};

    ~AdmissionRig() {
        StopHost();
        viewer.Close();
    }

    bool Start(bool authenticate = true) {
        if (!machines.Make()) return false;
        if (!GrantClientKey(machines.viewer)) return false;
        host.SetRecvTimeout(1);
        viewer.SetRecvTimeout(1);

        deskhubp::QuicSettings settings;
        settings.certPem = deskhubp::TransportCertificatePem(machines.host);
        settings.keyPemPath = machines.host.keyPath;
        if (!host.Listen(settings, kAdmissionPort, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = machines.host;
        deskhubp::TransportAuthCallbacks hooks;
        hooks.onPaired = [this](const NetAddr& peer, const deskhub::Fingerprint&,
                             std::string_view) {
            admitted.store(peer.Pack(), std::memory_order_release);
        };
        host.SetHostAuth(std::move(auth), std::move(hooks));

        pump = std::thread([this] {
            uint8_t buf[deskhub::kMaxRecordSize];
            while (!stop.load(std::memory_order_acquire)) {
                NetAddr from;
                if (host.RecvFrom(buf, sizeof(buf), from) > 0)
                    delivered.fetch_add(1, std::memory_order_relaxed);
            }
        });

        if (!viewer.Connect(deskhubp::QuicSettings{}, target, "admission-host")) return false;
        if (!viewer.WaitEstablished(target, kAuthTimeoutMs)) return false;
        if (!authenticate) return true;
        if (!SignIn(viewer, machines.viewer)) return false;
        return WaitUntil([this] { return Peer().Pack() != 0; }, kSettleMillis);
    }

    bool SignIn(deskhubp::SessionTransport& client, const deskhubp::HostIdentity& identity,
        std::vector<uint8_t> pairingToken = {}) {
        deskhub::AuthResultCode code = deskhub::AuthResultCode::NotPaired;
        return SignInWithCode(client, identity, code, std::move(pairingToken));
    }

    bool SignInWithCode(deskhubp::SessionTransport& client, const deskhubp::HostIdentity& identity,
        deskhub::AuthResultCode& code, std::vector<uint8_t> pairingToken = {}) {
        deskhubp::ClientAuthConfig config;
        config.identity = identity;
        config.hostFingerprint = machines.host.fingerprint;
        config.clientName = "admission-viewer";
        config.pairingToken = std::move(pairingToken);
        return client.RunClientAuth(target, std::move(config), kAuthTimeoutMs, code);
    }

    bool Dial(deskhubp::SessionTransport& client) const {
        client.SetRecvTimeout(1);
        return client.Connect(deskhubp::QuicSettings{}, target, "admission-host") &&
               client.WaitEstablished(target, kAuthTimeoutMs);
    }

    NetAddr Peer() const {
        return NetAddr::Unpack(admitted.load(std::memory_order_acquire));
    }

    void PumpViewer() {
        uint8_t buf[deskhub::kMaxRecordSize];
        NetAddr from;
        viewer.RecvFrom(buf, sizeof(buf), from);
    }

    void StopHost() {
        if (!pump.joinable()) return;
        stop.store(true, std::memory_order_release);
        pump.join();
        host.Close();
    }
};

bool Skipped(const char* tag) {
    if (deskhubp::QuicAvailable()) return false;
    std::printf("[%s] skipped: this build has no QUIC library\n", tag);
    return true;
}

bool BeginWithoutAnswer(deskhubp::SessionTransport& viewer, const NetAddr& target,
    const deskhubp::HostIdentity& identity) {
    deskhub::AuthStart start;
    start.publicKey = identity.publicKey;
    start.clientName = "pending-auth-test";
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildAuthStart(message, start));
    if (message.empty() || !viewer.SendRecord(target, message)) return false;
    return WaitUntil(
        [&] {
            uint8_t buf[deskhub::kMaxRecordSize];
            NetAddr from;
            const int got = viewer.RecvFrom(buf, sizeof(buf), from);
            if (got <= 0) return false;
            const auto header = deskhub::ParseCommonHeader(
                std::span<const uint8_t>(buf, size_t(got)));
            if (!header || header->type != deskhub::MsgType::AuthChallenge) return false;
            const auto challenge = deskhub::ParseAuthChallenge(
                deskhub::PayloadOf(std::span<const uint8_t>(buf, size_t(got))));
            return challenge && challenge->mode == deskhub::AuthMode::Signature;
        },
        kSettleMillis);
}

bool SendProof(deskhubp::SessionTransport& viewer, const NetAddr& target,
    std::vector<uint8_t> proof) {
    deskhub::AuthResponse response;
    response.proof = std::move(proof);
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildAuthResponse(message, response));
    if (message.empty() || !viewer.SendRecord(target, message)) return false;
    return WaitUntil(
        [&] {
            uint8_t buf[deskhub::kMaxRecordSize];
            NetAddr from;
            const int got = viewer.RecvFrom(buf, sizeof(buf), from);
            if (got <= 0) return false;
            const auto packet = std::span<const uint8_t>(buf, size_t(got));
            const auto header = deskhub::ParseCommonHeader(packet);
            if (!header || header->type != deskhub::MsgType::AuthResult) return false;
            const auto result = deskhub::ParseAuthResult(deskhub::PayloadOf(packet));
            return result && result->code == deskhub::AuthResultCode::BadSignature;
        },
        kSettleMillis);
}

bool SendInvalidProof(deskhubp::SessionTransport& viewer, const NetAddr& target) {
    return SendProof(viewer, target, std::vector<uint8_t>(64, 0));
}

using MessageBuilder = std::function<size_t(std::span<uint8_t>)>;

bool SendBuilt(deskhubp::SessionTransport& viewer, const NetAddr& target, uint64_t stream,
    const MessageBuilder& build) {
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(build(message));
    return !message.empty() && viewer.SendRecordOn(target, stream, message);
}

std::vector<std::pair<uint64_t, MessageBuilder>> PrivilegedRequests() {
    deskhub::TermOpen open;
    open.size = deskhub::TermSize{80, 24};
    open.clientName = "pre-auth";
    deskhub::FileOffer offer;
    offer.batchId = 1;
    offer.files.push_back(deskhub::TransferFile{4, "note.txt"});
    deskhub::Hello hello{};
    hello.clientId = 1;
    hello.maxWidth = 1280;
    hello.maxHeight = 720;
    const uint64_t control = deskhubp::kQuicControlStream;
    return {
        {control, [](std::span<uint8_t> out) { return deskhub::BuildListSources(out); }},
        {control, [hello](std::span<uint8_t> out) { return deskhub::BuildHello(out, hello); }},
        {control, [open](std::span<uint8_t> out) { return deskhub::BuildTermOpen(out, open); }},
        {control, [](std::span<uint8_t> out) { return deskhub::BuildTermList(out); }},
        {deskhubp::kQuicFileStream,
            [offer](std::span<uint8_t> out) { return deskhub::BuildFileOffer(out, offer); }},
        {control,
            [](std::span<uint8_t> out) {
                const deskhub::InputEvent key{deskhub::InputType::Key, 1, 30, 0, 1, 0};
                return deskhub::BuildInputEvents(out, 1, 1, std::span(&key, 1));
            }},
        {control,
            [](std::span<uint8_t> out) {
                const std::array<uint8_t, 4> text{'p', 'a', 's', 't'};
                deskhub::ClipboardChunkView clip;
                clip.revision = 1;
                clip.chunkCount = 1;
                clip.payload = text;
                return deskhub::BuildClipboardChunk(out, 1, clip);
            }},
    };
}

bool SendEveryPrivilegedRequest(deskhubp::SessionTransport& viewer, const NetAddr& target) {
    bool sent = true;
    for (const auto& [stream, build] : PrivilegedRequests())
        sent = SendBuilt(viewer, target, stream, build) && sent;
    return sent;
}

void TestRepeatedBadProofsAreLimited() {
    std::printf("[admission] repeated bad signatures temporarily block one source and key...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    Check(rig.Start(false), "the host and first viewer establish QUIC");
    if (!rig.viewer.Established(rig.target)) return;

    for (size_t i = 0; i < deskhub::kAuthFailureLimit; ++i) {
        deskhubp::SessionTransport next;
        deskhubp::SessionTransport* viewer = &rig.viewer;
        if (i != 0) {
            next.SetRecvTimeout(1);
            if (!next.Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") ||
                !next.WaitEstablished(rig.target, kAuthTimeoutMs)) {
                Check(false, "the next viewer establishes QUIC");
                return;
            }
            viewer = &next;
        }
        Check(BeginWithoutAnswer(*viewer, rig.target, rig.machines.viewer),
            "the authorized key can start authentication before the threshold");
        Check(SendInvalidProof(*viewer, rig.target),
            "a bad signature is rejected without admission");
        viewer->Close();
    }

    deskhubp::SessionTransport blocked;
    blocked.SetRecvTimeout(1);
    Check(blocked.Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") &&
              blocked.WaitEstablished(rig.target, kAuthTimeoutMs),
        "a new QUIC connection can still be made");
    deskhub::AuthStart start;
    start.publicKey = rig.machines.viewer.publicKey;
    start.clientName = "rate-limit-test";
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildAuthStart(message, start));
    Check(!message.empty() && blocked.SendRecord(rig.target, message),
        "the key requests another auth attempt");
    Check(WaitUntil(
              [&] {
                  uint8_t buf[deskhub::kMaxRecordSize];
                  NetAddr from;
                  blocked.RecvFrom(buf, sizeof(buf), from);
                  return !blocked.Established(rig.target);
              },
              kSettleMillis),
        "the host closes the repeated attempt before verifying another signature");
    Check(rig.Peer().Pack() == 0, "none of the invalid proofs received admission");
}

void TestPendingAuthHasACapAndDeadline() {
    std::printf("[admission] unfinished authentication has a cap and a deadline...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    Check(rig.Start(false), "the host and first viewer establish QUIC");
    if (!rig.viewer.Established(rig.target)) return;
    Check(BeginWithoutAnswer(rig.viewer, rig.target, rig.machines.viewer),
        "the first authorized key receives a challenge");

    deskhubp::SessionTransport silent;
    Check(rig.Dial(silent), "a second client establishes QUIC and then says nothing");

    std::vector<std::unique_ptr<deskhubp::SessionTransport>> waiting;
    for (size_t i = 2; i < deskhubp::kMaxPendingAuth; ++i) {
        auto viewer = std::make_unique<deskhubp::SessionTransport>();
        if (!rig.Dial(*viewer) || !BeginWithoutAnswer(*viewer, rig.target, rig.machines.viewer)) {
            Check(false, "each remaining pending slot accepts one challenge");
            return;
        }
        waiting.push_back(std::move(viewer));
    }
    Check(waiting.size() + 2 == deskhubp::kMaxPendingAuth,
        "the configured number of unauthenticated connections are waiting");

    deskhubp::SessionTransport excess;
    excess.SetRecvTimeout(1);
    constexpr uint32_t kExcessWaitMs = 1000;
    Check(excess.Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") &&
              !excess.WaitEstablished(rig.target, kExcessWaitMs),
        "a connection beyond the pending limit is not accepted at all");
    excess.Close();

    Check(WaitUntil(
              [&] {
                  rig.PumpViewer();
                  uint8_t buf[deskhub::kMaxRecordSize];
                  NetAddr from;
                  silent.RecvFrom(buf, sizeof(buf), from);
                  return !rig.viewer.Established(rig.target) && !silent.Established(rig.target);
              },
              int(deskhubp::kAuthResponseTimeoutUs / 1000) + kSettleMillis),
        "a client that never signs, or never even starts, is disconnected at the deadline");
    Check(rig.Peer().Pack() == 0, "no pending client is admitted");

    deskhubp::SessionTransport replacement;
    replacement.SetRecvTimeout(1);
    Check(replacement.Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") &&
              replacement.WaitEstablished(rig.target, kAuthTimeoutMs) &&
              BeginWithoutAnswer(replacement, rig.target, rig.machines.viewer),
        "an expired request frees a slot for a new client");
}

void TestAClosedConnectionTakesItsAdmissionWithIt() {
    std::printf("[admission] a connection that ends leaves nothing admitted behind it...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start();
    Check(started, "the authorized viewer signs the challenge and is let in");
    if (!started) return;

    const NetAddr peer = rig.Peer();
    Check(rig.host.Authenticated(peer), "the host counts that address as admitted");

    rig.viewer.Close();
    Check(WaitUntil([&] { return !rig.host.Authenticated(peer); }, kSettleMillis),
        "once the connection closes, the next one from the same address must prove itself again");
    deskhub::Fingerprint fingerprint{};
    std::string name;
    Check(!rig.host.PeerAuth(peer, fingerprint, name),
        "and the host no longer vouches for who was on it");

    deskhubp::SessionTransport reconnected;
    reconnected.SetRecvTimeout(1);
    Check(reconnected.Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") &&
              reconnected.WaitEstablished(rig.target, kAuthTimeoutMs),
        "the same client can establish a fresh QUIC connection");
    const NetAddr newPeer{0x7F000001u, reconnected.LocalPort()};
    Check(!rig.host.Authenticated(newPeer),
        "a fresh QUIC connection does not inherit the previous admission");

    deskhubp::ClientAuthConfig client;
    client.identity = rig.machines.viewer;
    client.hostFingerprint = rig.machines.host.fingerprint;
    client.clientName = "reconnected-viewer";
    deskhub::AuthResultCode code = deskhub::AuthResultCode::NotPaired;
    Check(reconnected.RunClientAuth(rig.target, std::move(client), kAuthTimeoutMs, code) &&
              WaitUntil([&] { return rig.host.Authenticated(newPeer); }, kSettleMillis),
        "the fresh connection is admitted only after another signature");
}

void TestHostCannotSendBeforeAuthentication() {
    std::printf("[admission] the host cannot send application data before authentication...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the viewer establishes QUIC without authentication");
    if (!started) return;

    const NetAddr peer{0x7F000001u, rig.viewer.LocalPort()};
    const std::array<uint8_t, 1> payload{0x42};
    Check(!rig.host.SendRecord(peer, payload),
        "the host refuses a reliable application record before authentication");
    Check(!rig.host.SendTo(peer, payload.data(), payload.size()),
        "the host refuses an application datagram before authentication");
    Check(!rig.host.Authenticated(peer), "QUIC establishment alone grants no admission");
}

void TestASecondHandshakeOnOneConnectionIsRefused() {
    std::printf("[admission] one connection gets exactly one handshake...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start();
    Check(started, "the viewer is let in");
    if (!started) return;
    const NetAddr peer = rig.Peer();

    deskhub::AuthStart again;
    again.publicKey = rig.machines.impostor.publicKey;
    again.clientName = "someone-else";
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildAuthStart(message, again));
    Check(!message.empty() && rig.viewer.SendRecord(rig.target, message),
        "the admitted viewer starts over, claiming a key it never proved");

    Check(WaitUntil([&] { return !rig.host.Authenticated(peer); }, kSettleMillis),
        "the host withdraws the admission instead of carrying it over");
    deskhub::Fingerprint fingerprint{};
    std::string name;
    Check(!rig.host.PeerAuth(peer, fingerprint, name) ||
              !(fingerprint == rig.machines.impostor.fingerprint),
        "and never reports the unproven key as the one on that connection");
    Check(WaitUntil(
              [&] {
                  rig.PumpViewer();
                  return !rig.viewer.Established(rig.target);
              },
              kSettleMillis),
        "the connection itself is closed");
}

void TestAnOldAuthStartIsRefused() {
    std::printf("[admission] an old auth start cannot enter the key-only handshake...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the transport connects before authentication");
    if (!started) return;

    deskhub::AuthStart start;
    start.publicKey = rig.machines.viewer.publicKey;
    start.clientName = "old-client";
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildAuthStart(message, start));
    Check(!message.empty(), "the new auth start can be encoded");
    if (message.empty()) return;
    message.pop_back();
    Check(rig.viewer.SendRecord(rig.target, message), "the old layout reaches the host");
    bool versionMismatch = false;
    Check(WaitUntil(
              [&] {
                  uint8_t buf[deskhub::kMaxRecordSize];
                  NetAddr from;
                  const int got = rig.viewer.RecvFrom(buf, sizeof(buf), from);
                  if (got <= 0) return false;
                  const std::span<const uint8_t> reply(buf, size_t(got));
                  const auto header = deskhub::ParseCommonHeader(reply);
                  if (!header || header->type != deskhub::MsgType::AuthResult) return false;
                  const auto result = deskhub::ParseAuthResult(deskhub::PayloadOf(reply));
                  versionMismatch = result &&
                                    result->code == deskhub::AuthResultCode::VersionMismatch;
                  return versionMismatch;
              },
              kSettleMillis),
        "the host returns an explicit auth version error");
    Check(versionMismatch, "the old layout never enters the key handshake");
    Check(WaitUntil(
              [&] {
                  rig.PumpViewer();
                  return !rig.viewer.Established(rig.target);
              },
              kSettleMillis),
        "the host closes the incompatible connection");
    Check(rig.Peer().Pack() == 0, "the old client is never admitted");
}

void TestForgettingADeviceClosesItsLiveConnection() {
    std::printf("[admission] forgetting a machine on the Devices page cuts it off now...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start();
    Check(started, "the viewer is let in and paired");
    if (!started) return;
    const NetAddr peer = rig.Peer();
    Check(deskhubp::IsClientKeyAuthorized(rig.machines.viewer.publicKey),
        "the authorized key remains on the list");

    GrantClientKey(rig.machines.impostor);
    deskhubp::ForgetAuthorizedClient(rig.machines.impostor.fingerprint);
    SleepUs(kQuietMillis * 1000);
    Check(rig.host.Authenticated(peer), "forgetting some other machine leaves this one alone");

    Check(deskhubp::ForgetAuthorizedClient(rig.machines.viewer.fingerprint),
        "the viewer is forgotten");
    Check(WaitUntil([&] { return !rig.host.Authenticated(peer); }, kSettleMillis),
        "and its live connection loses its admission without waiting for it to hang up");
    Check(WaitUntil(
              [&] {
                  rig.PumpViewer();
                  return !rig.viewer.Established(rig.target);
              },
              kSettleMillis),
        "the connection is closed");
}

void TestExternalKeyRevocationClosesItsLiveConnection() {
    std::printf("[admission] an external allowlist edit revokes a live connection...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start();
    Check(started, "the viewer is admitted before the external edit");
    if (!started) return;
    const NetAddr peer = rig.Peer();
    const uint64_t generation = deskhubp::AuthorizedKeysGeneration();
    Check(deskhubp::WriteAppDataFileAtomic(deskhubp::kAuthorizedKeysFileName, ""),
        "another process can replace the authorized key file");
    Check(deskhubp::AuthorizedKeysGeneration() == generation,
        "an external edit does not use the in-process generation counter");
    Check(WaitUntil([&] { return !rig.host.Authenticated(peer); }, kSettleMillis),
        "the host still notices the edit and withdraws admission");
}

NetAddr LoopbackPeer(const deskhubp::SessionTransport& client) {
    return NetAddr{0x7F000001u, client.LocalPort()};
}

bool ClosedByHost(deskhubp::SessionTransport& client, const NetAddr& target) {
    return WaitUntil(
        [&] {
            uint8_t buf[deskhub::kMaxRecordSize];
            NetAddr from;
            client.RecvFrom(buf, sizeof(buf), from);
            return !client.Established(target);
        },
        kSettleMillis);
}

void TestNothingReachesTheHostBeforeAuthentication() {
    std::printf("[admission] no source list, shell, file, input or clipboard before auth...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the viewer establishes QUIC without authenticating");
    if (!started) return;
    const size_t requests = PrivilegedRequests().size();

    Check(SendEveryPrivilegedRequest(rig.viewer, rig.target),
        "the viewer sends every privileged request over the encrypted connection");
    SleepUs(kQuietMillis * 1000);
    Check(rig.delivered.load() == 0,
        "the host application receives none of them before authentication");

    deskhubp::SessionTransport refused;
    Check(rig.Dial(refused), "a client with an unlisted key connects");
    Check(!rig.SignIn(refused, rig.machines.impostor), "and is refused");
    SendEveryPrivilegedRequest(refused, rig.target);
    SleepUs(kQuietMillis * 1000);
    Check(rig.delivered.load() == 0,
        "a refused key cannot reach any of them either");

    Check(rig.SignIn(rig.viewer, rig.machines.viewer),
        "the first viewer then signs in on the same connection");
    Check(SendEveryPrivilegedRequest(rig.viewer, rig.target), "and repeats the requests");
    Check(WaitUntil([&] { return size_t(rig.delivered.load()) >= requests; }, kSettleMillis),
        "only now does the host application receive them");
}

void TestAnAnswerBeforeAStartIsIgnored() {
    std::printf("[admission] auth messages out of order admit nobody...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the viewer establishes QUIC without authenticating");
    if (!started) return;
    const NetAddr peer = LoopbackPeer(rig.viewer);

    deskhub::AuthResponse response;
    response.proof.assign(64, 0x11);
    deskhub::AuthResult accepted;
    accepted.code = deskhub::AuthResultCode::Accepted;
    deskhub::AuthChallenge challenge;
    challenge.mode = deskhub::AuthMode::Signature;
    const uint64_t control = deskhubp::kQuicControlStream;
    Check(SendBuilt(rig.viewer, rig.target, control,
              [&](std::span<uint8_t> out) { return deskhub::BuildAuthResponse(out, response); }),
        "a response is sent before any start");
    Check(SendBuilt(rig.viewer, rig.target, control,
              [&](std::span<uint8_t> out) { return deskhub::BuildAuthResult(out, accepted); }),
        "a client claims an acceptance of its own");
    Check(SendBuilt(rig.viewer, rig.target, control,
              [&](std::span<uint8_t> out) { return deskhub::BuildAuthChallenge(out, challenge); }),
        "and sends the host a challenge");
    SleepUs(kQuietMillis * 1000);
    Check(!rig.host.Authenticated(peer) && rig.Peer().Pack() == 0,
        "none of the out-of-order messages admits the connection");
    Check(rig.delivered.load() == 0, "and none reaches the host application");
    Check(rig.SignIn(rig.viewer, rig.machines.viewer) &&
              WaitUntil([&] { return rig.host.Authenticated(peer); }, kSettleMillis),
        "a proper handshake afterwards is still what admits it");
}

void TestAProofForAnotherConnectionIsRefused() {
    std::printf("[admission] a valid signature made for another connection is refused...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the viewer establishes QUIC without authenticating");
    if (!started) return;
    Check(BeginWithoutAnswer(rig.viewer, rig.target, rig.machines.viewer),
        "the authorized key receives a challenge");

    deskhub::AuthSessionId elsewhere{};
    elsewhere.fill(0x41);
    const auto transcript = deskhub::AuthTranscript(deskhub::AuthRole::Client, elsewhere,
        rig.machines.viewer.publicKey, rig.machines.host.fingerprint);
    Check(SendProof(rig.viewer, rig.target,
              deskhubp::SignWithIdentity(rig.machines.viewer, transcript)),
        "the real key's signature bound to another TLS session is rejected as a bad signature");
    Check(rig.Peer().Pack() == 0 && !rig.host.Authenticated(LoopbackPeer(rig.viewer)),
        "and the connection is not admitted");
}

void TestAKeySwapMidHandshakeClosesTheConnection() {
    std::printf("[admission] a second key offered mid-handshake ends the connection...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the viewer establishes QUIC without authenticating");
    if (!started) return;
    Check(GrantClientKey(rig.machines.impostor), "a second key is authorized too");
    Check(BeginWithoutAnswer(rig.viewer, rig.target, rig.machines.viewer),
        "the first key receives a challenge");

    deskhub::AuthStart swap;
    swap.publicKey = rig.machines.impostor.publicKey;
    swap.clientName = "swapped";
    Check(SendBuilt(rig.viewer, rig.target, deskhubp::kQuicControlStream,
              [&](std::span<uint8_t> out) { return deskhub::BuildAuthStart(out, swap); }),
        "the client offers a different key before answering");
    Check(ClosedByHost(rig.viewer, rig.target), "the host closes the connection");
    Check(rig.Peer().Pack() == 0, "and admits neither key");
}

void TestRevokingAKeyClosesEveryConnectionItHolds() {
    std::printf("[admission] revoking a key cuts every connection it holds, and only those...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start();
    Check(started, "the first connection is admitted");
    if (!started) return;

    deskhubp::SessionTransport second;
    Check(rig.Dial(second) && rig.SignIn(second, rig.machines.viewer),
        "the same key opens and authenticates a second connection");
    deskhubp::SessionTransport bystander;
    Check(GrantClientKey(rig.machines.impostor) && rig.Dial(bystander) &&
              rig.SignIn(bystander, rig.machines.impostor),
        "another authorized key holds a third connection");
    const NetAddr first = LoopbackPeer(rig.viewer);
    const NetAddr other = LoopbackPeer(second);
    const NetAddr third = LoopbackPeer(bystander);
    Check(WaitUntil(
              [&] {
                  return rig.host.Authenticated(first) && rig.host.Authenticated(other) &&
                         rig.host.Authenticated(third);
              },
              kSettleMillis),
        "all three are admitted");

    Check(deskhubp::ForgetAuthorizedClient(rig.machines.viewer.fingerprint),
        "the shared key is revoked");
    Check(WaitUntil([&] { return !rig.host.Authenticated(first) && !rig.host.Authenticated(other); },
              kSettleMillis),
        "both of its connections lose admission");
    Check(ClosedByHost(rig.viewer, rig.target) && ClosedByHost(second, rig.target),
        "and both are closed");
    Check(rig.host.Authenticated(third), "the other key's connection is untouched");
}

}

void TestAnUnknownKeyWaitsForApprovalAndGetsInWhenApproved() {
    std::printf("[admission] an unknown key leaves a request and is admitted once approved...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the host and a viewer establish QUIC");
    if (!started) return;

    deskhub::AuthResultCode code = deskhub::AuthResultCode::Accepted;
    Check(!rig.SignInWithCode(rig.viewer, rig.machines.impostor, code) &&
              code == deskhub::AuthResultCode::AwaitingApproval,
        "an unlisted key is told to wait for approval");
    const auto requests = deskhubp::ListAccessRequests();
    Check(requests && requests->size() == 1 &&
              requests->front().fingerprint == rig.machines.impostor.fingerprint &&
              requests->front().label == "admission-viewer" &&
              !requests->front().address.empty(),
        "the host recorded who asked, under its name and address");
    Check(!rig.host.Authenticated(LoopbackPeer(rig.viewer)), "and admitted nothing");
    Check(WaitUntil(
              [&] {
                  rig.PumpViewer();
                  return !rig.viewer.Established(rig.target);
              },
              int(deskhubp::kRefusalLingerUs / 1000) + kSettleMillis),
        "the host closes the waiting connection itself once its reply has had time to land");

    Check(deskhubp::ApproveAccessRequest(rig.machines.impostor.fingerprint),
        "the owner approves the request");
    deskhubp::SessionTransport again;
    Check(rig.Dial(again) && rig.SignIn(again, rig.machines.impostor),
        "the next connection from that key is admitted");
    Check(WaitUntil([&] { return rig.host.Authenticated(LoopbackPeer(again)); }, kSettleMillis),
        "and the host counts it as admitted");
    Check(deskhubp::ListAccessRequests() && deskhubp::ListAccessRequests()->empty(),
        "the request is gone once approved");
}

void TestAPairingTokenAdmitsAnUnknownKeyOnce() {
    std::printf("[admission] a live pairing token admits an unknown key at once, and only once...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start(false);
    Check(started, "the host and a viewer establish QUIC");
    if (!started) return;

    const auto token = deskhubp::IssuePairingToken(deskhub::kPairingTokenTtlSeconds);
    Check(token.has_value(), "the host issued a pairing token");
    if (!token) return;
    std::vector<uint8_t> wrong(token->begin(), token->end());
    wrong[0] ^= 0xFF;
    deskhub::AuthResultCode code = deskhub::AuthResultCode::Accepted;
    Check(!rig.SignInWithCode(rig.viewer, rig.machines.impostor, code, wrong) &&
              code == deskhub::AuthResultCode::AwaitingApproval,
        "a wrong token earns no admission, only a request to approve");
    Check(!deskhubp::IsClientKeyAuthorized(rig.machines.impostor.publicKey),
        "and the key is not allowed in");

    deskhubp::SessionTransport forger;
    deskhubp::HostIdentity forged = rig.machines.viewer;
    forged.publicKey = rig.machines.impostor.publicKey;
    Check(rig.Dial(forger) &&
              !rig.SignInWithCode(forger, forged, code,
                  std::vector<uint8_t>(token->begin(), token->end())) &&
              code == deskhub::AuthResultCode::BadSignature,
        "the right token under a key the sender cannot sign for is refused");
    Check(!deskhubp::IsClientKeyAuthorized(rig.machines.impostor.publicKey),
        "and it spends nothing and saves no key");

    deskhubp::SessionTransport invited;
    Check(rig.Dial(invited) &&
              rig.SignIn(invited, rig.machines.impostor,
                  std::vector<uint8_t>(token->begin(), token->end())),
        "the right token admits the key at once");
    Check(WaitUntil([&] { return rig.host.Authenticated(LoopbackPeer(invited)); }, kSettleMillis),
        "the host counts the invited connection as admitted");
    Check(deskhubp::IsClientKeyAuthorized(rig.machines.impostor.publicKey),
        "and the key is now on the allowed list");
    const auto allowed = deskhubp::ListAuthorizedClients();
    Check(allowed && allowed->size() == 2, "beside the viewer that was allowed before");

    deskhubp::SessionTransport third;
    Check(rig.Dial(third) && rig.SignIn(third, rig.machines.impostor),
        "the invited key stays allowed without the token");
    Check(!deskhubp::RedeemPairingToken(*token), "the token itself is spent");
}

void RunTransportAdmissionTests() {
    TestAnUnknownKeyWaitsForApprovalAndGetsInWhenApproved();
    TestAPairingTokenAdmitsAnUnknownKeyOnce();
    TestRepeatedBadProofsAreLimited();
    TestPendingAuthHasACapAndDeadline();
    TestAClosedConnectionTakesItsAdmissionWithIt();
    TestHostCannotSendBeforeAuthentication();
    TestASecondHandshakeOnOneConnectionIsRefused();
    TestAnOldAuthStartIsRefused();
    TestForgettingADeviceClosesItsLiveConnection();
    TestExternalKeyRevocationClosesItsLiveConnection();
    TestNothingReachesTheHostBeforeAuthentication();
    TestAnAnswerBeforeAStartIsIgnored();
    TestAProofForAnotherConnectionIsRefused();
    TestAKeySwapMidHandshakeClosesTheConnection();
    TestRevokingAKeyClosesEveryConnectionItHolds();
}

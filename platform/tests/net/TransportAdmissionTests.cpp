#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/auth/AuthNegotiation.h"
#include "deskhubp/net/SessionTransport.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"

#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint16_t kAdmissionPort = 47867;
constexpr uint32_t kAuthTimeoutMs = 4000;
constexpr int kSettleMillis = 5000;
constexpr int kQuietMillis = 300;

struct SavedState {
    std::string cert{};
    std::string key{};
    std::string paired{};

    SavedState() {
        cert = deskhubp::ReadAppDataFile(deskhubp::kHostCertFileName);
        key = deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName);
        paired = deskhubp::ReadAppDataFile(deskhubp::kPairedDevicesFileName);
        deskhubp::ForgetAllPairedDevices();
    }

    ~SavedState() {
        if (!cert.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostCertFileName, cert);
        if (!key.empty()) deskhubp::WriteAppDataFile(deskhubp::kHostKeyFileName, key);
        if (paired.empty())
            deskhubp::ForgetAllPairedDevices();
        else
            deskhubp::WriteAppDataFile(deskhubp::kPairedDevicesFileName, paired);
    }
};

struct Machines {
    deskhubp::HostIdentity viewer{};
    deskhubp::HostIdentity impostor{};
    deskhubp::HostIdentity host{};

    bool Make() {
        ForgetHostIdentity();
        viewer = deskhubp::LoadOrCreateHostIdentity("admission-viewer");
        ForgetHostIdentity();
        impostor = deskhubp::LoadOrCreateHostIdentity("admission-impostor");
        ForgetHostIdentity();
        host = deskhubp::LoadOrCreateHostIdentity("admission-host");
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
    std::atomic<bool> stop{false};
    std::thread pump{};

    ~AdmissionRig() {
        StopHost();
        viewer.Close();
    }

    bool Start(bool authenticate = true) {
        if (!machines.Make()) return false;
        if (!deskhubp::RememberPairedDevice(machines.viewer.fingerprint,
                "admission-viewer", 500))
            return false;
        host.SetRecvTimeout(1);
        viewer.SetRecvTimeout(1);

        deskhubp::QuicSettings settings;
        settings.certPemPath = machines.host.certPath;
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
                host.RecvFrom(buf, sizeof(buf), from);
            }
        });

        if (!viewer.Connect(deskhubp::QuicSettings{}, target, "admission-host")) return false;
        if (!viewer.WaitEstablished(target, kAuthTimeoutMs)) return false;
        if (!authenticate) return true;

        deskhubp::ClientAuthConfig client;
        client.identity = machines.viewer;
        client.hostFingerprint = machines.host.fingerprint;
        client.clientName = "admission-viewer";
        deskhub::AuthResultCode code = deskhub::AuthResultCode::NotPaired;
        if (!viewer.RunClientAuth(target, std::move(client), kAuthTimeoutMs, code))
            return false;
        return WaitUntil([this] { return Peer().Pack() != 0; }, kSettleMillis);
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
    start.publicKey = deskhubp::IdentityPublicKey(identity);
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

bool SendInvalidProof(deskhubp::SessionTransport& viewer, const NetAddr& target) {
    deskhub::AuthResponse response;
    response.proof.assign(64, 0);
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
    start.publicKey = deskhubp::IdentityPublicKey(rig.machines.viewer);
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

    std::vector<std::unique_ptr<deskhubp::SessionTransport>> waiting;
    for (size_t i = 1; i < deskhubp::kMaxPendingAuth; ++i) {
        auto viewer = std::make_unique<deskhubp::SessionTransport>();
        viewer->SetRecvTimeout(1);
        if (!viewer->Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") ||
            !viewer->WaitEstablished(rig.target, kAuthTimeoutMs) ||
            !BeginWithoutAnswer(*viewer, rig.target, rig.machines.viewer)) {
            Check(false, "each remaining pending slot accepts one challenge");
            return;
        }
        waiting.push_back(std::move(viewer));
    }
    Check(waiting.size() + 1 == deskhubp::kMaxPendingAuth,
        "the configured number of auth requests are waiting");

    deskhubp::SessionTransport excess;
    excess.SetRecvTimeout(1);
    Check(excess.Connect(deskhubp::QuicSettings{}, rig.target, "admission-host") &&
              excess.WaitEstablished(rig.target, kAuthTimeoutMs),
        "an extra client establishes QUIC");
    deskhub::AuthStart start;
    start.publicKey = deskhubp::IdentityPublicKey(rig.machines.viewer);
    start.clientName = "excess-auth-test";
    std::vector<uint8_t> message(deskhub::kMaxRecordSize);
    message.resize(deskhub::BuildAuthStart(message, start));
    Check(!message.empty() && excess.SendRecord(rig.target, message),
        "the extra client requests authentication");
    Check(WaitUntil(
              [&] {
                  uint8_t buf[deskhub::kMaxRecordSize];
                  NetAddr from;
                  excess.RecvFrom(buf, sizeof(buf), from);
                  return !excess.Established(rig.target);
              },
              kSettleMillis),
        "the host closes the request beyond the pending limit");

    Check(WaitUntil(
              [&] {
                  rig.PumpViewer();
                  return !rig.viewer.Established(rig.target);
              },
              int(deskhubp::kAuthResponseTimeoutUs / 1000) + kSettleMillis),
        "a client that never signs is disconnected at the auth deadline");
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
    again.publicKey = deskhubp::IdentityPublicKey(rig.machines.impostor);
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
    start.publicKey = deskhubp::IdentityPublicKey(rig.machines.viewer);
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
    Check(deskhubp::CheckPairedDevice(rig.machines.viewer.fingerprint) ==
              deskhub::PairVerdict::Paired,
        "the authorized key remains on the paired list");

    deskhubp::RememberPairedDevice(rig.machines.impostor.fingerprint, "bystander", 1);
    deskhubp::ForgetPairedDevice(rig.machines.impostor.fingerprint);
    SleepUs(kQuietMillis * 1000);
    Check(rig.host.Authenticated(peer), "forgetting some other machine leaves this one alone");

    Check(deskhubp::ForgetPairedDevice(rig.machines.viewer.fingerprint),
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
    const uint64_t generation = deskhubp::PairedDevicesGeneration();
    Check(deskhubp::WriteAppDataFileAtomic(deskhubp::kPairedDevicesFileName, ""),
        "another process can replace the authorized key file");
    Check(deskhubp::PairedDevicesGeneration() == generation,
        "an external edit does not use the in-process generation counter");
    Check(WaitUntil([&] { return !rig.host.Authenticated(peer); }, kSettleMillis),
        "the host still notices the edit and withdraws admission");
}

}

void RunTransportAdmissionTests() {
    TestRepeatedBadProofsAreLimited();
    TestPendingAuthHasACapAndDeadline();
    TestAClosedConnectionTakesItsAdmissionWithIt();
    TestHostCannotSendBeforeAuthentication();
    TestASecondHandshakeOnOneConnectionIsRefused();
    TestAnOldAuthStartIsRefused();
    TestForgettingADeviceClosesItsLiveConnection();
    TestExternalKeyRevocationClosesItsLiveConnection();
}

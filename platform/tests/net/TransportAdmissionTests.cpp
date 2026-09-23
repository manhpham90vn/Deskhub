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

#include <atomic>
#include <cstdio>
#include <functional>
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

    bool Start() {
        if (!machines.Make()) return false;
        host.SetRecvTimeout(1);
        viewer.SetRecvTimeout(1);

        deskhubp::QuicSettings settings;
        settings.certPemPath = machines.host.certPath;
        settings.keyPemPath = machines.host.keyPath;
        if (!host.Listen(settings, kAdmissionPort, "127.0.0.1")) return false;

        deskhubp::HostAuthConfig auth;
        auth.identity = machines.host;
        auth.SetPasscode(deskhubp::LoadOrCreateAuthSalt(), kTestPasscode);
        auth.allowNewPairings = true;
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

        deskhubp::ClientAuthConfig client;
        client.identity = machines.viewer;
        client.passcode = kTestPasscode;
        client.hostFingerprint = machines.host.fingerprint;
        client.clientName = "admission-viewer";
        deskhub::AuthResultCode code = deskhub::AuthResultCode::NotPaired;
        bool hostProved = false;
        if (!viewer.RunClientAuth(target, std::move(client), kAuthTimeoutMs, code, hostProved))
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

void TestAClosedConnectionTakesItsAdmissionWithIt() {
    std::printf("[admission] a connection that ends leaves nothing admitted behind it...\n");
    if (Skipped("admission")) return;
    const SavedState guard;
    AdmissionRig rig;
    const bool started = rig.Start();
    Check(started, "the viewer proves the passcode and is let in");
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
        "the passcode left it on the paired list");

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

}

void RunTransportAdmissionTests() {
    TestAClosedConnectionTakesItsAdmissionWithIt();
    TestASecondHandshakeOnOneConnectionIsRefused();
    TestForgettingADeviceClosesItsLiveConnection();
}

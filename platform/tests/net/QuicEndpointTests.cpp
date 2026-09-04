#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/protocol/Wire.h"
#include "deskhubp/net/QuicEndpoint.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr uint16_t kTestPort = 47791;
constexpr int kMaxRounds = 400;

struct Peer {
    deskhubp::QuicEndpoint endpoint{};
    deskhubp::QuicConnId conn = 0;
    std::string stream{};
    std::vector<std::vector<uint8_t>> datagrams{};
    std::vector<std::vector<uint8_t>> foreign{};
    std::vector<uint64_t> broken{};
    bool connected = false;
    bool closed = false;
};

deskhubp::QuicCallbacks HooksFor(Peer& peer) {
    deskhubp::QuicCallbacks hooks;
    hooks.onConnected = [&peer](deskhubp::QuicConnId id, const NetAddr&) {
        peer.conn = id;
        peer.connected = true;
    };
    hooks.onClosed = [&peer](deskhubp::QuicConnId, const NetAddr&) { peer.closed = true; };
    hooks.onStream = [&peer](deskhubp::QuicConnId, uint64_t, std::span<const uint8_t> bytes,
                         bool) {
        peer.stream.append(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };
    hooks.onDatagram = [&peer](deskhubp::QuicConnId, std::span<const uint8_t> bytes) {
        peer.datagrams.emplace_back(bytes.begin(), bytes.end());
    };
    hooks.onForeignDatagram = [&peer](const NetAddr&, std::span<const uint8_t> bytes) {
        peer.foreign.emplace_back(bytes.begin(), bytes.end());
    };
    hooks.onStreamBroken = [&peer](deskhubp::QuicConnId, uint64_t stream) {
        peer.broken.push_back(stream);
    };
    return hooks;
}

void Pump(Peer& a, Peer& b, int rounds) {
    for (int i = 0; i < rounds; ++i) {
        a.endpoint.Poll(NowUs(), 1);
        b.endpoint.Poll(NowUs(), 1);
    }
}

void TestHandshakeStreamAndDatagram() {
    std::printf("[quic] two endpoints shake hands, then carry a stream and a datagram...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[quic] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    Check(identity.Valid(), "the host has an identity to present");
    if (!identity.Valid()) return;

    Peer server;
    Peer client;

    deskhubp::QuicSettings serverSettings;
    serverSettings.certPemPath = identity.certPath;
    serverSettings.keyPemPath = identity.keyPath;
    const bool listening =
        server.endpoint.Listen(serverSettings, "127.0.0.1", kTestPort, HooksFor(server));
    Check(listening, "the host binds its own UDP port");
    if (!listening) return;
    Check(server.endpoint.IsServer() && server.endpoint.IsOpen(), "and is open as a server");

    const NetAddr target{0x7F000001u, kTestPort};
    deskhubp::QuicSettings clientSettings;
    const bool dialed =
        client.endpoint.Connect(clientSettings, target, "deskhub-test", HooksFor(client));
    Check(dialed, "the client starts a connection to it");
    if (!dialed) return;

    for (int i = 0; i < kMaxRounds && !(server.connected && client.connected); ++i)
        Pump(client, server, 1);
    Check(client.connected && server.connected, "the TLS handshake completes over real sockets");
    if (!client.connected || !server.connected) return;

    Check(server.endpoint.ConnectionCount() == 1, "the host sees exactly one client");
    Check(client.endpoint.Established(client.conn), "the client agrees it is established");

    const auto fingerprint = client.endpoint.PeerFingerprint(client.conn);
    Check(fingerprint.has_value(), "the client can see the host's certificate");
    Check(fingerprint && *fingerprint == identity.fingerprint,
        "and its fingerprint is the one the host published - this is what TOFU compares");

    const std::string payload = "terminal: echo hello";
    Check(client.endpoint.SendStream(client.conn, deskhubp::kQuicFirstTerminalStream,
              std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(payload.data()),
                  payload.size())),
        "a stream write is accepted");
    for (int i = 0; i < kMaxRounds && server.stream.size() < payload.size(); ++i)
        Pump(client, server, 1);
    Check(server.stream == payload, "and every byte arrives in order on the far side");

    const std::vector<uint8_t> frame(deskhub::kMaxDatagram, 0x5A);
    Check(client.endpoint.MaxDatagramSize(client.conn) >= deskhub::kMaxDatagram,
        "a QUIC datagram is big enough for a video packet");
    Check(client.endpoint.SendDatagram(client.conn, frame), "a datagram write is accepted");
    for (int i = 0; i < kMaxRounds && server.datagrams.empty(); ++i) Pump(client, server, 1);
    Check(server.datagrams.size() == 1 && server.datagrams[0].size() == frame.size(),
        "and it arrives whole on the video path");

    uint8_t beacon[deskhub::kMaxDatagram];
    const size_t beaconSize = deskhub::BuildListSources(beacon);
    Check(client.endpoint.SendRaw(target, std::span<const uint8_t>(beacon, beaconSize)),
        "a plain beacon packet can share the port");
    for (int i = 0; i < kMaxRounds && server.foreign.empty(); ++i) Pump(client, server, 1);
    Check(server.foreign.size() == 1 && server.foreign[0].size() == beaconSize,
        "and reaches the beacon handler instead of the QUIC connection");
    Check(server.stream == payload, "without disturbing the stream");

    client.endpoint.CloseConnection(client.conn, 0, "done");
    Pump(client, server, 20);

    client.endpoint.Close();
    server.endpoint.Close();
    Check(!client.endpoint.IsOpen() && !server.endpoint.IsOpen(), "both endpoints close cleanly");
}

void TestARefusedStreamIsResetNotTruncated() {
    std::printf("[quic] a stream that stops taking bytes is reset, and its owner is told...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[quic] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    Peer server;
    Peer client;

    deskhubp::QuicSettings serverSettings;
    serverSettings.certPemPath = identity.certPath;
    serverSettings.keyPemPath = identity.keyPath;
    if (!server.endpoint.Listen(serverSettings, "127.0.0.1", uint16_t(kTestPort + 1),
            HooksFor(server)))
        return;

    const NetAddr target{0x7F000001u, uint16_t(kTestPort + 1)};
    if (!client.endpoint.Connect(deskhubp::QuicSettings{}, target, "deskhub-test",
            HooksFor(client)))
        return;

    for (int i = 0; i < kMaxRounds && !(server.connected && client.connected); ++i)
        Pump(client, server, 1);
    if (!client.connected || !server.connected) return;

    const std::string payload = "the first and only whole record";
    const auto asBytes = [](const std::string& text) {
        return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(text.data()),
            text.size());
    };

    Check(client.endpoint.SendStream(client.conn, deskhubp::kQuicFileStream, asBytes(payload),
              true),
        "the file stream carries a record and is finished");
    for (int i = 0; i < kMaxRounds && server.stream.size() < payload.size(); ++i)
        Pump(client, server, 1);
    Check(server.stream == payload, "which arrives whole");

    const std::string orphan = "bytes the stream can no longer take";
    Check(client.endpoint.SendStream(client.conn, deskhubp::kQuicFileStream, asBytes(orphan)),
        "a later write is queued before anyone knows the stream is finished");
    Pump(client, server, 20);

    Check(client.broken.size() == 1 && client.broken[0] == deskhubp::kQuicFileStream,
        "the owner is told the file stream broke, so the transfer fails instead of hanging");
    Check(server.stream == payload,
        "and not one orphan byte reached the peer, so its record framer never sees half a "
        "record and the connection survives");
    Check(!client.endpoint.SendStream(client.conn, deskhubp::kQuicFileStream, asBytes(orphan)),
        "further writes to the broken stream are refused rather than quietly buffered");

    client.endpoint.Close();
    server.endpoint.Close();
}

void TestAFloodedStreamIsDrainedInBoundedSlices() {
    std::printf("[quic] a flooded stream is drained in bounded slices, never in one gulp...\n");
    if (!deskhubp::QuicAvailable()) {
        std::printf("[quic] skipped: this build has no QUIC library\n");
        return;
    }

    const SavedIdentity guard;
    deskhubp::ForgetHostIdentity();
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity("deskhub-test");
    if (!identity.Valid()) return;

    Peer server;
    Peer client;

    deskhubp::QuicSettings serverSettings;
    serverSettings.certPemPath = identity.certPath;
    serverSettings.keyPemPath = identity.keyPath;
    if (!server.endpoint.Listen(serverSettings, "127.0.0.1", uint16_t(kTestPort + 2),
            HooksFor(server)))
        return;

    const NetAddr target{0x7F000001u, uint16_t(kTestPort + 2)};
    if (!client.endpoint.Connect(deskhubp::QuicSettings{}, target, "deskhub-test",
            HooksFor(client)))
        return;

    for (int i = 0; i < kMaxRounds && !(server.connected && client.connected); ++i)
        Pump(client, server, 1);
    if (!client.connected || !server.connected) return;

    constexpr size_t kFloodBytes = 512u << 10;
    constexpr size_t kSliceBudget = 64u << 10;
    std::string flood(kFloodBytes, '\0');
    for (size_t i = 0; i < flood.size(); ++i) flood[i] = char('a' + i % 23);
    Check(client.endpoint.SendStream(client.conn, deskhubp::kQuicFirstTerminalStream,
              std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(flood.data()),
                  flood.size())),
        "half a megabyte is accepted for the terminal stream");

    size_t biggestSlice = 0;
    size_t passesWithData = 0;
    for (int i = 0; i < kMaxRounds * 10 && server.stream.size() < flood.size(); ++i) {
        for (int burst = 0; burst < 8; ++burst) client.endpoint.Poll(NowUs(), 1);
        const size_t before = server.stream.size();
        server.endpoint.Poll(NowUs(), 1);
        const size_t slice = server.stream.size() - before;
        if (slice > biggestSlice) biggestSlice = slice;
        if (slice > 0) ++passesWithData;
    }

    Check(server.stream.size() == flood.size(), "every byte of the flood arrives");
    Check(server.stream == flood, "in order and uncorrupted");
    Check(biggestSlice <= kSliceBudget,
        "no single poll pass ever hands the consumer more than the 64 KiB budget, so acks "
        "and keepalives always get a turn between slices");
    Check(passesWithData >= flood.size() / kSliceBudget,
        "which forces the flood to be spread across many poll passes");

    client.endpoint.Close();
    server.endpoint.Close();
}

void TestUnstartedEndpointIsHarmless() {
    std::printf("[quic] an endpoint that never started refuses everything quietly...\n");
    deskhubp::QuicEndpoint idle;
    Check(!idle.IsOpen() && idle.ConnectionCount() == 0, "it holds no connections");
    Check(idle.FirstConnection() == 0 && idle.Connections().empty(), "and names none");
    const uint8_t byte = 1;
    Check(!idle.SendStream(0, 0, std::span<const uint8_t>(&byte, 1)), "a stream write fails");
    Check(!idle.SendDatagram(0, std::span<const uint8_t>(&byte, 1)), "a datagram write fails");
    Check(!idle.Established(0) && idle.MaxDatagramSize(0) == 0, "nothing is established");
    Check(!idle.PeerFingerprint(0).has_value(), "and there is no peer to fingerprint");
    Check(!idle.WaitReadable(1), "and there is nothing to wait on");
    idle.CloseConnection(0);
    idle.Poll(NowUs(), 1);
    idle.Close();
    Check(true, "polling and closing it does nothing at all");
}

}

void RunQuicEndpointTests() {
    TestHandshakeStreamAndDatagram();
    TestARefusedStreamIsResetNotTruncated();
    TestAFloodedStreamIsDrainedInBoundedSlices();
    TestUnstartedEndpointIsHarmless();
}

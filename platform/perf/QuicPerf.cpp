#include "PerfHarness.h"

#include "deskhubp/net/QuicEndpoint.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <vector>

using namespace deskhubp;
using namespace deskhub::perf;

namespace {

constexpr uint16_t kPerfPort = 47897;
constexpr int kHandshakeRounds = 400;
constexpr size_t kPumpRoundsCap = 200'000;
constexpr size_t kTerminalRecordBytes = 512;
constexpr size_t kThroughputChunkBytes = 16 * 1024;
constexpr size_t kThroughputChunks = 4;
constexpr size_t kDatagramsPerRun = 16;
constexpr size_t kDatagramBytes = 1200;
constexpr size_t kBytesPerKilobyte = 1024;

struct Sink {
    QuicConnId conn = 0;
    bool connected = false;
    uint64_t streamBytes = 0;
    uint64_t datagrams = 0;
};

QuicCallbacks HooksFor(Sink& sink) {
    QuicCallbacks hooks;
    hooks.onConnected = [&sink](QuicConnId id, const NetAddr&) {
        sink.conn = id;
        sink.connected = true;
    };
    hooks.onStream = [&sink](QuicConnId, uint64_t, std::span<const uint8_t> bytes, bool) {
        sink.streamBytes += bytes.size();
        Consume(bytes.size());
    };
    hooks.onDatagram = [&sink](QuicConnId, std::span<const uint8_t> bytes) {
        ++sink.datagrams;
        Consume(bytes.size());
    };
    return hooks;
}

struct LoopbackPair {
    Sink serverSink{};
    Sink clientSink{};
    QuicEndpoint server{};
    QuicEndpoint client{};

    bool Start(const HostIdentity& identity, uint16_t port) {
        QuicSettings serverSettings;
        serverSettings.certPemPath = identity.certPath;
        serverSettings.keyPemPath = identity.keyPath;
        if (!server.Listen(serverSettings, "127.0.0.1", port, HooksFor(serverSink)))
            return false;
        const NetAddr target{0x7F000001u, port};
        if (!client.Connect(QuicSettings{}, target, "deskhub-perf", HooksFor(clientSink)))
            return false;
        for (int i = 0; i < kHandshakeRounds && !(serverSink.connected && clientSink.connected);
            ++i)
            Pump();
        return serverSink.connected && clientSink.connected &&
               client.Established(clientSink.conn);
    }

    void Pump() {
        client.Poll(NowUs(), 0);
        server.Poll(NowUs(), 0);
    }

    void PumpUntilStreamed(uint64_t targetBytes) {
        for (size_t i = 0; i < kPumpRoundsCap && serverSink.streamBytes < targetBytes; ++i)
            Pump();
    }
};

uint64_t SendOnTerminalStream(LoopbackPair& pair, std::span<const uint8_t> bytes) {
    pair.client.SendStream(pair.clientSink.conn, kQuicControlStream, bytes);
    return bytes.size();
}

}

void RunQuicPerf() {
    BeginGroup("quic over loopback");

    if (!QuicAvailable()) {
        std::printf("skipped: this build has no QUIC library\n");
        return;
    }

    const std::string savedCert = ReadAppDataFile(kHostCertFileName);
    const std::string savedKey = ReadAppDataFile(kHostKeyFileName);
    RemoveAppDataFile(kHostCertFileName);
    RemoveAppDataFile(kHostKeyFileName);
    const HostIdentity identity = LoadOrCreateHostIdentity("deskhub-perf");
    const auto restoreIdentity = [&] {
        if (!savedCert.empty()) WriteAppDataFile(kHostCertFileName, savedCert);
        if (!savedKey.empty()) WriteAppDataFile(kHostKeyFileName, savedKey);
    };
    if (!identity.Valid()) {
        std::printf("skipped: could not create a host identity\n");
        restoreIdentity();
        return;
    }

    LoopbackPair pair;
    if (!pair.Start(identity, kPerfPort)) {
        std::printf("skipped: the loopback pair failed to establish\n");
        restoreIdentity();
        return;
    }

    Measure(Workload{"quic/handshake", "handshake", 1, 0, 130.0, [&] {
                         Sink freshSink;
                         QuicEndpoint fresh;
                         const NetAddr target{0x7F000001u, kPerfPort};
                         fresh.Connect(QuicSettings{}, target, "deskhub-perf",
                             HooksFor(freshSink));
                         for (int i = 0; i < kHandshakeRounds && !freshSink.connected; ++i) {
                             fresh.Poll(NowUs(), 0);
                             pair.server.Poll(NowUs(), 0);
                         }
                         Consume(freshSink.connected ? 1 : 0);
                         fresh.CloseConnection(freshSink.conn, 0, "perf done");
                         for (int i = 0; i < 10; ++i) {
                             fresh.Poll(NowUs(), 0);
                             pair.server.Poll(NowUs(), 0);
                         }
                         fresh.Close();
                     }});

    std::vector<uint8_t> record(kTerminalRecordBytes);
    FillRandom(record);
    uint64_t expectedBytes = pair.serverSink.streamBytes;
    Measure(Workload{"quic/terminal-record-delivery", "record", 1, record.size(), 10.0, [&] {
                         expectedBytes += SendOnTerminalStream(pair, record);
                         pair.PumpUntilStreamed(expectedBytes);
                     }});

    std::vector<uint8_t> chunk(kThroughputChunkBytes);
    FillRandom(chunk);
    Measure(Workload{"quic/stream-throughput-64k", "KB",
        kThroughputChunks * kThroughputChunkBytes / kBytesPerKilobyte,
        kThroughputChunks * kThroughputChunkBytes, 2.0, [&] {
            for (size_t i = 0; i < kThroughputChunks; ++i)
                expectedBytes += SendOnTerminalStream(pair, chunk);
            pair.PumpUntilStreamed(expectedBytes);
        }});

    std::vector<uint8_t> datagram(kDatagramBytes);
    FillRandom(datagram);
    uint64_t expectedDatagrams = pair.serverSink.datagrams;
    Measure(Workload{"quic/datagram-delivery", "datagram", kDatagramsPerRun,
        kDatagramsPerRun * kDatagramBytes, 2.0, [&] {
            for (size_t i = 0; i < kDatagramsPerRun; ++i)
                pair.client.SendDatagram(pair.clientSink.conn, datagram);
            expectedDatagrams += kDatagramsPerRun;
            for (size_t i = 0;
                i < kPumpRoundsCap && pair.serverSink.datagrams < expectedDatagrams; ++i)
                pair.Pump();
            expectedDatagrams = pair.serverSink.datagrams;
        }});

    Measure(Workload{"quic/poll-idle", "poll", 2, 0, 3.0, [&] {
                         pair.Pump();
                     }});

    MeasureScaling(ScalingWorkload{"quic/stream-drain-scaling", "KB", 64, 256, 8.0,
        [&](uint64_t units) {
            uint64_t sent = 0;
            while (sent < units * kBytesPerKilobyte) {
                const size_t take =
                    size_t(std::min<uint64_t>(chunk.size(), units * kBytesPerKilobyte - sent));
                expectedBytes +=
                    SendOnTerminalStream(pair, std::span<const uint8_t>(chunk.data(), take));
                sent += take;
            }
            pair.PumpUntilStreamed(expectedBytes);
        }});

    pair.client.Close();
    pair.server.Close();
    restoreIdentity();
}

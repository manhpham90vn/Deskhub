#include "PerfHarness.h"

#include "deskhubp/net/UdpSocket.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

using namespace deskhub::perf;

namespace {

constexpr uint32_t kLoopbackIp = 0x7F000001u;
constexpr uint16_t kFirstPerfPort = 47870;
constexpr uint16_t kLastPerfPort = 47890;
constexpr size_t kBurst = kMaxRecvBatch;
constexpr size_t kDatagramBytes = 1200;
constexpr uint32_t kDrainTimeoutMs = 50;
constexpr int kDrainRoundsCap = 64;

struct LoopbackUdp {
    UdpSocket sender{};
    UdpSocket receiver{};
    NetAddr to{};
    std::vector<uint8_t> payload = std::vector<uint8_t>(kDatagramBytes);
    std::vector<OutboundDatagram> outbound{};
    std::vector<std::array<uint8_t, 2048>> buffers = std::vector<std::array<uint8_t, 2048>>(kBurst);
    std::vector<InboundDatagram> slots = std::vector<InboundDatagram>(kBurst);
    uint64_t sent = 0;
    uint64_t delivered = 0;

    bool Start() {
        for (uint16_t port = kFirstPerfPort; port <= kLastPerfPort; ++port)
            if (receiver.Open(port)) {
                to = NetAddr{kLoopbackIp, port};
                break;
            }
        if (to.port == 0 || !sender.Open(0)) return false;
        receiver.SetRecvTimeout(kDrainTimeoutMs);
        FillRandom(payload);
        for (size_t i = 0; i < kBurst; ++i)
            outbound.push_back(OutboundDatagram{payload.data(), payload.size()});
        return true;
    }

    void ResetSlots() {
        for (size_t i = 0; i < kBurst; ++i)
            slots[i] = InboundDatagram{buffers[i].data(), buffers[i].size(), 0, NetAddr{}};
    }

    void SendOneAtATime() {
        for (size_t i = 0; i < kBurst; ++i) sender.SendTo(to, payload.data(), payload.size());
        sent += kBurst;
    }

    void SendAsOneBurst() {
        sender.SendBatch(to, outbound);
        sent += kBurst;
    }

    void ReceiveOneAtATime() {
        NetAddr from{};
        size_t got = 0;
        for (int round = 0; round < kDrainRoundsCap && got < kBurst; ++round) {
            const int n = receiver.RecvFrom(buffers[0].data(), buffers[0].size(), from);
            if (n <= 0) break;
            Consume(size_t(n));
            ++got;
        }
        delivered += got;
    }

    void ReceiveAsBatches() {
        size_t got = 0;
        for (int round = 0; round < kDrainRoundsCap && got < kBurst; ++round) {
            ResetSlots();
            const int n = receiver.RecvBatch(slots);
            if (n <= 0) break;
            for (int i = 0; i < n; ++i) Consume(slots[size_t(i)].len);
            got += size_t(n);
        }
        delivered += got;
    }
};

}

void RunUdpPerf() {
    BeginGroup("udp datagram batching over loopback");

    LoopbackUdp link;
    if (!link.Start()) {
        std::printf("skipped: no free loopback port pair in %u..%u\n", unsigned(kFirstPerfPort),
            unsigned(kLastPerfPort));
        return;
    }

    Measure(Workload{"udp/loopback-one-syscall-each", "datagram", kBurst,
        kBurst * kDatagramBytes, 1.0, [&] {
            link.SendOneAtATime();
            link.ReceiveOneAtATime();
        }});

    Measure(Workload{"udp/loopback-batched", "datagram", kBurst, kBurst * kDatagramBytes, 1.0,
        [&] {
            link.SendAsOneBurst();
            link.ReceiveAsBatches();
        }});

    Measure(Workload{"udp/loopback-send-batched-recv-each", "datagram", kBurst,
        kBurst * kDatagramBytes, 1.0, [&] {
            link.SendAsOneBurst();
            link.ReceiveOneAtATime();
        }});

    if (link.delivered < link.sent)
        std::printf("loopback dropped %llu of %llu datagrams - read the rows above as a floor\n",
            static_cast<unsigned long long>(link.sent - link.delivered),
            static_cast<unsigned long long>(link.sent));

    link.sender.Close();
    link.receiver.Close();
}

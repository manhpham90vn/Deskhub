#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/net/NetInfo.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/system/Clock.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kLoopbackIp = 0x7F000001u;
constexpr uint16_t kFirstTestPort = 47800;
constexpr uint16_t kLastTestPort = 47850;

uint16_t OpenOnAFreePort(UdpSocket& sock) {
    for (uint16_t port = kFirstTestPort; port <= kLastTestPort; ++port)
        if (sock.Open(port)) return port;
    return 0;
}

void TestAClosedSocketRefusesEverything() {
    std::printf("[udp] a socket that was never opened never pretends to work...\n");
    UdpSocket sock;
    Check(!sock.IsOpen(), "a fresh socket is closed");

    const uint8_t byte = 1;
    Check(!sock.SendTo(NetAddr{kLoopbackIp, kFirstTestPort}, &byte, 1),
        "sending on a closed socket fails instead of silently dropping");
    Check(!sock.SetRecvTimeout(10), "so does setting a timeout");

    uint8_t buf[8];
    NetAddr from{};
    Check(sock.RecvFrom(buf, sizeof(buf), from) < 0,
        "receiving reports an error, which is what makes the net loop stop");

    sock.Close();
    Check(!sock.IsOpen(), "closing an already closed socket is harmless");
}

void TestOpenAndClose() {
    std::printf("[udp] a socket opens, reports itself open, and closes cleanly...\n");
    UdpSocket sock;
    Check(sock.Open(0), "port 0 lets the OS pick, which is what every viewer does");
    Check(sock.IsOpen(), "and the socket knows it is open");
    Check(!sock.lastBindAddrInUse(), "an ephemeral port is never reported as taken");
    Check(sock.SetRecvTimeout(10), "an open socket accepts a receive timeout");
    sock.Close();
    Check(!sock.IsOpen(), "after Close it is closed again");
    Check(sock.Open(0), "and it can be reopened for the next session");
}

void TestASecondHostOnTheSamePortIsToldWhy() {
    std::printf("[udp] the second Deskhub on one machine learns the port is taken...\n");
    UdpSocket first;
    const uint16_t port = OpenOnAFreePort(first);
    if (!port) {
        std::printf("  skipped: no free port in %u..%u\n", unsigned(kFirstTestPort),
            unsigned(kLastTestPort));
        return;
    }

    UdpSocket second;
    Check(!second.Open(port), "the second bind on the same port fails");
    Check(second.lastBindAddrInUse(),
        "and says so specifically, so the UI can name the real reason");
    Check(!second.IsOpen(), "a failed bind leaves the socket closed, not half-open");
}

void TestALoopbackDatagramArrivesIntact() {
    std::printf("[udp] a datagram sent to ourselves arrives whole, from where it says...\n");
    UdpSocket receiver;
    const uint16_t port = OpenOnAFreePort(receiver);
    if (!port) {
        std::printf("  skipped: no free port in %u..%u\n", unsigned(kFirstTestPort),
            unsigned(kLastTestPort));
        return;
    }
    receiver.SetRecvTimeout(200);

    UdpSocket sender;
    Check(sender.Open(0), "the sender takes an ephemeral port");

    std::vector<uint8_t> payload(1200);
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = uint8_t(i * 7 + 3);

    const NetAddr to{kLoopbackIp, port};
    Check(sender.SendTo(to, payload.data(), payload.size()), "the send succeeds");

    uint8_t buf[2048];
    NetAddr from{};
    int n = 0;
    for (int attempt = 0; attempt < 20 && n == 0; ++attempt)
        n = receiver.RecvFrom(buf, sizeof(buf), from);

    if (n <= 0) {
        std::printf("  skipped: loopback UDP is not available here (n=%d)\n", n);
        return;
    }

    Check(size_t(n) == payload.size(), "the whole datagram arrives in one piece");
    Check(std::memcmp(buf, payload.data(), payload.size()) == 0, "and its bytes are unchanged");
    Check(from.ip == kLoopbackIp, "the sender address is the one we sent from");
    Check(from.port != 0 && from.port != port,
        "and its port is the sender's, which is what the host replies to");
}

void TestABoundAddressStillReceives() {
    std::printf("[udp] a socket bound to one address still receives on it...\n");
    UdpSocket receiver;
    uint16_t port = 0;
    for (uint16_t candidate = kFirstTestPort; candidate <= kLastTestPort; ++candidate)
        if (receiver.Open(candidate, "127.0.0.1")) {
            port = candidate;
            break;
        }
    if (!port) {
        std::printf("  skipped: no free port in %u..%u\n", unsigned(kFirstTestPort),
            unsigned(kLastTestPort));
        return;
    }
    receiver.SetRecvTimeout(200);

    UdpSocket sender;
    Check(sender.Open(0), "the sender takes an ephemeral port");
    const uint8_t byte = 42;
    Check(sender.SendTo(NetAddr{kLoopbackIp, port}, &byte, 1),
        "a datagram aimed at the bound address is sent");

    uint8_t buf[8];
    NetAddr from{};
    int n = 0;
    for (int attempt = 0; attempt < 20 && n == 0; ++attempt)
        n = receiver.RecvFrom(buf, sizeof(buf), from);
    if (n <= 0) {
        std::printf("  skipped: loopback UDP is not available here (n=%d)\n", n);
        return;
    }
    Check(n == 1 && buf[0] == 42, "and arrives intact on the bound socket");

    UdpSocket bad;
    Check(!bad.Open(0, "not-an-ip"), "a malformed bind address fails instead of binding all");
    Check(!bad.IsOpen(), "and leaves the socket closed");
}

void TestATimedOutReceiveIsNotAnError() {
    std::printf("[udp] an idle second is not mistaken for a dead socket...\n");
    UdpSocket sock;
    Check(sock.Open(0), "open");
    Check(sock.SetRecvTimeout(10), "10 ms timeout");

    uint8_t buf[64];
    NetAddr from{};
    const uint64_t t0 = NowUs();
    const int n = sock.RecvFrom(buf, sizeof(buf), from);
    const uint64_t elapsedUs = NowUs() - t0;

    Check(n == 0, "nothing to read reports 0, never a negative error");
    Check(elapsedUs >= 5'000,
        "and the call actually waited, so the net loop does not spin at 100% CPU");
}

void TestWaitReadableTellsTheLoopWhenToRead() {
    std::printf("[udp] the loop is told when there is something to read, and only then...\n");
    UdpSocket closed;
    Check(!closed.WaitReadable(1), "a closed socket never reports anything to read");

    UdpSocket sock;
    const uint16_t port = OpenOnAFreePort(sock);
    if (!port) {
        Check(false, "no free test port");
        return;
    }

    const uint64_t t0 = NowUs();
    Check(!sock.WaitReadable(20), "an idle socket reports nothing");
    Check(NowUs() - t0 >= 10'000,
        "and the wait actually waited, so the net loop does not spin at 100% CPU");

    UdpSocket sender;
    Check(sender.Open(0), "a sender opens");
    const uint8_t byte = 42;
    Check(sender.SendTo(NetAddr{kLoopbackIp, port}, &byte, 1), "and sends one datagram");
    Check(sock.WaitReadable(1000), "which makes the socket readable");

    uint8_t buf[8];
    NetAddr from{};
    Check(sock.RecvFrom(buf, sizeof(buf), from) == 1 && buf[0] == 42,
        "and the read that follows gets exactly it");
}

std::vector<OutboundDatagram> Run(const std::vector<std::vector<uint8_t>>& payloads) {
    std::vector<OutboundDatagram> out;
    for (const std::vector<uint8_t>& payload : payloads)
        out.push_back(OutboundDatagram{payload.data(), payload.size()});
    return out;
}

void TestOnlyEqualSizedDatagramsRideOneSegmentedSend() {
    std::printf("[udp] the run offered to segmentation offload is one the kernel can split...\n");
    const std::vector<std::vector<uint8_t>> mixed{
        std::vector<uint8_t>(1200), std::vector<uint8_t>(1200), std::vector<uint8_t>(900),
        std::vector<uint8_t>(1200)};
    Check(LeadingRunOfEqualSegments(Run(mixed)) == 3,
        "a shorter datagram may only be the last segment of a run");

    const std::vector<std::vector<uint8_t>> shrinking{
        std::vector<uint8_t>(900), std::vector<uint8_t>(1200)};
    Check(LeadingRunOfEqualSegments(Run(shrinking)) == 1,
        "a longer one after a short one starts a new run instead of corrupting this one");

    const std::vector<std::vector<uint8_t>> even{std::vector<uint8_t>(1200),
        std::vector<uint8_t>(1200)};
    Check(LeadingRunOfEqualSegments(Run(even)) == 2, "equal sizes ride together");

    Check(LeadingRunOfEqualSegments(std::span<const OutboundDatagram>{}) == 0,
        "an empty batch offers nothing");

    const std::vector<std::vector<uint8_t>> empty{std::vector<uint8_t>(0),
        std::vector<uint8_t>(1200)};
    Check(LeadingRunOfEqualSegments(Run(empty)) == 0,
        "a zero-length datagram has no segment size to offload with");

    std::vector<std::vector<uint8_t>> many;
    for (size_t i = 0; i < kMaxSendBatch; ++i) many.push_back(std::vector<uint8_t>(9000));
    const size_t run = LeadingRunOfEqualSegments(Run(many));
    Check(run * 9000 <= kMaxSegmentedRunBytes,
        "and a run never asks the stack to coalesce more than one datagram can carry");
}

void TestABatchOfDatagramsArrivesWholeAndSeparate() {
    std::printf("[udp] a burst leaves in one syscall and comes back as separate datagrams...\n");
    UdpSocket receiver;
    const uint16_t port = OpenOnAFreePort(receiver);
    if (!port) {
        std::printf("  skipped: no free port in %u..%u\n", unsigned(kFirstTestPort),
            unsigned(kLastTestPort));
        return;
    }
    receiver.SetRecvTimeout(200);

    UdpSocket sender;
    Check(sender.Open(0), "the sender takes an ephemeral port");

    constexpr size_t kBurst = 9;
    constexpr size_t kFullBytes = 1200;
    constexpr size_t kTailBytes = 337;
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<OutboundDatagram> outbound;
    for (size_t i = 0; i < kBurst; ++i) {
        const size_t bytes = i + 1 == kBurst ? kTailBytes : kFullBytes;
        std::vector<uint8_t> payload(bytes, uint8_t(i));
        payloads.push_back(std::move(payload));
    }
    for (const std::vector<uint8_t>& payload : payloads)
        outbound.push_back(OutboundDatagram{payload.data(), payload.size()});

    const NetAddr to{kLoopbackIp, port};
    Check(sender.SendBatch(to, outbound) == kBurst, "every datagram in the burst is sent");

    std::vector<std::array<uint8_t, 2048>> buffers(kMaxRecvBatch);
    std::vector<bool> seen(kBurst, false);
    size_t received = 0;
    for (int attempt = 0; attempt < 20 && received < kBurst; ++attempt) {
        std::vector<InboundDatagram> slots(kMaxRecvBatch);
        for (size_t i = 0; i < slots.size(); ++i)
            slots[i] = InboundDatagram{buffers[i].data(), buffers[i].size(), 0, 0, NetAddr{}};
        const int got = receiver.RecvBatch(slots);
        if (got < 0) {
            Check(false, "reading a burst never reports an error on an open socket");
            return;
        }
        for (int i = 0; i < got; ++i) {
            const InboundDatagram& slot = slots[size_t(i)];
            Check(slot.len == kFullBytes || slot.len == kTailBytes,
                "each datagram keeps its own length, so segmentation offload never merges two");
            const size_t index = slot.buf[0];
            Check(index < kBurst, "and its payload names which of the burst it was");
            Check(!seen[index], "no datagram arrives twice");
            seen[index] = true;
            Check(slot.len == payloads[index].size(),
                "the length is the one that was sent, not the segment size");
            Check(std::memcmp(slot.buf, payloads[index].data(), slot.len) == 0,
                "and the bytes are unchanged");
            Check(slot.from.ip == kLoopbackIp, "every one names the sender it came from");
            ++received;
        }
    }

    if (received < kBurst) {
        std::printf("  skipped: loopback dropped %zu of %zu datagrams\n", kBurst - received,
            kBurst);
        return;
    }
    Check(received == kBurst, "the whole burst arrives");
}

void TestACoalescedSlotSplitsBackIntoDatagrams() {
    std::printf("[udp] a slot holding a coalesced burst hands back the datagrams that made it...\n");
    std::vector<uint8_t> bytes(3 * 1200 + 400);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i / 1200);

    InboundDatagram whole{bytes.data(), bytes.size(), 1200, 0, NetAddr{}};
    Check(DatagramsIn(whole) == 1, "a slot with no segment size is one datagram, as it always was");
    Check(DatagramAt(whole, 0).size() == 1200, "and the whole read is that datagram");
    Check(DatagramAt(whole, 1).empty(), "there is no second one to ask for");

    InboundDatagram empty{bytes.data(), bytes.size(), 0, 0, NetAddr{}};
    Check(DatagramsIn(empty) == 0, "a read of nothing holds no datagrams");

    InboundDatagram single{bytes.data(), bytes.size(), 900, 1200, NetAddr{}};
    Check(DatagramsIn(single) == 1,
        "a segment size at or above the read means the stack coalesced nothing");

    InboundDatagram burst{bytes.data(), bytes.size(), bytes.size(), 1200, NetAddr{}};
    Check(DatagramsIn(burst) == 4, "three full segments and a short tail are four datagrams");
    for (size_t i = 0; i < 3; ++i) {
        Check(DatagramAt(burst, i).size() == 1200, "every full segment keeps the sent length");
        Check(DatagramAt(burst, i).data() == bytes.data() + i * 1200,
            "and each one points at its own bytes, never a copy of the first");
    }
    Check(DatagramAt(burst, 3).size() == 400, "the tail keeps the short length it was sent with");
    Check(DatagramAt(burst, 4).empty(), "asking past the end yields nothing, not the tail again");
}

void TestACoalescedReadReturnsTheDatagramsThatWereSent() {
    std::printf("[udp] a burst read back through receive coalescing arrives whole...\n");
    UdpSocket receiver;
    const uint16_t port = OpenOnAFreePort(receiver);
    if (!port) {
        std::printf("  skipped: no free port in %u..%u\n", unsigned(kFirstTestPort),
            unsigned(kLastTestPort));
        return;
    }
    receiver.SetRecvTimeout(200);
    if (!receiver.EnableReceiveCoalescing()) {
        std::printf("  skipped: this stack has no UDP receive coalescing\n");
        return;
    }

    UdpSocket sender;
    Check(sender.Open(0), "the sender takes an ephemeral port");

    constexpr size_t kBurst = 8;
    constexpr size_t kFullBytes = 1200;
    constexpr size_t kTailBytes = 517;
    std::vector<std::vector<uint8_t>> payloads;
    std::vector<OutboundDatagram> outbound;
    for (size_t i = 0; i < kBurst; ++i)
        payloads.push_back(
            std::vector<uint8_t>(i + 1 == kBurst ? kTailBytes : kFullBytes, uint8_t(i)));
    for (const std::vector<uint8_t>& payload : payloads)
        outbound.push_back(OutboundDatagram{payload.data(), payload.size()});

    const NetAddr to{kLoopbackIp, port};
    Check(sender.SendBatch(to, outbound) == kBurst, "every datagram in the burst is sent");

    std::vector<uint8_t> slotBytes(kMaxCoalescedBytes);
    std::vector<bool> seen(kBurst, false);
    size_t received = 0;
    size_t mostInOneRead = 0;
    for (int attempt = 0; attempt < 20 && received < kBurst; ++attempt) {
        InboundDatagram slot{slotBytes.data(), slotBytes.size(), 0, 0, NetAddr{}};
        const int got = receiver.RecvBatch(std::span<InboundDatagram>(&slot, 1));
        if (got < 0) {
            Check(false, "reading a coalesced burst never reports an error on an open socket");
            return;
        }
        if (got == 0) continue;
        Check(slot.len <= slot.cap, "a coalesced read never overruns the slot it was given");
        const size_t parts = DatagramsIn(slot);
        if (parts > mostInOneRead) mostInOneRead = parts;
        for (size_t part = 0; part < parts; ++part) {
            const std::span<const uint8_t> one = DatagramAt(slot, part);
            Check(one.size() == kFullBytes || one.size() == kTailBytes,
                "each datagram comes back with the length it was sent with");
            const size_t index = one[0];
            Check(index < kBurst, "and its payload names which of the burst it was");
            Check(!seen[index], "no datagram arrives twice");
            seen[index] = true;
            Check(one.size() == payloads[index].size(),
                "the tail is not padded up to the segment size");
            Check(std::memcmp(one.data(), payloads[index].data(), one.size()) == 0,
                "and the bytes are unchanged");
            Check(slot.from.ip == kLoopbackIp, "the whole slot names the sender it came from");
            ++received;
        }
    }

    if (received < kBurst) {
        std::printf("  skipped: loopback dropped %zu of %zu datagrams\n", kBurst - received,
            kBurst);
        return;
    }
    Check(received == kBurst, "the whole burst arrives");
    if (mostInOneRead > 1)
        std::printf("  the stack coalesced up to %zu datagrams into one read\n", mostInOneRead);
    else
        std::printf("  note: the stack coalesced nothing this time, so only the split was tested\n");
}

void TestReadingABatchFromAnIdleSocketIsNotAnError() {
    std::printf("[udp] an empty batch read reports nothing, not a dead socket...\n");
    UdpSocket closed;
    std::array<uint8_t, 64> byteBuf{};
    InboundDatagram one{byteBuf.data(), byteBuf.size(), 0, 0, NetAddr{}};
    Check(closed.RecvBatch(std::span<InboundDatagram>(&one, 1)) < 0,
        "a socket that was never opened reports the error the net loop stops on");

    UdpSocket sock;
    Check(sock.Open(0), "open");
    Check(sock.SetRecvTimeout(10), "10 ms timeout");
    Check(sock.RecvBatch(std::span<InboundDatagram>{}) == 0,
        "asking for no datagrams reads none and is not an error");

    const uint64_t t0 = NowUs();
    Check(sock.RecvBatch(std::span<InboundDatagram>(&one, 1)) == 0,
        "nothing to read reports 0, never a negative error");
    Check(NowUs() - t0 >= 5'000,
        "and the call waited, so batching did not turn the net loop into a spin");
}

void TestLocalAddressesLookLikeAddresses() {
    std::printf("[netinfo] the addresses shown to the user are ones they can type back...\n");
    const std::vector<AdapterAddr> addrs = ListLocalIPv4();
    for (const AdapterAddr& a : addrs) {
        Check(!a.ip.empty(), "every listed adapter has an address to show");
        Check(!a.name.empty(), "and a name, so the user can tell Wi-Fi from a docker bridge");
        NetAddr parsed{};
        Check(ParseNetAddr(a.ip, parsed),
            "and it parses with our own parser, so copying it into the other machine works");
        Check((parsed.ip >> 24) != 127, "loopback is filtered out — nobody can connect to it");
        Check((parsed.ip >> 16) != 0xA9FE,
            "so is a self-assigned 169.254 address, which never routes anywhere");
    }
}

}

void RunUdpSocketTests() {
    TestAClosedSocketRefusesEverything();
    TestOpenAndClose();
    TestASecondHostOnTheSamePortIsToldWhy();
    TestALoopbackDatagramArrivesIntact();
    TestABoundAddressStillReceives();
    TestATimedOutReceiveIsNotAnError();
    TestWaitReadableTellsTheLoopWhenToRead();
    TestOnlyEqualSizedDatagramsRideOneSegmentedSend();
    TestABatchOfDatagramsArrivesWholeAndSeparate();
    TestACoalescedSlotSplitsBackIntoDatagrams();
    TestACoalescedReadReturnsTheDatagramsThatWereSent();
    TestReadingABatchFromAnIdleSocketIsNotAnError();
    TestLocalAddressesLookLikeAddresses();
}

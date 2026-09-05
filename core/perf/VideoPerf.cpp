#include "Perf.h"
#include "PerfHarness.h"

#include "deskhub/media/FrameMailbox.h"
#include "deskhub/media/RgbDownscale.h"
#include "deskhub/protocol/ByteOrder.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/transport/FecScheme.h"
#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/Reassembler.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace deskhub;
using namespace deskhub::perf;

namespace {

constexpr size_t kIdrFrameBytes = 120'000;
constexpr size_t kScalingFrameBytes = 512 * 1024;
constexpr uint64_t kFrameIntervalUs = 16'667;
constexpr size_t kDroppedPacketIndex = 3;
constexpr uint32_t kSourceWidth = 1920;
constexpr uint32_t kSourceHeight = 1080;
constexpr uint32_t kScaledWidth = 1280;
constexpr uint32_t kScaledHeight = 720;

using Datagram = std::vector<uint8_t>;

std::vector<Datagram> PacketizeOnce(std::span<const uint8_t> nal, bool fec,
    std::string_view scheme = kDefaultFecScheme) {
    Packetizer packetizer;
    packetizer.SetFecEnabled(fec);
    packetizer.SetFecScheme(scheme);
    std::vector<Datagram> packets;
    const Packetizer::SendFn collect = [&packets](std::span<const uint8_t> datagram) {
        packets.emplace_back(datagram.begin(), datagram.end());
    };
    packetizer.SendFrame(nal, 0, 0, true, collect);
    return packets;
}

bool IsParity(const Datagram& packet) {
    const auto header = ParseCommonHeader(packet);
    return header && header->type == MsgType::FecPacket;
}

void SetFrameId(Datagram& packet, uint32_t frameId) {
    PutU32(packet.data() + kCommonHeaderSize, frameId);
}

void FeedOne(Reassembler& reassembler, const Datagram& packet, uint64_t nowUs) {
    const auto header = ParseCommonHeader(packet);
    if (!header) return;
    if (header->type == MsgType::FecPacket) {
        const auto parity = ParseFecPacket(*header, PayloadOf(packet));
        if (parity) reassembler.PushFec(*parity, nowUs);
        return;
    }
    const auto video = ParseVideoPacket(*header, PayloadOf(packet));
    if (video) reassembler.Push(*video, nowUs);
}

size_t FeedFrame(Reassembler& reassembler, std::vector<Datagram>& packets,
    const std::vector<size_t>& order, uint32_t frameId, uint64_t nowUs) {
    for (Datagram& packet : packets) SetFrameId(packet, frameId);
    for (size_t index : order) FeedOne(reassembler, packets[index], nowUs);
    const auto frame = reassembler.PopReady(nowUs);
    return frame ? frame->nal.size() : 0;
}

std::vector<size_t> ArrivalOrder(size_t count) {
    std::vector<size_t> order(count);
    for (size_t i = 0; i < count; ++i) order[i] = i;
    return order;
}

std::vector<size_t> ScrambledArrivalOrder(size_t count) {
    std::vector<size_t> order = ArrivalOrder(count);
    for (size_t i = count; i > 1; --i) {
        const size_t swapWith = NextRandom() % i;
        std::swap(order[i - 1], order[swapWith]);
    }
    return order;
}

std::vector<size_t> OrderMissingOnePacket(const std::vector<Datagram>& packets) {
    std::vector<size_t> order;
    for (size_t i = 0; i < packets.size(); ++i) {
        if (i == kDroppedPacketIndex && !IsParity(packets[i])) continue;
        order.push_back(i);
    }
    return order;
}

size_t DataPacketCount(const std::vector<Datagram>& packets) {
    size_t count = 0;
    for (const Datagram& packet : packets)
        if (!IsParity(packet)) ++count;
    return count;
}

}

void RunVideoPerf() {
    BeginGroup("video");

    std::vector<uint8_t> nal(kIdrFrameBytes);
    FillRandom(nal);

    const Packetizer::SendFn sink = [](std::span<const uint8_t> datagram) {
        Consume(datagram.size());
    };

    Packetizer plainPacketizer;
    plainPacketizer.SetSessionId(1);
    uint32_t plainFrameId = 0;
    Measure(Workload{"video/packetize", "frame", 1, nal.size(), 0.0, [&] {
                         plainPacketizer.SendFrame(nal, plainFrameId, plainFrameId * kFrameIntervalUs, true, sink);
                         ++plainFrameId;
                     }});

    Packetizer fecPacketizer;
    fecPacketizer.SetSessionId(1);
    fecPacketizer.SetFecEnabled(true);
    uint32_t fecFrameId = 0;
    Measure(Workload{"video/packetize-with-fec", "frame", 1, nal.size(), 0.0, [&] {
                         fecPacketizer.SendFrame(nal, fecFrameId, fecFrameId * kFrameIntervalUs, true, sink);
                         ++fecFrameId;
                     }});

    std::vector<Datagram> packets = PacketizeOnce(nal, false);
    const std::vector<size_t> inOrder = ArrivalOrder(packets.size());
    const std::vector<size_t> scrambled = ScrambledArrivalOrder(packets.size());

    Reassembler orderedReassembler(kFrameIntervalUs);
    uint32_t orderedFrameId = 0;
    Measure(Workload{"video/reassemble-in-order", "packet", packets.size(), nal.size(), 1.2, [&] {
                         Consume(FeedFrame(orderedReassembler, packets, inOrder, orderedFrameId,
                             orderedFrameId * kFrameIntervalUs));
                         ++orderedFrameId;
                     }});

    Reassembler scrambledReassembler(kFrameIntervalUs);
    uint32_t scrambledFrameId = 0;
    Measure(Workload{"video/reassemble-out-of-order", "packet", packets.size(), nal.size(), 1.2, [&] {
                         Consume(FeedFrame(scrambledReassembler, packets, scrambled, scrambledFrameId,
                             scrambledFrameId * kFrameIntervalUs));
                         ++scrambledFrameId;
                     }});

    std::vector<Datagram> fecPackets = PacketizeOnce(nal, true);
    const std::vector<size_t> lossy = OrderMissingOnePacket(fecPackets);
    const size_t fecDataPackets = DataPacketCount(fecPackets);

    Reassembler recoveringReassembler(kFrameIntervalUs);
    uint32_t recoveredFrameId = 0;
    Measure(Workload{"video/reassemble-fec-recovery", "packet", fecDataPackets, nal.size(), 1.4, [&] {
                         Consume(FeedFrame(recoveringReassembler, fecPackets, lossy, recoveredFrameId,
                             recoveredFrameId * kFrameIntervalUs));
                         ++recoveredFrameId;
                     }});

    for (std::string_view scheme : FecSchemeNames()) {
        const std::string encodeName = "video/fec-encode-" + std::string(scheme);
        Packetizer schemePacketizer;
        schemePacketizer.SetSessionId(1);
        schemePacketizer.SetFecEnabled(true);
        schemePacketizer.SetFecScheme(scheme);
        uint32_t schemeFrameId = 0;
        Measure(Workload{encodeName.c_str(), "frame", 1, nal.size(), 0.0, [&] {
                             schemePacketizer.SendFrame(nal, schemeFrameId,
                                 schemeFrameId * kFrameIntervalUs, true, sink);
                             ++schemeFrameId;
                         }});

        const std::string recoverName = "video/fec-recover-" + std::string(scheme);
        std::vector<Datagram> schemePackets = PacketizeOnce(nal, true, scheme);
        const std::vector<size_t> schemeLossy = OrderMissingOnePacket(schemePackets);
        const size_t schemeFedPackets = schemePackets.size();
        Reassembler schemeReassembler(kFrameIntervalUs);
        schemeReassembler.SetFecScheme(scheme);
        uint32_t schemeRecoverId = 0;
        Measure(Workload{recoverName.c_str(), "packet", schemeFedPackets, nal.size(), 1.3, [&] {
                             Consume(FeedFrame(schemeReassembler, schemePackets, schemeLossy,
                                 schemeRecoverId, schemeRecoverId * kFrameIntervalUs));
                             ++schemeRecoverId;
                         }});
    }

    Reassembler nackReassembler(kFrameIntervalUs);
    std::vector<uint16_t> nackIndices(kMaxNackIndices);
    const std::vector<size_t> everySecondPacket = [&] {
        std::vector<size_t> order;
        for (size_t i = 0; i < packets.size(); i += 2) order.push_back(i);
        return order;
    }();
    uint32_t nackFrameId = 0;
    uint64_t nackNowUs = 0;
    Measure(Workload{"video/plan-nack", "plan", 1, 0, 70.0, [&] {
                         for (Datagram& packet : packets) SetFrameId(packet, nackFrameId);
                         for (size_t index : everySecondPacket) FeedOne(nackReassembler, packets[index], nackNowUs);
                         nackNowUs += kFrameIntervalUs;
                         uint32_t frameId = 0;
                         Consume(nackReassembler.PlanNack(nackNowUs, 20'000, frameId, nackIndices));
                         ++nackFrameId;
                     }});

    std::vector<uint8_t> source(size_t(kSourceWidth) * kSourceHeight * media::kPackedPixelBytes);
    std::vector<uint8_t> scaled(size_t(kScaledWidth) * kScaledHeight * media::kPackedPixelBytes);
    FillRandom(source);
    media::RgbDownscaler downscaler;
    downscaler.Configure(kSourceWidth, kSourceHeight, kScaledWidth, kScaledHeight);
    Measure(Workload{"video/downscale-1080p-to-720p", "frame", 1, source.size(), 0.0, [&] {
                         downscaler.Scale(source.data(), kSourceWidth * media::kPackedPixelBytes, scaled.data(),
                             kScaledWidth * media::kPackedPixelBytes);
                     }});

    media::FrameMailbox<std::vector<uint8_t>> mailbox;
    std::vector<uint8_t> decoded(kIdrFrameBytes);
    std::vector<uint8_t> presented;
    Measure(Workload{"video/frame-mailbox-handoff", "frame", 1, 0, 0.0, [&] {
                         mailbox.Put(std::move(decoded));
                         mailbox.TakeWait(presented);
                         decoded = std::move(presented);
                         Consume(decoded.size());
                     }});

    std::vector<uint8_t> scalingNal(kScalingFrameBytes);
    FillRandom(scalingNal);
    Packetizer scalingPacketizer;
    uint32_t scalingFrameId = 0;
    MeasureScaling(ScalingWorkload{"video/packetize-scaling", "KB", 64, 256, 8.0,
        [&](uint64_t units) {
            scalingPacketizer.SendFrame(
                std::span<const uint8_t>(scalingNal.data(), size_t(units) * 1024),
                scalingFrameId, scalingFrameId * kFrameIntervalUs, true, sink);
            ++scalingFrameId;
        }});

    Reassembler scalingReassembler(kFrameIntervalUs);
    uint32_t scalingReassembleId = 0;
    MeasureScaling(ScalingWorkload{"video/reassemble-scaling", "frame", 8, 32, 6.0,
        [&](uint64_t units) {
            for (uint64_t i = 0; i < units; ++i) {
                Consume(FeedFrame(scalingReassembler, packets, inOrder, scalingReassembleId,
                    scalingReassembleId * kFrameIntervalUs));
                ++scalingReassembleId;
            }
        }});
}

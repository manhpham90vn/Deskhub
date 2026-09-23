#include "Perf.h"
#include "PerfHarness.h"

#include "deskhub/control/VideoPacer.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/transport/AudioJitterBuffer.h"
#include "deskhub/transport/RetransmitCache.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using namespace deskhub;
using namespace deskhub::perf;

namespace {

constexpr size_t kAudioPayloadBytes = 80;
constexpr size_t kInputBatchEvents = 24;
constexpr size_t kCachedFramePackets = 64;
constexpr uint64_t kFrameIntervalUs = 16'667;
constexpr uint32_t kReorderEvery = 7;

}

void RunControlPerf() {
    BeginGroup("wire, audio and control");

    std::vector<uint8_t> videoPayload(kMaxVideoPayload);
    FillRandom(videoPayload);
    std::array<uint8_t, kMaxDatagram> datagram{};
    VideoHeader videoHeader{0, 0, 0, 900};
    const size_t videoPacketBytes =
        BuildVideoPacket(datagram, 1, videoHeader, false, false, videoPayload);

    Measure(Workload{"wire/build-video-packet", "packet", 1, videoPacketBytes, 0.0, [&] {
                         ++videoHeader.pktIndex;
                         Consume(BuildVideoPacket(datagram, 1, videoHeader, false, false, videoPayload));
                     }});

    Measure(Workload{"wire/parse-video-packet", "packet", 1, videoPacketBytes, 0.0, [&] {
                         const std::span<const uint8_t> wire(datagram.data(), videoPacketBytes);
                         const auto header = ParseCommonHeader(wire);
                         if (!header) return;
                         const auto packet = ParseVideoPacket(*header, PayloadOf(wire));
                         if (packet) Consume(packet->payload.size());
                     }});

    std::vector<InputEvent> events(kInputBatchEvents);
    for (size_t i = 0; i < events.size(); ++i)
        events[i] = InputEvent{InputType::MouseMove, i * 1000ull, int32_t(i), int32_t(i * 2), 0, 1};
    const size_t inputPacketBytes = BuildInputEvents(datagram, 1, 0, events);
    std::vector<InputEvent> parsed(kInputBatchEvents);

    Measure(Workload{"wire/build-parse-input-events", "event", kInputBatchEvents, inputPacketBytes,
        0.0, [&] {
            const size_t written = BuildInputEvents(datagram, 1, 0, events);
            uint32_t firstSeq = 0;
            Consume(ParseInputEvents(
                std::span<const uint8_t>(datagram.data() + kCommonHeaderSize,
                    written - kCommonHeaderSize),
                firstSeq, parsed));
        }});

    std::vector<uint8_t> audioPayload(kAudioPayloadBytes);
    FillRandom(audioPayload);
    AudioJitterBuffer jitter;
    uint32_t audioSeq = 0;
    Measure(Workload{"audio/jitter-buffer-push-pop", "frame", 1, kAudioPayloadBytes, 3.5, [&] {
                         const AudioPacketView packet{AudioHeader{audioSeq, audioSeq * kAudioFrameMs * 1000},
                             audioPayload};
                         jitter.Push(packet);
                         ++audioSeq;
                         const auto frame = jitter.Pop();
                         if (frame) Consume(frame->payload.size());
                     }});

    RetransmitCache cache;
    uint32_t cachedFrameId = 0;
    Measure(Workload{"transport/retransmit-cache", "packet", kCachedFramePackets,
        kCachedFramePackets * videoPacketBytes, 0.2, [&] {
            VideoHeader header{cachedFrameId, cachedFrameId * kFrameIntervalUs, 0,
                uint16_t(kCachedFramePackets)};
            for (uint16_t index = 0; index < kCachedFramePackets; ++index) {
                header.pktIndex = index;
                const size_t written =
                    BuildVideoPacket(datagram, 1, header, false, index + 1 == kCachedFramePackets,
                        videoPayload);
                cache.Store(std::span<const uint8_t>(datagram.data(), written));
            }
            for (uint16_t index = 0; index < kCachedFramePackets; index += 4)
                Consume(cache.Find(cachedFrameId, index).size());
            ++cachedFrameId;
        }});

    VideoPacer pacer;
    uint64_t pacerNowUs = 0;
    Measure(Workload{"control/video-pacer", "frame", 1, 0, 0.0, [&] {
                         pacerNowUs += kFrameIntervalUs;
                         pacer.ObserveArrival(pacerNowUs, pacerNowUs + 3000);
                         Consume(pacer.DisplayTimeUs(pacerNowUs, pacerNowUs + 3000));
                     }});

    AudioJitterBuffer scalingJitter;
    uint32_t scalingSeq = 0;
    MeasureScaling(ScalingWorkload{"audio/jitter-buffer-scaling", "frame", 64, 256, 6.0,
        [&](uint64_t units) {
            for (uint64_t i = 0; i < units; ++i) {
                const uint32_t seq =
                    (scalingSeq % kReorderEvery == 0) ? scalingSeq + 1 : scalingSeq;
                const AudioPacketView packet{AudioHeader{seq, seq * kAudioFrameMs * 1000}, audioPayload};
                scalingJitter.Push(packet);
                ++scalingSeq;
                const auto frame = scalingJitter.Pop();
                if (frame) Consume(frame->payload.size());
            }
        }});
}

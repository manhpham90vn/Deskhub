#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhubp/audio/AudioBroadcaster.h"
#include "deskhubp/system/Clock.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <span>
#include <vector>

namespace {

constexpr int kOfferBurst = 13;
constexpr uint64_t kBurstBudgetUs = 50'000;
constexpr uint64_t kSendStallUs = 50'000;
constexpr uint64_t kMinCaptureToSendGapUs = 80'000;

struct SentFrame {
    uint32_t seq = 0;
    uint64_t timestampUs = 0;
    uint64_t sentAtUs = 0;
};

std::vector<int16_t> SawtoothFrame(size_t samples) {
    std::vector<int16_t> pcm(samples);
    for (size_t i = 0; i < samples; ++i) pcm[i] = int16_t((i % 128) * 200 - 12'800);
    return pcm;
}

void TestOfferNeverBlocksOnTheSendPath() {
    std::printf("[audio] Offer returns immediately even while every send is slow...\n");
    deskhubp::AudioBroadcaster broadcaster;
    std::mutex recorded;
    std::vector<SentFrame> sent;

    const bool started = broadcaster.Start(
        [&](std::span<const uint8_t>, uint32_t seq, uint64_t timestampUs) {
            {
                const std::lock_guard<std::mutex> lock(recorded);
                sent.push_back({seq, timestampUs, NowUs()});
            }
            SleepUs(kSendStallUs);
        });
    Check(started, "the broadcaster starts with the default format");
    if (!started) return;

    const std::vector<int16_t> pcm = SawtoothFrame(broadcaster.format().interleavedSamples());

    const uint64_t burstStartUs = NowUs();
    for (int i = 0; i < kOfferBurst; ++i) broadcaster.Offer(pcm);
    const uint64_t burstUs = NowUs() - burstStartUs;
    Check(burstUs < kBurstBudgetUs,
        "thirteen offers return in under 50 ms although each send takes 50 ms, so the "
        "capture thread never pays for the network");
    size_t delivered = 0;
    for (int i = 0; i < 3000; ++i) {
        SleepUs(kSendStallUs * 2);
        const std::lock_guard<std::mutex> lock(recorded);
        if (sent.size() == delivered) break;
        delivered = sent.size();
    }
    broadcaster.Stop();

    const std::lock_guard<std::mutex> lock(recorded);
    Check(sent.size() < size_t(kOfferBurst),
        "a full queue drops frames instead of waiting, so not every offer is sent");
    Check(sent.size() >= 2, "the queued frames drain behind the slow sends");
    bool ordered = true;
    for (size_t i = 1; i < sent.size(); ++i)
        ordered = ordered && sent[i].seq == sent[i - 1].seq + 1;
    Check(ordered, "sequence numbers stay dense and ordered across the backlog");
    bool stampedAtCapture = false;
    for (const SentFrame& frame : sent)
        stampedAtCapture =
            stampedAtCapture || frame.sentAtUs - frame.timestampUs >= kMinCaptureToSendGapUs;
    Check(stampedAtCapture,
        "timestamps carry the capture time, not the moment the send finally ran");
}

void TestStopIsPromptAndFinal() {
    std::printf("[audio] Stop joins the worker promptly and Offer after Stop is a no-op...\n");
    deskhubp::AudioBroadcaster broadcaster;
    std::atomic<int> sends{0};
    const bool started = broadcaster.Start(
        [&](std::span<const uint8_t>, uint32_t, uint64_t) { ++sends; });
    Check(started, "the broadcaster starts");
    if (!started) return;

    const std::vector<int16_t> pcm = SawtoothFrame(broadcaster.format().interleavedSamples());
    broadcaster.Offer(pcm);
    for (int i = 0; i < 400 && sends.load() == 0; ++i) SleepUs(1'000);
    Check(sends.load() == 1, "an offered frame flows through with no consumer nudging");

    const uint64_t stopStartUs = NowUs();
    broadcaster.Stop();
    Check(NowUs() - stopStartUs < 200'000, "Stop returns within the worker's poll interval");
    Check(!broadcaster.running(), "and the broadcaster reports stopped");

    broadcaster.Offer(pcm);
    SleepUs(10'000);
    Check(sends.load() == 1, "an Offer after Stop goes nowhere");
}

}

void RunAudioBroadcasterTests() {
    TestOfferNeverBlocksOnTheSendPath();
    TestStopIsPromptAndFinal();
}

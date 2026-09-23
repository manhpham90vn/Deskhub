#pragma once
#include "deskhub/media/AudioTypes.h"
#include "deskhubp/media/OpusCodec.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <vector>

namespace deskhubp {

class AudioBroadcaster {
public:
    using SendFn = std::function<void(std::span<const uint8_t> opusFrame, uint32_t seq,
        uint64_t timestampUs)>;

    AudioBroadcaster() = default;
    ~AudioBroadcaster();
    AudioBroadcaster(const AudioBroadcaster&) = delete;
    AudioBroadcaster& operator=(const AudioBroadcaster&) = delete;

    bool Start(SendFn send, const deskhub::media::AudioFormat& format = {},
        uint32_t bitrateBps = deskhub::media::kAudioBitrateBps);
    void Stop();

    void Offer(std::span<const int16_t> pcm);

    bool running() const {
        return running_.load(std::memory_order_acquire);
    }

    const deskhub::media::AudioFormat& format() const {
        return format_;
    }

    uint64_t framesRefused() const {
        return framesRefused_.load(std::memory_order_relaxed);
    }

private:
    static constexpr uint64_t kReportIntervalUs = 2'000'000;
    static constexpr size_t kQueueDepth = 8;
    static constexpr uint64_t kWorkerIdlePollUs = 2'000;

    struct PendingFrame {
        std::vector<int16_t> pcm;
        size_t samples = 0;
        uint64_t capturedAtUs = 0;
    };

    void WorkerLoop();
    void EncodeAndSend(std::span<const int16_t> pcm, uint64_t capturedAtUs);

    std::mutex encoderMutex_;
    OpusAudioEncoder encoder_;
    SendFn send_;
    deskhub::media::AudioFormat format_{};
    std::vector<uint8_t> packet_;
    uint32_t nextSeq_ = 0;
    uint64_t bytesEncoded_ = 0;
    uint64_t reportedAtUs_ = 0;
    uint64_t nextReportUs_ = 0;
    std::array<PendingFrame, kQueueDepth> queue_{};
    std::atomic<uint32_t> queueWriteAt_{0};
    std::atomic<uint32_t> queueReadAt_{0};
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> framesEncoded_{0};
    std::atomic<uint64_t> framesRefused_{0};
};

}

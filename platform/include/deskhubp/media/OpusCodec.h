#pragma once
#include "deskhub/media/AudioContract.h"
#include "deskhub/media/AudioTypes.h"

#include <cstddef>
#include <cstdint>
#include <span>

struct OpusEncoder;
struct OpusDecoder;

namespace deskhubp {

inline constexpr size_t kMaxOpusPacketBytes = 1275;

class OpusAudioEncoder {
public:
    OpusAudioEncoder() = default;
    ~OpusAudioEncoder();
    OpusAudioEncoder(const OpusAudioEncoder&) = delete;
    OpusAudioEncoder& operator=(const OpusAudioEncoder&) = delete;

    bool Open(const deskhub::media::AudioFormat& format, uint32_t bitrateBps);
    void Close();

    size_t Encode(std::span<const int16_t> pcm, std::span<uint8_t> out);

    bool SetBitrate(uint32_t bitrateBps);

    bool IsOpen() const {
        return enc_ != nullptr;
    }

    const deskhub::media::AudioFormat& format() const {
        return format_;
    }

    static const char* BackendName();

private:
    OpusEncoder* enc_ = nullptr;
    deskhub::media::AudioFormat format_{};
};

class OpusAudioDecoder {
public:
    OpusAudioDecoder() = default;
    ~OpusAudioDecoder();
    OpusAudioDecoder(const OpusAudioDecoder&) = delete;
    OpusAudioDecoder& operator=(const OpusAudioDecoder&) = delete;

    bool Open(const deskhub::media::AudioFormat& format);
    void Close();

    size_t Decode(std::span<const uint8_t> packet, std::span<int16_t> pcm);

    size_t Conceal(std::span<int16_t> pcm);

    size_t Recover(std::span<const uint8_t> nextPacket, std::span<int16_t> pcm);

    bool IsOpen() const {
        return dec_ != nullptr;
    }

    const deskhub::media::AudioFormat& format() const {
        return format_;
    }

private:
    OpusDecoder* dec_ = nullptr;
    deskhub::media::AudioFormat format_{};
};

static_assert(deskhub::media::AudioEncoderLike<OpusAudioEncoder>);
static_assert(deskhub::media::AudioDecoderLike<OpusAudioDecoder>);

}

#include "deskhubp/media/OpusCodec.h"
#include "deskhubp/diag/Log.h"

#include <opus/opus.h>

namespace deskhubp {

using deskhub::media::AudioFormat;

namespace {

bool FormatFits(const AudioFormat& format, std::span<const int16_t> pcm) {
    return deskhub::media::IsSupportedAudioFormat(format) &&
           pcm.size() == format.interleavedSamples();
}

}

OpusAudioEncoder::~OpusAudioEncoder() {
    Close();
}

bool OpusAudioEncoder::Open(const AudioFormat& format, uint32_t bitrateBps) {
    Close();
    if (!deskhub::media::IsSupportedAudioFormat(format)) {
        LOGE("[audio] evt=enc_open_bad_format rate=%u ch=%u frame=%u", format.sampleRate,
            format.channels, format.samplesPerFrame);
        return false;
    }

    int err = OPUS_OK;
    enc_ = opus_encoder_create(int(format.sampleRate), int(format.channels),
        OPUS_APPLICATION_AUDIO, &err);
    if (enc_ == nullptr || err != OPUS_OK) {
        LOGE("[audio] evt=enc_open_fail err=%s", opus_strerror(err));
        enc_ = nullptr;
        return false;
    }

    format_ = format;
    opus_encoder_ctl(enc_, OPUS_SET_BITRATE(int(bitrateBps)));
    opus_encoder_ctl(enc_, OPUS_SET_INBAND_FEC(1));
    opus_encoder_ctl(enc_, OPUS_SET_PACKET_LOSS_PERC(int(deskhub::media::kAudioExpectedLossPct)));
    opus_encoder_ctl(enc_, OPUS_SET_DTX(1));
    return true;
}

void OpusAudioEncoder::Close() {
    if (enc_ != nullptr) opus_encoder_destroy(enc_);
    enc_ = nullptr;
}

size_t OpusAudioEncoder::Encode(std::span<const int16_t> pcm, std::span<uint8_t> out) {
    if (enc_ == nullptr || !FormatFits(format_, pcm) || out.empty()) return 0;
    const int written = opus_encode(enc_, pcm.data(), int(format_.samplesPerFrame), out.data(),
        opus_int32(out.size()));
    if (written < 0) {
        LOGW("[audio] evt=encode_fail err=%s", opus_strerror(written));
        return 0;
    }
    return size_t(written);
}

bool OpusAudioEncoder::SetBitrate(uint32_t bitrateBps) {
    if (enc_ == nullptr) return false;
    return opus_encoder_ctl(enc_, OPUS_SET_BITRATE(int(bitrateBps))) == OPUS_OK;
}

const char* OpusAudioEncoder::BackendName() {
    return opus_get_version_string();
}

OpusAudioDecoder::~OpusAudioDecoder() {
    Close();
}

bool OpusAudioDecoder::Open(const AudioFormat& format) {
    Close();
    if (!deskhub::media::IsSupportedAudioFormat(format)) {
        LOGE("[audio] evt=dec_open_bad_format rate=%u ch=%u frame=%u", format.sampleRate,
            format.channels, format.samplesPerFrame);
        return false;
    }

    int err = OPUS_OK;
    dec_ = opus_decoder_create(int(format.sampleRate), int(format.channels), &err);
    if (dec_ == nullptr || err != OPUS_OK) {
        LOGE("[audio] evt=dec_open_fail err=%s", opus_strerror(err));
        dec_ = nullptr;
        return false;
    }
    format_ = format;
    return true;
}

void OpusAudioDecoder::Close() {
    if (dec_ != nullptr) opus_decoder_destroy(dec_);
    dec_ = nullptr;
}

size_t OpusAudioDecoder::Decode(std::span<const uint8_t> packet, std::span<int16_t> pcm) {
    if (dec_ == nullptr || packet.empty() || !FormatFits(format_, pcm)) return 0;
    const int samples = opus_decode(dec_, packet.data(), opus_int32(packet.size()), pcm.data(),
        int(format_.samplesPerFrame), 0);
    if (samples < 0) {
        LOGW("[audio] evt=decode_fail err=%s", opus_strerror(samples));
        return 0;
    }
    return size_t(samples);
}

size_t OpusAudioDecoder::Recover(std::span<const uint8_t> nextPacket, std::span<int16_t> pcm) {
    if (dec_ == nullptr || nextPacket.empty() || !FormatFits(format_, pcm)) return 0;
    const int samples = opus_decode(dec_, nextPacket.data(), opus_int32(nextPacket.size()),
        pcm.data(), int(format_.samplesPerFrame), 1);
    if (samples < 0) {
        LOGW("[audio] evt=fec_recover_fail err=%s", opus_strerror(samples));
        return 0;
    }
    return size_t(samples);
}

size_t OpusAudioDecoder::Conceal(std::span<int16_t> pcm) {
    if (dec_ == nullptr || !FormatFits(format_, pcm)) return 0;
    const int samples = opus_decode(dec_, nullptr, 0, pcm.data(), int(format_.samplesPerFrame), 0);
    if (samples < 0) {
        LOGW("[audio] evt=conceal_fail err=%s", opus_strerror(samples));
        return 0;
    }
    return size_t(samples);
}

}

#include "encode/MediaCodecEncoder.h"

#include <media/NdkMediaFormat.h>

#include <span>

#include "deskhub/media/AnnexB.h"
#include "deskhub/media/H264Sps.h"
#include "deskhubp/diag/Log.h"

namespace {

constexpr const char* kMimeH264 = "video/avc";
constexpr int32_t kColorFormatSurface = 0x7F000789;
constexpr int32_t kBitrateModeCbr = 2;
constexpr int32_t kKeyframeIntervalSecs = 3600;
constexpr int64_t kRepeatStaticFrameAfterUs = 100'000;
constexpr int64_t kDequeueTimeoutUs = 50'000;

AMediaFormat* MakeEncoderFormat(const EncoderConfig& cfg) {
    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, kMimeH264);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, int32_t(cfg.width));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, int32_t(cfg.height));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_COLOR_FORMAT, kColorFormatSurface);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_BIT_RATE, int32_t(cfg.bitrateBps));
    AMediaFormat_setFloat(fmt, AMEDIAFORMAT_KEY_FRAME_RATE, float(cfg.fps));
    AMediaFormat_setFloat(fmt, "max-fps-to-encoder", float(cfg.fps));
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, kKeyframeIntervalSecs);
    AMediaFormat_setInt32(fmt, "bitrate-mode", kBitrateModeCbr);
    AMediaFormat_setInt32(fmt, "max-bframes", 0);
    AMediaFormat_setInt64(fmt, "repeat-previous-frame-after", kRepeatStaticFrameAfterUs);
    if (cfg.lowLatency) {
        AMediaFormat_setInt32(fmt, "low-latency", 1);
        AMediaFormat_setInt32(fmt, "priority", 0);
    }
    return fmt;
}

}

MediaCodecEncoder::~MediaCodecEncoder() {
    Finish();
}

bool MediaCodecEncoder::Init(const EncoderConfig& cfg) {
    Finish();
    if (!cfg.width || !cfg.height) {
        LOGE("[Encoder] Bad size %ux%u.", cfg.width, cfg.height);
        return false;
    }
    cfg_ = cfg;

    codec_ = AMediaCodec_createEncoderByType(kMimeH264);
    if (!codec_) {
        LOGE("[Encoder] createEncoderByType(%s) failed.", kMimeH264);
        return false;
    }

    AMediaFormat* fmt = MakeEncoderFormat(cfg);
    const media_status_t configured =
        AMediaCodec_configure(codec_, fmt, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
    AMediaFormat_delete(fmt);
    if (configured != AMEDIA_OK) {
        LOGE("[Encoder] configure failed: %d", int(configured));
        Finish();
        return false;
    }

    const media_status_t surfaced = AMediaCodec_createInputSurface(codec_, &inputWindow_);
    if (surfaced != AMEDIA_OK || !inputWindow_) {
        LOGE("[Encoder] createInputSurface failed: %d", int(surfaced));
        Finish();
        return false;
    }

    if (AMediaCodec_start(codec_) != AMEDIA_OK) {
        LOGE("[Encoder] start failed.");
        Finish();
        return false;
    }

    parameterSets_.clear();
    syncRequested_.store(false, std::memory_order_relaxed);
    draining_.store(true, std::memory_order_release);
    drainThread_ = std::thread([this] { DrainLoop(); });

    LOGI("[Encoder] %s H.264 %ux%u @%ufps, %.1f Mbps, CBR%s.", BackendName(), cfg.width,
        cfg.height, cfg.fps, cfg.bitrateBps / 1e6, cfg.lowLatency ? ", low latency" : "");
    return true;
}

bool MediaCodecEncoder::Encode(void*, uint64_t, bool forceKeyframe) {
    if (!codec_) return false;
    if (forceKeyframe) syncRequested_.store(true, std::memory_order_release);
    return true;
}

bool MediaCodecEncoder::SetParameter(const char* key, int32_t value) {
    if (!codec_) return false;
    AMediaFormat* params = AMediaFormat_new();
    AMediaFormat_setInt32(params, key, value);
    const media_status_t st = AMediaCodec_setParameters(codec_, params);
    AMediaFormat_delete(params);
    return st == AMEDIA_OK;
}

bool MediaCodecEncoder::SetBitrate(uint32_t bitrateBps) {
    if (!bitrateBps || !SetParameter("video-bitrate", int32_t(bitrateBps))) return false;
    cfg_.bitrateBps = bitrateBps;
    return true;
}

bool MediaCodecEncoder::SetFps(uint32_t fps) {
    if (!codec_ || !fps || fps == cfg_.fps) return codec_ != nullptr;
    cfg_.fps = fps;
    SetParameter("max-fps-to-encoder", int32_t(fps));
    return SetBitrate(cfg_.bitrateBps);
}

void MediaCodecEncoder::ApplyPendingSyncRequest() {
    if (!syncRequested_.exchange(false, std::memory_order_acq_rel)) return;
    SetParameter("request-sync", 0);
}

void MediaCodecEncoder::EmitAccessUnit(const uint8_t* payload, size_t size, uint64_t ptsUs) {
    if (!cfg_.onPacket) return;

    const std::span<const uint8_t> unit(payload, size);
    const bool keyframe = deskhub::media::ContainsIdr(unit);
    const bool inlineParameterSets = deskhub::media::FirstVclOffset(unit) > 0;

    annexb_.clear();
    annexb_.reserve(size + parameterSets_.size());
    if (keyframe && !inlineParameterSets)
        annexb_.insert(annexb_.end(), parameterSets_.begin(), parameterSets_.end());

    const std::vector<uint8_t> lowDelay =
        inlineParameterSets ? deskhub::media::AnnexBStreamWithZeroReorder(unit)
                            : std::vector<uint8_t>{};
    if (lowDelay.empty())
        annexb_.insert(annexb_.end(), payload, payload + size);
    else
        annexb_.insert(annexb_.end(), lowDelay.begin(), lowDelay.end());

    if (annexb_.empty()) return;
    cfg_.onPacket(annexb_.data(), annexb_.size(), ptsUs, keyframe);
}

void MediaCodecEncoder::DrainLoop() {
    while (draining_.load(std::memory_order_acquire)) {
        ApplyPendingSyncRequest();

        AMediaCodecBufferInfo info{};
        const ssize_t idx = AMediaCodec_dequeueOutputBuffer(codec_, &info, kDequeueTimeoutUs);
        if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER ||
            idx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
            continue;
        if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* of = AMediaCodec_getOutputFormat(codec_);
            int32_t w = 0, h = 0;
            AMediaFormat_getInt32(of, AMEDIAFORMAT_KEY_WIDTH, &w);
            AMediaFormat_getInt32(of, AMEDIAFORMAT_KEY_HEIGHT, &h);
            LOGI("[Encoder] output format settled at %dx%d", w, h);
            AMediaFormat_delete(of);
            continue;
        }
        if (idx < 0) {
            LOGE("[Encoder] dequeueOutputBuffer error: %zd", idx);
            break;
        }

        size_t cap = 0;
        const uint8_t* out = AMediaCodec_getOutputBuffer(codec_, size_t(idx), &cap);
        if (out && info.size > 0) {
            const uint8_t* payload = out + info.offset;
            const size_t size = size_t(info.size);
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) {
                const std::span<const uint8_t> sets(payload, size);
                const std::vector<uint8_t> lowDelay =
                    deskhub::media::AnnexBStreamWithZeroReorder(sets);
                parameterSets_ = lowDelay.empty() ? std::vector<uint8_t>(sets.begin(), sets.end())
                                                  : lowDelay;
            } else {
                EmitAccessUnit(payload, size, uint64_t(info.presentationTimeUs));
            }
        }
        AMediaCodec_releaseOutputBuffer(codec_, size_t(idx), false);
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) break;
    }
}

void MediaCodecEncoder::Finish() {
    draining_.store(false, std::memory_order_release);
    if (drainThread_.joinable()) drainThread_.join();

    if (codec_) {
        if (inputWindow_) AMediaCodec_signalEndOfInputStream(codec_);
        AMediaCodec_stop(codec_);
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (inputWindow_) {
        ANativeWindow_release(inputWindow_);
        inputWindow_ = nullptr;
    }
    parameterSets_.clear();
    annexb_.clear();
}

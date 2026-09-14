#include "decode/MediaCodecDecoder.h"

#include <media/NdkMediaFormat.h>

#include <cstring>
#include <span>

#include "deskhub/media/AnnexB.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

namespace {

constexpr const char* kMimeH264 = "video/avc";

}

MediaCodecDecoder::~MediaCodecDecoder() {
    Shutdown();
}

bool MediaCodecDecoder::Init(ANativeWindow* window, int width, int height) {
    Shutdown();
    if (!window || width <= 0 || height <= 0) return false;

    AMediaFormat* fmt = AMediaFormat_new();
    AMediaFormat_setString(fmt, AMEDIAFORMAT_KEY_MIME, kMimeH264);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_WIDTH, width);
    AMediaFormat_setInt32(fmt, AMEDIAFORMAT_KEY_HEIGHT, height);
    AMediaFormat_setInt32(fmt, "low-latency", 1);

    codec_ = AMediaCodec_createDecoderByType(kMimeH264);
    if (!codec_) {
        LOGE("[Decoder] createDecoderByType(%s) failed.", kMimeH264);
        AMediaFormat_delete(fmt);
        return false;
    }

    const media_status_t st = AMediaCodec_configure(codec_, fmt, window, nullptr, 0);
    AMediaFormat_delete(fmt);
    if (st != AMEDIA_OK) {
        LOGE("[Decoder] configure failed: %d", int(st));
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
        return false;
    }
    if (AMediaCodec_start(codec_) != AMEDIA_OK) {
        LOGE("[Decoder] start failed.");
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
        return false;
    }

    sentCsd_ = false;
    counters_.Reset();
    LOGI("[Decoder] MediaCodec H.264 %dx%d ready (low-latency).", width, height);
    return true;
}

void MediaCodecDecoder::Shutdown() {
    if (!codec_) return;
    AMediaCodec_stop(codec_);
    AMediaCodec_delete(codec_);
    codec_ = nullptr;
    sentCsd_ = false;
}

bool MediaCodecDecoder::Decode(const uint8_t* nal, size_t len, uint64_t ptsUs) {
    if (!codec_ || !nal || len == 0) return false;

    if (!sentCsd_) {
        const size_t csdLen =
            deskhub::media::FirstVclOffset(std::span<const uint8_t>(nal, len));
        if (csdLen == 0) return true;
        const ssize_t idx = AMediaCodec_dequeueInputBuffer(codec_, 100'000);
        if (idx < 0) {
            LOGE("[Decoder] no input buffer for codec config.");
            return false;
        }
        size_t cap = 0;
        uint8_t* buf = AMediaCodec_getInputBuffer(codec_, size_t(idx), &cap);
        if (!buf || cap < csdLen) {
            AMediaCodec_queueInputBuffer(codec_, size_t(idx), 0, 0, 0, 0);
            return false;
        }
        std::memcpy(buf, nal, csdLen);
        if (AMediaCodec_queueInputBuffer(codec_, size_t(idx), 0, csdLen, 0,
                AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) != AMEDIA_OK)
            return false;
        sentCsd_ = true;
    }

    const ssize_t idx = AMediaCodec_dequeueInputBuffer(codec_, 20'000);
    if (idx < 0) {
        counters_.CountCongestionDrop();
        DrainOutput();
        return true;
    }

    size_t cap = 0;
    uint8_t* buf = AMediaCodec_getInputBuffer(codec_, size_t(idx), &cap);
    if (!buf || cap < len) {
        if (buf) LOGE("[Decoder] input buffer too small: %zu < %zu", cap, len);
        AMediaCodec_queueInputBuffer(codec_, size_t(idx), 0, 0, 0, 0);
        return false;
    }
    std::memcpy(buf, nal, len);
    if (AMediaCodec_queueInputBuffer(codec_, size_t(idx), 0, len, ptsUs, 0) != AMEDIA_OK) {
        LOGE("[Decoder] queueInputBuffer failed.");
        return false;
    }

    return DrainOutput();
}

bool MediaCodecDecoder::DrainOutput() {
    if (!codec_) return false;
    for (;;) {
        AMediaCodecBufferInfo info{};
        const ssize_t idx = AMediaCodec_dequeueOutputBuffer(codec_, &info, 0);
        if (idx >= 0) {
            AMediaCodec_releaseOutputBuffer(codec_, size_t(idx), true);
            counters_.FramePresented(uint64_t(info.presentationTimeUs), NowUs());
            continue;
        }
        if (idx == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat* of = AMediaCodec_getOutputFormat(codec_);
            int32_t w = 0, h = 0;
            AMediaFormat_getInt32(of, AMEDIAFORMAT_KEY_WIDTH, &w);
            AMediaFormat_getInt32(of, AMEDIAFORMAT_KEY_HEIGHT, &h);
            LOGI("[Decoder] output format changed: %dx%d", w, h);
            AMediaFormat_delete(of);
            continue;
        }
        if (idx == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) continue;
        if (idx == AMEDIACODEC_INFO_TRY_AGAIN_LATER) return true;
        LOGE("[Decoder] dequeueOutputBuffer error: %zd", idx);
        return false;
    }
}

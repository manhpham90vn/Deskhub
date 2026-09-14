#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <VideoToolbox/VideoToolbox.h>

#include "deskhubp/media/VtDecoder.h"

#include <cstring>
#include <span>
#include <vector>

#include "deskhub/media/AnnexB.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

namespace {

struct Nal {
    const uint8_t* ptr;
    size_t len;
    uint8_t type;
};

std::vector<Nal> ParseAnnexB(const uint8_t* d, size_t n) {
    std::vector<Nal> out;
    for (const deskhub::media::NalRef& x :
        deskhub::media::ParseAnnexB(std::span<const uint8_t>(d, n)))
        out.push_back(Nal{d + x.offset, x.size, deskhub::media::H264NalType(x.header)});
    return out;
}

void AppendAvcc(std::vector<uint8_t>& out, const uint8_t* nal, size_t len) {
    deskhub::media::AppendLengthPrefixed(out, std::span<const uint8_t>(nal, len));
}

}

VtDecoder::~VtDecoder() {
    Shutdown();
}

bool VtDecoder::Init(void* layer, int width, int height) {
    Shutdown();
    if (!layer) return false;
    layer_ = layer;
    AVSampleBufferVideoRenderer* fresh =
        ((__bridge AVSampleBufferDisplayLayer*)layer).sampleBufferRenderer;
    if (fresh.requiresFlushToResumeDecoding) [fresh flush];
    counters_.Reset();
    LOGI("[Decoder] VideoToolbox H.264 target %dx%d ready (AVSampleBufferDisplayLayer).",
        width, height);
    return true;
}

void VtDecoder::Shutdown() {
    if (formatDesc_) {
        CFRelease((CMFormatDescriptionRef)formatDesc_);
        formatDesc_ = nullptr;
    }
    if (layer_) {
        AVSampleBufferDisplayLayer* l = (__bridge AVSampleBufferDisplayLayer*)layer_;
        if (timebase_) l.controlTimebase = nullptr;
        [l.sampleBufferRenderer flush];
        layer_ = nullptr;
    }
    if (timebase_) {
        CFRelease((CMTimebaseRef)timebase_);
        timebase_ = nullptr;
    }
    pacer_.Reset();
    timebaseRunning_ = false;
    paceDisabled_ = false;
    pacedCongestionRun_ = 0;
    spsLen_ = ppsLen_ = 0;
}

bool VtDecoder::EnsurePacedTimebase(uint64_t ptsUs, uint64_t nowUs) {
    if (paceDisabled_) return false;
    pacer_.ObserveArrival(ptsUs, nowUs);

    AVSampleBufferDisplayLayer* l = (__bridge AVSampleBufferDisplayLayer*)layer_;
    if (!timebase_) {
        CMTimebaseRef tb = nullptr;
        if (CMTimebaseCreateWithSourceClock(kCFAllocatorDefault, CMClockGetHostTimeClock(),
                &tb) != noErr ||
            !tb) {
            LOGW("[Decoder] no control timebase — frames will display on arrival.");
            paceDisabled_ = true;
            return false;
        }
        timebase_ = (void*)tb;
        l.controlTimebase = tb;
    }

    CMTimebaseRef tb = (CMTimebaseRef)timebase_;
    const int64_t currentUs =
        CMTimeConvertScale(CMTimebaseGetTime(tb), 1'000'000, kCMTimeRoundingMethod_Default)
            .value;
    if (!timebaseRunning_ || pacer_.NeedsResync(currentUs, nowUs)) {
        const OSStatus timeSet =
            CMTimebaseSetTime(tb, CMTimeMake(pacer_.DesiredTimebaseUs(nowUs), 1'000'000));
        const OSStatus rateSet = CMTimebaseSetRate(tb, 1.0);
        if (timeSet != noErr || rateSet != noErr) {
            DisablePacing();
            return false;
        }
        timebaseRunning_ = true;
    }
    return true;
}

void VtDecoder::DisablePacing() {
    paceDisabled_ = true;
    timebaseRunning_ = false;
    if (layer_) {
        AVSampleBufferDisplayLayer* l = (__bridge AVSampleBufferDisplayLayer*)layer_;
        if (timebase_) l.controlTimebase = nullptr;
        [l.sampleBufferRenderer flush];
    }
    if (timebase_) {
        CFRelease((CMTimebaseRef)timebase_);
        timebase_ = nullptr;
    }
    LOGW("[Decoder] paced frames were not being consumed — falling back to "
         "display-immediately.");
}

bool VtDecoder::Decode(const uint8_t* nal, size_t len, uint64_t ptsUs) {
    if (!layer_ || !nal || len == 0) return false;

    AVSampleBufferVideoRenderer* r =
        ((__bridge AVSampleBufferDisplayLayer*)layer_).sampleBufferRenderer;
    if (r.requiresFlushToResumeDecoding) {
        LOGW("[Decoder] the display layer stopped decoding while the app was off screen and "
             "swallows every frame until it is flushed — flushing; the picture comes back with "
             "the next keyframe.");
        [r flush];
        return false;
    }

    const std::vector<Nal> nals = ParseAnnexB(nal, len);

    const Nal* sps = nullptr;
    const Nal* pps = nullptr;
    for (const Nal& x : nals) {
        if (x.type == 7)
            sps = &x;
        else if (x.type == 8)
            pps = &x;
    }
    if (sps && pps && (sps->len > sizeof(sps_) || pps->len > sizeof(pps_))) {
        LOGE("[Decoder] SPS/PPS too large (%zu/%zu bytes) — rejecting frame.",
            sps->len, pps->len);
        return false;
    }
    if (sps && pps) {
        const bool changed = !formatDesc_ || sps->len != spsLen_ || pps->len != ppsLen_ ||
                             std::memcmp(sps->ptr, sps_, spsLen_) != 0 ||
                             std::memcmp(pps->ptr, pps_, ppsLen_) != 0;
        if (changed) {
            if (formatDesc_) {
                CFRelease((CMFormatDescriptionRef)formatDesc_);
                formatDesc_ = nullptr;
            }
            const uint8_t* params[2] = {sps->ptr, pps->ptr};
            const size_t sizes[2] = {sps->len, pps->len};
            CMFormatDescriptionRef fmt = nullptr;
            const OSStatus st = CMVideoFormatDescriptionCreateFromH264ParameterSets(
                kCFAllocatorDefault, 2, params, sizes, 4, &fmt);
            if (st != noErr || !fmt) {
                LOGE("[Decoder] CMVideoFormatDescription from SPS/PPS failed: %d", int(st));
                return false;
            }
            formatDesc_ = (void*)fmt;
            std::memcpy(sps_, sps->ptr, sps->len);
            spsLen_ = sps->len;
            std::memcpy(pps_, pps->ptr, pps->len);
            ppsLen_ = pps->len;
        }
    }

    if (!formatDesc_) return true;

    avcc_.clear();
    avcc_.reserve(len);
    for (const Nal& x : nals) {
        if (x.type == 7 || x.type == 8 || x.type == 9) continue;
        AppendAvcc(avcc_, x.ptr, x.len);
    }
    if (avcc_.empty()) return true;

    CMFormatDescriptionRef fmt = (CMFormatDescriptionRef)formatDesc_;

    CMBlockBufferRef bb = nullptr;
    OSStatus st = CMBlockBufferCreateWithMemoryBlock(
        kCFAllocatorDefault, nullptr, avcc_.size(), kCFAllocatorDefault, nullptr,
        0, avcc_.size(), kCMBlockBufferAssureMemoryNowFlag, &bb);
    if (st != noErr || !bb) {
        LOGE("[Decoder] CMBlockBufferCreate failed: %d", int(st));
        return false;
    }
    st = CMBlockBufferReplaceDataBytes(avcc_.data(), bb, 0, avcc_.size());
    if (st != noErr) {
        CFRelease(bb);
        return false;
    }

    CMSampleTimingInfo timing;
    timing.duration = kCMTimeInvalid;
    timing.presentationTimeStamp = CMTimeMake(int64_t(ptsUs), 1'000'000);
    timing.decodeTimeStamp = kCMTimeInvalid;
    const size_t sampleSize = avcc_.size();

    CMSampleBufferRef sb = nullptr;
    st = CMSampleBufferCreateReady(kCFAllocatorDefault, bb, fmt, 1, 1, &timing,
        1, &sampleSize, &sb);
    CFRelease(bb);
    if (st != noErr || !sb) {
        LOGE("[Decoder] CMSampleBufferCreateReady failed: %d", int(st));
        return false;
    }

    const uint64_t nowUs = NowUs();
    const bool paced = EnsurePacedTimebase(ptsUs, nowUs);
    if (!paced) {
        if (CFArrayRef atts = CMSampleBufferGetSampleAttachmentsArray(sb, true)) {
            if (CFArrayGetCount(atts) > 0) {
                CFMutableDictionaryRef d0 =
                    (CFMutableDictionaryRef)CFArrayGetValueAtIndex(atts, 0);
                CFDictionarySetValue(d0, kCMSampleAttachmentKey_DisplayImmediately,
                    kCFBooleanTrue);
            }
        }
    }

    if (r.status == AVQueuedSampleBufferRenderingStatusFailed) {
        LOGW("[Decoder] display layer failed (%s); flushing.",
            r.error ? r.error.localizedDescription.UTF8String : "unknown");
        [r flush];
        CFRelease(sb);
        return false;
    }

    if (!r.isReadyForMoreMediaData) {
        counters_.CountCongestionDrop();
        if (paced && ++pacedCongestionRun_ >= kPacedStallDrops) DisablePacing();
        CFRelease(sb);
        return true;
    }
    pacedCongestionRun_ = 0;

    [r enqueueSampleBuffer:sb];
    CFRelease(sb);

    const uint64_t shownUs = paced ? pacer_.DisplayTimeUs(ptsUs, nowUs) : nowUs;
    counters_.FramePresented(ptsUs, shownUs);
    if (paced) counters_.RecordPresentDelayMs(uint32_t((shownUs - nowUs) / 1000));
    return true;
}

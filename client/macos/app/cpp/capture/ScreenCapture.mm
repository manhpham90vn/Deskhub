#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "capture/ScreenCapture.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

#include "deskhub/control/StreamSize.h"
#include "deskhub/media/AudioTypes.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

@class DeskhubStreamOutput;

struct ScreenCapture::Impl {
    uint32_t displayId = 0;
    uint32_t fps = 60;
    uint32_t maxDim = 0;
    bool wantAudio = false;
    FrameHandler onFrame;

    dispatch_queue_t audioQueue = nullptr;
    std::mutex audioMutex;
    AudioHandler onAudio;
    std::vector<int16_t> staging;
    size_t staged = 0;

    void FeedAudio(const float* const* channels, size_t channelCount, size_t frames);

    uint32_t cliW = 0, cliH = 0;

    SCStream* stream = nil;
    SCContentFilter* filter = nil;
    SCStreamConfiguration* config = nil;
    DeskhubStreamOutput* output = nil;
    dispatch_queue_t frameQueue = nil;
    dispatch_source_t sizeTimer = nil;

    double scale = 1.0;

    std::mutex sizeMutex;
    uint32_t curW = 0, curH = 0;
    uint32_t cfgW = 0, cfgH = 0;
    uint32_t qualityPct = 100;

    std::atomic<bool> closed{false};
    std::atomic<bool> running{false};
    std::atomic<uint32_t> idleFrames{0};
};

struct DeinterleavedBufferList {
    AudioBufferList list;
    AudioBuffer extra[deskhub::media::kAudioChannels - 1];
};

@interface DeskhubStreamOutput : NSObject <SCStreamOutput, SCStreamDelegate>
@property(nonatomic, assign) ScreenCapture::Impl* impl;
- (void)forwardAudio:(CMSampleBufferRef)sampleBuffer to:(ScreenCapture::Impl*)impl;
@end

@implementation DeskhubStreamOutput

- (void)stream:(SCStream*)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
    (void)stream;
    ScreenCapture::Impl* impl = self.impl;
    if (!impl || !impl->running.load(std::memory_order_acquire)) return;
    if (!CMSampleBufferIsValid(sampleBuffer)) return;
    if (type == SCStreamOutputTypeAudio) {
        [self forwardAudio:sampleBuffer to:impl];
        return;
    }
    if (type != SCStreamOutputTypeScreen) return;

    NSArray* attachments =
        (__bridge NSArray*)CMSampleBufferGetSampleAttachmentsArray(sampleBuffer, NO);
    NSDictionary* info = attachments.count ? attachments[0] : nil;
    if (info) {
        NSNumber* st = info[SCStreamFrameInfoStatus];
        if (st && SCFrameStatus(st.integerValue) != SCFrameStatusComplete) {
            impl->idleFrames.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }

    CVPixelBufferRef pb = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pb) return;

    MacFrameInfo fi;
    fi.handle = (void*)pb;
    fi.meta.width = uint32_t(CVPixelBufferGetWidth(pb));
    fi.meta.height = uint32_t(CVPixelBufferGetHeight(pb));
    fi.meta.timestampUs = NowUs();
    if (fi.meta.width && fi.meta.height && impl->onFrame) impl->onFrame(fi);
}

- (void)forwardAudio:(CMSampleBufferRef)sampleBuffer to:(ScreenCapture::Impl*)impl {
    DeinterleavedBufferList storage{};
    AudioBufferList* list = &storage.list;
    CMBlockBufferRef block = nullptr;
    const OSStatus status = CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(sampleBuffer,
        nullptr, list, sizeof(storage), nullptr, nullptr,
        kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &block);
    if (status != noErr || list->mNumberBuffers == 0) {
        if (block) CFRelease(block);
        return;
    }

    const float* channels[deskhub::media::kAudioChannels] = {};
    const size_t channelCount = std::min<size_t>(list->mNumberBuffers,
        deskhub::media::kAudioChannels);
    for (size_t i = 0; i < channelCount; ++i)
        channels[i] = static_cast<const float*>(list->mBuffers[i].mData);

    const size_t frames = list->mBuffers[0].mDataByteSize / sizeof(float);
    if (channels[0] && frames) impl->FeedAudio(channels, channelCount, frames);
    CFRelease(block);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
    (void)stream;
    LOGW("[Capture] Stream stopped: %s",
        error ? error.localizedDescription.UTF8String : "no error");
    if (self.impl) self.impl->closed.store(true, std::memory_order_release);
}

@end

namespace {

bool CurrentSourceSize(uint32_t displayId, double scale, uint32_t& w, uint32_t& h) {
    const CGDirectDisplayID did = CGDirectDisplayID(displayId);
    const size_t pw = CGDisplayPixelsWide(did), ph = CGDisplayPixelsHigh(did);
    if (!pw || !ph) return false;
    w = deskhub::EvenDown(double(pw) * scale);
    h = deskhub::EvenDown(double(ph) * scale);
    return w && h;
}

double ScaleForFrame(CGRect frame) {
    double bestArea = 0;
    double scale = [NSScreen mainScreen] ? double([NSScreen mainScreen].backingScaleFactor) : 1.0;
    for (NSScreen* s in [NSScreen screens]) {
        const CGRect inter = CGRectIntersection(frame, s.frame);
        if (CGRectIsNull(inter)) continue;
        const double area = inter.size.width * inter.size.height;
        if (area > bestArea) {
            bestArea = area;
            scale = double(s.backingScaleFactor);
        }
    }
    return scale;
}

SCStreamConfiguration* MakeConfig(uint32_t w, uint32_t h, uint32_t fps, bool scaled,
    bool withAudio) {
    SCStreamConfiguration* cfg = [[SCStreamConfiguration alloc] init];
    cfg.width = w;
    cfg.height = h;
    cfg.pixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    cfg.minimumFrameInterval = CMTimeMake(1, int32_t(fps));
    cfg.showsCursor = YES;
    cfg.queueDepth = 8;
    cfg.capturesAudio = withAudio ? YES : NO;
    if (withAudio) {
        cfg.sampleRate = int(deskhub::media::kAudioSampleRate);
        cfg.channelCount = int(deskhub::media::kAudioChannels);
        cfg.excludesCurrentProcessAudio = YES;
    }
    cfg.scalesToFit = scaled ? YES : NO;
    return cfg;
}

deskhub::StreamSize ApplySizeLocked(ScreenCapture::Impl* impl) {
    const deskhub::StreamSize t = deskhub::TargetStreamSize(impl->curW, impl->curH, impl->maxDim,
        impl->cliW, impl->cliH, impl->qualityPct);
    if (!t.width || !t.height) return {impl->cfgW, impl->cfgH};
    if (t.width == impl->cfgW && t.height == impl->cfgH) return t;

    impl->cfgW = t.width;
    impl->cfgH = t.height;
    SCStreamConfiguration* cfg = MakeConfig(t.width, t.height, impl->fps,
        t.width != impl->curW || t.height != impl->curH, impl->wantAudio);
    impl->config = cfg;
    [impl->stream updateConfiguration:cfg
                    completionHandler:^(NSError* e) {
                      if (e)
                          LOGW("[Capture] updateConfiguration failed: %s",
                              e.localizedDescription.UTF8String);
                    }];
    return t;
}

}

ScreenCapture::ScreenCapture() : impl_(std::make_unique<Impl>()) {}

ScreenCapture::~ScreenCapture() {
    Stop();
}

uint32_t ScreenCapture::TakeIdleCount() {
    return impl_ ? impl_->idleFrames.exchange(0, std::memory_order_relaxed) : 0;
}

bool ScreenCapture::Closed() const {
    return impl_->closed.load(std::memory_order_acquire);
}

bool ScreenCapture::Start(uint64_t targetId, const deskhub::media::CaptureOptions& opt,
    FrameHandler onFrame) {
    const uint32_t displayId = uint32_t(targetId);
    const uint32_t fps = opt.fps;
    const uint32_t maxDim = opt.maxDim;
    if (!displayId) return false;
    Stop();

    impl_->displayId = displayId;
    impl_->fps = fps ? fps : 60;
    impl_->maxDim = maxDim;
    impl_->wantAudio = opt.audio;
    impl_->onFrame = std::move(onFrame);
    impl_->cliW = impl_->cliH = 0;
    impl_->qualityPct = 100;
    impl_->closed.store(false, std::memory_order_release);

    __block SCShareableContent* content = nil;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [SCShareableContent getShareableContentExcludingDesktopWindows:YES
                                               onScreenWindowsOnly:YES
                                                 completionHandler:^(SCShareableContent* c, NSError* err) {
                                                   if (err)
                                                       LOGE("[Capture] SCShareableContent failed: %s",
                                                           err.localizedDescription.UTF8String);
                                                   content = c;
                                                   dispatch_semaphore_signal(sem);
                                                 }];
    if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC)) != 0) {
        LOGE("[Capture] SCShareableContent timed out (2s).");
        return false;
    }
    if (!content) return false;

    SCDisplay* found = nil;
    for (SCDisplay* d in content.displays)
        if (uint32_t(d.displayID) == displayId) found = d;
    if (!found) {
        LOGE("[Capture] Display %u not found.", displayId);
        return false;
    }
    const CGRect srcFrame = found.frame;
    impl_->filter = [[SCContentFilter alloc] initWithDisplay:found
                                            excludingWindows:@[]];

    impl_->scale = ScaleForFrame(srcFrame);

    uint32_t w = 0, h = 0;
    if (!CurrentSourceSize(displayId, impl_->scale, w, h)) {
        w = deskhub::EvenDown(srcFrame.size.width * impl_->scale);
        h = deskhub::EvenDown(srcFrame.size.height * impl_->scale);
    }
    if (!w || !h) {
        LOGE("[Capture] Display %u has no usable size.", displayId);
        return false;
    }
    impl_->curW = w;
    impl_->curH = h;
    const deskhub::StreamSize t0 = deskhub::TargetStreamSize(w, h, impl_->maxDim, impl_->cliW,
        impl_->cliH, impl_->qualityPct);
    impl_->cfgW = t0.width;
    impl_->cfgH = t0.height;
    impl_->config = MakeConfig(t0.width, t0.height, impl_->fps,
        t0.width != w || t0.height != h, impl_->wantAudio);

    impl_->output = [[DeskhubStreamOutput alloc] init];
    impl_->output.impl = impl_.get();
    impl_->stream = [[SCStream alloc] initWithFilter:impl_->filter
                                       configuration:impl_->config
                                            delegate:impl_->output];

    impl_->frameQueue = dispatch_queue_create("com.manhpham.deskhub.capture",
        dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,
            QOS_CLASS_USER_INTERACTIVE, 0));

    NSError* err = nil;
    if (impl_->wantAudio) {
        impl_->audioQueue = dispatch_queue_create("com.manhpham.deskhub.audio",
            DISPATCH_QUEUE_SERIAL);
        NSError* audioErr = nil;
        if (![impl_->stream addStreamOutput:impl_->output
                                       type:SCStreamOutputTypeAudio
                         sampleHandlerQueue:impl_->audioQueue
                                      error:&audioErr]) {
            LOGW("[Capture] sharing without sound: %s",
                audioErr ? audioErr.localizedDescription.UTF8String : "?");
        }
    }

    if (![impl_->stream addStreamOutput:impl_->output
                                   type:SCStreamOutputTypeScreen
                     sampleHandlerQueue:impl_->frameQueue
                                  error:&err]) {
        LOGE("[Capture] addStreamOutput failed: %s",
            err ? err.localizedDescription.UTF8String : "?");
        Stop();
        return false;
    }

    __block bool started = false;
    dispatch_semaphore_t startSem = dispatch_semaphore_create(0);
    [impl_->stream startCaptureWithCompletionHandler:^(NSError* e) {
      if (e)
          LOGE("[Capture] startCapture failed: %s", e.localizedDescription.UTF8String);
      started = (e == nil);
      dispatch_semaphore_signal(startSem);
    }];
    if (dispatch_semaphore_wait(startSem, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) != 0) {
        LOGE("[Capture] startCapture timed out (5s).");
        Stop();
        return false;
    }
    if (!started) {
        Stop();
        return false;
    }
    impl_->running.store(true, std::memory_order_release);

    Impl* impl = impl_.get();
    impl_->sizeTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
        dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
    dispatch_source_set_timer(impl_->sizeTimer,
        dispatch_time(DISPATCH_TIME_NOW, 500 * NSEC_PER_MSEC),
        500 * NSEC_PER_MSEC, 100 * NSEC_PER_MSEC);
    dispatch_source_set_event_handler(impl_->sizeTimer, ^{
      if (!impl->running.load(std::memory_order_acquire)) return;
      uint32_t nw = 0, nh = 0;
      if (!CurrentSourceSize(impl->displayId, impl->scale, nw, nh)) {
          LOGI("[Capture] Display %u disappeared.", impl->displayId);
          impl->closed.store(true, std::memory_order_release);
          return;
      }
      std::lock_guard<std::mutex> lk(impl->sizeMutex);
      if (nw == impl->curW && nh == impl->curH) return;
      const uint32_t oldW = impl->curW, oldH = impl->curH;
      impl->curW = nw;
      impl->curH = nh;
      const deskhub::StreamSize t = ApplySizeLocked(impl);
      LOGI("[Capture] Source resized %ux%u -> %ux%u, streaming at %ux%u.", oldW, oldH, nw, nh,
          t.width, t.height);
    });
    dispatch_resume(impl_->sizeTimer);

    if (impl_->cfgW != w || impl_->cfgH != h)
        LOGI("[Capture] Capturing display %u: native %ux%u -> streaming %ux%u"
             " (cap %u px), max %u fps.",
            displayId, w, h, impl_->cfgW, impl_->cfgH, impl_->maxDim, impl_->fps);
    else
        LOGI("[Capture] Capturing display %u at %ux%u (scale %.1f), max %u fps.",
            displayId, w, h, impl_->scale, impl_->fps);
    return true;
}

void ScreenCapture::SetClientSize(uint32_t clientW, uint32_t clientH, uint32_t& outW,
    uint32_t& outH) {
    std::lock_guard<std::mutex> lk(impl_->sizeMutex);
    if (clientW != impl_->cliW || clientH != impl_->cliH) {
        impl_->cliW = clientW;
        impl_->cliH = clientH;
        const deskhub::StreamSize t = ApplySizeLocked(impl_.get());
        if (clientW && clientH)
            LOGI("[Capture] Client screen %ux%u -> streaming %ux%u (source %ux%u).", clientW,
                clientH, t.width, t.height, impl_->curW, impl_->curH);
    }
    outW = impl_->cfgW;
    outH = impl_->cfgH;
}

void ScreenCapture::SetQuality(uint32_t scalePct, uint32_t fps, uint32_t& outW,
    uint32_t& outH) {
    std::lock_guard<std::mutex> lk(impl_->sizeMutex);
    const uint32_t pct = scalePct ? (scalePct > 100 ? 100 : scalePct) : 100;
    const uint32_t f = fps ? fps : impl_->fps;
    if (pct != impl_->qualityPct || f != impl_->fps) {
        impl_->qualityPct = pct;
        impl_->fps = f;
        impl_->cfgW = impl_->cfgH = 0;
        const deskhub::StreamSize t = ApplySizeLocked(impl_.get());
        LOGI("[Capture] Quality step: %u%% @%ufps -> streaming %ux%u.", pct, f, t.width,
            t.height);
    }
    outW = impl_->cfgW;
    outH = impl_->cfgH;
}

void ScreenCapture::Impl::FeedAudio(const float* const* channels, size_t channelCount,
    size_t frames) {
    std::lock_guard<std::mutex> lock(audioMutex);
    if (!onAudio) return;
    if (staging.empty()) staging.assign(deskhub::media::kAudioSamplesPerFrameInterleaved, 0);

    for (size_t f = 0; f < frames; ++f) {
        for (size_t c = 0; c < deskhub::media::kAudioChannels; ++c) {
            const float sample = channels[std::min(c, channelCount - 1)][f];
            const float scaled = std::clamp(sample, -1.0f, 1.0f) * 32767.0f;
            staging[staged++] = int16_t(scaled);
            if (staged < staging.size()) continue;
            onAudio(staging);
            staged = 0;
        }
    }
}

void ScreenCapture::SetAudioHandler(AudioHandler onAudio) {
    std::lock_guard<std::mutex> lock(impl_->audioMutex);
    impl_->onAudio = std::move(onAudio);
    impl_->staged = 0;
}

void ScreenCapture::Stop() {
    if (!impl_) return;
    impl_->running.store(false, std::memory_order_release);

    if (impl_->sizeTimer) {
        dispatch_source_cancel(impl_->sizeTimer);
        impl_->sizeTimer = nil;
    }

    if (impl_->stream) {
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        [impl_->stream stopCaptureWithCompletionHandler:^(NSError* e) {
          (void)e;
          dispatch_semaphore_signal(sem);
        }];
        dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC));
        impl_->stream = nil;
    }

    if (impl_->output) {
        impl_->output.impl = nullptr;
        impl_->output = nil;
    }
    impl_->filter = nil;
    impl_->config = nil;
    impl_->frameQueue = nil;
    impl_->onFrame = nullptr;
}

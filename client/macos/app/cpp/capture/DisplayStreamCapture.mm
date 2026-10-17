#import <CoreGraphics/CoreGraphics.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <IOSurface/IOSurface.h>

#include "capture/DisplayStreamCapture.h"

#include <utility>

#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

constexpr int kQueueDepth = 3;
constexpr OSType kPixelFormat = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;

void OfferSurface(IOSurfaceRef surface, const DisplayStreamCapture::FrameHandler& onFrame) {
    CVPixelBufferRef pixelBuffer = nullptr;
    if (CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface, nullptr, &pixelBuffer) !=
            kCVReturnSuccess ||
        !pixelBuffer)
        return;
    MacFrameInfo frame;
    frame.handle = pixelBuffer;
    frame.meta.width = uint32_t(CVPixelBufferGetWidth(pixelBuffer));
    frame.meta.height = uint32_t(CVPixelBufferGetHeight(pixelBuffer));
    frame.meta.timestampUs = NowUs();
    if (frame.meta.width && frame.meta.height) onFrame(frame);
    CVPixelBufferRelease(pixelBuffer);
}

}

DisplayStreamCapture::~DisplayStreamCapture() {
    Stop();
}

bool DisplayStreamCapture::Start(uint32_t displayId, uint32_t width, uint32_t height,
    uint32_t fps, FrameHandler onFrame, IdleHandler onIdle) {
    Stop();
    if (!displayId || !width || !height || !fps) return false;
    dispatch_queue_t queue = dispatch_queue_create("com.manhpham.deskhub.displaystream",
        dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_USER_INTERACTIVE,
            0));
    NSDictionary* properties = @{
        (__bridge NSString*)kCGDisplayStreamMinimumFrameTime : @(1.0 / double(fps)),
        (__bridge NSString*)kCGDisplayStreamShowCursor : @YES,
        (__bridge NSString*)kCGDisplayStreamQueueDepth : @(kQueueDepth),
    };
    FrameHandler frameHandler = std::move(onFrame);
    IdleHandler idleHandler = std::move(onIdle);
    CGDisplayStreamRef stream = CGDisplayStreamCreateWithDispatchQueue(CGDirectDisplayID(displayId),
        size_t(width), size_t(height), int32_t(kPixelFormat), (__bridge CFDictionaryRef)properties,
        queue,
        ^(CGDisplayStreamFrameStatus status, uint64_t, IOSurfaceRef surface,
            CGDisplayStreamUpdateRef) {
          if (status == kCGDisplayStreamFrameStatusFrameComplete && surface)
              OfferSurface(surface, frameHandler);
          else if (status == kCGDisplayStreamFrameStatusFrameIdle && idleHandler)
              idleHandler();
        });
    if (!stream) {
        LOGE("[Capture] CGDisplayStream refused display %u at %ux%u.", displayId, width, height);
        return false;
    }
    const CGError started = CGDisplayStreamStart(stream);
    if (started != kCGErrorSuccess) {
        LOGE("[Capture] CGDisplayStreamStart failed for display %u: %d", displayId, int(started));
        CFRelease(stream);
        return false;
    }
    stream_ = (void*)stream;
    queue_ = (__bridge_retained void*)queue;
    LOGI("[Capture] Display %u streams through CGDisplayStream at %ux%u, max %u fps.", displayId,
        width, height, fps);
    return true;
}

void DisplayStreamCapture::Stop() {
    if (stream_) {
        auto stream = static_cast<CGDisplayStreamRef>(stream_);
        CGDisplayStreamStop(stream);
        CFRelease(stream);
        stream_ = nullptr;
    }
    if (queue_) {
        dispatch_queue_t queue = (__bridge_transfer dispatch_queue_t)queue_;
        queue = nil;
        queue_ = nullptr;
    }
}

#pragma clang diagnostic pop

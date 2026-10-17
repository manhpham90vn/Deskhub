#import <AppKit/AppKit.h>

#include "deskhubp/input/LocalInput.h"

#include <atomic>

#include "deskhubp/system/Clock.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/UnattendedMode.h"

struct LocalInputMonitor::Impl {
    std::atomic<uint64_t> lastUs{0};
    void* monitor = nullptr;
};

LocalInputMonitor::LocalInputMonitor() : impl_(std::make_unique<Impl>()) {}

LocalInputMonitor::~LocalInputMonitor() {
    Stop();
}

void LocalInputMonitor::Start() {
    Impl* im = impl_.get();
    if (im->monitor) return;
    if (deskhubp::UnattendedMode()) {
        LOGI("[Input] No local input monitor before sign-in: nobody sits at the login window, "
             "and AppKit has no event connection there.");
        return;
    }

    dispatch_block_t install = ^{
      if (im->monitor) return;
      const NSEventMask mask = NSEventMaskKeyDown | NSEventMaskKeyUp |
                               NSEventMaskFlagsChanged | NSEventMaskMouseMoved |
                               NSEventMaskLeftMouseDown | NSEventMaskRightMouseDown |
                               NSEventMaskOtherMouseDown | NSEventMaskLeftMouseDragged |
                               NSEventMaskRightMouseDragged | NSEventMaskScrollWheel;
      id m = [NSEvent addGlobalMonitorForEventsMatchingMask:mask
                                                    handler:^(NSEvent* e) {
                                                      CGEventRef cg = e.CGEvent;
                                                      if (cg &&
                                                          CGEventGetIntegerValueField(cg, kCGEventSourceUserData) ==
                                                              LocalInputMonitor::kInjectedUserData)
                                                          return;
                                                      im->lastUs.store(NowUs(), std::memory_order_relaxed);
                                                    }];
      im->monitor = (__bridge_retained void*)m;
    };
    if ([NSThread isMainThread])
        install();
    else
        dispatch_sync(dispatch_get_main_queue(), install);

    if (!im->monitor)
        LOGW("[Input] Could not install local input monitor — "
             "\"host wins\" arbitration is off (needs Accessibility permission).");
}

void LocalInputMonitor::Stop() {
    Impl* im = impl_.get();
    if (!im->monitor) return;
    void* m = im->monitor;
    im->monitor = nullptr;
    dispatch_block_t remove = ^{
      [NSEvent removeMonitor:(__bridge_transfer id)m];
    };
    if ([NSThread isMainThread])
        remove();
    else
        dispatch_sync(dispatch_get_main_queue(), remove);
}

uint64_t LocalInputMonitor::lastLocalUs() const {
    return impl_->lastUs.load(std::memory_order_relaxed);
}

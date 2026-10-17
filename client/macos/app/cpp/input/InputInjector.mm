#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hidsystem/IOHIDLib.h>
#import <IOKit/hidsystem/IOLLEvent.h>

#include "input/InputInjector.h"

#include <cmath>

#include "deskhub/input/PointerMap.h"
#include "deskhub/input/VirtualKeys.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/UnattendedMode.h"
#include "deskhubp/input/LocalInput.h"
#include "deskhubp/input/NativeKeyMap.h"
#include "Permissions.h"
#include "deskhubp/system/Clock.h"

namespace {

constexpr uint64_t kRectCacheUs = 200'000;
constexpr uint8_t kHidFullPressure = 255;
constexpr int32_t kHidWheelPointsPerLine = 10;

constexpr uint64_t kDoubleClickUs = 500'000;
constexpr double kDoubleClickSlopPt = 4.0;

CGEventType MoveTypeFor(const std::set<deskhub::MouseButton>& down) {
    if (down.count(deskhub::MouseButton::Left)) return kCGEventLeftMouseDragged;
    if (down.count(deskhub::MouseButton::Right)) return kCGEventRightMouseDragged;
    if (!down.empty()) return kCGEventOtherMouseDragged;
    return kCGEventMouseMoved;
}

bool ButtonCodes(deskhub::MouseButton b, CGEventType& downType, CGEventType& upType,
    CGMouseButton& which) {
    switch (b) {
        case deskhub::MouseButton::Left:
            downType = kCGEventLeftMouseDown;
            upType = kCGEventLeftMouseUp;
            which = kCGMouseButtonLeft;
            return true;
        case deskhub::MouseButton::Right:
            downType = kCGEventRightMouseDown;
            upType = kCGEventRightMouseUp;
            which = kCGMouseButtonRight;
            return true;
        case deskhub::MouseButton::Middle:
            downType = kCGEventOtherMouseDown;
            upType = kCGEventOtherMouseUp;
            which = kCGMouseButtonCenter;
            return true;
        case deskhub::MouseButton::X1:
            downType = kCGEventOtherMouseDown;
            upType = kCGEventOtherMouseUp;
            which = CGMouseButton(3);
            return true;
        case deskhub::MouseButton::X2:
            downType = kCGEventOtherMouseDown;
            upType = kCGEventOtherMouseUp;
            which = CGMouseButton(4);
            return true;
    }
    return false;
}

io_connect_t OpenHidSystem() {
    const io_service_t service =
        IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(kIOHIDSystemClass));
    if (!service) {
        LOGW("[Input] IOHIDSystem is not registered; input before sign-in is off.");
        return 0;
    }
    io_connect_t connect = 0;
    const kern_return_t rc =
        IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &connect);
    IOObjectRelease(service);
    if (rc != KERN_SUCCESS) {
        LOGW("[Input] IOHIDSystem could not be opened (0x%x); input before sign-in is off.",
            unsigned(rc));
        return 0;
    }
    LOGI("[Input] Posting input through IOHIDSystem before sign-in.");
    return connect;
}

uint32_t HidButtonType(deskhub::MouseButton btn, bool down) {
    switch (btn) {
        case deskhub::MouseButton::Left: return down ? NX_LMOUSEDOWN : NX_LMOUSEUP;
        case deskhub::MouseButton::Right: return down ? NX_RMOUSEDOWN : NX_RMOUSEUP;
        default: return down ? NX_OMOUSEDOWN : NX_OMOUSEUP;
    }
}

uint8_t HidButtonNumber(deskhub::MouseButton btn) {
    switch (btn) {
        case deskhub::MouseButton::Left: return 0;
        case deskhub::MouseButton::Right: return 1;
        default: return 2;
    }
}

uint32_t HidMoveType(const std::set<deskhub::MouseButton>& down) {
    if (down.count(deskhub::MouseButton::Left)) return NX_LMOUSEDRAGGED;
    if (down.count(deskhub::MouseButton::Right)) return NX_RMOUSEDRAGGED;
    if (!down.empty()) return NX_OMOUSEDRAGGED;
    return NX_MOUSEMOVED;
}

}

InputInjector::InputInjector() {
    if (deskhubp::UnattendedMode()) {
        hid_ = OpenHidSystem();
        return;
    }
    source_ = (void*)CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source_) LOGW("[Input] CGEventSourceCreate failed — using default source.");
}

InputInjector::~InputInjector() {
    ReleaseAll();
    if (hid_) {
        IOServiceClose(hid_);
        hid_ = 0;
    }
    if (source_) {
        CFRelease((CGEventSourceRef)source_);
        source_ = nullptr;
    }
}

bool InputInjector::Init(uint32_t displayId) {
    if (!displayId) return false;
    if (deskhubp::UnattendedMode() && !hid_) return false;
    displayId_ = displayId;
    rectUs_ = 0;

    if (!macperm::HasAccessibility()) {
        LOGW("[Input] Accessibility permission missing — injected input will be "
             "silently dropped by macOS until it is granted.");
    }

    double x, y, w, h;
    if (!SourceRect(x, y, w, h)) {
        LOGE("[Input] Display %u has no bounds — input disabled for it.", displayId);
        return false;
    }
    return true;
}

CGPoint InputInjector::CursorPoint() const {
    if (deskhubp::UnattendedMode()) return CGPointMake(postedX_, postedY_);
    CGEventRef probe = CGEventCreate(nullptr);
    const CGPoint p = probe ? CGEventGetLocation(probe) : CGPointZero;
    if (probe) CFRelease(probe);
    return p;
}

CGRect InputInjector::DesktopBounds() const {
    if (deskhubp::UnattendedMode()) return CGRectMake(rectX_, rectY_, rectW_, rectH_);
    CGRect all = CGRectNull;
    for (NSScreen* s in [NSScreen screens])
        all = CGRectIsNull(all) ? s.frame : CGRectUnion(all, s.frame);
    return CGRectIsNull(all) ? CGRectMake(0, 0, 1920, 1080) : all;
}

bool InputInjector::SourceRect(double& x, double& y, double& w, double& h) {
    const uint64_t now = NowUs();
    const bool fresh = rectUs_ && now - rectUs_ < kRectCacheUs;
    if (rectW_ > 0 && (fresh || deskhubp::UnattendedMode())) {
        x = rectX_;
        y = rectY_;
        w = rectW_;
        h = rectH_;
        return true;
    }

    const CGRect r = CGDisplayBounds(CGDirectDisplayID(displayId_));
    if (r.size.width <= 0) return false;
    rectX_ = r.origin.x;
    rectY_ = r.origin.y;
    rectW_ = r.size.width;
    rectH_ = r.size.height;

    rectUs_ = now;
    x = rectX_;
    y = rectY_;
    w = rectW_;
    h = rectH_;
    return true;
}

uint64_t InputInjector::CurrentFlags() const {
    CGEventFlags f = 0;
    for (const auto& [vk, keycode] : held_.heldKeys()) {
        switch (deskhub::ModifierKeyOf(vk)) {
            case deskhub::ModifierKey::Shift: f |= kCGEventFlagMaskShift; break;
            case deskhub::ModifierKey::Control: f |= kCGEventFlagMaskControl; break;
            case deskhub::ModifierKey::Menu: f |= kCGEventFlagMaskAlternate; break;
            case deskhub::ModifierKey::Win: f |= kCGEventFlagMaskCommand; break;
            case deskhub::ModifierKey::CapsLock: f |= kCGEventFlagMaskAlphaShift; break;
            case deskhub::ModifierKey::None: break;
        }
    }
    return uint64_t(f);
}

void InputInjector::Apply(const deskhub::InputEvent& e) {
    if (!enabled()) return;
    DispatchInput(e, localUserActive());
}

void InputInjector::OnLocalUserTookOver() {
    LOGI("[Input] Someone is using this Mac — remote input paused.");
}

void InputInjector::OnLocalUserIdle() {
    LOGI("[Input] Remote input resumed.");
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool InputInjector::HidPost(uint32_t type, const void* data, uint32_t options) {
    const IOGPoint at{SInt16(postedX_), SInt16(postedY_)};
    const kern_return_t rc = IOHIDPostEvent(hid_, type, at,
        static_cast<const NXEventData*>(data), kNXEventDataVersion,
        IOOptionBits(CurrentFlags()), options | kIOHIDSetGlobalEventFlags);
    if (rc == KERN_SUCCESS) return true;
    LOGW("[Input] IOHIDPostEvent(%u) failed: 0x%x.", unsigned(type), unsigned(rc));
    return false;
}
#pragma clang diagnostic pop

void InputInjector::SendKey(int32_t vk, int32_t, bool down) {
    int32_t native = 0;
    if (!deskhubp::WinVkToNative(vk, native)) return;
    const uint16_t keycode = uint16_t(native);

    held_.SetKey(vk, keycode, down);
    if (hid_) {
        NXEventData data{};
        data.key.keyCode = keycode;
        data.key.charSet = NX_ASCIISET;
        data.key.origCharSet = NX_ASCIISET;
        const bool modifier = deskhub::ModifierKeyOf(vk) != deskhub::ModifierKey::None;
        HidPost(modifier ? NX_FLAGSCHANGED : (down ? NX_KEYDOWN : NX_KEYUP), &data, 0);
        return;
    }

    CGEventRef ev = CGEventCreateKeyboardEvent((CGEventSourceRef)source_,
        CGKeyCode(keycode), down);
    if (!ev) return;
    CGEventSetFlags(ev, CGEventFlags(CurrentFlags()));
    CGEventSetIntegerValueField(ev, kCGEventSourceUserData, LocalInputMonitor::kInjectedUserData);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

void InputInjector::PostMouseAt(double x, double y, int32_t dx, int32_t dy) {
    const CGEventType type = MoveTypeFor(held_.heldButtons());
    CGMouseButton which = kCGMouseButtonLeft;
    if (held_.ButtonIsDown(deskhub::MouseButton::Right))
        which = kCGMouseButtonRight;
    else if (held_.ButtonIsDown(deskhub::MouseButton::Middle))
        which = kCGMouseButtonCenter;

    postedX_ = x;
    postedY_ = y;
    if (hid_) {
        NXEventData data{};
        data.mouseMove.dx = dx;
        data.mouseMove.dy = dy;
        HidPost(HidMoveType(held_.heldButtons()), &data, kIOHIDSetCursorPosition);
        return;
    }
    CGEventRef ev = CGEventCreateMouseEvent((CGEventSourceRef)source_, type,
        CGPointMake(x, y), which);
    if (!ev) return;
    CGEventSetIntegerValueField(ev, kCGMouseEventDeltaX, dx);
    CGEventSetIntegerValueField(ev, kCGMouseEventDeltaY, dy);
    CGEventSetFlags(ev, CGEventFlags(CurrentFlags()));
    CGEventSetIntegerValueField(ev, kCGEventSourceUserData, LocalInputMonitor::kInjectedUserData);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

void InputInjector::SendMoveAbsolute(int32_t nx, int32_t ny) {
    double rx, ry, rw, rh;
    if (!SourceRect(rx, ry, rw, rh)) return;
    const double x = deskhub::AbsCoordToAxis(nx, rx, rw);
    const double y = deskhub::AbsCoordToAxis(ny, ry, rh);
    PostMouseAt(x, y, 0, 0);
}

void InputInjector::SendMoveRelative(int32_t dx, int32_t dy) {
    const CGPoint cur = CursorPoint();
    const CGRect bounds = DesktopBounds();
    double x = cur.x + double(dx);
    double y = cur.y + double(dy);
    x = std::fmin(std::fmax(x, CGRectGetMinX(bounds)), CGRectGetMaxX(bounds) - 1);
    y = std::fmin(std::fmax(y, CGRectGetMinY(bounds)), CGRectGetMaxY(bounds) - 1);
    PostMouseAt(x, y, dx, dy);
}

void InputInjector::SendButton(deskhub::MouseButton btn, bool down) {
    CGEventType downType, upType;
    CGMouseButton which;
    if (!ButtonCodes(btn, downType, upType, which)) return;

    const CGPoint p = CursorPoint();

    if (down) {
        const uint64_t now = NowUs();
        const bool near = std::fabs(p.x - lastClickX_) <= kDoubleClickSlopPt &&
                          std::fabs(p.y - lastClickY_) <= kDoubleClickSlopPt;
        if (btn == lastClickBtn_ && near && lastClickUs_ && now - lastClickUs_ <= kDoubleClickUs)
            ++clickState_;
        else
            clickState_ = 1;
        lastClickUs_ = now;
        lastClickX_ = p.x;
        lastClickY_ = p.y;
        lastClickBtn_ = btn;
    }
    held_.SetButton(btn, down);
    if (hid_) {
        NXEventData data{};
        data.mouse.click = SInt32(clickState_);
        data.mouse.pressure = down ? kHidFullPressure : 0;
        data.mouse.buttonNumber = HidButtonNumber(btn);
        data.mouse.subType = NX_SUBTYPE_DEFAULT;
        HidPost(HidButtonType(btn, down), &data, 0);
        return;
    }

    CGEventRef ev = CGEventCreateMouseEvent((CGEventSourceRef)source_,
        down ? downType : upType, p, which);
    if (!ev) return;
    CGEventSetIntegerValueField(ev, kCGMouseEventClickState, clickState_);
    CGEventSetFlags(ev, CGEventFlags(CurrentFlags()));
    CGEventSetIntegerValueField(ev, kCGEventSourceUserData, LocalInputMonitor::kInjectedUserData);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

void InputInjector::SendWheel(int32_t delta) {
    if (!delta) return;
    const int32_t lines = deskhub::TakeWheelNotches(delta, wheelCarry_) * 3;
    if (!lines) return;
    if (hid_) {
        NXEventData data{};
        data.scrollWheel.deltaAxis1 = SInt16(lines);
        data.scrollWheel.fixedDeltaAxis1 = lines << 16;
        data.scrollWheel.pointDeltaAxis1 = lines * kHidWheelPointsPerLine;
        HidPost(NX_SCROLLWHEELMOVED, &data, 0);
        return;
    }
    CGEventRef ev = CGEventCreateScrollWheelEvent((CGEventSourceRef)source_,
        kCGScrollEventUnitLine, 1, lines);
    if (!ev) return;
    CGEventSetFlags(ev, CGEventFlags(CurrentFlags()));
    CGEventSetIntegerValueField(ev, kCGEventSourceUserData, LocalInputMonitor::kInjectedUserData);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

void InputInjector::ReleaseAll() {
    if (held_.nothingHeld()) return;
    const auto keys = held_.heldKeys();
    for (const auto& [vk, keycode] : keys) SendKey(vk, 0, false);
    for (deskhub::MouseButton b : held_.TakeHeldButtons()) SendButton(b, false);
}

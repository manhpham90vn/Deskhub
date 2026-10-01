#include "input/InputInjector.h"

#include <linux/uinput.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>

#include "deskhubp/diag/Log.h"
#include "deskhub/input/PointerMap.h"
#include "deskhubp/input/LocalInput.h"
#include "deskhubp/input/NativeKeyMap.h"

namespace {

bool WinVkToEvdev(int32_t vk, uint16_t& code) {
    int32_t native = 0;
    if (!deskhubp::WinVkToNative(vk, native)) return false;
    code = uint16_t(native);
    return true;
}

bool Emit(int fd, uint16_t type, uint16_t code, int32_t value) {
    if (fd < 0) return false;
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    return write(fd, &ev, sizeof(ev)) == ssize_t(sizeof(ev));
}

bool Sync(int fd) {
    return Emit(fd, EV_SYN, SYN_REPORT, 0);
}

int CreateDevice(const char* name, const uint16_t* keys, size_t keyCount, bool withRel,
    bool withAbs) {
    const int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LOGE(
            "[Inject] Cannot open /dev/uinput (%s). Install the deb/rpm package, or run "
            "scripts/setup-uinput.sh (make setup-linux-permissions) to add the udev rule.",
            std::strerror(errno));
        return -1;
    }

    if (keyCount) {
        ioctl(fd, UI_SET_EVBIT, EV_KEY);
        for (size_t i = 0; i < keyCount; ++i) ioctl(fd, UI_SET_KEYBIT, keys[i]);
    }
    if (withRel) {
        ioctl(fd, UI_SET_EVBIT, EV_REL);
        ioctl(fd, UI_SET_RELBIT, REL_X);
        ioctl(fd, UI_SET_RELBIT, REL_Y);
        ioctl(fd, UI_SET_RELBIT, REL_WHEEL);
        ioctl(fd, UI_SET_RELBIT, REL_HWHEEL);
    }
    if (withAbs) {
        ioctl(fd, UI_SET_EVBIT, EV_ABS);
        ioctl(fd, UI_SET_ABSBIT, ABS_X);
        ioctl(fd, UI_SET_ABSBIT, ABS_Y);
        for (uint16_t axis : {uint16_t(ABS_X), uint16_t(ABS_Y)}) {
            uinput_abs_setup abs{};
            abs.code = axis;
            abs.absinfo.minimum = 0;
            abs.absinfo.maximum = deskhub::kAbsCoordMax;
            ioctl(fd, UI_ABS_SETUP, &abs);
        }
    }

    uinput_setup us{};
    us.id.bustype = BUS_VIRTUAL;
    us.id.vendor = 0xDE5C;
    us.id.product = 0x4855;
    us.id.version = 1;
    std::snprintf(us.name, sizeof(us.name), "%s", name);
    if (ioctl(fd, UI_DEV_SETUP, &us) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        LOGE("[Inject] Failed to create virtual device \"%s\": %s", name, std::strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

}

InputInjector::InputInjector() = default;

InputInjector::~InputInjector() {
    ReleaseAll();
    for (int* fd : {&kbdFd_, &mouseFd_, &absFd_}) {
        if (*fd >= 0) {
            ioctl(*fd, UI_DEV_DESTROY);
            close(*fd);
            *fd = -1;
        }
    }
}

bool InputInjector::Init(int32_t srcX, int32_t srcY, uint32_t srcW, uint32_t srcH, int32_t deskX,
    int32_t deskY, uint32_t deskW, uint32_t deskH) {
    srcX_ = srcX;
    srcY_ = srcY;
    srcW_ = srcW;
    srcH_ = srcH;
    deskX_ = deskW ? deskX : srcX;
    deskY_ = deskH ? deskY : srcY;
    deskW_ = deskW ? deskW : srcW;
    deskH_ = deskH ? deskH : srcH;

    uint16_t keys[256];
    size_t nKeys = 0;
    for (int vk = 0; vk < 256 && nKeys < 256; ++vk) {
        uint16_t code = 0;
        if (WinVkToEvdev(vk, code)) keys[nKeys++] = code;
    }
    kbdFd_ = CreateDevice(kKeyboardName, keys, nKeys, false, false);

    const uint16_t buttons[] = {BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA};
    mouseFd_ = CreateDevice(kPointerName, buttons, sizeof(buttons) / sizeof(buttons[0]), true,
        false);

    const uint16_t absButtons[] = {BTN_LEFT};
    absFd_ = CreateDevice(kAbsPointerName, absButtons, 1, false, true);

    if (kbdFd_ < 0 || mouseFd_ < 0 || absFd_ < 0) return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    LOGI("[Inject] Virtual input ready — source %ux%u at %d,%d inside desktop %ux%u at %d,%d.",
        srcW_, srcH_, srcX_, srcY_, deskW_, deskH_, deskX_, deskY_);
    return true;
}

void InputInjector::Apply(const deskhub::InputEvent& e) {
    if (!enabled() || kbdFd_ < 0) return;
    DispatchInput(e, localUserActive());
}

void InputInjector::OnLocalUserTookOver() {
    LOGI("[Inject] Local user is typing — remote input yields.");
}

void InputInjector::OnLocalUserIdle() {
    LOGI("[Inject] Local user idle — remote input resumed.");
}

void InputInjector::SendKey(int32_t vk, int32_t, bool down) {
    uint16_t code = 0;
    if (!down) {
        if (const uint16_t* stored = held_.FindKey(vk)) code = *stored;
    }
    if (!code && !WinVkToEvdev(vk, code)) return;

    if (!Emit(kbdFd_, EV_KEY, code, down ? 1 : 0)) return;
    Sync(kbdFd_);

    held_.SetKey(vk, code, down);
}

void InputInjector::SendButton(deskhub::MouseButton btn, bool down) {
    uint16_t code = 0;
    switch (btn) {
        case deskhub::MouseButton::Left: code = BTN_LEFT; break;
        case deskhub::MouseButton::Right: code = BTN_RIGHT; break;
        case deskhub::MouseButton::Middle: code = BTN_MIDDLE; break;
        case deskhub::MouseButton::X1: code = BTN_SIDE; break;
        case deskhub::MouseButton::X2: code = BTN_EXTRA; break;
        default: return;
    }
    if (!Emit(mouseFd_, EV_KEY, code, down ? 1 : 0)) return;
    Sync(mouseFd_);

    held_.SetButton(btn, down);
}

void InputInjector::SendMoveAbsolute(int32_t nx, int32_t ny) {
    if (!srcW_ || !srcH_ || !deskW_ || !deskH_) return;

    const int64_t globalX = deskhub::AbsCoordToPixel(nx, srcX_, srcW_);
    const int64_t globalY = deskhub::AbsCoordToPixel(ny, srcY_, srcH_);

    Emit(absFd_, EV_ABS, ABS_X, deskhub::AxisToAbsCoord(globalX, deskX_, deskW_));
    Emit(absFd_, EV_ABS, ABS_Y, deskhub::AxisToAbsCoord(globalY, deskY_, deskH_));
    Sync(absFd_);
}

void InputInjector::SendMoveRelative(int32_t dx, int32_t dy) {
    if (!dx && !dy) return;
    if (dx) Emit(mouseFd_, EV_REL, REL_X, dx);
    if (dy) Emit(mouseFd_, EV_REL, REL_Y, dy);
    Sync(mouseFd_);
}

void InputInjector::SendWheel(int32_t delta) {
    const int32_t v = deskhub::TakeWheelNotches(delta, wheelCarry_);
    if (!v) return;
    Emit(mouseFd_, EV_REL, REL_WHEEL, v);
    Sync(mouseFd_);
}

void InputInjector::ReleaseAll() {
    if (kbdFd_ < 0 || held_.nothingHeld()) return;
    LOGI("[Inject] Releasing %zu keys + %zu mouse buttons still held.", held_.heldKeyCount(),
        held_.heldButtonCount());
    ReleaseAllHeld();
}

void InputInjector::ReleaseKey(int32_t, uint16_t native) {
    Emit(kbdFd_, EV_KEY, native, 0);
    Sync(kbdFd_);
}

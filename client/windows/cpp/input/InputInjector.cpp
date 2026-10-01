#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "input/InputInjector.h"

#include <cstdio>

#include "deskhubp/diag/Log.h"
#include "deskhub/input/PointerMap.h"
#include "deskhub/input/Set1Scancodes.h"
#include "deskhubp/input/LocalInput.h"

#pragma comment(lib, "user32.lib")

namespace {

void ScreenToVirtualDesk(int px, int py, LONG& nx, LONG& ny) {
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    nx = LONG(deskhub::AxisToAbsCoord(px, vx, vw));
    ny = LONG(deskhub::AxisToAbsCoord(py, vy, vh));
}

DWORD ButtonFlag(deskhub::MouseButton b, bool down) {
    switch (b) {
        case deskhub::MouseButton::Left: return down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        case deskhub::MouseButton::Right: return down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        case deskhub::MouseButton::Middle: return down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        case deskhub::MouseButton::X1:
        case deskhub::MouseButton::X2: return down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP;
    }
    return 0;
}

}

bool InputInjector::Init(HMONITOR monitor) {
    if (!monitor) return false;
    monitor_ = monitor;
    return true;
}

void InputInjector::OnLocalUserTookOver() {
    LOGI("[Inject] Local user is active - pausing remote input (host wins).");
}

void InputInjector::OnLocalUserIdle() {
    LOGI("[Inject] Local user idle - remote input resumed.");
}

void InputInjector::SendKey(int32_t vk, int32_t scan, bool down) {
    held_.SetKey(vk, scan, down);

    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (deskhub::NeedsVirtualKeyInjection(vk))
        scan = 0;
    else if (!(scan & 0xFF))
        scan = int32_t(MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_VSC));
    if (scan & 0xFF) {
        in.ki.wScan = WORD(scan & 0xFF);
        in.ki.dwFlags |= KEYEVENTF_SCANCODE;
        if (scan & deskhub::kScanExtended) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    } else {
        in.ki.wVk = WORD(vk);
    }
    SendInput(1, &in, sizeof(INPUT));
}

void InputInjector::SendButton(deskhub::MouseButton btn, bool down) {
    held_.SetButton(btn, down);

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = ButtonFlag(btn, down);
    if (btn == deskhub::MouseButton::X1) in.mi.mouseData = XBUTTON1;
    if (btn == deskhub::MouseButton::X2) in.mi.mouseData = XBUTTON2;
    if (in.mi.dwFlags) SendInput(1, &in, sizeof(INPUT));
}

void InputInjector::SendMoveAbsolute(int32_t nx, int32_t ny) {
    MONITORINFO mi{sizeof(MONITORINFO)};
    if (!GetMonitorInfoW(monitor_, &mi)) return;
    const int w = mi.rcMonitor.right - mi.rcMonitor.left;
    const int h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    if (w <= 1 || h <= 1) return;
    POINT pt{};
    pt.x = LONG(deskhub::AbsCoordToPixel(nx, mi.rcMonitor.left, w));
    pt.y = LONG(deskhub::AbsCoordToPixel(ny, mi.rcMonitor.top, h));

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    ScreenToVirtualDesk(pt.x, pt.y, in.mi.dx, in.mi.dy);
    SendInput(1, &in, sizeof(INPUT));
}

void InputInjector::SendMoveRelative(int32_t dx, int32_t dy) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    SendInput(1, &in, sizeof(INPUT));
}

void InputInjector::Apply(const deskhub::InputEvent& e) {
    if (!enabled() || !monitor_) return;
    DispatchInput(e, localUserActive());
}

void InputInjector::SendWheel(int32_t delta) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = DWORD(delta);
    SendInput(1, &in, sizeof(INPUT));
}

void InputInjector::ReleaseAll() {
    if (held_.nothingHeld()) return;
    LOGI("[Inject] Releasing %zu keys + %zu mouse buttons still held.",
        held_.heldKeyCount(), held_.heldButtonCount());
    ReleaseAllHeld();
}

void InputInjector::ReleaseKey(int32_t vk, int32_t native) {
    SendKey(vk, native, false);
}

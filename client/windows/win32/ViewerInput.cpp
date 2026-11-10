#include "ViewerInput.h"

#include "deskhub/input/LocalCursor.h"
#include "deskhub/media/ViewFit.h"
#include "deskhub/protocol/Wire.h"

#include <windowsx.h>
#include <algorithm>
#include <cstdio>
#include <vector>

#include "deskhubp/diag/Log.h"
#include "deskhubp/ffi/ScreenFfi.h"

namespace {

constexpr USHORT kUsagePageGeneric = 0x01;
constexpr USHORT kUsageKeyboard = 0x06;
using deskhub::kScanExtended;

int32_t Normalize(int v, uint32_t extent) {
    return deskhub::NormalizeAxis(double(v), double(extent));
}

deskhub::MouseButton XButtonOf(WPARAM wp) {
    return GET_XBUTTON_WPARAM(wp) == XBUTTON1 ? deskhub::MouseButton::X1
                                              : deskhub::MouseButton::X2;
}

HHOOK g_windowsKeyHook = nullptr;
std::vector<HWND> g_windowsKeyOwners;

bool IsWindowsKey(DWORD vk) {
    return vk == VK_LWIN || vk == VK_RWIN;
}

bool FocusIsOnAViewer() {
    const HWND focus = GetFocus();
    return focus && std::find(g_windowsKeyOwners.begin(), g_windowsKeyOwners.end(), focus) !=
                        g_windowsKeyOwners.end();
}

LRESULT CALLBACK KeepWindowsKeyFromTheShell(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
        if (IsWindowsKey(key->vkCode) && FocusIsOnAViewer()) return 1;
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

void ClaimWindowsKey(HWND hwnd) {
    g_windowsKeyOwners.push_back(hwnd);
    if (g_windowsKeyHook) return;
    g_windowsKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeepWindowsKeyFromTheShell,
        GetModuleHandleW(nullptr), 0);
    if (!g_windowsKeyHook)
        LOGW(
            "[Input] SetWindowsHookEx(WH_KEYBOARD_LL) failed: %lu; Windows keeps its own "
            "Win+key shortcuts while a viewer is focused.",
            GetLastError());
}

void ReleaseWindowsKey(HWND hwnd) {
    g_windowsKeyOwners.erase(
        std::remove(g_windowsKeyOwners.begin(), g_windowsKeyOwners.end(), hwnd),
        g_windowsKeyOwners.end());
    if (!g_windowsKeyOwners.empty() || !g_windowsKeyHook) return;
    UnhookWindowsHookEx(g_windowsKeyHook);
    g_windowsKeyHook = nullptr;
}

}

bool ViewerInput::Attach(HWND hwnd, DHScreen* session) {
    if (!hwnd) return false;
    hwnd_ = hwnd;
    session_ = session;

    RAWINPUTDEVICE rid{};
    rid.usUsagePage = kUsagePageGeneric;
    rid.usUsage = kUsageKeyboard;
    rid.dwFlags = RIDEV_NOHOTKEYS;
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
        LOGE("[Input] RegisterRawInputDevices failed: %lu", GetLastError());
        return false;
    }
    ClaimWindowsKey(hwnd);
    attached_ = true;
    return true;
}

void ViewerInput::Detach() {
    if (!attached_) return;
    ReleaseHeldInput();
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = kUsagePageGeneric;
    rid.usUsage = kUsageKeyboard;
    rid.dwFlags = RIDEV_REMOVE;
    RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
    ReleaseWindowsKey(hwnd_);
    attached_ = false;
    hwnd_ = nullptr;
    session_ = nullptr;
}

void ViewerInput::ReleaseHeldInput() {
    if (session_) dh_screen_release_all_input(session_);
}

void ViewerInput::EmitButton(deskhub::MouseButton button, bool down) {
    if (down) {
        if (buttonsDown_++ == 0) SetCapture(hwnd_);
    } else if (buttonsDown_ > 0) {
        if (--buttonsDown_ == 0 && GetCapture() == hwnd_) ReleaseCapture();
    }
    if (session_) dh_screen_mouse_button(session_, int32_t(button), down);
}

void ViewerInput::OnRawInput(LPARAM lp) {
    UINT size = 0;
    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0)
        return;
    alignas(8) BYTE buf[sizeof(RAWINPUT) + 64];
    if (size > sizeof(buf)) return;
    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, buf, &size, sizeof(RAWINPUTHEADER)) != size)
        return;
    const RAWINPUT* ri = (const RAWINPUT*)buf;

    if (ri->header.dwType == RIM_TYPEKEYBOARD) {
        const RAWKEYBOARD& kb = ri->data.keyboard;
        if (kb.VKey == 0xFF) return;
        const bool down = (kb.Flags & RI_KEY_BREAK) == 0;

        if (deskhub::IsViewerShortcutVk(int32_t(kb.VKey))) return;

        int scan = kb.MakeCode;
        if (kb.Flags & RI_KEY_E0) scan |= kScanExtended;
        if (session_) dh_screen_key(session_, int32_t(kb.VKey), scan, down);
    }
}

bool ViewerInput::OnMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (!attached_ || hwnd != hwnd_) return false;

    switch (msg) {
        case WM_INPUT:
            OnRawInput(lp);
            return false;

        case WM_MOUSEMOVE: {
            RECT r{};
            GetClientRect(hwnd_, &r);
            if (session_)
                dh_screen_mouse_move(session_,
                    Normalize(GET_X_LPARAM(lp), uint32_t(r.right - r.left)),
                    Normalize(GET_Y_LPARAM(lp), uint32_t(r.bottom - r.top)));
            return true;
        }

        case WM_LBUTTONDOWN: EmitButton(deskhub::MouseButton::Left, true); return true;
        case WM_LBUTTONUP: EmitButton(deskhub::MouseButton::Left, false); return true;
        case WM_RBUTTONDOWN: EmitButton(deskhub::MouseButton::Right, true); return true;
        case WM_RBUTTONUP: EmitButton(deskhub::MouseButton::Right, false); return true;
        case WM_MBUTTONDOWN: EmitButton(deskhub::MouseButton::Middle, true); return true;
        case WM_MBUTTONUP: EmitButton(deskhub::MouseButton::Middle, false); return true;
        case WM_XBUTTONDOWN:
            EmitButton(XButtonOf(wp), true);
            return true;
        case WM_XBUTTONUP:
            EmitButton(XButtonOf(wp), false);
            return true;

        case WM_MOUSEWHEEL:
            if (session_) dh_screen_mouse_wheel(session_, GET_WHEEL_DELTA_WPARAM(wp));
            return true;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
            return true;

        case WM_KILLFOCUS:
            ReleaseHeldInput();
            return false;
    }
    return false;
}

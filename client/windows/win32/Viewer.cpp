#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Viewer.h"

#include <windows.h>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>

#include "deskhub/input/LocalCursor.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/media/ViewFit.h"
#include "deskhub/media/ViewerTitle.h"
#include "deskhub/session/client/OpenViewers.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/ffi/ScreenFfi.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "AppIcon.h"
#include "FlatButton.h"
#include "ViewerInput.h"
#include "WinControls.h"
#include "WinText.h"

namespace {

constexpr wchar_t kFrameClass[] = L"DeskhubViewerFrame";
constexpr wchar_t kVideoClass[] = L"DeskhubViewerVideo";

constexpr UINT WM_APP_STATS = WM_APP + 1;
constexpr UINT WM_APP_SIZE = WM_APP + 2;
constexpr UINT WM_APP_CLOSED = WM_APP + 3;
constexpr UINT kTimerHint = 1;
constexpr UINT kTimerClipboard = 2;

constexpr int kCursorButtonId = 101;
constexpr int kFullScreenButtonId = 102;
constexpr int kBarHeightDip = 36;
constexpr int kBarPaddingDip = 6;
constexpr int kDefaultDpi = 96;
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
constexpr DWORD kFullScreenStyle = WS_POPUP | WS_CLIPCHILDREN;

int ScreenDpi() {
    HDC screen = GetDC(nullptr);
    const int dpi = screen ? GetDeviceCaps(screen, LOGPIXELSY) : kDefaultDpi;
    if (screen) ReleaseDC(nullptr, screen);
    return dpi > 0 ? dpi : kDefaultDpi;
}

int Dip(int value) {
    return MulDiv(value, ScreenDpi(), kDefaultDpi);
}

bool IsKeyRepeat(LPARAM lp) {
    return (lp & (LPARAM(1) << 30)) != 0;
}

std::string ReadClipboardText(HWND owner) {
    if (!OpenClipboard(owner)) return {};
    std::string out;
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        if (const wchar_t* w = static_cast<const wchar_t*>(GlobalLock(h))) {
            out = ToUtf8(w);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    return out;
}

void WriteClipboardText(HWND owner, const std::string& utf8) {
    const std::wstring wide = FromUtf8(utf8);
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* dst = GlobalLock(mem)) {
            std::memcpy(dst, wide.c_str(), bytes);
            GlobalUnlock(mem);
            if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
        } else {
            GlobalFree(mem);
        }
    }
    CloseClipboard();
}

struct ViewerFrame {
    HWND hwnd = nullptr;
    HWND video = nullptr;
    HWND cursorButton = nullptr;
    HWND fullScreenButton = nullptr;
    DHScreen* session = nullptr;
    deskhub::OpenViewerCount* openCount = nullptr;
    bool viewOnly = false;
    ViewerInput input;
    std::string baseTitle;
    std::wstring shownTitle;
    uint32_t fittedW = 0, fittedH = 0;
    deskhub::LocalCursorState cursor;
    bool fullScreen = false;
    WINDOWPLACEMENT windowed{sizeof(WINDOWPLACEMENT)};

    std::mutex mu;
    std::string statsLine;
    std::string closedReason;
    uint32_t videoW = 0, videoH = 0;

    void Relayout() {
        uint32_t w, h;
        {
            std::lock_guard<std::mutex> lk(mu);
            w = videoW;
            h = videoH;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int bar = fullScreen ? 0 : Dip(kBarHeightDip);
        LayoutBar(rc.right);
        const int aw = rc.right, ah = rc.bottom - bar;
        if (!w || !h || aw <= 0 || ah <= 0) {
            MoveWindow(video, 0, bar, std::max(1, aw), std::max(1, ah), TRUE);
            return;
        }
        const deskhub::ViewRect r = deskhub::FitVideoRect(aw, ah, double(w) / double(h));
        MoveWindow(video, int(r.x), bar + int(r.y), std::max(1, int(r.width)),
            std::max(1, int(r.height)), TRUE);
    }

    void LayoutBar(int clientWidth) {
        const int show = fullScreen ? SW_HIDE : SW_SHOW;
        const int pad = Dip(kBarPaddingDip);
        const int bh = Dip(kBarHeightDip) - 2 * pad;
        int right = clientWidth - pad;
        for (HWND button : {fullScreenButton, cursorButton}) {
            if (!button) continue;
            const int bw = FlatButtonWidth(button);
            ShowWindow(button, show);
            MoveWindow(button, std::max(pad, right - bw), pad, bw, bh, TRUE);
            right -= bw + pad;
        }
    }

    void PaintBackground(HDC dc) {
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const int bar = fullScreen ? 0 : Dip(kBarHeightDip);
        const RECT barRect{rc.left, rc.top, rc.right, rc.top + bar};
        const RECT videoArea{rc.left, rc.top + bar, rc.right, rc.bottom};
        if (bar > 0) PaintButtonBar(dc, barRect);
        FillRect(dc, &videoArea, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    void ToggleFullScreen() {
        if (!fullScreen) {
            GetWindowPlacement(hwnd, &windowed);
            MONITORINFO mi{sizeof(MONITORINFO)};
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
            fullScreen = true;
            SetWindowLongPtrW(hwnd, GWL_STYLE, LONG_PTR(kFullScreenStyle | WS_VISIBLE));
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        } else {
            fullScreen = false;
            SetWindowLongPtrW(hwnd, GWL_STYLE, LONG_PTR(kWindowedStyle | WS_VISIBLE));
            SetWindowPlacement(hwnd, &windowed);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        input.SetFullScreen(fullScreen);
        Relayout();
        InvalidateRect(hwnd, nullptr, TRUE);
        SetFocus(video);
    }

    bool HidesCursorOverVideo() const {
        return !viewOnly && cursor.HidesOverVideo(true);
    }

    void ToggleLocalCursor() {
        if (viewOnly) return;
        cursor.Toggle();
        if (cursorButton) {
            SetWindowTextW(cursorButton, FromUtf8(cursor.ButtonLabel()).c_str());
            Relayout();
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        POINT p{};
        if (GetCursorPos(&p) && WindowFromPoint(p) == video)
            SetCursor(HidesCursorOverVideo() ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
        SetFocus(video);
    }

    bool HandleShortcut(WPARAM vk, bool down, LPARAM lp) {
        if (!deskhub::IsViewerShortcutVk(int32_t(vk), fullScreen)) return false;
        if (!down || IsKeyRepeat(lp)) return true;
        if (deskhub::TogglesViewerFullScreen(int32_t(vk), fullScreen)) ToggleFullScreen();
        if (int32_t(vk) == deskhub::kViewerCursorToggleVk) ToggleLocalCursor();
        return true;
    }

    void UpdateTitle() {
        std::string line;
        {
            std::lock_guard<std::mutex> lk(mu);
            line = statsLine;
        }
        const std::string title =
            viewOnly ? deskhub::ComposeViewerTitle(baseTitle, line, deskhub::kViewerViewOnlyHint)
                     : deskhub::ComposeViewerTitle(baseTitle, line);
        std::wstring t = FromUtf8(title);
        if (t == shownTitle) return;
        shownTitle = std::move(t);
        SetWindowTextW(hwnd, shownTitle.c_str());
    }

    void SizeToVideo() {
        uint32_t w, h;
        {
            std::lock_guard<std::mutex> lk(mu);
            w = videoW;
            h = videoH;
        }
        if (fullScreen || !deskhub::ShouldRefitViewer(fittedW, fittedH, w, h)) return;
        fittedW = w;
        fittedH = h;
        RECT wa{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        RECT wr{0, 0, LONG(w), LONG(h) + Dip(kBarHeightDip)};
        AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, TRUE);
        const deskhub::ViewSize fitted = deskhub::ScaleToFit(uint32_t(wr.right - wr.left),
            uint32_t(wr.bottom - wr.top),
            uint32_t(std::max<LONG>(1, wa.right - wa.left - deskhub::kViewerMarginPx)),
            uint32_t(std::max<LONG>(1, wa.bottom - wa.top - deskhub::kViewerMarginPx)));
        SetWindowPos(hwnd, nullptr, 0, 0, int(fitted.width), int(fitted.height),
            SWP_NOMOVE | SWP_NOZORDER);
    }
};

LRESULT CALLBACK VideoProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* f = (ViewerFrame*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (msg == WM_LBUTTONDOWN) SetFocus(h);
    if (f && (msg == WM_KEYDOWN || msg == WM_KEYUP) && f->HandleShortcut(wp, msg == WM_KEYDOWN, lp))
        return 0;
    if (f && msg == WM_SETCURSOR && LOWORD(lp) == HTCLIENT && f->HidesCursorOverVideo()) {
        SetCursor(nullptr);
        return TRUE;
    }
    if (f && f->input.OnMessage(h, msg, wp, lp)) return 0;
    return DefWindowProcW(h, msg, wp, lp);
}

LRESULT CALLBACK FrameProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* f = (ViewerFrame*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_SIZE:
            if (f) f->Relayout();
            return 0;
        case WM_ERASEBKGND:
            if (!f) break;
            f->PaintBackground(HDC(wp));
            return 1;
        case WM_DRAWITEM:
            if (lp && DrawFlatButton(*reinterpret_cast<const DRAWITEMSTRUCT*>(lp))) return TRUE;
            break;
        case WM_COMMAND:
            if (f && LOWORD(wp) == kFullScreenButtonId) f->ToggleFullScreen();
            if (f && LOWORD(wp) == kCursorButtonId) f->ToggleLocalCursor();
            return 0;
        case WM_TIMER:
            if (f && wp == kTimerHint) f->UpdateTitle();
            if (f && wp == kTimerClipboard && f->session) {
                char remote[deskhub::kMaxClipboardTextBytes + 1];
                if (dh_screen_clip_take(f->session, remote, sizeof(remote)) > 0) {
                    WriteClipboardText(h, remote);
                } else {
                    const std::string local = ReadClipboardText(h);
                    if (!local.empty()) dh_screen_clip_offer(f->session, local.c_str());
                }
            }
            return 0;
        case WM_APP_STATS:
            if (f) f->UpdateTitle();
            return 0;
        case WM_APP_SIZE:
            if (f) {
                f->SizeToVideo();
                f->Relayout();
            }
            return 0;
        case WM_APP_CLOSED: {
            if (!f) return 0;
            std::string reason;
            {
                std::lock_guard<std::mutex> lk(f->mu);
                reason = f->closedReason;
            }
            const std::wstring msgText = FromUtf8(deskhub::ui::kConnectionEndedTitle) + L": " +
                                         FromUtf8(reason.empty() ? deskhub::ui::kDisconnected : reason.c_str());
            MessageBoxW(h, msgText.c_str(), L"Deskhub", MB_OK | MB_ICONINFORMATION);
            DestroyWindow(h);
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            if (f) {
                f->input.Detach();
                if (f->openCount && f->openCount->Closed()) PostQuitMessage(0);
            }
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void RegisterClasses() {
    static std::once_flag once;
    std::call_once(once, [] {
        WNDCLASSW wc{};
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);

        wc.lpfnWndProc = FrameProc;
        wc.lpszClassName = kFrameClass;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassW(&wc);

        wc.lpfnWndProc = VideoProc;
        wc.lpszClassName = kVideoClass;
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassW(&wc);
    });
}

std::unique_ptr<ViewerFrame> OpenFrame(const std::string& addr, uint8_t sourceId,
    const std::string& nameUtf8, deskhub::OpenViewerCount& openCount, bool control) {
    auto f = std::make_unique<ViewerFrame>();

    f->openCount = &openCount;
    f->viewOnly = !control;
    f->baseTitle = deskhub::ViewerBaseTitle(nameUtf8);

    const std::wstring initialTitle = FromUtf8(f->baseTitle);
    f->hwnd = CreateWindowExW(0, kFrameClass, initialTitle.c_str(), kWindowedStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 600, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!f->hwnd) return nullptr;
    SetAppWindowIcon(f->hwnd);

    SetWindowLongPtrW(f->hwnd, GWLP_USERDATA, (LONG_PTR)f.get());

    f->video = CreateWindowExW(0, kVideoClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, 16, 16,
        f->hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetWindowLongPtrW(f->video, GWLP_USERDATA, (LONG_PTR)f.get());

    f->fullScreenButton = CreateFlatButton(
        f->hwnd, FromUtf8(deskhub::ui::kFullScreenButton).c_str(), kFullScreenButtonId);
    if (control)
        f->cursorButton = CreateFlatButton(
            f->hwnd, FromUtf8(f->cursor.ButtonLabel()).c_str(), kCursorButtonId);

    DHScreenCallbacks callbacks{};
    callbacks.user = f.get();
    callbacks.onStatus = [](const char* line, void* user) {
        auto* fr = (ViewerFrame*)user;
        {
            std::lock_guard<std::mutex> lk(fr->mu);
            fr->statsLine = line;
        }
        PostMessageW(fr->hwnd, WM_APP_STATS, 0, 0);
    };
    callbacks.onSize = [](uint32_t w, uint32_t h, void* user) {
        auto* fr = (ViewerFrame*)user;
        {
            std::lock_guard<std::mutex> lk(fr->mu);
            fr->videoW = w;
            fr->videoH = h;
        }
        PostMessageW(fr->hwnd, WM_APP_SIZE, 0, 0);
    };
    callbacks.onClosed = [](const char* reason, void* user) {
        auto* fr = (ViewerFrame*)user;
        {
            std::lock_guard<std::mutex> lk(fr->mu);
            fr->closedReason = reason ? reason : deskhub::ui::kDisconnected;
        }
        PostMessageW(fr->hwnd, WM_APP_CLOSED, 0, 0);
    };

    f->session = dh_screen_start(addr.c_str(), sourceId, f->video, &callbacks);
    if (!f->session) {
        DestroyWindow(f->hwnd);
        return nullptr;
    }

    if (control) f->input.Attach(f->video, f->session);

    openCount.Opened();
    SetTimer(f->hwnd, kTimerHint, 500, nullptr);
    if (deskhubp::LoadUiSettings().clipboardSync)
        SetTimer(f->hwnd, kTimerClipboard, 1000, nullptr);
    f->UpdateTitle();
    f->Relayout();
    ShowWindow(f->hwnd, SW_SHOW);
    SetFocus(f->video);
    return f;
}

}

void RunViewer(const std::string& addrUtf8, const std::vector<deskhub::SourceInfo>& sources,
    bool control) {
    RegisterClasses();
    deskhub::OpenViewerCount openFrames;

    std::vector<std::unique_ptr<ViewerFrame>> frames;
    for (const auto& s : sources)
        if (auto f = OpenFrame(addrUtf8, s.sourceId, s.name, openFrames, control))
            frames.push_back(std::move(f));
    if (frames.empty()) {
        MessageBoxW(nullptr, FromUtf8(deskhub::ui::kViewerOpenFailed).c_str(), L"Deskhub",
            MB_OK | MB_ICONWARNING);
        return;
    }

    PumpMessagesUntil(nullptr, [] { return true; });

    for (auto& f : frames)
        if (f->session) dh_screen_stop(f->session);
}

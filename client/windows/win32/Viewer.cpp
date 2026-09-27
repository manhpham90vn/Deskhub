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

#include "deskhub/input/PointerLockState.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/media/ViewFit.h"
#include "deskhub/media/ViewerTitle.h"
#include "deskhub/session/client/OpenViewers.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/ffi/ScreenFfi.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "AppIcon.h"
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
    DHScreen* session = nullptr;
    deskhub::OpenViewerCount* openCount = nullptr;
    bool viewOnly = false;
    ViewerInput input;
    std::string baseTitle;
    std::wstring shownTitle;
    uint32_t fittedW = 0, fittedH = 0;

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
        const int aw = rc.right, ah = rc.bottom;
        if (!w || !h || aw <= 0 || ah <= 0) {
            MoveWindow(video, 0, 0, std::max(1, aw), std::max(1, ah), TRUE);
            return;
        }
        const deskhub::ViewRect r = deskhub::FitVideoRect(aw, ah, double(w) / double(h));
        MoveWindow(video, int(r.x), int(r.y), std::max(1, int(r.width)),
            std::max(1, int(r.height)), TRUE);
    }

    void UpdateTitle() {
        std::string line;
        {
            std::lock_guard<std::mutex> lk(mu);
            line = statsLine;
        }
        const std::string title =
            viewOnly
                ? deskhub::ComposeViewerTitle(baseTitle, line, deskhub::kViewerViewOnlyHint)
                : deskhub::PointerLockState(input.relativeMode()).TitleFor(baseTitle, line);
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
        if (!deskhub::ShouldRefitViewer(fittedW, fittedH, w, h)) return;
        fittedW = w;
        fittedH = h;
        RECT wa{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        RECT wr{0, 0, LONG(w), LONG(h)};
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
    if (f && f->input.OnMessage(h, msg, wp, lp)) return 0;
    return DefWindowProcW(h, msg, wp, lp);
}

LRESULT CALLBACK FrameProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* f = (ViewerFrame*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_SIZE:
            if (f) f->Relayout();
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
    const std::string& nameUtf8, deskhub::OpenViewerCount& openCount, bool control,
    const std::string& passcode) {
    auto f = std::make_unique<ViewerFrame>();

    f->openCount = &openCount;
    f->viewOnly = !control;
    f->baseTitle = deskhub::ViewerBaseTitle(nameUtf8);

    const std::wstring initialTitle = FromUtf8(f->baseTitle);
    f->hwnd = CreateWindowExW(0, kFrameClass, initialTitle.c_str(), WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1024, 600, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!f->hwnd) return nullptr;
    SetAppWindowIcon(f->hwnd);

    SetWindowLongPtrW(f->hwnd, GWLP_USERDATA, (LONG_PTR)f.get());

    f->video = CreateWindowExW(0, kVideoClass, L"", WS_CHILD | WS_VISIBLE, 0, 0, 16, 16,
        f->hwnd, nullptr, GetModuleHandleW(nullptr), nullptr);
    SetWindowLongPtrW(f->video, GWLP_USERDATA, (LONG_PTR)f.get());

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

    f->session = dh_screen_start(addr.c_str(), sourceId, f->video, &callbacks,
        passcode.c_str());
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
    bool control, const std::string& passcode) {
    RegisterClasses();
    deskhub::OpenViewerCount openFrames;

    std::vector<std::unique_ptr<ViewerFrame>> frames;
    for (const auto& s : sources)
        if (auto f = OpenFrame(addrUtf8, s.sourceId, s.name, openFrames, control, passcode))
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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "SourcePickerDialog.h"

#include <string>

#include "deskhub/media/SourceLabel.h"
#include "deskhub/session/client/ConnectFlow.h"
#include "deskhub/ui/Strings.h"
#include "WinControls.h"
#include "WinText.h"

namespace {

constexpr wchar_t kWndClass[] = L"DeskhubSourcePicker";

constexpr int kIdList = 300;
constexpr int kIdOk = 301;
constexpr int kIdCancel = 302;
constexpr int kIdHint = 303;

struct State {
    HWND hwnd = nullptr;
    HWND list = nullptr;
    const std::vector<deskhub::SourceInfo>* sources = nullptr;
    std::vector<deskhub::SourceInfo> result;
    bool done = false;
};

void Confirm(State& st) {
    const LRESULT count = SendMessageW(st.list, LB_GETSELCOUNT, 0, 0);
    if (count <= 0) return;

    std::vector<int> sel(static_cast<size_t>(count), 0);
    SendMessageW(st.list, LB_GETSELITEMS, WPARAM(count), (LPARAM)sel.data());

    st.result.clear();
    for (int i : sel)
        if (i >= 0 && size_t(i) < st.sources->size())
            st.result.push_back((*st.sources)[size_t(i)]);
    if (st.result.empty()) return;
    st.done = true;
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = (State*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
        case WM_COMMAND:
            if (!st) break;
            switch (LOWORD(wp)) {
                case kIdOk: Confirm(*st); return 0;
                case kIdCancel: st->done = true; return 0;
                case kIdList:
                    if (HIWORD(wp) == LBN_DBLCLK) {
                        Confirm(*st);
                        return 0;
                    }
                    break;
            }
            break;
        case WM_CLOSE:
            if (st) st->done = true;
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}

bool ShowSourcePickerDialog(HWND owner, const std::vector<deskhub::SourceInfo>& sources,
    std::vector<deskhub::SourceInfo>& outSelected) {
    outSelected.clear();
    if (sources.empty()) return false;
    if (!deskhub::DecideAfterSourceQuery(sources).showPicker) {
        outSelected = sources;
        return true;
    }

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWndClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassW(&wc);

    constexpr int kW = 460, kH = 340;
    RECT ownerRect{};
    if (owner)
        GetWindowRect(owner, &ownerRect);
    else
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &ownerRect, 0);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - kW) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - kH) / 2;

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    RECT wr{0, 0, kW, kH};
    AdjustWindowRect(&wr, style, FALSE);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kWndClass,
        FromUtf8(deskhub::ui::kPickerTitle).c_str(),
        style, x, y, wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, wc.hInstance, nullptr);
    if (!dlg) return false;

    State st;
    st.hwnd = dlg;
    st.sources = &sources;
    SetWindowLongPtrW(dlg, GWLP_USERDATA, (LONG_PTR)&st);

    const ChildControlFactory mk(dlg, wc.hInstance);

    st.list = mk(L"LISTBOX", nullptr,
        LBS_NOTIFY | LBS_HASSTRINGS | LBS_MULTIPLESEL | WS_VSCROLL | WS_BORDER,
        12, 12, kW - 24, kH - 92, kIdList);
    for (const auto& s : sources) {
        const std::wstring line =
            FromUtf8(deskhub::media::SourcePickerLabel(s.name, s.sourceId, s.width, s.height));
        SendMessageW(st.list, LB_ADDSTRING, 0, (LPARAM)line.c_str());
    }
    SendMessageW(st.list, LB_SETSEL, TRUE, LPARAM(-1));
    mk(L"STATIC", FromUtf8(deskhub::ui::kPickerEachWindow).c_str(),
        0, 12, kH - 74, kW - 24, 18, kIdHint);
    mk(L"BUTTON", L"View", BS_DEFPUSHBUTTON, kW - 24 - 180, kH - 46, 86, 26, kIdOk);
    mk(L"BUTTON", L"Cancel", 0, kW - 24 - 88, kH - 46, 86, 26, kIdCancel);

    if (owner) EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    SetForegroundWindow(dlg);

    if (PumpMessagesUntil(dlg, [&st] { return !st.done; })) PostQuitMessage(0);

    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    DestroyWindow(dlg);

    outSelected = std::move(st.result);
    return !outSelected.empty();
}

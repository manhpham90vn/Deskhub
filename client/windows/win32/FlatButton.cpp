#include "FlatButton.h"

#include <commctrl.h>

#include "deskhub/ui/Theme.h"

namespace {

using deskhub::ui::ThemeColor;

constexpr wchar_t kHotProp[] = L"DeskhubFlatButtonHot";
constexpr UINT_PTR kHoverSubclassId = 1;
constexpr int kMaxLabelChars = 128;
constexpr int kTextPaddingDip = 14;
constexpr int kCornerDip = 6;

COLORREF Colour(ThemeColor color) {
    const deskhub::ui::Rgb rgb = deskhub::ui::ThemeRgb(color, deskhub::ui::ThemeMode::Dark);
    return RGB(rgb.r, rgb.g, rgb.b);
}

int Scale(HWND hwnd, int dip) {
    return MulDiv(dip, int(GetDpiForWindow(hwnd)), USER_DEFAULT_SCREEN_DPI);
}

HFONT ButtonFont() {
    static const HFONT font = [] {
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
            return (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        return CreateFontIndirectW(&metrics.lfMessageFont);
    }();
    return font;
}

bool IsHot(HWND button) {
    return GetPropW(button, kHotProp) != nullptr;
}

void SetHot(HWND button, bool hot) {
    if (IsHot(button) == hot) return;
    if (hot)
        SetPropW(button, kHotProp, button);
    else
        RemovePropW(button, kHotProp);
    InvalidateRect(button, nullptr, FALSE);
}

void TrackLeave(HWND button) {
    TRACKMOUSEEVENT track{};
    track.cbSize = sizeof(track);
    track.dwFlags = TME_LEAVE;
    track.hwndTrack = button;
    TrackMouseEvent(&track);
}

LRESULT CALLBACK HoverProc(HWND h, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
    if (msg == WM_MOUSEMOVE && !IsHot(h)) {
        TrackLeave(h);
        SetHot(h, true);
    }
    if (msg == WM_MOUSELEAVE) SetHot(h, false);
    if (msg == WM_NCDESTROY) {
        RemovePropW(h, kHotProp);
        RemoveWindowSubclass(h, HoverProc, kHoverSubclassId);
    }
    return DefSubclassProc(h, msg, wp, lp);
}

COLORREF FillFor(const DRAWITEMSTRUCT& item) {
    if (item.itemState & ODS_SELECTED) return Colour(ThemeColor::ControlPressed);
    if (IsHot(item.hwndItem)) return Colour(ThemeColor::ControlHover);
    return Colour(ThemeColor::Control);
}

void FillSolid(HDC dc, const RECT& rc, COLORREF colour) {
    SetDCBrushColor(dc, colour);
    FillRect(dc, &rc, (HBRUSH)GetStockObject(DC_BRUSH));
}

}

HWND CreateFlatButton(HWND parent, const wchar_t* text, int id) {
    HWND button = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0,
        0, 1, 1, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    if (!button) return nullptr;
    SendMessageW(button, WM_SETFONT, (WPARAM)ButtonFont(), FALSE);
    SetWindowSubclass(button, HoverProc, kHoverSubclassId, 0);
    return button;
}

int FlatButtonWidth(HWND button) {
    wchar_t text[kMaxLabelChars]{};
    const int length = GetWindowTextW(button, text, kMaxLabelChars);
    SIZE size{};
    if (HDC dc = GetDC(button)) {
        const HGDIOBJ previous = SelectObject(dc, ButtonFont());
        GetTextExtentPoint32W(dc, text, length, &size);
        SelectObject(dc, previous);
        ReleaseDC(button, dc);
    }
    return int(size.cx) + 2 * Scale(button, kTextPaddingDip);
}

bool DrawFlatButton(const DRAWITEMSTRUCT& item) {
    if (item.CtlType != ODT_BUTTON) return false;
    HDC dc = item.hDC;
    RECT rc = item.rcItem;
    FillSolid(dc, rc, Colour(ThemeColor::ViewerRow));

    const int corner = Scale(item.hwndItem, kCornerDip);
    const HGDIOBJ previousBrush = SelectObject(dc, GetStockObject(DC_BRUSH));
    const HGDIOBJ previousPen = SelectObject(dc, GetStockObject(DC_PEN));
    SetDCBrushColor(dc, FillFor(item));
    SetDCPenColor(dc, Colour(ThemeColor::Border));
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, corner, corner);
    SelectObject(dc, previousPen);
    SelectObject(dc, previousBrush);

    wchar_t text[kMaxLabelChars]{};
    GetWindowTextW(item.hwndItem, text, kMaxLabelChars);
    const HGDIOBJ previousFont = SelectObject(dc, ButtonFont());
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, Colour(ThemeColor::NavText));
    DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, previousFont);
    return true;
}

void PaintButtonBar(HDC dc, const RECT& bar) {
    FillSolid(dc, bar, Colour(ThemeColor::ViewerRow));
    const RECT line{bar.left, bar.bottom - 1, bar.right, bar.bottom};
    FillSolid(dc, line, Colour(ThemeColor::RowLine));
}

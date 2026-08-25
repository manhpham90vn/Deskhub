#include "TerminalWindow.h"

#include <wx/dcbuffer.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "WxUi.h"
#include "deskhub/terminal/KeyEncoder.h"
#include "deskhub/terminal/Palette.h"
#include "deskhub/terminal/ScrollAnchor.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/client/TerminalFeed.h"
#include "deskhubp/host/TerminalHost.h"
#include "deskhubp/client/TerminalViewer.h"

namespace {

namespace ui = deskhub::ui;
namespace term = deskhub::term;

constexpr int kRedrawTimerId = 41;
constexpr int kRedrawMs = 33;
constexpr int kGridPadding = 6;

wxColour ToWxColour(const term::Rgb& rgb) {
    return wxColour(rgb.r, rgb.g, rgb.b);
}

bool IsPrintableKey(int code) {
    return code >= 32 && code != WXK_DELETE;
}

term::TermMods ModsOf(const wxKeyEvent& event) {
    term::TermMods mods;
    mods.shift = event.ShiftDown();
    mods.alt = event.AltDown();
    mods.ctrl = event.ControlDown();
    return mods;
}

bool NamedKeyOf(int code, term::TermKey& out) {
    switch (code) {
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER: out = term::TermKey::Enter; return true;
        case WXK_BACK: out = term::TermKey::Backspace; return true;
        case WXK_TAB: out = term::TermKey::Tab; return true;
        case WXK_ESCAPE: out = term::TermKey::Escape; return true;
        case WXK_UP: out = term::TermKey::Up; return true;
        case WXK_DOWN: out = term::TermKey::Down; return true;
        case WXK_LEFT: out = term::TermKey::Left; return true;
        case WXK_RIGHT: out = term::TermKey::Right; return true;
        case WXK_HOME: out = term::TermKey::Home; return true;
        case WXK_END: out = term::TermKey::End; return true;
        case WXK_PAGEUP: out = term::TermKey::PageUp; return true;
        case WXK_PAGEDOWN: out = term::TermKey::PageDown; return true;
        case WXK_INSERT: out = term::TermKey::Insert; return true;
        case WXK_DELETE: out = term::TermKey::Delete; return true;
        case WXK_F1: out = term::TermKey::F1; return true;
        case WXK_F2: out = term::TermKey::F2; return true;
        case WXK_F3: out = term::TermKey::F3; return true;
        case WXK_F4: out = term::TermKey::F4; return true;
        case WXK_F5: out = term::TermKey::F5; return true;
        case WXK_F6: out = term::TermKey::F6; return true;
        case WXK_F7: out = term::TermKey::F7; return true;
        case WXK_F8: out = term::TermKey::F8; return true;
        case WXK_F9: out = term::TermKey::F9; return true;
        case WXK_F10: out = term::TermKey::F10; return true;
        case WXK_F11: out = term::TermKey::F11; return true;
        case WXK_F12: out = term::TermKey::F12; return true;
        default: return false;
    }
}

class TerminalGrid final : public wxWindow {
public:
    explicit TerminalGrid(wxWindow* parent)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS) {
        snapshot_.size = deskhub::TermSize{0, 0};
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(ToWxColour(term::kDefaultBackground));
        SetFont(wxFont(wxFontInfo(11).FaceName("Consolas").Family(wxFONTFAMILY_TELETYPE)));
        MeasureCell();
        SetMinSize(FromDIP(wxSize(640, 320)));
        Bind(wxEVT_PAINT, &TerminalGrid::OnPaint, this);
        Bind(wxEVT_SIZE, &TerminalGrid::OnSize, this);
        Bind(wxEVT_KEY_DOWN, &TerminalGrid::OnKeyDown, this);
        Bind(wxEVT_CHAR, &TerminalGrid::OnChar, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { SetFocus(); });
        Bind(wxEVT_MOUSEWHEEL, &TerminalGrid::OnWheel, this);
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent&) { Refresh(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent&) { Refresh(); });
    }

    bool AcceptsFocus() const override {
        return true;
    }
    bool AcceptsFocusFromKeyboard() const override {
        return true;
    }

    void Attach(deskhubp::TerminalFeed* feed) {
        feed_ = feed;
        PullSnapshot();
    }

    void PullSnapshot() {
        if (feed_ != nullptr && feed_->Alive()) {
            const size_t was = snapshot_.scrollbackRows;
            snapshot_ = feed_->Snapshot(scrollOffset_);
            const size_t anchored =
                term::AnchorScroll(scrollOffset_, was, snapshot_.scrollbackRows);
            if (anchored != scrollOffset_) {
                scrollOffset_ = anchored;
                snapshot_ = feed_->Snapshot(scrollOffset_);
            }
            scrollOffset_ = snapshot_.scrollOffset;
        } else {
            snapshot_ = deskhubp::TerminalSnapshot{};
            snapshot_.size = deskhub::TermSize{0, 0};
            scrollOffset_ = 0;
        }
        Refresh(false);
    }

    void ScrollBy(int rows) {
        const size_t before = scrollOffset_;
        scrollOffset_ = term::ScrollByRows(scrollOffset_, rows, snapshot_.scrollbackRows);
        if (scrollOffset_ != before) PullSnapshot();
    }

    void ScrollToBottom() {
        if (scrollOffset_ == 0) return;
        scrollOffset_ = 0;
        PullSnapshot();
    }

    deskhub::TermSize CellsThatFit() const {
        const wxSize client = GetClientSize();
        const int cols = (client.GetWidth() - 2 * kGridPadding) / std::max(1, cellWidth_);
        const int rows = (client.GetHeight() - 2 * kGridPadding) / std::max(1, cellHeight_);
        return deskhub::ClampTermSize(
            deskhub::TermSize{uint16_t(std::max(1, cols)), uint16_t(std::max(1, rows))});
    }

private:
    void MeasureCell() {
        wxClientDC dc(this);
        dc.SetFont(GetFont());
        const wxSize extent = dc.GetTextExtent("M");
        cellWidth_ = std::max(1, extent.GetWidth());
        cellHeight_ = std::max(1, extent.GetHeight());
    }

    void OnSize(wxSizeEvent& event) {
        if (onResize_) onResize_();
        Refresh(false);
        event.Skip();
    }

    void OnWheel(wxMouseEvent& event) {
        const int lines = event.GetLinesPerAction() > 0 ? event.GetLinesPerAction() : 3;
        ScrollBy(event.GetWheelRotation() > 0 ? lines : -lines);
    }

    void OnKeyDown(wxKeyEvent& event) {
        term::TermKey named{};
        const int code = event.GetKeyCode();
        if (event.ShiftDown() && (code == WXK_PAGEUP || code == WXK_PAGEDOWN)) {
            const int page = std::max(1, snapshot_.size.rows - 1);
            ScrollBy(code == WXK_PAGEUP ? page : -page);
            return;
        }
        ScrollToBottom();
        if (feed_ != nullptr && NamedKeyOf(event.GetKeyCode(), named)) {
            term::TermKeyEvent key;
            key.key = named;
            key.mods = ModsOf(event);
            feed_->SendKey(key);
            return;
        }
        if (feed_ != nullptr && event.ControlDown() && !event.AltDown()) {
            const int unicode = event.GetUnicodeKey();
            if (unicode != WXK_NONE && unicode > 0) {
                term::TermKeyEvent key;
                key.key = term::TermKey::Char;
                key.codepoint = char32_t(unicode);
                key.mods = ModsOf(event);
                feed_->SendKey(key);
                return;
            }
        }
        event.Skip();
    }

    void OnChar(wxKeyEvent& event) {
        ScrollToBottom();
        const int code = event.GetUnicodeKey();
        if (feed_ == nullptr || code == WXK_NONE || !IsPrintableKey(code)) {
            event.Skip();
            return;
        }
        if (event.ControlDown()) return;
        LOGI("[TermKey] char cp=%d", code);
        term::TermKeyEvent key;
        key.key = term::TermKey::Char;
        key.codepoint = char32_t(code);
        key.mods.alt = event.AltDown();
        feed_->SendKey(key);
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(ToWxColour(term::kDefaultBackground)));
        dc.Clear();
        dc.SetFont(GetFont());

        for (uint16_t row = 0; row < snapshot_.size.rows; ++row) {
            for (uint16_t col = 0; col < snapshot_.size.cols; ++col) {
                const term::Cell& cell = snapshot_.At(row, col);
                const term::CellColors colors = term::ResolveCell(cell.pen, false);
                const int x = kGridPadding + col * cellWidth_;
                const int y = kGridPadding + row * cellHeight_;

                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(ToWxColour(colors.bg)));
                dc.DrawRectangle(x, y, cellWidth_, cellHeight_);

                if (cell.ch == U' ') continue;
                wxFont font = GetFont();
                if ((cell.pen.attrs & term::kAttrBold) != 0) font = font.Bold();
                if ((cell.pen.attrs & term::kAttrItalic) != 0) font = font.Italic();
                if ((cell.pen.attrs & term::kAttrUnderline) != 0) font = font.Underlined();
                dc.SetFont(font);
                dc.SetTextForeground(ToWxColour(colors.fg));
                dc.DrawText(ToWx(term::EncodeUtf8(cell.ch)), x, y);
            }
        }

        if (snapshot_.cursor.visible && snapshot_.size.rows > 0) {
            const int x = kGridPadding + snapshot_.cursor.col * cellWidth_;
            const int y = kGridPadding + snapshot_.cursor.row * cellHeight_;
            dc.SetPen(wxPen(ToWxColour(term::kCursorColor)));
            dc.SetBrush(HasFocus() ? wxBrush(ToWxColour(term::kCursorColor))
                                   : *wxTRANSPARENT_BRUSH);
            dc.DrawRectangle(x, y, cellWidth_, cellHeight_);
        }
    }

public:
    std::function<void()> onResize_{};

private:
    deskhubp::TerminalFeed* feed_ = nullptr;
    deskhubp::TerminalSnapshot snapshot_{};
    size_t scrollOffset_ = 0;
    int cellWidth_ = 8;
    int cellHeight_ = 16;
};

class TerminalFrame final : public wxFrame {
public:
    TerminalFrame(wxWindow* parent, const wxString& title)
        : wxFrame(parent, wxID_ANY, title) {
        CreateStatusBar();
        grid_ = new TerminalGrid(this);
        grid_->onResize_ = [this] { ResizeToGrid(); };
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(grid_, wxSizerFlags(1).Expand());
        SetSizerAndFit(sizer);
        Bind(wxEVT_CLOSE_WINDOW, &TerminalFrame::OnClose, this);
        redrawTimer_.SetOwner(this, kRedrawTimerId);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { OnRedrawTick(); }, kRedrawTimerId);
    }

    ~TerminalFrame() override {
        redrawTimer_.Stop();
        if (feed_) feed_->Shutdown();
    }

    bool StartRemote(const TerminalLaunch& launch) {
        NetAddr host{};
        if (!ParseNetAddr(launch.address, host)) return false;

        auto feed = std::make_unique<deskhubp::RemoteTerminalFeed>();
        remote_ = feed.get();

        deskhubp::TerminalViewerConfig config;
        config.host = host;
        config.hostLabel = ui::AddressHost(launch.address);
        config.passcode = launch.passcode;
        config.clientName = launch.clientName;
        config.size = grid_->CellsThatFit();

        deskhubp::TerminalViewerCallbacks hooks;
        hooks.onState = [this](deskhubp::TerminalViewerState state, std::string_view message) {
            const std::string copy(message);
            CallAfter([this, state, copy] { OnViewerState(state, copy); });
        };
        hooks.onTrustAsked = [this](deskhub::TrustVerdict verdict, std::string_view fingerprint) {
            const std::string copy(fingerprint);
            CallAfter([this, verdict, copy] { AskAboutKey(verdict, copy); });
        };

        if (!remote_->viewer.Start(config, std::move(hooks))) {
            remote_ = nullptr;
            return false;
        }
        feed_ = std::move(feed);
        StartFeeding(ui::kTerminalConnecting);
        return true;
    }

    bool StartLocal(deskhubp::TerminalHost& host, uint32_t termId) {
        if (!host.LocalAlive(termId)) return false;
        feed_ = std::make_unique<deskhubp::LocalTerminalFeed>(host, termId);
        StartFeeding(ui::kTerminalAttachedHere);
        return true;
    }

private:
    void StartFeeding(const char* status) {
        grid_->Attach(feed_.get());
        grid_->SetFocus();
        redrawTimer_.Start(kRedrawMs);
        SetStatusText(ToWx(status));
    }

    void OnRedrawTick() {
        grid_->PullSnapshot();
        if (remote_ == nullptr && !feed_->Alive()) ShowLocalShellEnded();
    }

    void ShowLocalShellEnded() {
        if (localEndShown_) return;
        localEndShown_ = true;
        SetStatusText(ToWx(ui::kTerminalClosed));
        redrawTimer_.Stop();
    }

    void OnClose(wxCloseEvent& event) {
        redrawTimer_.Stop();
        if (feed_) feed_->Shutdown();
        grid_->Attach(nullptr);
        event.Skip();
    }

    void ResizeToGrid() {
        if (!feed_ || !feed_->Alive()) return;
        feed_->Resize(grid_->CellsThatFit());
    }

    void OnViewerState(deskhubp::TerminalViewerState state, const std::string& message) {
        SetStatusText(ToWx(message));
        if (state == deskhubp::TerminalViewerState::Live) grid_->SetFocus();
        if (state == deskhubp::TerminalViewerState::Ended ||
            state == deskhubp::TerminalViewerState::Failed ||
            state == deskhubp::TerminalViewerState::Refused) {
            redrawTimer_.Stop();
        }
    }

    void AskAboutKey(deskhub::TrustVerdict verdict, const std::string& fingerprint) {
        if (remote_ == nullptr) return;
        const bool changed = verdict == deskhub::TrustVerdict::Changed;
        wxString body = ToWx(changed ? ui::kTrustChangedBody : ui::kTrustNewHostBody);
        body += "\n\n";
        body += ToWx(ui::kTrustFingerprintLabel);
        body += " ";
        body += ToWx(fingerprint);

        wxMessageDialog dialog(this, body,
            ToWx(changed ? ui::kTrustChangedTitle : ui::kTrustNewHostTitle),
            wxYES_NO | wxNO_DEFAULT | (changed ? wxICON_WARNING : wxICON_QUESTION));
        dialog.SetYesNoLabels(ToWx(ui::kTrustAccept), ToWx(ui::kTrustReject));
        if (dialog.ShowModal() == wxID_YES)
            remote_->viewer.AcceptFingerprint();
        else
            remote_->viewer.RejectFingerprint();
    }

    std::unique_ptr<deskhubp::TerminalFeed> feed_{};
    deskhubp::RemoteTerminalFeed* remote_ = nullptr;
    bool localEndShown_ = false;
    TerminalGrid* grid_ = nullptr;
    wxTimer redrawTimer_{};
};

}

bool OpenTerminalWindow(wxWindow* parent, const TerminalLaunch& launch) {
    auto* frame = new TerminalFrame(parent, ToWx(ui::AddressHost(launch.address)));
    if (!frame->StartRemote(launch)) {
        frame->Destroy();
        return false;
    }
    frame->Show();
    frame->Raise();
    return true;
}

bool OpenHostTerminalWindow(wxWindow* parent, deskhubp::TerminalHost& host, uint32_t termId) {
    auto* frame = new TerminalFrame(parent, ToWx(ui::kTerminalLocalWindowTitle));
    if (!frame->StartLocal(host, termId)) {
        frame->Destroy();
        return false;
    }
    frame->Show();
    frame->Raise();
    return true;
}

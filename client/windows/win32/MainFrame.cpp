#include <wx/wx.h>

#include <wx/dcbuffer.h>
#include <wx/dirdlg.h>
#include <wx/hyperlink.h>
#include <wx/init.h>
#include <wx/listctrl.h>
#include <wx/notifmsg.h>
#include <wx/scrolwin.h>
#include <wx/clipbrd.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/taskbar.h>
#include <wx/weakref.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "MainFrame.h"

#include "AccessRequestListener.h"
#include "SourcePickerDialog.h"
#include "FileSendWindow.h"
#include "TerminalWindow.h"
#include "Viewer.h"
#include "WxUi.h"
#include "deskhub/auth/PairingTokens.h"
#include "deskhub/media/QualityPreset.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/net/BindAddress.h"
#include "deskhub/net/PairingInvite.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/qr/QrCode.h"
#include "deskhub/session/host/ShareFlow.h"
#include "deskhub/ui/AutoShareGate.h"
#include "deskhub/ui/HostProfiles.h"
#include "deskhub/ui/HostRows.h"
#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/SettingsLayout.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/host/PairingInvite.h"
#include "deskhubp/host/ShareDriver.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/client/HostProfiles.h"
#include "deskhubp/client/SourceQuery.h"
#include "deskhubp/client/SourceQueryAsync.h"
#include "deskhubp/host/ShareController.h"
#include "deskhubp/system/AccessRequestsFile.h"
#include "deskhubp/system/AuthProof.h"
#include "deskhubp/system/FileStore.h"
#include "deskhubp/system/FolderOpen.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/Autostart.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairingTokenFile.h"
#include "deskhubp/system/TrustStoreFile.h"
#include "deskhubp/system/RecentDevicesFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

namespace ui = deskhub::ui;

constexpr int kHostTimerId = 1;
constexpr int kClipTimerId = 3;
constexpr int kAutoShareTimerId = 4;
constexpr int kCopiedTimerId = 5;
constexpr int kCopiedButtonTimerId = 6;
constexpr int kDevicesTimerId = 7;
constexpr int kQrTickTimerId = 8;
constexpr int kQrCopiedTimerId = 9;
constexpr int kQrTickMs = 1000;
constexpr int kDevicesPollMs = 1000;
constexpr int kCopiedRevertMs = 1500;
constexpr int kQrViewDip = 240;
constexpr int kQrQuietZoneModules = 4;
constexpr int kConnectionWindowWidth = 460;
constexpr int kConnectionWindowCascade = 28;
constexpr int kListMinH = 130;
constexpr int kHostListMinH = 170;
constexpr int kPagePad = 16;
constexpr int kPageGap = 10;
constexpr int kSectionGap = 6;
constexpr int kCardPad = 10;
constexpr int kPickerPad = 8;
constexpr int kPickerGap = 6;
constexpr int kSectionMargin = 8;
constexpr int kDevicesRowGap = 6;
constexpr int kSettingsRowGap = 10;
constexpr int kSettingsAreaPad = 16;
constexpr int kSidebarLinkInset = 8;
constexpr int kWindowW = 1240;
constexpr int kWindowH = 780;
constexpr int kWindowMinW = 1000;
constexpr int kWindowMinH = 640;
constexpr int kBannerWrapWidth = 520;

enum Page { kPageHost = 0,
    kPageClient = 1,
    kPageDevices = 2,
    kPageSettings = 3,
    kPageCount = 4 };

const char* const kPageLabels[kPageCount] = {ui::kSidebarHost, ui::kSidebarClient,
    ui::kSidebarDevices, ui::kSidebarSettings};

const wxColour kSidebarBg = ThemeColour(ui::ThemeColor::Sidebar);
const wxColour kSidebarHover = ThemeColour(ui::ThemeColor::SidebarHover);
const wxColour kAccent = ThemeColour(ui::ThemeColor::Accent);
const wxColour kNavText = ThemeColour(ui::ThemeColor::NavText);
const wxColour kSidebarFootnote = ThemeColour(ui::ThemeColor::Footnote);
const wxColour kOnline = ThemeColour(ui::ThemeColor::Online);
const wxColour kOffline = ThemeColour(ui::ThemeColor::Offline);
const wxColour kErrorText = ThemeColour(ui::ThemeColor::OfflinePressed);
const wxColour kOnlineText(0x07, 0x5e, 0x2b);
const wxColour kWarning = ThemeColour(ui::ThemeColor::Warning);
const wxColour kRowLine = ThemeColour(ui::ThemeColor::RowLine);
const wxColour kViewerRowBg = ThemeColour(ui::ThemeColor::ViewerRow);
const wxColour kBannerIdleBg = ThemeColour(ui::ThemeColor::PanelIdle);
const wxColour kBannerLiveBg = ThemeColour(ui::ThemeColor::PanelLive);
const wxColour kBannerBusyBg = ThemeColour(ui::ThemeColor::PanelBusy);
const wxColour kPortCardBg = ThemeColour(ui::ThemeColor::InfoCard);

enum class HostShareState { kIdle,
    kStarting,
    kSharing };

enum class ShareTrigger { kUser,
    kAutomatic };

struct HostStateStyle {
    const char* label;
    wxColour tint;
    wxColour background;
};

constexpr int kHostColumnCount = int(ui::kHostColumns.size());
constexpr int kHostActionsWidth = ui::kHostActionWidth + ui::kHostCellGap + ui::kHostActionWidth;
constexpr int kPortPointSize = 26;

long WxAlign(ui::ColumnAlign align) {
    return align == ui::ColumnAlign::Trailing ? wxALIGN_RIGHT : wxALIGN_LEFT;
}

struct HostRowView {
    wxPanel* panel = nullptr;
    wxWindow* bar = nullptr;
    wxStaticText* cells[kHostColumnCount] = {};
};

wxFont MonoFont(const wxWindow* window) {
    wxFont font = window->GetFont();
    font.SetFamily(wxFONTFAMILY_TELETYPE);
    font.SetFaceName("Consolas");
    return font;
}

void PaintStatePill(wxStaticText* label) {
    label->SetFont(label->GetFont().Bold());
    label->SetForegroundColour(kOnlineText);
    label->SetBackgroundColour(kBannerLiveBg);
}

HostStateStyle StyleFor(HostShareState state) {
    switch (state) {
        case HostShareState::kSharing:
            return {ui::kShareStateOn, kOnline, kBannerLiveBg};
        case HostShareState::kStarting:
            return {ui::kStartingShare, kAccent, kBannerBusyBg};
        case HostShareState::kIdle: break;
    }
    return {ui::kShareStateOff, kMutedText, kBannerIdleBg};
}

struct TableRow {
    wxPanel* panel = nullptr;
    wxBoxSizer* cells = nullptr;
};

TableRow BeginTableRow(wxWindow* list, const wxColour& background) {
    TableRow row;
    row.panel = new wxPanel(list);
    row.panel->SetBackgroundColour(background);
    row.cells = new wxBoxSizer(wxHORIZONTAL);
    return row;
}

wxStaticText* AddTableCell(const TableRow& row, const wxString& value, int width, bool heading) {
    auto* label = new wxStaticText(row.panel, wxID_ANY, value, wxDefaultPosition,
        row.panel->FromDIP(wxSize(width, -1)), wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    if (heading) {
        label->SetForegroundColour(kMutedText);
        label->SetFont(label->GetFont().Bold().Scaled(0.85f));
    }
    row.cells->Add(label, wxSizerFlags().CentreVertical().Border(wxRIGHT, row.panel->FromDIP(8)));
    return label;
}

void EndTableRow(wxBoxSizer* rows, const TableRow& row) {
    auto* rowSizer = new wxBoxSizer(wxVERTICAL);
    rowSizer->Add(row.cells, wxSizerFlags(1).Expand().Border(wxALL, row.panel->FromDIP(8)));
    row.panel->SetSizer(rowSizer);
    row.panel->SetMinSize(rowSizer->GetMinSize());
    rows->Add(row.panel, wxSizerFlags().Expand());
}

void RelayoutTable(wxWindow* list) {
    if (auto* scrolled = dynamic_cast<wxScrolledWindow*>(list)) scrolled->FitInside();
    list->Layout();
    for (wxWindow* ancestor = list->GetParent(); ancestor != nullptr;
        ancestor = ancestor->GetParent()) {
        auto* page = dynamic_cast<wxScrolledWindow*>(ancestor);
        if (page == nullptr) {
            ancestor->Layout();
            continue;
        }
        page->FitInside();
        page->Layout();
        return;
    }
}

struct SettingsArea {
    wxPanel* card = nullptr;
    wxPanel* body = nullptr;
    wxBoxSizer* sizer = nullptr;
    int rowGap = kSettingsRowGap;
};

SettingsArea MakeSettingsArea(wxWindow* parent, const char* title) {
    SettingsArea area;
    area.card = new wxPanel(parent);
    area.card->SetBackgroundColour(kRowLine);
    area.body = new wxPanel(area.card);
    area.body->SetBackgroundColour(*wxWHITE);

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* bar = new wxWindow(area.body, wxID_ANY, wxDefaultPosition,
        area.body->FromDIP(wxSize(ui::kSettingsAreaBarWidth, -1)));
    bar->SetBackgroundColour(kAccent);
    row->Add(bar, wxSizerFlags().Expand());

    area.sizer = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(area.body, wxID_ANY, ToWx(title));
    heading->SetFont(heading->GetFont().Bold().Scaled(1.25f));
    heading->SetForegroundColour(kHeadingText);
    area.sizer->Add(heading);
    row->Add(area.sizer, wxSizerFlags(1).Expand().Border(wxALL, area.body->FromDIP(kSettingsAreaPad)));
    area.body->SetSizer(row);

    auto* cardSizer = new wxBoxSizer(wxVERTICAL);
    cardSizer->Add(area.body,
        wxSizerFlags(1).Expand().Border(wxTOP | wxRIGHT | wxBOTTOM, area.body->FromDIP(1)));
    area.card->SetSizer(cardSizer);
    return area;
}

wxSizerFlags AreaRowFlags(const SettingsArea& area) {
    return wxSizerFlags().Border(wxTOP, area.body->FromDIP(area.rowGap));
}

wxSizerFlags AreaWideRowFlags(const SettingsArea& area) {
    return wxSizerFlags().Expand().Border(wxTOP, area.body->FromDIP(area.rowGap));
}

wxSizerFlags DevicesSectionFlags(const wxWindow* body) {
    return wxSizerFlags().Expand().Border(wxTOP, body->FromDIP(kPageGap + kSectionMargin));
}

SettingsArea BeginDevicesSection(wxPanel* body, wxBoxSizer* page, const char* heading) {
    SettingsArea section{nullptr, body, new wxBoxSizer(wxVERTICAL), kDevicesRowGap};
    section.sizer->Add(MakeSection(body, heading));
    page->Add(section.sizer, DevicesSectionFlags(body));
    return section;
}

void AddAreaHint(const SettingsArea& area, const char* hint) {
    area.sizer->Add(MakeHint(area.body, ToWx(hint)), AreaRowFlags(area));
}

void AddAreaSection(const SettingsArea& area, const char* heading) {
    area.sizer->Add(MakeSection(area.body, heading), AreaRowFlags(area));
}

struct AccessRequestsView {
    wxPanel* list = nullptr;
    wxBoxSizer* rows = nullptr;
    wxStaticText* hint = nullptr;
};

std::vector<deskhubp::PendingClient> PendingAccessRequests() {
    const auto requests = deskhubp::ListAccessRequests();
    return requests ? *requests : std::vector<deskhubp::PendingClient>{};
}

wxStaticText* MakeErrorLabel(wxWindow* parent, const char* text) {
    auto* label = new wxStaticText(parent, wxID_ANY, ToWx(text));
    label->SetForegroundColour(kErrorText);
    label->Wrap(HintWrapWidth(label));
    label->Hide();
    return label;
}

bool WriteClipboardText(const wxString& text) {
    if (!OpenClipboard(nullptr)) return false;
    bool written = false;
    if (EmptyClipboard()) {
        const std::wstring wide = text.ToStdWstring();
        const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
            if (void* target = GlobalLock(memory)) {
                std::memcpy(target, wide.c_str(), bytes);
                GlobalUnlock(memory);
                written = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!written) GlobalFree(memory);
        }
    }
    CloseClipboard();
    return written;
}

std::optional<std::string> ReadClipboardText() {
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(nullptr))
        return std::nullopt;
    std::optional<std::string> text;
    if (HANDLE data = GetClipboardData(CF_UNICODETEXT)) {
        if (const auto* wide = static_cast<const wchar_t*>(GlobalLock(data))) {
            text = std::string(wxString(wide).utf8_str());
            GlobalUnlock(data);
        }
    }
    CloseClipboard();
    return text;
}

void CopyTextToClipboard(const wxString& text) {
    if (!text.empty()) WriteClipboardText(text);
}

class QrView final : public wxPanel {
public:
    explicit QrView(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
        SetName("pairing-qr");
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(kQrViewDip, kQrViewDip)));
        Bind(wxEVT_PAINT, &QrView::OnPaint, this);
    }

    void SetCode(std::optional<deskhub::QrCode> code) {
        code_ = std::move(code);
        Refresh();
    }

private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(*wxWHITE_BRUSH);
        dc.Clear();
        if (!code_ || code_->size <= 0) return;

        const wxSize area = GetClientSize();
        const int cells = code_->size + 2 * kQrQuietZoneModules;
        const int scale = std::max(1, std::min(area.GetWidth(), area.GetHeight()) / cells);
        const int originX = (area.GetWidth() - scale * cells) / 2 + scale * kQrQuietZoneModules;
        const int originY = (area.GetHeight() - scale * cells) / 2 + scale * kQrQuietZoneModules;

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(*wxBLACK_BRUSH);
        for (int y = 0; y < code_->size; ++y) {
            for (int x = 0; x < code_->size; ++x) {
                if (!code_->Dark(x, y)) continue;
                dc.DrawRectangle(originX + x * scale, originY + y * scale, scale, scale);
            }
        }
    }

    std::optional<deskhub::QrCode> code_{};
};

class QrWindow final : public wxFrame {
public:
    QrWindow(wxWindow* parent, std::function<std::string()> makeInvite,
        std::function<void()> onClosed)
        : wxFrame(parent, wxID_ANY, ToWx(ui::kQrWindowTitle), wxDefaultPosition, wxDefaultSize,
              wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU | wxFRAME_FLOAT_ON_PARENT),
          makeInvite_(std::move(makeInvite)),
          onClosed_(std::move(onClosed)) {
        SetName("pairing-qr-window");
        SetBackgroundColour(*wxWHITE);
        auto* panel = new wxPanel(this);
        panel->SetBackgroundColour(*wxWHITE);
        auto* column = new wxBoxSizer(wxVERTICAL);

        view_ = new QrView(panel);
        column->Add(view_, wxSizerFlags().CentreHorizontal());

        auto* inviteRow = new wxBoxSizer(wxHORIZONTAL);
        invite_ = new wxStaticText(panel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize,
            wxST_ELLIPSIZE_MIDDLE | wxST_NO_AUTORESIZE);
        invite_->SetName("pairing-invite");
        invite_->SetFont(MonoFont(invite_));
        invite_->SetMinSize(FromDIP(wxSize(kQrViewDip - ui::kButtonMinWidth - kPageGap, -1)));
        inviteRow->Add(invite_, wxSizerFlags(1).CentreVertical());
        copy_ = MakeButton(panel, ToWx(ui::kCopyButton));
        copy_->SetName("copy-pairing-invite");
        copy_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CopyInvite(); });
        inviteRow->Add(copy_, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(kPageGap)));
        column->Add(inviteRow, wxSizerFlags().Expand().Border(wxTOP, FromDIP(kPageGap)));

        expiry_ = new wxStaticText(panel, wxID_ANY, wxString());
        expiry_->SetName("qr-expiry");
        expiry_->SetForegroundColour(kMutedText);
        column->Add(expiry_, wxSizerFlags().Border(wxTOP, FromDIP(kPageGap)));

        renew_ = MakeButton(panel, ToWx(ui::kNewQrAction));
        renew_->SetName("new-qr");
        PaintButton(renew_, kAccent);
        renew_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Renew(); });
        column->Add(renew_, wxSizerFlags().Expand().Border(wxTOP, FromDIP(kPageGap)));

        auto* hint = new wxStaticText(panel, wxID_ANY, ToWx(ui::kQrHint));
        hint->SetForegroundColour(kMutedText);
        hint->Wrap(FromDIP(kQrViewDip));
        column->Add(hint, wxSizerFlags().Border(wxTOP, FromDIP(kPageGap)));

        auto* padded = new wxBoxSizer(wxVERTICAL);
        padded->Add(column, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kPagePad)));
        panel->SetSizer(padded);
        auto* frameSizer = new wxBoxSizer(wxVERTICAL);
        frameSizer->Add(panel, wxSizerFlags(1).Expand());
        SetSizerAndFit(frameSizer);

        tick_.SetOwner(this, kQrTickTimerId);
        copied_.SetOwner(this, kQrCopiedTimerId);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Tick(); }, kQrTickTimerId);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { copy_->SetLabel(ToWx(ui::kCopyButton)); }, kQrCopiedTimerId);
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) {
            tick_.Stop();
            onClosed_();
            Destroy();
        });
    }

    bool Renew() {
        const std::string invite = makeInvite_();
        if (invite.empty()) return false;
        invite_->SetLabelText(ToWx(invite));
        view_->SetCode(deskhub::EncodeQr(invite));
        expiresAt_ = std::chrono::steady_clock::now() +
                     std::chrono::seconds(deskhub::kPairingTokenTtlSeconds);
        copy_->Enable();
        renew_->Hide();
        Tick();
        tick_.Start(kQrTickMs);
        Refit();
        return true;
    }

private:
    void Tick() {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            expiresAt_ - std::chrono::steady_clock::now())
                              .count();
        if (left > 0) {
            expiry_->SetLabel(ToWx(ui::QrExpiryLine((left + 999) / 1000)));
            return;
        }
        tick_.Stop();
        view_->SetCode(std::nullopt);
        invite_->SetLabelText(wxString());
        copy_->Disable();
        expiry_->SetLabel(ToWx(ui::kQrExpiredNote));
        expiry_->Wrap(FromDIP(kQrViewDip));
        renew_->Show();
        Refit();
    }

    void CopyInvite() {
        CopyTextToClipboard(invite_->GetLabelText());
        copy_->SetLabel(ToWx(ui::kCopiedButton));
        copied_.StartOnce(kCopiedRevertMs);
    }

    void Refit() {
        Layout();
        GetSizer()->SetSizeHints(this);
    }

    std::function<std::string()> makeInvite_;
    std::function<void()> onClosed_;
    QrView* view_ = nullptr;
    wxStaticText* invite_ = nullptr;
    wxButton* copy_ = nullptr;
    wxStaticText* expiry_ = nullptr;
    wxButton* renew_ = nullptr;
    wxTimer tick_;
    wxTimer copied_;
    std::chrono::steady_clock::time_point expiresAt_{};
};

class NavItem final : public wxWindow {
public:
    NavItem(wxWindow* parent, const wxString& label, std::function<void()> onClick)
        : wxWindow(parent, wxID_ANY), label_(label), onClick_(std::move(onClick)) {
        SetName("nav-" + label);
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(160, 42)));
        SetCursor(wxCursor(wxCURSOR_HAND));
        SetFont(GetFont().Scaled(1.1f));
        Bind(wxEVT_PAINT, &NavItem::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) {
            if (onClick_) onClick_();
        });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) { SetHover(true); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) { SetHover(false); });
    }

    void SetSelected(bool selected) {
        if (selected_ == selected) return;
        selected_ = selected;
        Refresh();
    }

private:
    void SetHover(bool hover) {
        if (hover_ == hover) return;
        hover_ = hover;
        Refresh();
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(kSidebarBg));
        dc.Clear();

        const wxRect rect = GetClientRect();
        if (selected_ || hover_) {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(selected_ ? kAccent : kSidebarHover));
            dc.DrawRoundedRectangle(rect, FromDIP(8));
        }

        dc.SetFont(selected_ ? GetFont().Bold() : GetFont());
        dc.SetTextForeground(selected_ ? *wxWHITE : kNavText);
        const wxSize extent = dc.GetTextExtent(label_);
        dc.DrawText(label_, FromDIP(16), (rect.GetHeight() - extent.GetHeight()) / 2);
    }

    wxString label_;
    std::function<void()> onClick_;
    bool selected_ = false;
    bool hover_ = false;
};

class MainFrame;
class ConnectionFrame;

class DeskhubTrayIcon final : public wxTaskBarIcon {
public:
    explicit DeskhubTrayIcon(MainFrame& frame) : frame_(frame) {
        Bind(wxEVT_TASKBAR_LEFT_DOWN, &DeskhubTrayIcon::OnLeftDown, this);
    }

protected:
    wxMenu* CreatePopupMenu() override;

private:
    void OnLeftDown(wxTaskBarIconEvent& event);
    MainFrame& frame_;
};

class MainFrame final : public wxFrame {
public:
    MainFrame();

private:
    friend class DeskhubTrayIcon;
    friend class ConnectionFrame;

    void ApplyTrayMode();
    bool EnsureTrayAttached();
    void ToggleWindowFromTray();
    void QuitFromTray();

    wxWindow* BuildSidebar();
    wxWindow* BuildHostPage(wxWindow* parent);
    wxWindow* BuildClientPage(wxWindow* parent);
    wxWindow* BuildDevicesPage(wxWindow* parent);
    wxWindow* BuildSettingsPage(wxWindow* parent);
    void AddSetting(const SettingsArea& area, const ui::SettingsEntry& entry,
        wxFlexGridSizer*& grid);
    void AddLabelledSetting(const SettingsArea& area, wxFlexGridSizer*& grid, const char* label,
        wxWindow* control);
    wxSizer* MakeTransferFolderRow(wxWindow* area, const char* label);
    void AddThisMachineSection(const SettingsArea& area);
    void RefreshDeviceNameRow();
    void AddAllowedClientsSection(const SettingsArea& area);
    void AddSavedHostsSection(const SettingsArea& area);
    void RefreshPairedDevices();
    void ForgetEveryDevice();
    void AllowClientKey();
    void CopyWithFeedback(wxButton* button, const wxString& text);
    void RestoreCopiedButton();
    void RefreshSavedHosts();
    void ShowHostProfileError(ui::HostProfileError error);
    void RemoveSavedHost(const std::string& alias);
    void ConnectSavedHost(const std::string& endpoint);

    void SelectPage(int page);
    void RefreshDeviceList();
    void OnShare(ShareTrigger trigger = ShareTrigger::kUser);
    void BeginAutoShare();
    void OnAutoShareTimer(wxTimerEvent& event);
    void OnDevicesTimer(wxTimerEvent& event);
    void ReportShareProblem(const wxString& text, const wxString& title);
    bool Sharing() const;
    bool TerminalTicked() const;
    bool FilesTicked() const;
    std::filesystem::path TransferFolder() const;
    void StartHosting(const std::vector<ShareSource>& sources, const ShareOptions& options);
    void OnHostStarted(bool started, const std::string& error, uint16_t port,
        bool allowInput);
    void StopHosting();
    void StartFileShare();
    void ChooseTransferFolder();
    void OpenFileSend(const NetAddr& server);
    void ApplySharingBanner();
    void OnHostTimer(wxTimerEvent& event);
    void OnClipboardTimer(wxTimerEvent& event);
    void RefreshDisplayChoices();
    void OnDisplayChanged(wxDisplayChangedEvent& event);
    void UpdateHostRows(const std::vector<ShareSourceStatus>& rows);
    wxWindow* BuildHostTable(wxWindow* parent);
    wxScrolledWindow* BuildSourcePicker(wxWindow* parent);
    wxButton* MakeRowAction(wxWindow* parent, const ui::HostRow& ref);
    wxButton* MakeRowAttach(wxWindow* parent, const ui::HostRow& ref);
    wxButton* MakeRowOpenFolder(wxWindow* parent);
    void RebuildHostTable();
    void ShowHostTable(bool sharing);
    void RelayoutHostPage();
    void StopDisplay(uint8_t sourceId);
    void KickViewer(uint8_t sourceId, const std::string& viewerAddr);
    void ApplyHostState(HostShareState state, const wxString& detail);
    void ShowIdleHostState();
    void ShowPortCard();
    void CopySharePort();
    void ShowQrWindow();
    void CloseQrWindow();
    std::string MakePairingInvite();
    void OnQrWindowClosed();
    wxWindow* BuildAccessRequestsPanel(wxWindow* parent, AccessRequestsView& view);
    void RefreshAccessRequests();
    void RebuildAccessRequestRows(const AccessRequestsView& view,
        const std::vector<deskhubp::PendingClient>& pending);
    void AddAccessRequestRow(const AccessRequestsView& view,
        const deskhubp::PendingClient& client);
    void AnnounceNewAccessRequests(const std::vector<deskhubp::PendingClient>& pending);
    void OnAccessRequestedByEngine();
    void ApproveRequestRow(const deskhub::Fingerprint& fingerprint);
    void DenyRequestRow(const deskhub::Fingerprint& fingerprint);
    void AccessRequestsChanged();

    deskhubp::SourceQueryAsync::UiPost UiPoster() const;
    SourceQueryRequest ConnectRequest();
    void StartConnect(const std::string& addr);
    void StartConnectByInvite(const std::string& invite);
    void LaunchQuery(const NetAddr& server, const std::string& invite, const std::string& addr);
    void ShowConnectProgress(const std::string& text);
    void CancelConnect();
    void OpenShell(const NetAddr& server);
    void SetClientControl(bool on);
    void ForgetConnection(ConnectionFrame* frame);
    void SetClientStatus(const wxString& text, const wxColour& colour);
    void ShowAddressInFields(const std::string& addr);
    void ConnectToDevice(const std::string& addr);
    void ConnectRow(long row);
    void OnSourcesReady(const std::string& addr, const deskhubp::ConnectOutcome& outcome);
    void ConfirmNewHost(const std::string& addr, const deskhub::Fingerprint& fingerprint);
    void OpenConnectionWindow(const std::string& addr, const deskhubp::ConnectOutcome& outcome);
    ConnectionFrame* ConnectionFor(const std::string& addr) const;
    void CloseEveryConnection();
    void SaveSettings();
    void SaveDeviceName();
    void PopulateBindChoice();
    void RebuildHostAddressRows();
    void OnClose(wxCloseEvent& event);

    wxSimplebook* book_ = nullptr;
    wxScrolledWindow* hostPage_ = nullptr;
    NavItem* pageButtons_[kPageCount] = {};
    wxTextCtrl* addrCtrl_ = nullptr;
    wxTextCtrl* connectPortCtrl_ = nullptr;
    wxButton* connectBtn_ = nullptr;
    wxStaticText* clientStatus_ = nullptr;
    wxButton* cancelConnectBtn_ = nullptr;
    bool connectCancelled_ = false;
    wxListCtrl* deviceList_ = nullptr;
    wxStaticText* deviceHint_ = nullptr;
    std::vector<ConnectionFrame*> connections_;
    wxScrolledWindow* pairedList_ = nullptr;
    wxBoxSizer* pairedRows_ = nullptr;
    wxStaticText* pairedHint_ = nullptr;
    std::vector<deskhubp::AuthorizedClient> pairedDevices_;
    wxTextCtrl* allowClientCtrl_ = nullptr;
    wxStaticText* allowClientError_ = nullptr;
    wxWeakRef<wxButton> copiedButton_;
    wxString copiedButtonLabel_;
    wxScrolledWindow* savedHostList_ = nullptr;
    wxBoxSizer* savedHostRows_ = nullptr;
    wxStaticText* savedHostHint_ = nullptr;
    wxStaticText* hostProfileError_ = nullptr;
    wxPanel* hostAddrPanel_ = nullptr;
    wxButton* qrBtn_ = nullptr;
    QrWindow* qrWindow_ = nullptr;
    wxWindow* accessRequestsPanel_ = nullptr;
    AccessRequestsView hostRequests_;
    AccessRequestsView deviceRequests_;
    std::optional<uint64_t> accessRequestsSeen_{};
    std::set<std::string> announcedRequests_;
    wxStaticText* deviceNameValue_ = nullptr;
    wxPanel* hostBanner_ = nullptr;
    wxWindow* hostBannerBar_ = nullptr;
    wxStaticText* hostStateLabel_ = nullptr;
    wxStaticText* hostStatusLabel_ = nullptr;
    wxPanel* hostPortPanel_ = nullptr;
    wxStaticText* hostPortLabel_ = nullptr;
    wxButton* hostPortCopyBtn_ = nullptr;
    wxStaticText* hostHint_ = nullptr;
    wxScrolledWindow* hostPickerFrame_ = nullptr;
    wxBoxSizer* displayChecksBox_ = nullptr;
    std::vector<wxCheckBox*> displayChecks_;
    wxCheckBox* hostTerminalCheck_ = nullptr;
    wxCheckBox* hostFilesCheck_ = nullptr;
    wxWindow* hostTableHolder_ = nullptr;
    wxScrolledWindow* hostTable_ = nullptr;
    std::vector<HostRowView> hostRowViews_;
    wxButton* shareBtn_ = nullptr;
    wxTextCtrl* deviceNameCtrl_ = nullptr;
    NumberField* fpsCtrl_ = nullptr;
    NumberField* bitrateCtrl_ = nullptr;
    NumberField* portCtrl_ = nullptr;
    wxChoice* qualityChoice_ = nullptr;
    wxChoice* bindChoice_ = nullptr;
    std::map<ui::SettingField, wxCheckBox*> settingChecks_;
    wxStaticText* transferDirLabel_ = nullptr;
    DeskhubTrayIcon* trayIcon_ = nullptr;
    bool quitting_ = false;
    std::vector<std::string> bindChoices_;

    deskhub::ui::UiSettings settings_;
    std::vector<ShareSource> availableDisplays_;
    std::vector<ShareSourceStatus> hostStatus_;
    std::optional<std::string> pendingClipboard_;
    bool screenSharing_ = false;
    bool terminalRequested_ = false;
    bool filesRequested_ = false;
    uint16_t sharePort_ = 0;
    std::string shareBindWarning_;
    bool shareViewOnly_ = false;
    std::vector<ui::HostRow> hostRows_;
    std::vector<ui::RecentDevice> recent_;
    deskhubp::SourceQueryAsync connectDriver_;
    deskhubp::ShareController share_;
    deskhubp::ShareDriver shareDriver_;
    wxTimer hostTimer_;
    wxTimer clipTimer_;
    wxTimer autoShareTimer_;
    wxTimer copiedTimer_;
    wxTimer copiedButtonTimer_;
    wxTimer devicesTimer_;
    ui::AutoShareGate autoShareGate_;
    ShareTrigger shareTrigger_ = ShareTrigger::kUser;
    bool hosting_ = false;
    bool hostStarting_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

class ConnectionFrame final : public wxFrame {
public:
    ConnectionFrame(MainFrame* owner, std::string address, deskhub::HostCaps caps, std::vector<deskhub::SourceInfo> sources, bool control);

    const std::string& Address() const {
        return address_;
    }

private:
    void OpenDesktopSession();
    void OpenShellSession();
    void OpenFileSendSession();

    MainFrame* owner_ = nullptr;
    std::string address_;
    deskhub::HostCaps caps_{};
    std::vector<deskhub::SourceInfo> sources_;
    bool control_ = false;
    wxStaticText* stateLabel_ = nullptr;
    wxStaticText* pingLabel_ = nullptr;
};

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, ToWx(ui::kAppTitle)) {
    settings_ = deskhubp::LoadUiSettings();
    recent_ = deskhubp::LoadRecentDevices();

    auto* root = new wxBoxSizer(wxHORIZONTAL);
    root->Add(BuildSidebar(), wxSizerFlags().Expand());

    book_ = new wxSimplebook(this);
    book_->AddPage(BuildHostPage(book_), wxString());
    book_->AddPage(BuildClientPage(book_), wxString());
    book_->AddPage(BuildDevicesPage(book_), wxString());
    book_->AddPage(BuildSettingsPage(book_), wxString());
    root->Add(book_, wxSizerFlags(1).Expand());

    SetIcon(wxICON(deskhub_app_icon));

    SetSizer(root);
    SetMinClientSize(FromDIP(wxSize(kWindowMinW, kWindowMinH)));
    SetClientSize(FromDIP(wxSize(kWindowW, kWindowH)));
    Centre();

    hostTimer_.SetOwner(this, kHostTimerId);
    clipTimer_.SetOwner(this, kClipTimerId);
    autoShareTimer_.SetOwner(this, kAutoShareTimerId);
    copiedTimer_.SetOwner(this, kCopiedTimerId);
    copiedButtonTimer_.SetOwner(this, kCopiedButtonTimerId);
    devicesTimer_.SetOwner(this, kDevicesTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnHostTimer, this, kHostTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnClipboardTimer, this, kClipTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnAutoShareTimer, this, kAutoShareTimerId);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { ShowPortCard(); }, kCopiedTimerId);
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { RestoreCopiedButton(); }, kCopiedButtonTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnDevicesTimer, this, kDevicesTimerId);
    Bind(wxEVT_DISPLAY_CHANGED, &MainFrame::OnDisplayChanged, this);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);

    share_.sharingHost().SetTerminal(&share_.terminalHost());
    share_.sharingHost().SetFiles(&share_.fileHost());

    deskhubp::ShareController::Hooks hooks;
    hooks.onError = [this](const std::string& message) {
        wxMessageBox(ToWx(message), "Deskhub", wxOK | wxICON_ERROR, this);
    };
    hooks.postToUi = [this](std::function<void()> fn) { CallAfter(std::move(fn)); };
    hooks.openLocalTerminal = [this](uint32_t termId) {
        return OpenHostTerminalWindow(this, share_.terminalHost(), termId);
    };
    hooks.onRowsChanged = [this] { UpdateHostRows(hostStatus_); };
    hooks.onBannerChanged = [this] { ApplySharingBanner(); };
    hooks.onNothingLeftShared = [this] { StopHosting(); };
    share_.SetHooks(std::move(hooks));
    SetAccessRequestedListener([this, post = UiPoster()] {
        post([this] { OnAccessRequestedByEngine(); });
    });

    RefreshAccessRequests();
    RefreshDeviceList();
    SelectPage(kPageClient);
    ApplyTrayMode();

    if (settings_.autoShare) {
        SelectPage(kPageHost);
        CallAfter([this] { BeginAutoShare(); });
    }
}

void MainFrame::ApplyTrayMode() {
    if (settings_.startHidden && !trayIcon_) {
        EnsureTrayAttached();
        return;
    }
    if (!settings_.startHidden && trayIcon_) {
        trayIcon_->RemoveIcon();
        delete trayIcon_;
        trayIcon_ = nullptr;
        if (!IsShown()) Show(true);
    }
}

bool MainFrame::EnsureTrayAttached() {
    if (trayIcon_) return true;
    trayIcon_ = new DeskhubTrayIcon(*this);
    if (!trayIcon_->SetIcon(wxICON(deskhub_app_icon), "Deskhub")) {
        delete trayIcon_;
        trayIcon_ = nullptr;
        return false;
    }
    return true;
}

void MainFrame::ToggleWindowFromTray() {
    if (!IsShown()) {
        Show(true);
        Raise();
        return;
    }
    if (IsIconized()) {
        Iconize(false);
        Raise();
        return;
    }
    Hide();
}

void MainFrame::QuitFromTray() {
    quitting_ = true;
    CallAfter([this] { Close(true); });
}

wxWindow* MainFrame::BuildSidebar() {
    auto* panel = new wxPanel(this);
    panel->SetBackgroundColour(kSidebarBg);

    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(panel, wxID_ANY, "Deskhub");
    title->SetFont(title->GetFont().Bold().Scaled(1.6f));
    title->SetForegroundColour(*wxWHITE);
    sizer->Add(title, wxSizerFlags().Border(wxALL, FromDIP(16)));

    for (int i = 0; i < kPageCount; ++i) {
        auto* item = new NavItem(panel, ToWx(kPageLabels[i]), [this, i] { SelectPage(i); });
        sizer->Add(item, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(10)));
        pageButtons_[i] = item;
    }

    sizer->AddStretchSpacer(1);

    auto* repoLink = new wxHyperlinkCtrl(panel, wxID_ANY, ToWx(ui::kProjectLinkLabel),
        ToWx(ui::kProjectUrl));
    repoLink->SetBackgroundColour(kSidebarBg);
    repoLink->SetNormalColour(kNavText);
    repoLink->SetVisitedColour(kNavText);
    repoLink->SetHoverColour(*wxWHITE);
    repoLink->SetToolTip(ToWx(ui::kProjectUrl));
    sizer->Add(repoLink, wxSizerFlags().Border(wxLEFT, FromDIP(kSidebarLinkInset)));

    auto* version = new wxStaticText(panel, wxID_ANY, ToWx(ui::VersionLine()));
    version->SetForegroundColour(kSidebarFootnote);
    sizer->Add(version, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(kPagePad)));

    panel->SetSizer(sizer);
    return panel;
}

wxWindow* MainFrame::BuildHostPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    hostPage_ = panel;
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    outer->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kPagePad)));
    const wxSizerFlags row = wxSizerFlags().Border(wxTOP, FromDIP(kPageGap));
    const wxSizerFlags wideRow = wxSizerFlags().Expand().Border(wxTOP, FromDIP(kPageGap));
    const wxSizerFlags tallRow = wxSizerFlags(1).Expand().Border(wxTOP, FromDIP(kPageGap));

    sizer->Add(MakeHeading(panel, ui::kHostHeading));
    sizer->Add(MakeHint(panel, ToWx(ui::kHostIpIntro)), row);

    auto* netRow = new wxBoxSizer(wxHORIZONTAL);
    netRow->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kBindInterfaceLabel)),
        wxSizerFlags().CentreVertical());
    bindChoice_ = new wxChoice(panel, wxID_ANY);
    PopulateBindChoice();
    bindChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
        SaveSettings();
        RebuildHostAddressRows();
    });
    netRow->Add(bindChoice_, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(14)));
    sizer->Add(netRow, row);

    auto* addrRow = new wxBoxSizer(wxHORIZONTAL);
    hostAddrPanel_ = new wxPanel(panel);
    addrRow->Add(hostAddrPanel_, wxSizerFlags(1).Expand());
    qrBtn_ = MakeButton(panel, ToWx(ui::kShowQrAction));
    qrBtn_->SetName("toggle-qr");
    qrBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowQrWindow(); });
    addrRow->Add(qrBtn_, wxSizerFlags().Top().Border(wxLEFT, FromDIP(14)));
    sizer->Add(addrRow, wideRow);
    RebuildHostAddressRows();

    hostBanner_ = new wxPanel(panel);
    auto* bannerRow = new wxBoxSizer(wxHORIZONTAL);
    hostBannerBar_ = new wxWindow(hostBanner_, wxID_ANY, wxDefaultPosition,
        FromDIP(wxSize(4, -1)));
    bannerRow->Add(hostBannerBar_, wxSizerFlags().Expand());

    auto* bannerText = new wxBoxSizer(wxVERTICAL);
    hostStateLabel_ = new wxStaticText(hostBanner_, wxID_ANY, wxString());
    hostStateLabel_->SetFont(hostStateLabel_->GetFont().Bold().Scaled(1.1f));
    bannerText->Add(hostStateLabel_, wxSizerFlags().Border(wxBOTTOM, FromDIP(4)));
    hostStatusLabel_ = new wxStaticText(hostBanner_, wxID_ANY, wxString());
    hostStatusLabel_->SetForegroundColour(kMutedText);
    bannerText->Add(hostStatusLabel_);
    bannerRow->Add(bannerText, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kCardPad)));

    hostBanner_->SetSizer(bannerRow);
    sizer->Add(hostBanner_, wideRow);

    hostPortPanel_ = new wxPanel(panel);
    hostPortPanel_->SetBackgroundColour(kPortCardBg);
    auto* portRow = new wxBoxSizer(wxHORIZONTAL);
    auto* portText = new wxBoxSizer(wxVERTICAL);
    auto* portHeading = new wxStaticText(hostPortPanel_, wxID_ANY, ToWx(ui::kUdpPortLabel));
    portHeading->SetForegroundColour(kMutedText);
    portText->Add(portHeading);
    hostPortLabel_ = new wxStaticText(hostPortPanel_, wxID_ANY, wxString());
    wxFont portFont = MonoFont(hostPortLabel_).Bold();
    portFont.SetPointSize(kPortPointSize);
    hostPortLabel_->SetFont(portFont);
    portText->Add(hostPortLabel_, wxSizerFlags().Border(wxTOP, FromDIP(2)));
    portRow->Add(portText, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kCardPad)));
    hostPortCopyBtn_ = MakeButton(hostPortPanel_, ToWx(ui::kCopyButton));
    hostPortCopyBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CopySharePort(); });
    portRow->Add(hostPortCopyBtn_,
        wxSizerFlags().CentreVertical().Border(wxRIGHT, FromDIP(kCardPad)));
    hostPortPanel_->SetSizer(portRow);
    sizer->Add(hostPortPanel_, wideRow);

    hostPickerFrame_ = BuildSourcePicker(panel);
    sizer->Add(hostPickerFrame_, tallRow);

    accessRequestsPanel_ = BuildAccessRequestsPanel(panel, hostRequests_);
    sizer->Add(accessRequestsPanel_, wideRow);

    hostTableHolder_ = BuildHostTable(panel);
    hostTableHolder_->SetMinSize(FromDIP(wxSize(-1, kHostListMinH)));
    sizer->Add(hostTableHolder_, tallRow);

    hostHint_ = MakeHint(panel, ToWx(ui::kPickSourcesHint));
    sizer->Add(hostHint_, row);

    shareBtn_ = MakePrimaryButton(panel, wxString());
    shareBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnShare(); });
    sizer->Add(shareBtn_, wideRow);

    panel->SetSizer(outer);
    panel->FitInside();
    ShowIdleHostState();
    RefreshDisplayChoices();
    return panel;
}

wxScrolledWindow* MainFrame::BuildSourcePicker(wxWindow* parent) {
    auto* frame = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxVSCROLL | wxBORDER_SIMPLE);
    frame->SetName("source-picker");
    frame->SetBackgroundColour(*wxWHITE);
    frame->SetScrollRate(0, FromDIP(10));
    frame->SetMinSize(FromDIP(wxSize(-1, kHostListMinH)));

    auto* picker = new wxBoxSizer(wxVERTICAL);
    displayChecksBox_ = new wxBoxSizer(wxVERTICAL);
    picker->Add(displayChecksBox_, wxSizerFlags().Expand());
    hostTerminalCheck_ = new wxCheckBox(frame, wxID_ANY, ToWx(ui::kTerminalPickerLabel));
    hostTerminalCheck_->SetValue(true);
    picker->Add(hostTerminalCheck_, wxSizerFlags().Border(wxTOP, FromDIP(kPickerGap)));
    hostFilesCheck_ = new wxCheckBox(frame, wxID_ANY, ToWx(ui::kFilesPickerLabel));
    hostFilesCheck_->SetValue(true);
    hostFilesCheck_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        if (!Sharing()) ShowIdleHostState();
    });
    picker->Add(hostFilesCheck_, wxSizerFlags().Border(wxTOP, FromDIP(kPickerGap)));

    auto* padded = new wxBoxSizer(wxVERTICAL);
    padded->Add(picker, wxSizerFlags().Expand().Border(wxALL, FromDIP(kPickerPad)));
    frame->SetSizer(padded);
    return frame;
}

void MainFrame::PopulateBindChoice() {
    bindChoice_->Clear();
    bindChoices_.clear();
    bindChoices_.push_back("");
    bindChoice_->Append(ToWx(ui::kBindAllInterfaces));
    int active = 0;
    for (const AdapterAddr& adapter : ListLocalIPv4()) {
        bindChoice_->Append(ToWx(adapter.ip + "  (" + adapter.name + ")"));
        bindChoices_.push_back(adapter.ip);
        if (adapter.ip == settings_.bindIp) active = int(bindChoices_.size() - 1);
    }
    if (!settings_.bindIp.empty() && active == 0) {
        bindChoice_->Append(
            ToWx(settings_.bindIp + "  (" + ui::kBindNotConnectedNote + ")"));
        bindChoices_.push_back(settings_.bindIp);
        active = int(bindChoices_.size() - 1);
    }
    bindChoice_->SetSelection(active);
}

void MainFrame::RebuildHostAddressRows() {
    hostAddrPanel_->DestroyChildren();
    auto* holder = new wxBoxSizer(wxVERTICAL);
    std::vector<AdapterAddr> shown;
    for (const auto& a : ListLocalIPv4())
        if (settings_.bindIp.empty() || a.ip == settings_.bindIp) shown.push_back(a);
    if (shown.empty()) {
        const std::string text = settings_.bindIp.empty()
                                     ? std::string(ui::kNoNetworkAddress)
                                     : settings_.bindIp + "  (" + ui::kBindNotConnectedNote + ")";
        holder->Add(new wxStaticText(hostAddrPanel_, wxID_ANY, ToWx(text)));
    } else {
        auto* grid = new wxFlexGridSizer(3, FromDIP(wxSize(14, 10)));
        for (const auto& a : shown) {
            grid->Add(new wxStaticText(hostAddrPanel_, wxID_ANY, ToWx(a.name)),
                wxSizerFlags().CentreVertical());
            auto* ipText = new wxStaticText(hostAddrPanel_, wxID_ANY, ToWx(a.ip));
            ipText->SetFont(ipText->GetFont().Bold());
            grid->Add(ipText, wxSizerFlags().CentreVertical());
            auto* copy = MakeButton(hostAddrPanel_, ToWx(ui::kCopyButton));
            const wxString ip = ToWx(a.ip);
            copy->Bind(wxEVT_BUTTON, [ip](wxCommandEvent&) {
                CopyTextToClipboard(ip);
            });
            grid->Add(copy);
        }
        holder->Add(grid);
    }
    hostAddrPanel_->SetSizer(holder);
    RelayoutHostPage();
}

void MainFrame::ShowQrWindow() {
    if (qrWindow_ != nullptr) {
        qrWindow_->Show();
        qrWindow_->Raise();
        return;
    }
    qrWindow_ = new QrWindow(this, [this] { return MakePairingInvite(); }, [this] { OnQrWindowClosed(); });
    if (!qrWindow_->Renew()) {
        qrWindow_->Close();
        wxMessageBox(ToWx(ui::kQrUnavailable), "Deskhub", wxOK | wxICON_WARNING, this);
        return;
    }
    qrWindow_->CentreOnParent();
    qrWindow_->Show();
    qrWindow_->Raise();
}

void MainFrame::CloseQrWindow() {
    if (qrWindow_ != nullptr) qrWindow_->Close();
}

void MainFrame::OnQrWindowClosed() {
    qrWindow_ = nullptr;
    deskhubp::RevokePairingTokens();
}

std::string MainFrame::MakePairingInvite() {
    deskhubp::RevokePairingTokens();
    const uint16_t port = uint16_t(hosting_ ? sharePort_ : settings_.port);
    return deskhubp::BuildPairingInvite(port, settings_.bindIp,
        ui::TruncateDeviceName(deskhubp::SessionDeviceName()));
}

wxWindow* MainFrame::BuildAccessRequestsPanel(wxWindow* parent, AccessRequestsView& view) {
    auto* holder = new wxPanel(parent);
    holder->SetBackgroundColour(*wxWHITE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    sizer->Add(MakeSection(holder, ui::kAccessRequestsHeading));

    view.list = new wxPanel(holder);
    view.list->SetName("access-requests");
    view.list->SetBackgroundColour(*wxWHITE);
    view.rows = new wxBoxSizer(wxVERTICAL);
    view.list->SetSizer(view.rows);
    sizer->Add(view.list, wxSizerFlags().Expand().Border(wxTOP, FromDIP(kSectionGap)));

    view.hint = MakeHint(holder, ToWx(ui::kAccessRequestsEmpty));
    sizer->Add(view.hint, wxSizerFlags().Border(wxTOP, FromDIP(kSectionGap)));

    holder->SetSizer(sizer);
    RebuildAccessRequestRows(view, PendingAccessRequests());
    return holder;
}

void MainFrame::RefreshAccessRequests() {
    const uint64_t generation = deskhubp::AccessRequestsGeneration();
    if (accessRequestsSeen_ == generation) return;
    accessRequestsSeen_ = generation;
    const std::vector<deskhubp::PendingClient> pending = PendingAccessRequests();
    RebuildAccessRequestRows(hostRequests_, pending);
    RebuildAccessRequestRows(deviceRequests_, pending);
    AnnounceNewAccessRequests(pending);
}

void MainFrame::OnAccessRequestedByEngine() {
    accessRequestsSeen_.reset();
    RefreshAccessRequests();
}

void MainFrame::AnnounceNewAccessRequests(
    const std::vector<deskhubp::PendingClient>& pending) {
    for (const deskhubp::PendingClient& client : pending) {
        const bool firstSeen =
            announcedRequests_.insert(deskhub::FormatFingerprint(client.fingerprint)).second;
        if (!firstSeen) continue;
        wxNotificationMessage notice(ToWx(ui::kAccessRequestNotificationTitle),
            ToWx(ui::AccessRequestNotificationBody(client.label, client.address)), this);
        notice.SetIcon(wxICON(deskhub_app_icon));
        notice.Show();
    }
}

void MainFrame::RebuildAccessRequestRows(const AccessRequestsView& view,
    const std::vector<deskhubp::PendingClient>& pending) {
    if (view.list == nullptr) return;
    view.rows->Clear(true);
    for (const deskhubp::PendingClient& client : pending)
        AddAccessRequestRow(view, client);
    view.hint->Show(pending.empty());
    RelayoutTable(view.list);
}

void MainFrame::AddAccessRequestRow(const AccessRequestsView& view,
    const deskhubp::PendingClient& client) {
    const TableRow row = BeginTableRow(view.list, *wxWHITE);
    AddTableCell(row, ToWx(client.label.empty() ? std::string(ui::kUnnamedClient) : client.label),
        180, false);
    AddTableCell(row, ToWx(deskhub::ShortFingerprint(client.fingerprint)), 130, false)
        ->SetToolTip(ToWx(deskhub::FormatFingerprint(client.fingerprint)));
    AddTableCell(row, ToWx(client.address), 170, false);

    const deskhub::Fingerprint fingerprint = client.fingerprint;
    auto* approve = MakeRowButton(row.panel, ToWx(ui::kApproveAction), kAccent);
    approve->SetName("approve-request");
    approve->Bind(wxEVT_BUTTON, [this, fingerprint](wxCommandEvent&) {
        CallAfter([this, fingerprint] { ApproveRequestRow(fingerprint); });
    });
    row.cells->Add(approve, wxSizerFlags().CentreVertical().Border(wxRIGHT, FromDIP(8)));

    auto* deny = MakeRowButton(row.panel, ToWx(ui::kDenyAction), kOffline);
    deny->SetName("deny-request");
    deny->Bind(wxEVT_BUTTON, [this, fingerprint](wxCommandEvent&) {
        CallAfter([this, fingerprint] { DenyRequestRow(fingerprint); });
    });
    row.cells->Add(deny, wxSizerFlags().CentreVertical());
    EndTableRow(view.rows, row);
}

void MainFrame::ApproveRequestRow(const deskhub::Fingerprint& fingerprint) {
    deskhubp::ApproveAccessRequest(fingerprint);
    AccessRequestsChanged();
}

void MainFrame::DenyRequestRow(const deskhub::Fingerprint& fingerprint) {
    deskhubp::DenyAccessRequest(fingerprint);
    AccessRequestsChanged();
}

void MainFrame::AccessRequestsChanged() {
    accessRequestsSeen_.reset();
    RefreshAccessRequests();
    RefreshPairedDevices();
}

wxWindow* MainFrame::BuildClientPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    outer->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kPagePad)));
    const wxSizerFlags row = wxSizerFlags().Border(wxTOP, FromDIP(kPageGap));
    const wxSizerFlags wideRow = wxSizerFlags().Expand().Border(wxTOP, FromDIP(kPageGap));

    sizer->Add(MakeHeading(panel, ui::kClientHeading));

    auto connectNow = [this](wxCommandEvent&) {
        const std::string typed = ui::TrimAscii(std::string(addrCtrl_->GetValue().utf8_str()));
        if (deskhub::IsPairingInvite(typed)) {
            StartConnectByInvite(typed);
            return;
        }
        const uint16_t port =
            ui::PortOrDefault(std::string(connectPortCtrl_->GetValue().utf8_str()));
        StartConnect(ui::AddressWithPort(typed, port));
    };

    auto* grid = new wxFlexGridSizer(2, FromDIP(wxSize(12, 12)));
    grid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kClientIpPrompt)),
        wxSizerFlags().CentreVertical());
    const Field<wxTextCtrl> address =
        MakeTextField(panel, wxString(), ui::kAddressFieldWidth, wxTE_PROCESS_ENTER);
    addrCtrl_ = address.control;
    addrCtrl_->SetName("address-field");
    addrCtrl_->SetHint(ToWx(ui::kClientIpPlaceholder));
    addrCtrl_->Bind(wxEVT_TEXT_ENTER, connectNow);
    grid->Add(address.box, wxSizerFlags().CentreVertical());
    grid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kUdpPortLabel)),
        wxSizerFlags().CentreVertical());
    const Field<wxTextCtrl> port = MakeTextField(panel,
        ToWx(std::to_string(deskhub::kDeskhubPort)), ui::kPortFieldWidth, wxTE_PROCESS_ENTER);
    connectPortCtrl_ = port.control;
    connectPortCtrl_->Bind(wxEVT_TEXT_ENTER, connectNow);
    grid->Add(port.box, wxSizerFlags().CentreVertical());
    sizer->Add(grid, row);

    connectBtn_ = MakePrimaryButton(panel, ToWx(ui::kConnectButton));
    connectBtn_->SetName("connect-button");
    PaintButton(connectBtn_, kAccent);
    connectBtn_->Bind(wxEVT_BUTTON, connectNow);
    sizer->Add(connectBtn_, wideRow);

    auto* statusRow = new wxBoxSizer(wxHORIZONTAL);
    clientStatus_ = new wxStaticText(panel, wxID_ANY, wxString());
    clientStatus_->SetName("client-status");
    clientStatus_->SetForegroundColour(kMutedText);
    statusRow->Add(clientStatus_, wxSizerFlags(1).Top());
    cancelConnectBtn_ = MakeButton(panel, ToWx(ui::kCancelAction));
    cancelConnectBtn_->SetName("cancel-connect");
    cancelConnectBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CancelConnect(); });
    cancelConnectBtn_->Hide();
    statusRow->Add(cancelConnectBtn_, wxSizerFlags().Top().Border(wxLEFT, FromDIP(kPageGap)));
    sizer->Add(statusRow, wideRow);

    sizer->Add(MakeHeading(panel, ui::kDevicesHeading), row);

    deviceList_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL);
    deviceList_->InsertColumn(0, ToWx(ui::kDeviceNameLabel), wxLIST_FORMAT_LEFT, FromDIP(180));
    deviceList_->InsertColumn(1, ToWx(ui::kHostAddressLabel), wxLIST_FORMAT_LEFT, FromDIP(170));
    deviceList_->InsertColumn(2, "Last connected", wxLIST_FORMAT_LEFT, FromDIP(150));
    deviceList_->SetMinSize(FromDIP(wxSize(-1, kListMinH)));
    deviceList_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
        const long index = event.GetIndex();
        CallAfter([this, index] { ConnectRow(index); });
    });
    sizer->Add(deviceList_, wxSizerFlags(1).Expand().Border(wxTOP, FromDIP(kPageGap)));

    deviceHint_ = MakeHint(panel, ToWx(ui::kLanDevicesEmpty));
    sizer->Add(deviceHint_, row);

    panel->SetSizer(outer);
    panel->FitInside();
    return panel;
}

wxWindow* MainFrame::BuildDevicesPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    outer->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kPagePad)));

    sizer->Add(MakeHeading(panel, ui::kDevicesHeading));
    AddThisMachineSection(BeginDevicesSection(panel, sizer, ui::kThisMachineHeading));
    sizer->Add(BuildAccessRequestsPanel(panel, deviceRequests_), DevicesSectionFlags(panel));
    AddAllowedClientsSection(BeginDevicesSection(panel, sizer, ui::kPairedHeading));
    AddSavedHostsSection(BeginDevicesSection(panel, sizer, ui::kSavedHostsHeading));

    panel->SetSizer(outer);
    panel->FitInside();
    RefreshPairedDevices();
    RefreshSavedHosts();
    return panel;
}

void MainFrame::AddThisMachineSection(const SettingsArea& area) {
    AddAreaHint(area, ui::kThisMachineHint);

    auto* nameRow = new wxBoxSizer(wxHORIZONTAL);
    auto* nameLabel = new wxStaticText(area.body, wxID_ANY, ToWx(ui::kDeviceNameLabel));
    nameLabel->SetForegroundColour(kMutedText);
    nameRow->Add(nameLabel, wxSizerFlags().CentreVertical().Border(wxRIGHT, FromDIP(kPageGap)));
    deviceNameValue_ = new wxStaticText(area.body, wxID_ANY, wxString());
    deviceNameValue_->SetName("this-machine-name");
    nameRow->Add(deviceNameValue_, wxSizerFlags().CentreVertical());
    area.sizer->Add(nameRow, AreaRowFlags(area));
    RefreshDeviceNameRow();

    const deskhubp::HostIdentity hostIdentity = deskhubp::LoadOrCreateHostIdentity();
    const bool valid = hostIdentity.Valid();
    const wxString fingerprintText = ToWx(valid
                                              ? deskhub::FormatFingerprint(hostIdentity.fingerprint)
                                              : std::string(ui::kShareNoHostIdentity));

    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* fingerprint = new wxTextCtrl(area.body, wxID_ANY, fingerprintText, wxDefaultPosition,
        wxDefaultSize, wxTE_READONLY | wxBORDER_NONE);
    fingerprint->SetName("host-fingerprint");
    fingerprint->SetFont(MonoFont(fingerprint));
    fingerprint->SetBackgroundColour(*wxWHITE);
    fingerprint->SetInitialSize(
        wxSize(fingerprint->GetTextExtent(fingerprintText).x + FromDIP(kPageGap), -1));
    row->Add(fingerprint, wxSizerFlags().CentreVertical());

    auto* copy = MakeButton(area.body, ToWx(ui::kCopyButton));
    copy->SetName("copy-host-fingerprint");
    copy->Enable(valid);
    copy->Bind(wxEVT_BUTTON, [this, copy, fingerprint](wxCommandEvent&) {
        CopyWithFeedback(copy, fingerprint->GetValue());
    });
    row->Add(copy, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(kPageGap)));

    auto* copyPublicKey = MakeButton(area.body, ToWx(ui::kCopyPublicKeyAction));
    copyPublicKey->SetName("copy-public-key");
    copyPublicKey->Enable(valid);
    copyPublicKey->Bind(wxEVT_BUTTON, [this, copyPublicKey, hostIdentity](wxCommandEvent&) {
        CopyWithFeedback(copyPublicKey,
            ToWx(deskhubp::IdentityPublicKeyLine(hostIdentity,
                ui::TruncateDeviceName(deskhubp::SessionDeviceName()))));
    });
    row->Add(copyPublicKey, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(kPageGap)));
    area.sizer->Add(row, AreaRowFlags(area));
}

void MainFrame::RefreshDeviceNameRow() {
    if (deviceNameValue_ == nullptr) return;
    deviceNameValue_->SetLabel(ToWx(deskhubp::SessionDeviceName()));
    deviceNameValue_->GetParent()->Layout();
}

void MainFrame::AddAllowedClientsSection(const SettingsArea& area) {
    AddAreaHint(area, ui::kPairedHint);

    auto* allowRow = new wxBoxSizer(wxHORIZONTAL);
    const Field<wxTextCtrl> allowKey =
        MakeTextField(area.body, wxString(), -1, wxTE_PROCESS_ENTER);
    allowClientCtrl_ = allowKey.control;
    allowClientCtrl_->SetName("allow-client-field");
    allowClientCtrl_->SetHint(ToWx(ui::kAllowClientPlaceholder));
    allowClientCtrl_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { AllowClientKey(); });
    allowRow->Add(allowKey.box, wxSizerFlags(1).CentreVertical());
    auto* allow = MakeButton(area.body, ToWx(ui::kAllowClientAction));
    allow->SetName("allow-client");
    allow->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { AllowClientKey(); });
    allowRow->Add(allow, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(kPageGap)));
    area.sizer->Add(allowRow, AreaWideRowFlags(area));

    allowClientError_ = MakeErrorLabel(area.body, ui::kAllowClientInvalid);
    allowClientError_->SetName("allow-client-error");
    area.sizer->Add(allowClientError_, AreaRowFlags(area));

    pairedList_ = new wxScrolledWindow(area.body, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxVSCROLL | wxHSCROLL | wxBORDER_SIMPLE);
    pairedList_->SetScrollRate(FromDIP(10), FromDIP(10));
    pairedRows_ = new wxBoxSizer(wxVERTICAL);
    pairedList_->SetSizer(pairedRows_);
    pairedList_->SetMinSize(FromDIP(wxSize(-1, kListMinH)));
    area.sizer->Add(pairedList_, AreaWideRowFlags(area));

    pairedHint_ = MakeHint(area.body, ToWx(ui::kPairedEmpty));
    area.sizer->Add(pairedHint_, AreaRowFlags(area));

    auto* forgetAll = MakePrimaryButton(area.body, ToWx(ui::kPairedForgetAll));
    forgetAll->SetName("forget-all-devices");
    forgetAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ForgetEveryDevice(); });
    area.sizer->Add(forgetAll, AreaRowFlags(area));
    AddAreaHint(area, ui::kPairedForgetNote);
}

void MainFrame::AllowClientKey() {
    const std::string keyText =
        ui::TrimAscii(std::string(allowClientCtrl_->GetValue().utf8_str()));
    const bool allowed = deskhubp::RememberAuthorizedKey(keyText);
    allowClientError_->Show(!allowed);
    if (allowed) allowClientCtrl_->Clear();
    RefreshPairedDevices();
}

void MainFrame::CopyWithFeedback(wxButton* button, const wxString& text) {
    CopyTextToClipboard(text);
    RestoreCopiedButton();
    copiedButton_ = button;
    copiedButtonLabel_ = button->GetLabel();
    button->SetLabel(ToWx(ui::kCopiedButton));
    copiedButtonTimer_.StartOnce(kCopiedRevertMs);
}

void MainFrame::RestoreCopiedButton() {
    copiedButtonTimer_.Stop();
    if (copiedButton_) copiedButton_->SetLabel(copiedButtonLabel_);
    copiedButton_ = nullptr;
}

void MainFrame::RefreshPairedDevices() {
    if (pairedList_ == nullptr) return;
    const auto authorized = deskhubp::ListAuthorizedClients();
    pairedDevices_ = authorized ? *authorized : std::vector<deskhubp::AuthorizedClient>{};

    pairedRows_->Clear(true);
    const TableRow header = BeginTableRow(pairedList_, *wxWHITE);
    AddTableCell(header, ToWx(ui::kPairedColumnName), 200, true);
    AddTableCell(header, ToWx(ui::kPairedColumnKey), 130, true);
    header.cells->AddSpacer(FromDIP(ui::kHostActionWidth));
    EndTableRow(pairedRows_, header);
    for (const deskhubp::AuthorizedClient& device : pairedDevices_) {
        const TableRow row = BeginTableRow(pairedList_, *wxWHITE);
        AddTableCell(row,
            ToWx(device.label.empty() ? std::string(ui::kUnnamedClient) : device.label), 200,
            false);
        AddTableCell(row, ToWx(deskhub::ShortFingerprint(device.fingerprint)), 130, false);
        auto* forget = MakeRowButton(row.panel, ToWx(ui::kPairedForget), kOffline);
        forget->SetName("forget-device");
        const deskhub::Fingerprint fingerprint = device.fingerprint;
        forget->Bind(wxEVT_BUTTON, [this, fingerprint](wxCommandEvent&) {
            deskhubp::ForgetAuthorizedClient(fingerprint);
            RefreshPairedDevices();
        });
        row.cells->Add(forget, wxSizerFlags().CentreVertical());
        EndTableRow(pairedRows_, row);
    }
    pairedHint_->Show(pairedDevices_.empty());
    RelayoutTable(pairedList_);
}

void MainFrame::AddSavedHostsSection(const SettingsArea& area) {
    wxWindow* panel = area.body;
    AddAreaHint(area, ui::kSavedHostsHint);

    savedHostList_ = new wxScrolledWindow(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxVSCROLL | wxHSCROLL | wxBORDER_SIMPLE);
    savedHostList_->SetScrollRate(FromDIP(10), FromDIP(10));
    savedHostRows_ = new wxBoxSizer(wxVERTICAL);
    savedHostList_->SetSizer(savedHostRows_);
    savedHostList_->SetMinSize(FromDIP(wxSize(-1, kListMinH)));
    area.sizer->Add(savedHostList_, AreaWideRowFlags(area));

    savedHostHint_ = MakeHint(panel, ToWx(ui::kSavedHostsEmpty));
    area.sizer->Add(savedHostHint_, AreaRowFlags(area));

    hostProfileError_ = MakeErrorLabel(panel, "");
    hostProfileError_->SetName("host-profile-error");
    area.sizer->Add(hostProfileError_, AreaRowFlags(area));
}

void MainFrame::RefreshSavedHosts() {
    if (savedHostList_ == nullptr) return;
    const std::optional<deskhub::TrustStore> store = deskhubp::TryLoadTrustStore();
    const std::vector<deskhub::TrustedHost> hosts =
        store ? store->Hosts() : std::vector<deskhub::TrustedHost>{};

    savedHostRows_->Clear(true);
    const TableRow header = BeginTableRow(savedHostList_, *wxWHITE);
    AddTableCell(header, ToWx(ui::kHostNameLabel), 150, true);
    AddTableCell(header, ToWx(ui::kHostLastAddressLabel), 170, true);
    AddTableCell(header, ToWx(ui::kHostKeyLabel), 130, true);
    header.cells->AddSpacer(FromDIP(2 * ui::kHostActionWidth + 8));
    EndTableRow(savedHostRows_, header);
    for (const deskhub::TrustedHost& host : hosts) {
        const TableRow row = BeginTableRow(savedHostList_, *wxWHITE);
        AddTableCell(row, ToWx(host.label), 150, false);
        AddTableCell(row, ToWx(host.endpoint), 170, false);
        AddTableCell(row, ToWx(deskhub::ShortFingerprint(host.fingerprint)), 130, false)
            ->SetToolTip(ToWx(deskhub::FormatFingerprint(host.fingerprint)));

        auto* connect = MakeRowButton(row.panel, ToWx(ui::kConnectButton), kAccent);
        connect->SetName("connect-saved-host");
        connect->Bind(wxEVT_BUTTON,
            [this, endpoint = host.endpoint](wxCommandEvent&) { ConnectSavedHost(endpoint); });
        row.cells->Add(connect, wxSizerFlags().CentreVertical().Border(wxRIGHT, FromDIP(8)));

        auto* remove = MakeRowButton(row.panel, ToWx(ui::kRemoveHostAction), kOffline);
        remove->SetName("remove-saved-host");
        remove->Bind(wxEVT_BUTTON,
            [this, alias = host.label](wxCommandEvent&) {
                CallAfter([this, alias] { RemoveSavedHost(alias); });
            });
        row.cells->Add(remove, wxSizerFlags().CentreVertical());
        EndTableRow(savedHostRows_, row);
    }
    savedHostHint_->Show(store.has_value() && hosts.empty());
    if (!store) ShowHostProfileError(ui::HostProfileError::StoreUnreadable);
    RelayoutTable(savedHostList_);
}

void MainFrame::ShowHostProfileError(ui::HostProfileError error) {
    const bool failed = error != ui::HostProfileError::None;
    hostProfileError_->SetLabel(failed ? ToWx(ui::HostProfileErrorText(error)) : wxString());
    hostProfileError_->Wrap(HintWrapWidth(hostProfileError_));
    hostProfileError_->Show(failed);
    RelayoutTable(savedHostList_);
}

void MainFrame::RemoveSavedHost(const std::string& alias) {
    ShowHostProfileError(deskhubp::RemoveHostProfile(alias));
    RefreshSavedHosts();
}

void MainFrame::ConnectSavedHost(const std::string& endpoint) {
    SelectPage(kPageClient);
    ConnectToDevice(endpoint);
}

void MainFrame::ForgetEveryDevice() {
    if (pairedDevices_.empty()) return;
    wxMessageDialog dialog(this, ToWx(ui::kPairedForgetAllPrompt), "Deskhub",
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (dialog.ShowModal() != wxID_YES) return;
    deskhubp::ClearAuthorizedKeys();
    RefreshPairedDevices();
}

void MainFrame::AddLabelledSetting(const SettingsArea& area, wxFlexGridSizer*& grid,
    const char* label, wxWindow* control) {
    if (grid == nullptr) {
        grid = new wxFlexGridSizer(2, FromDIP(wxSize(14, 10)));
        area.sizer->Add(grid, AreaRowFlags(area));
    }
    grid->Add(new wxStaticText(area.body, wxID_ANY, ToWx(label)), wxSizerFlags().CentreVertical());
    grid->Add(control);
}

wxSizer* MainFrame::MakeTransferFolderRow(wxWindow* area, const char* label) {
    auto* folderRow = new wxBoxSizer(wxHORIZONTAL);
    folderRow->Add(new wxStaticText(area, wxID_ANY, ToWx(label)), wxSizerFlags().CentreVertical());
    transferDirLabel_ = new wxStaticText(area, wxID_ANY, ToWx(deskhubp::PathText(TransferFolder())),
        wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
    transferDirLabel_->SetForegroundColour(kMutedText);
    folderRow->Add(transferDirLabel_,
        wxSizerFlags(1).CentreVertical().Border(wxLEFT, FromDIP(kPageGap)));
    auto* folderBtn = MakeButton(area, ToWx(ui::kTransferChooseButton));
    folderBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ChooseTransferFolder(); });
    folderRow->Add(folderBtn, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(kPageGap)));
    return folderRow;
}

void MainFrame::AddSetting(const SettingsArea& area, const ui::SettingsEntry& entry,
    wxFlexGridSizer*& grid) {
    const wxSizerFlags pad = AreaRowFlags(area);
    wxWindow* parent = area.body;
    Field<wxTextCtrl> name;
    switch (entry.field) {
        case ui::SettingField::Fps:
            fpsCtrl_ = new NumberField(parent, settings_.fps, ui::kMaxSettingsFps);
            AddLabelledSetting(area, grid, entry.text, fpsCtrl_);
            return;
        case ui::SettingField::Bitrate:
            bitrateCtrl_ =
                new NumberField(parent, settings_.bitrateMbps, ui::kMaxSettingsBitrateMbps);
            AddLabelledSetting(area, grid, entry.text, bitrateCtrl_);
            return;
        case ui::SettingField::Quality:
            qualityChoice_ = new wxChoice(parent, wxID_ANY);
            for (const auto& preset : deskhub::media::kQualityPresets)
                qualityChoice_->Append(ToWx(preset.label));
            qualityChoice_->SetSelection(
                int(deskhub::media::QualityPresetIndex(settings_.maxDim)));
            AddLabelledSetting(area, grid, entry.text, qualityChoice_);
            return;
        case ui::SettingField::Port:
            portCtrl_ = new NumberField(parent, settings_.port, ui::kMaxSettingsPort);
            AddLabelledSetting(area, grid, entry.text, portCtrl_);
            return;
        case ui::SettingField::DeviceName:
            name = MakeTextField(parent, ToWx(settings_.deviceName), ui::kAddressFieldWidth);
            deviceNameCtrl_ = name.control;
            deviceNameCtrl_->SetName("name-field");
            deviceNameCtrl_->SetMaxLength(deskhub::kMaxClientNameBytes);
            deviceNameCtrl_->SetHint(ToWx(deskhubp::LocalDeviceName()));
            AddLabelledSetting(area, grid, entry.text, name.box);
            return;
        case ui::SettingField::TransferFolder:
            grid = nullptr;
            area.sizer->Add(MakeTransferFolderRow(parent, entry.text), AreaWideRowFlags(area));
            return;
        case ui::SettingField::None:
        case ui::SettingField::Permissions:
        case ui::SettingField::Count: return;
        case ui::SettingField::AllowInput:
        case ui::SettingField::ShareAudio:
        case ui::SettingField::AutoShare:
        case ui::SettingField::PlayAudio:
        case ui::SettingField::ClipboardSync:
        case ui::SettingField::KeepAwake:
        case ui::SettingField::Autostart:
        case ui::SettingField::CloseToTray: break;
    }
    grid = nullptr;
    auto* check = new wxCheckBox(parent, wxID_ANY, ToWx(entry.text));
    const bool* flag = ui::SettingFlag(settings_, entry.field);
    check->SetValue(entry.field == ui::SettingField::Autostart ? deskhubp::AutostartEnabled()
                                                               : flag != nullptr && *flag);
    check->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    settingChecks_[entry.field] = check;
    area.sizer->Add(check, pad);
}

wxWindow* MainFrame::BuildSettingsPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    auto* outer = new wxBoxSizer(wxVERTICAL);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    outer->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kPagePad)));

    sizer->Add(MakeHeading(panel, ui::kSidebarSettings));

    SettingsArea area;
    wxFlexGridSizer* grid = nullptr;
    for (const ui::SettingsEntry& entry : ui::DesktopSettingsLayout()) {
        switch (entry.kind) {
            case ui::SettingsEntryKind::Area:
                area = MakeSettingsArea(panel, entry.text);
                sizer->Add(area.card, wxSizerFlags().Expand().Border(wxTOP, FromDIP(kPageGap)));
                grid = nullptr;
                break;
            case ui::SettingsEntryKind::Hint:
                grid = nullptr;
                AddAreaHint(area, entry.text);
                break;
            case ui::SettingsEntryKind::Section:
                grid = nullptr;
                AddAreaSection(area, entry.text);
                break;
            case ui::SettingsEntryKind::Setting: AddSetting(area, entry, grid); break;
        }
    }

    fpsCtrl_->OnChange([this] { SaveSettings(); });
    bitrateCtrl_->OnChange([this] { SaveSettings(); });
    portCtrl_->OnChange([this] { SaveSettings(); });
    qualityChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { SaveSettings(); });
    deviceNameCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { SaveDeviceName(); });

    panel->SetSizer(outer);
    panel->FitInside();
    return panel;
}

void MainFrame::SelectPage(int page) {
    for (int i = 0; i < kPageCount; ++i) pageButtons_[i]->SetSelected(i == page);
    book_->ChangeSelection(size_t(page));
    if (page == kPageHost && !Sharing()) RefreshDisplayChoices();
    if (page != kPageDevices) {
        devicesTimer_.Stop();
        return;
    }
    RefreshDeviceNameRow();
    RefreshPairedDevices();
    RefreshSavedHosts();
    RefreshAccessRequests();
    devicesTimer_.Start(kDevicesPollMs);
}

void MainFrame::OnDevicesTimer(wxTimerEvent&) {
    RefreshAccessRequests();
}

void MainFrame::RefreshDisplayChoices() {
    std::map<std::string, bool> previousTicks;
    for (size_t i = 0; i < displayChecks_.size() && i < availableDisplays_.size(); ++i)
        previousTicks[availableDisplays_[i].name] = displayChecks_[i]->GetValue();

    displayChecksBox_->Clear(true);
    displayChecks_.clear();
    availableDisplays_ = deskhubp::ListDisplays();
    hostRows_.clear();
    RebuildHostTable();
    for (size_t i = 0; i < availableDisplays_.size(); ++i) {
        const ShareSource& source = availableDisplays_[i];
        auto* check = new wxCheckBox(hostPickerFrame_, wxID_ANY,
            ToWx(deskhub::media::SourcePickerLabel(source.name, uint8_t(i), source.width,
                source.height)));
        const auto seen = previousTicks.find(source.name);
        check->SetValue(seen == previousTicks.end() || seen->second);
        displayChecksBox_->Add(check,
            wxSizerFlags().Border(wxTOP, i == 0 ? 0 : FromDIP(kPickerGap)));
        displayChecks_.push_back(check);
    }
    hostPickerFrame_->FitInside();
    ShowHostTable(false);
}

bool MainFrame::TerminalTicked() const {
    return hostTerminalCheck_->GetValue();
}

bool MainFrame::FilesTicked() const {
    return hostFilesCheck_->GetValue();
}

std::filesystem::path MainFrame::TransferFolder() const {
    if (settings_.transferDir.empty()) return deskhubp::DefaultTransferDir();
    const std::u8string wide(settings_.transferDir.begin(), settings_.transferDir.end());
    return std::filesystem::path(wide);
}

void MainFrame::ChooseTransferFolder() {
    wxDirDialog picker(this, ToWx(ui::kTransferFolderLabel), ToWx(deskhubp::PathText(TransferFolder())),
        wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (picker.ShowModal() != wxID_OK) return;

    settings_.transferDir = ui::TruncateSettingsPath(std::string(picker.GetPath().utf8_str()));
    deskhubp::SaveUiSettings(settings_);
    transferDirLabel_->SetLabel(ToWx(deskhubp::PathText(TransferFolder())));
    if (!Sharing()) ShowIdleHostState();
}

bool MainFrame::Sharing() const {
    return hosting_ || hostStarting_ || share_.terminalHost().Running() || share_.fileHost().Running();
}

wxWindow* MainFrame::BuildHostTable(wxWindow* parent) {
    auto* card = new wxPanel(parent);
    card->SetBackgroundColour(kRowLine);
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* holder = new wxPanel(card);
    holder->SetBackgroundColour(*wxWHITE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxPanel(holder);
    header->SetBackgroundColour(kBannerIdleBg);
    header->SetMinSize(FromDIP(wxSize(-1, 30)));
    auto* headerRow = new wxBoxSizer(wxHORIZONTAL);
    headerRow->AddSpacer(FromDIP(ui::kHostRowBarWidth + ui::kHostCellGap));
    for (const ui::HostColumn& column : ui::kHostColumns) {
        auto* title = new wxStaticText(header, wxID_ANY, ToWx(column.title),
            wxDefaultPosition, FromDIP(wxSize(column.width, -1)), WxAlign(column.align));
        title->SetForegroundColour(kMutedText);
        title->SetFont(title->GetFont().Bold().Scaled(0.85f));
        headerRow->Add(title, wxSizerFlags().CentreVertical().Border(wxRIGHT,
                                  FromDIP(ui::kHostCellGap)));
    }
    headerRow->AddSpacer(FromDIP(kHostActionsWidth));
    header->SetSizer(headerRow);
    sizer->Add(header, wxSizerFlags().Expand());

    auto* headerLine = new wxWindow(holder, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 1)));
    headerLine->SetBackgroundColour(kRowLine);
    sizer->Add(headerLine, wxSizerFlags().Expand());

    hostTable_ = new wxScrolledWindow(holder);
    hostTable_->SetBackgroundColour(*wxWHITE);
    hostTable_->SetScrollRate(FromDIP(8), FromDIP(8));
    hostTable_->SetSizer(new wxBoxSizer(wxVERTICAL));
    sizer->Add(hostTable_, wxSizerFlags(1).Expand());

    holder->SetSizer(sizer);
    cardSizer->Add(holder, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(1)));
    card->SetSizer(cardSizer);
    return card;
}

bool IsAttachedLocally(const ui::HostRow& ref) {
    return ref.terminal && ref.viewer && ref.shellState == deskhub::TerminalState::Local;
}

bool CanAttachLocally(const ui::HostRow& ref) {
    return ref.terminal && ref.viewer && ref.shellState != deskhub::TerminalState::Local;
}

wxButton* MainFrame::MakeRowAction(wxWindow* parent, const ui::HostRow& ref) {
    const bool viewer = ref.viewer;
    if (ref.files) {
        auto* stop = MakeRowButton(parent, ToWx(ui::kStopDisplayAction), kOffline);
        stop->Bind(wxEVT_BUTTON,
            [this](wxCommandEvent&) { share_.StopFilesRow(screenSharing_); });
        stop->Show(!viewer);
        return stop;
    }
    const bool remoteRow = viewer && !IsAttachedLocally(ref);
    auto* button = MakeRowButton(parent,
        ToWx(remoteRow ? ui::kDisconnectViewerAction : ui::kStopDisplayAction),
        remoteRow ? kWarning : kOffline);

    if (ref.terminal) {
        const uint32_t termId = ref.termId;
        button->Bind(wxEVT_BUTTON, [this, viewer, termId](wxCommandEvent&) {
            if (viewer) {
                share_.KickShell(termId);
            } else {
                share_.StopTerminalRow(screenSharing_);
            }
        });
        return button;
    }

    const uint8_t sourceId = ref.sourceId;
    const std::string addr = ref.viewerAddr;
    button->Bind(wxEVT_BUTTON, [this, viewer, sourceId, addr](wxCommandEvent&) {
        if (viewer) {
            KickViewer(sourceId, addr);
        } else {
            StopDisplay(sourceId);
        }
    });
    return button;
}

wxButton* MainFrame::MakeRowAttach(wxWindow* parent, const ui::HostRow& ref) {
    auto* button = MakeRowButton(parent, ToWx(ui::kAttachShellAction), kOffline);
    const uint32_t termId = ref.termId;
    button->Bind(wxEVT_BUTTON,
        [this, termId](wxCommandEvent&) { share_.StopAndAttachShell(termId); });
    return button;
}

wxButton* MainFrame::MakeRowOpenFolder(wxWindow* parent) {
    auto* button = MakeRowButton(parent, ToWx(ui::kOpenFolderAction), kAccent);
    button->Bind(wxEVT_BUTTON,
        [this](wxCommandEvent&) { deskhubp::OpenFolder(TransferFolder()); });
    return button;
}

void MainFrame::RebuildHostTable() {
    hostRowViews_.clear();
    wxSizer* rows = hostTable_->GetSizer();
    rows->Clear(true);

    for (const ui::HostRow& ref : hostRows_) {
        if (!ref.viewer && !hostRowViews_.empty()) {
            auto* line = new wxWindow(hostTable_, wxID_ANY, wxDefaultPosition,
                FromDIP(wxSize(-1, 1)));
            line->SetBackgroundColour(kRowLine);
            rows->Add(line, wxSizerFlags().Expand().Border(wxTOP | wxBOTTOM, FromDIP(4)));
        }

        HostRowView view;
        view.panel = new wxPanel(hostTable_);
        view.panel->SetBackgroundColour(ref.viewer ? kViewerRowBg : *wxWHITE);
        view.panel->SetMinSize(FromDIP(wxSize(-1, ui::kHostRowHeight)));

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        view.bar = new wxWindow(view.panel, wxID_ANY, wxDefaultPosition,
            FromDIP(wxSize(ui::kHostRowBarWidth, -1)));
        row->Add(view.bar, wxSizerFlags().Expand());
        row->AddSpacer(FromDIP(ui::kHostCellGap));

        for (int c = 0; c < kHostColumnCount; ++c) {
            const ui::HostColumn& column = ui::kHostColumns[size_t(c)];
            view.cells[c] = new wxStaticText(view.panel, wxID_ANY, wxString(), wxDefaultPosition,
                FromDIP(wxSize(column.width, -1)), WxAlign(column.align));
            if (column.mono) view.cells[c]->SetFont(MonoFont(view.cells[c]));
            if (c == 0 && !ref.viewer)
                view.cells[c]->SetFont(view.cells[c]->GetFont().Bold());
            row->Add(view.cells[c], wxSizerFlags().CentreVertical().Border(wxRIGHT,
                                        FromDIP(ui::kHostCellGap)));
        }
        row->Add(MakeRowAction(view.panel, ref), wxSizerFlags().CentreVertical());
        row->AddSpacer(FromDIP(ui::kHostCellGap));
        if (CanAttachLocally(ref)) {
            row->Add(MakeRowAttach(view.panel, ref), wxSizerFlags().CentreVertical());
        } else if (ref.files && !ref.viewer) {
            row->Add(MakeRowOpenFolder(view.panel), wxSizerFlags().CentreVertical());
        } else {
            row->AddSpacer(FromDIP(ui::kHostActionWidth));
        }
        row->AddSpacer(FromDIP(ui::kHostCellGap));
        view.panel->SetSizer(row);

        rows->Add(view.panel, wxSizerFlags().Expand());
        hostRowViews_.push_back(view);
    }

    hostTable_->FitInside();
    hostTable_->Layout();
}

void MainFrame::ShowHostTable(bool sharing) {
    hostPickerFrame_->Show(!sharing);
    hostTableHolder_->Show(sharing);
    hostHint_->Show(!sharing);
    RelayoutHostPage();
}

void MainFrame::RefreshDeviceList() {
    deviceList_->DeleteAllItems();
    for (size_t i = 0; i < recent_.size(); ++i) {
        const ui::RecentDevice& device = recent_[i];
        const long row = deviceList_->InsertItem(
            long(i), device.name.empty() ? wxString("-") : ToWx(device.name));
        deviceList_->SetItem(row, 1, ToWx(device.addr));
        deviceList_->SetItem(row, 2,
            device.lastConnectedUnix != 0 ? ToWx(FormatUnixMinute(device.lastConnectedUnix))
                                          : wxString("-"));
    }
}

void MainFrame::ApplyHostState(HostShareState state, const wxString& detail) {
    const HostStateStyle style = StyleFor(state);
    const bool live = hosting_ || state == HostShareState::kStarting;

    hostStateLabel_->SetLabel(ToWx(style.label));
    hostStateLabel_->SetForegroundColour(style.tint);
    hostStateLabel_->SetBackgroundColour(style.background);
    hostStatusLabel_->SetLabel(detail);
    hostStatusLabel_->Wrap(FromDIP(kBannerWrapWidth));
    hostStatusLabel_->Show(!detail.empty());
    hostStatusLabel_->SetBackgroundColour(style.background);
    hostBanner_->Show(state != HostShareState::kIdle);
    ShowPortCard();
    hostPortPanel_->Show(live);
    hostBannerBar_->SetBackgroundColour(style.tint);
    hostBanner_->SetBackgroundColour(style.background);
    hostBanner_->Layout();
    RelayoutHostPage();
    hostBanner_->Refresh();

    shareBtn_->SetLabel(ToWx(live ? ui::kStopSharing : ui::kStartSharing));
    PaintButton(shareBtn_, live ? kOffline : kAccent);
    shareBtn_->Refresh();

    bindChoice_->Enable(!live);
    qrBtn_->Show(live);
    if (!live) CloseQrWindow();
    accessRequestsPanel_->Show(live);
    ShowHostTable(live);
}

void MainFrame::ShowIdleHostState() {
    ApplyHostState(HostShareState::kIdle,
        ToWx(ui::UdpPortLine(uint16_t(settings_.port)) + "."));
}

void MainFrame::ShowPortCard() {
    copiedTimer_.Stop();
    hostPortLabel_->SetLabel(ToWx(std::to_string(hosting_ ? sharePort_ : settings_.port)));
    hostPortCopyBtn_->SetLabel(ToWx(ui::kCopyButton));
    hostPortPanel_->Layout();
}

void MainFrame::CopySharePort() {
    CopyTextToClipboard(hostPortLabel_->GetLabel());
    hostPortCopyBtn_->SetLabel(ToWx(ui::kCopiedButton));
    copiedTimer_.StartOnce(kCopiedRevertMs);
}

void MainFrame::BeginAutoShare() {
    if (Sharing() || autoShareGate_.Decided()) {
        autoShareTimer_.Stop();
        return;
    }

    RefreshDisplayChoices();
    const ui::AutoShareStep step = autoShareGate_.Advance(!availableDisplays_.empty());
    if (step == ui::AutoShareStep::KeepWaiting) {
        ApplyHostState(HostShareState::kIdle, ToWx(ui::kWaitingForDisplays));
        if (!autoShareTimer_.IsRunning()) autoShareTimer_.Start(int(autoShareGate_.ProbeMs()));
        return;
    }

    autoShareTimer_.Stop();
    if (step == ui::AutoShareStep::GiveUpWaiting)
        LOGW("[Share] No display showed up in the %u ms after launch; sharing without one.",
            autoShareGate_.WaitedMs());
    OnShare(ShareTrigger::kAutomatic);
}

void MainFrame::OnAutoShareTimer(wxTimerEvent&) {
    BeginAutoShare();
}

void MainFrame::ReportShareProblem(const wxString& text, const wxString& title) {
    if (shareTrigger_ == ShareTrigger::kAutomatic) {
        LOGW("[Share] %s", std::string(text.utf8_str()).c_str());
        ApplyHostState(HostShareState::kIdle, text);
        return;
    }
    wxMessageBox(text, title, wxOK | wxICON_WARNING, this);
}

void MainFrame::OnDisplayChanged(wxDisplayChangedEvent& event) {
    event.Skip();
    if (Sharing()) return;
    RefreshDisplayChoices();
}

void MainFrame::OnShare(ShareTrigger trigger) {
    if (hostStarting_) return;
    if (Sharing()) {
        StopHosting();
        return;
    }
    shareTrigger_ = trigger;

    const bool terminal = TerminalTicked();
    const bool files = FilesTicked();
    if (availableDisplays_.empty() && !terminal && !files) {
        const std::string err = deskhubp::ListDisplaysError();
        ReportShareProblem(err.empty() ? ToWx(ui::kNoDisplayFound) : ToWx(err),
            ToWx(ui::kCaptureUnavailableTitle));
        return;
    }

    std::vector<ShareSource> chosen;
    for (size_t i = 0; i < displayChecks_.size() && i < availableDisplays_.size(); ++i)
        if (displayChecks_[i]->GetValue()) chosen.push_back(availableDisplays_[i]);
    if (chosen.empty() && !terminal && !files) {
        ReportShareProblem(ToWx(ui::kNoDisplayTicked), "Deskhub");
        return;
    }

    const deskhub::ShareClampResult clamp = deskhub::ClampShareSources(chosen);
    if (clamp.clamped) ReportShareProblem(ToWx(ui::ShareClampWarning()), "Deskhub");

    const std::vector<ShareSource>& sources = clamp.sources;

    const ShareOptions options = deskhub::ShareOptionsOf(settings_, terminal, files);

    terminalRequested_ = terminal;
    filesRequested_ = files;
    StartHosting(sources, options);
}

void MainFrame::StartFileShare() {
    if (!share_.StartFileShare(TransferFolder())) filesRequested_ = false;
}

void MainFrame::ApplySharingBanner() {
    deskhubp::ShareBanner banner;
    banner.screenSharing = screenSharing_;
    banner.hosting = hosting_;
    banner.port = sharePort_;
    banner.viewOnly = shareViewOnly_;
    banner.bindWarning = shareBindWarning_;
    ApplyHostState(HostShareState::kSharing, ToWx(share_.BannerText(banner)));
}

void MainFrame::StartHosting(const std::vector<ShareSource>& sources,
    const ShareOptions& options) {
    hostStarting_ = true;
    shareBtn_->Disable();
    ApplyHostState(HostShareState::kStarting, wxString());
    hostRows_.clear();
    RebuildHostTable();

    shareDriver_.Join();
    shareDriver_.StartAsync(
        share_.sharingHost(), sources, options, UiPoster(),
        [this, port = options.port, allowInput = options.allowInput](bool started,
            const std::string& error) { OnHostStarted(started, error, port, allowInput); });
}

void MainFrame::OnHostStarted(bool started, const std::string& error, uint16_t port,
    bool allowInput) {
    hostStarting_ = false;
    shareBtn_->Enable();

    if (!started) {
        terminalRequested_ = false;
        filesRequested_ = false;
        ShowIdleHostState();
        RefreshDisplayChoices();
        ReportShareProblem(ToWx(std::string(ui::kShareStartFailed) + ".\n\n" + error), "Deskhub");
        return;
    }

    hosting_ = true;
    screenSharing_ = !share_.sharingHost().Status().empty();
    sharePort_ = port;
    shareViewOnly_ = !allowInput;
    shareBindWarning_ = share_.sharingHost().BindWarning();
    if (terminalRequested_) share_.StartTerminalShare();
    if (filesRequested_) StartFileShare();
    ApplySharingBanner();
    accessRequestsSeen_.reset();
    RefreshAccessRequests();
    hostTimer_.Start(int(deskhubp::kShareStatusPollMs));
    if (settings_.clipboardSync) clipTimer_.Start(1000);
}

void MainFrame::OnClipboardTimer(wxTimerEvent&) {
    if (!hosting_) return;
    if (!pendingClipboard_) pendingClipboard_ = share_.sharingHost().TakeRemoteClipboard();
    if (pendingClipboard_) {
        if (WriteClipboardText(wxString::FromUTF8(*pendingClipboard_))) pendingClipboard_.reset();
        return;
    }
    const std::optional<std::string> text = ReadClipboardText();
    if (text && !text->empty()) share_.sharingHost().OfferLocalClipboard(*text);
}

void MainFrame::StopHosting() {
    hostTimer_.Stop();
    clipTimer_.Stop();
    share_.StopTerminalShare();
    share_.StopFileShare();
    share_.sharingHost().Stop();
    shareDriver_.Join();
    hosting_ = false;
    screenSharing_ = false;
    terminalRequested_ = false;
    filesRequested_ = false;
    pendingClipboard_.reset();
    shareBindWarning_.clear();
    shareViewOnly_ = false;
    hostStatus_.clear();
    ShowIdleHostState();
    RefreshDisplayChoices();
}

void MainFrame::OnHostTimer(wxTimerEvent&) {
    if (!Sharing()) {
        hostTimer_.Stop();
        return;
    }

    if (hosting_) {
        std::vector<ShareSourceStatus> rows;
        const deskhubp::ShareDriveState state = shareDriver_.Poll(share_.sharingHost(), rows);
        if (state == deskhubp::ShareDriveState::Stopped) {
            StopHosting();
            return;
        }
        if (state == deskhubp::ShareDriveState::Running) {
            hostStatus_ = std::move(rows);
            if (screenSharing_ && hostStatus_.empty()) {
                screenSharing_ = false;
                ApplySharingBanner();
            }
        }
    }

    share_.RefreshShells();
    share_.RefreshTransfers();
    UpdateHostRows(hostStatus_);
    RefreshAccessRequests();
}

void MainFrame::UpdateHostRows(const std::vector<ShareSourceStatus>& rows) {
    std::vector<ui::HostRow> refs = ui::BuildHostRows(rows, share_.terminalHost().Running(), share_.shells(),
        share_.fileHost().Running(), share_.transfers());

    if (refs != hostRows_) {
        hostRows_ = std::move(refs);
        RebuildHostTable();
        RelayoutHostPage();
    }

    for (size_t i = 0; i < hostRows_.size() && i < hostRowViews_.size(); ++i) {
        const ui::HostRow& ref = hostRows_[i];
        ui::HostRowCells cells;
        if (ref.files) {
            cells = ui::FilesRowText(ref, deskhubp::PathText(share_.fileHost().Directory()), share_.transfers());
        } else if (ref.terminal) {
            cells = ui::TerminalRowText(ref, uint16_t(settings_.port), share_.shells());
        } else {
            const ShareSourceStatus* s = ui::FindHostSource(rows, ref.sourceId);
            if (!s) continue;
            cells = ui::HostRowText(ref, *s);
        }

        const wxString texts[kHostColumnCount] = {ToWx(cells.source), ToWx(cells.size),
            ToWx(cells.viewers), ToWx(cells.client), ToWx(cells.capture), ToWx(cells.send),
            ToWx(cells.mbps), ToWx(cells.rtt)};
        const HostRowView& view = hostRowViews_[i];
        const wxColour colour = cells.online ? kHeadingText : kMutedText;
        view.bar->SetBackgroundColour(cells.online ? kOnline : kRowLine);
        view.bar->Refresh();

        for (int c = 0; c < kHostColumnCount; ++c) {
            wxStaticText* cell = view.cells[c];
            if (cell->GetLabel() != texts[c]) cell->SetLabel(texts[c]);
            cell->SetForegroundColour(colour);
        }
    }
}

void MainFrame::RelayoutHostPage() {
    if (!hostPage_) return;
    hostPage_->Layout();
    hostPage_->FitInside();
}

void MainFrame::StopDisplay(uint8_t sourceId) {
    if (!hosting_) return;
    share_.sharingHost().StopSource(sourceId);
}

void MainFrame::KickViewer(uint8_t sourceId, const std::string& viewerAddr) {
    if (!hosting_) return;
    NetAddr addr{};
    if (!ParseNetAddr(viewerAddr, addr)) return;
    share_.sharingHost().KickViewer(sourceId, addr.Pack());
}

void MainFrame::SetClientStatus(const wxString& text, const wxColour& colour) {
    clientStatus_->SetLabel(text);
    clientStatus_->SetForegroundColour(colour);
    clientStatus_->Wrap(HintWrapWidth(clientStatus_));
    clientStatus_->GetParent()->Layout();
}

void MainFrame::OpenFileSend(const NetAddr& server) {
    FileSendLaunch launch;
    launch.address = server.ToString();
    launch.clientName = deskhubp::SessionDeviceName();

    std::thread([launch] { RunStandaloneFileSend(launch); }).detach();
}

void MainFrame::OpenShell(const NetAddr& server) {
    TerminalLaunch launch;
    launch.address = server.ToString();
    launch.clientName = deskhubp::SessionDeviceName();

    if (!OpenTerminalWindow(this, launch))
        SetClientStatus(ToWx(ui::kTerminalUnreachable), kErrorText);
}

deskhubp::SourceQueryAsync::UiPost MainFrame::UiPoster() const {
    return [alive = alive_](std::function<void()> fn) {
        if (!wxTheApp) return;
        wxTheApp->CallAfter([alive, fn = std::move(fn)] {
            if (*alive) fn();
        });
    };
}

SourceQueryRequest MainFrame::ConnectRequest() {
    SourceQueryRequest request;
    request.onProgress = [this, post = UiPoster()](std::string_view text) {
        post([this, line = std::string(text)] { ShowConnectProgress(line); });
    };
    return request;
}

void MainFrame::StartConnect(const std::string& rawAddr) {
    LOGI("[UI] Connect requested for \"%s\".", rawAddr.c_str());
    SetClientStatus(wxString(), kMutedText);
    const std::string addr = ui::TrimAscii(rawAddr);
    if (addr.empty()) {
        wxMessageBox("Enter the host machine's IP address first (e.g., 192.168.1.10).",
            "Deskhub", wxOK | wxICON_WARNING, this);
        return;
    }

    NetAddr server{};
    if (!ParseNetAddr(addr, server)) {
        wxMessageBox(ToWx(ui::InvalidAddressLine(addr) + "\n" + ui::InvalidAddressHint()),
            "Deskhub", wxOK | wxICON_ERROR, this);
        return;
    }
    LaunchQuery(server, std::string(), addr);
}

void MainFrame::StartConnectByInvite(const std::string& invite) {
    LOGI("[UI] Connect requested with a pairing invite.");
    SetClientStatus(wxString(), kMutedText);
    if (!deskhub::ParsePairingInvite(invite)) {
        SetClientStatus(ToWx(ui::kInviteInvalid), kErrorText);
        return;
    }
    LaunchQuery(NetAddr{}, invite, std::string());
}

void MainFrame::LaunchQuery(const NetAddr& server, const std::string& invite,
    const std::string& addr) {
    const bool started = connectDriver_.QueryAsync(server, ConnectRequest(), invite, UiPoster(),
        [this, addr](const deskhubp::ConnectOutcome& outcome) { OnSourcesReady(addr, outcome); });
    if (!started) return;
    connectCancelled_ = false;
    cancelConnectBtn_->Enable();
    connectBtn_->Disable();
    SetClientStatus(ToWx(ui::kQueryingSources), kMutedText);
}

void MainFrame::ShowConnectProgress(const std::string& text) {
    cancelConnectBtn_->Show();
    SetClientStatus(ToWx(text), kMutedText);
}

void MainFrame::CancelConnect() {
    connectCancelled_ = true;
    connectDriver_.Cancel();
    cancelConnectBtn_->Disable();
}

void MainFrame::ConnectRow(long row) {
    if (row < 0 || size_t(row) >= recent_.size()) return;
    const std::string addr = recent_[size_t(row)].addr;
    ConnectToDevice(addr);
}

void MainFrame::ShowAddressInFields(const std::string& addr) {
    const uint16_t port = ui::AddressPort(addr);
    addrCtrl_->ChangeValue(ToWx(ui::AddressHost(addr)));
    connectPortCtrl_->ChangeValue(
        ToWx(std::to_string(port != 0 ? port : deskhub::kDeskhubPort)));
}

void MainFrame::ConnectToDevice(const std::string& addr) {
    ShowAddressInFields(addr);
    StartConnect(addr);
}

void MainFrame::OnSourcesReady(const std::string& addr, const deskhubp::ConnectOutcome& outcome) {
    connectBtn_->Enable();
    cancelConnectBtn_->Hide();
    SetClientStatus(wxString(), kMutedText);

    const std::string address = outcome.answeredAddress.empty() ? addr : outcome.answeredAddress;
    if (!outcome.ok && connectCancelled_) return;
    if (!outcome.ok && outcome.unknownHostKey) {
        ConfirmNewHost(address, *outcome.unknownHostKey);
        return;
    }
    if (!outcome.ok) {
        if (outcome.failureKind == SourceQueryFailure::AwaitingApproval) ShowAddressInFields(address);
        wxMessageBox(ToWx(outcome.failure), "Deskhub", wxOK | wxICON_ERROR, this);
        return;
    }

    ShowAddressInFields(address);
    deskhubp::RememberRecentDevice(address, outcome.hostName);
    recent_ = deskhubp::LoadRecentDevices();
    RefreshDeviceList();
    RefreshSavedHosts();

    OpenConnectionWindow(address, outcome);
}

void MainFrame::ConfirmNewHost(const std::string& addr, const deskhub::Fingerprint& fingerprint) {
    const std::string prompt = ui::TrustNewHostPrompt(addr,
        deskhub::FormatFingerprint(fingerprint),
        deskhubp::PreviousOwnerWarningFor(addr, fingerprint));
    wxMessageDialog dialog(this, ToWx(prompt), ToWx(ui::kTrustNewHostTitle),
        wxOK | wxCANCEL | wxCANCEL_DEFAULT | wxICON_WARNING);
    dialog.SetOKCancelLabels(ToWx(ui::kTrustNewHostAction), ToWx(ui::kCancelAction));
    if (dialog.ShowModal() != wxID_OK) return;

    const ui::HostProfileError error = deskhubp::TrustNewHost(addr, fingerprint);
    if (error != ui::HostProfileError::None) {
        wxMessageBox(ToWx(ui::HostProfileErrorText(error)), "Deskhub", wxOK | wxICON_ERROR, this);
        return;
    }
    RefreshSavedHosts();
    StartConnect(addr);
}

ConnectionFrame* MainFrame::ConnectionFor(const std::string& addr) const {
    for (ConnectionFrame* frame : connections_)
        if (ui::SameDeviceAddr(frame->Address(), addr)) return frame;
    return nullptr;
}

void MainFrame::OpenConnectionWindow(const std::string& addr,
    const deskhubp::ConnectOutcome& outcome) {
    if (ConnectionFrame* open = ConnectionFor(addr)) {
        open->Raise();
        open->SetFocus();
        return;
    }

    auto* frame = new ConnectionFrame(this, addr, outcome.caps, outcome.sources,
        settings_.clientControl);
    const int cascade = FromDIP(kConnectionWindowCascade) * int(connections_.size());
    frame->Move(GetPosition() + wxPoint(FromDIP(48) + cascade, FromDIP(48) + cascade));
    connections_.push_back(frame);
    frame->Show();
}

void MainFrame::ForgetConnection(ConnectionFrame* frame) {
    connections_.erase(std::remove(connections_.begin(), connections_.end(), frame),
        connections_.end());
}

void MainFrame::CloseEveryConnection() {
    const std::vector<ConnectionFrame*> open = connections_;
    connections_.clear();
    for (ConnectionFrame* frame : open) frame->Destroy();
}

void MainFrame::SetClientControl(bool on) {
    if (settings_.clientControl == on) return;
    settings_.clientControl = on;
    deskhubp::SaveUiSettings(settings_);
}

void MainFrame::SaveSettings() {
    settings_.fps = uint32_t(fpsCtrl_->GetValue());
    settings_.bitrateMbps = uint32_t(bitrateCtrl_->GetValue());
    settings_.port = uint32_t(portCtrl_->GetValue());
    const bool autostartWas = settings_.autostart;
    for (const auto& [field, check] : settingChecks_)
        if (bool* flag = ui::SettingFlag(settings_, field)) *flag = check->GetValue();
    const int quality = qualityChoice_->GetSelection();
    if (quality != wxNOT_FOUND)
        settings_.maxDim = deskhub::media::QualityPresetMaxDim(size_t(quality),
            settings_.maxDim);
    const int bindSel = bindChoice_->GetSelection();
    if (bindSel != wxNOT_FOUND && size_t(bindSel) < bindChoices_.size())
        settings_.bindIp = bindChoices_[size_t(bindSel)];
    ApplyTrayMode();
    if (settings_.autostart != autostartWas) {
        deskhubp::SetAutostartEnabled(settings_.autostart);
        settings_.autostart = deskhubp::AutostartEnabled();
        settingChecks_[ui::SettingField::Autostart]->SetValue(settings_.autostart);
    }
    deskhubp::SaveUiSettings(settings_);
    if (!Sharing()) ShowIdleHostState();
}

void MainFrame::SaveDeviceName() {
    const std::string name = ui::TruncateDeviceName(
        ui::TrimAscii(std::string(deviceNameCtrl_->GetValue().utf8_str())));
    if (name == settings_.deviceName) return;
    settings_.deviceName = name;
    SaveSettings();
}

void MainFrame::OnClose(wxCloseEvent& event) {
    const bool keepRunning = settings_.startHidden || Sharing();
    if (!quitting_ && keepRunning && event.CanVeto() && EnsureTrayAttached()) {
        event.Veto();
        Hide();
        return;
    }
    if (trayIcon_) {
        trayIcon_->RemoveIcon();
        delete trayIcon_;
        trayIcon_ = nullptr;
    }
    *alive_ = false;
    SetAccessRequestedListener(nullptr);
    devicesTimer_.Stop();
    CloseQrWindow();
    CloseEveryConnection();
    share_.terminalHost().Stop();
    hostTimer_.Stop();
    clipTimer_.Stop();
    autoShareTimer_.Stop();
    share_.sharingHost().Stop();
    shareDriver_.Join();
    event.Skip();
}

ConnectionFrame::ConnectionFrame(MainFrame* owner, std::string address, deskhub::HostCaps caps,
    std::vector<deskhub::SourceInfo> sources, bool control)
    : wxFrame(nullptr, wxID_ANY, ToWx(address)), owner_(owner), address_(std::move(address)), caps_(caps), sources_(std::move(sources)), control_(control) {
    SetIcon(wxICON(deskhub_app_icon));
    SetBackgroundColour(*wxWHITE);

    auto* panel = new wxPanel(this);
    panel->SetBackgroundColour(*wxWHITE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    const wxSizerFlags row = wxSizerFlags().Border(wxTOP, FromDIP(kPageGap));
    const wxSizerFlags wideRow = wxSizerFlags().Expand().Border(wxTOP, FromDIP(kPageGap));

    auto* addressRow = new wxBoxSizer(wxHORIZONTAL);
    addressRow->Add(MakeSection(panel, address_.c_str()), wxSizerFlags(1).CentreVertical());
    auto* disconnectBtn = MakeButton(panel, ToWx(ui::kDisconnectButton));
    disconnectBtn->SetName("disconnect-button");
    PaintButton(disconnectBtn, kOffline);
    disconnectBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
    addressRow->Add(disconnectBtn, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(12)));
    sizer->Add(addressRow, wxSizerFlags().Expand());

    auto* stateRow = new wxBoxSizer(wxHORIZONTAL);
    stateLabel_ = new wxStaticText(panel, wxID_ANY, ToWx(ui::kConnectedPickSession));
    PaintStatePill(stateLabel_);
    stateRow->Add(stateLabel_, wxSizerFlags().CentreVertical());
    pingLabel_ = new wxStaticText(panel, wxID_ANY, wxString());
    PaintStatePill(pingLabel_);
    stateRow->Add(pingLabel_, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(8)));
    sizer->Add(stateRow, row);

    auto* openDesktopBtn = MakeButton(panel, ToWx(ui::kOpenDesktopLabel));
    openDesktopBtn->SetName("open-desktop");
    openDesktopBtn->Enable(!sources_.empty());
    openDesktopBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenDesktopSession(); });
    sizer->Add(openDesktopBtn, wideRow);

    auto* controlRow = new wxBoxSizer(wxHORIZONTAL);
    controlRow->AddSpacer(FromDIP(24));
    auto* controlCtrl = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kRequestControlLabel));
    controlCtrl->SetName("request-control");
    controlCtrl->SetValue(control_);
    controlCtrl->Enable(!sources_.empty());
    controlCtrl->Bind(wxEVT_CHECKBOX, [this, controlCtrl](wxCommandEvent&) {
        control_ = controlCtrl->GetValue();
        owner_->SetClientControl(control_);
    });
    controlRow->Add(controlCtrl, wxSizerFlags().CentreVertical());
    sizer->Add(controlRow, row);

    auto* openShellBtn = MakeButton(panel, ToWx(ui::kOpenShellLabel));
    openShellBtn->SetName("open-shell");
    openShellBtn->Enable(caps_.terminal);
    openShellBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenShellSession(); });
    sizer->Add(openShellBtn, wideRow);

    auto* openFilesBtn = MakeButton(panel, ToWx(ui::kOpenFilesLabel));
    openFilesBtn->SetName("open-files");
    openFilesBtn->Enable(caps_.files);
    openFilesBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenFileSendSession(); });
    sizer->Add(openFilesBtn, wideRow);

    sizer->Add(MakeHint(panel, ToWx(ui::kMobileHostNote)), row);

    auto* pagePad = new wxBoxSizer(wxVERTICAL);
    pagePad->Add(sizer, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(kPagePad)));
    panel->SetSizerAndFit(pagePad);

    auto* frameSizer = new wxBoxSizer(wxVERTICAL);
    frameSizer->Add(panel, wxSizerFlags(1).Expand());
    SetSizerAndFit(frameSizer);
    const wxSize fitted = GetSize();
    SetMinSize(fitted);
    SetSize(wxSize(std::max(fitted.GetWidth(), FromDIP(kConnectionWindowWidth)),
        fitted.GetHeight()));

    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) {
        owner_->ForgetConnection(this);
        Destroy();
    });
}

void ConnectionFrame::OpenDesktopSession() {
    if (sources_.empty()) return;
    std::vector<deskhub::SourceInfo> picked;
    if (!ShowSourcePickerDialog(HWND(GetHandle()), sources_, picked)) return;
    std::thread([addr = address_, picked = std::move(picked), control = control_] {
        RunViewer(addr, picked, control);
    })
        .detach();
}

void ConnectionFrame::OpenShellSession() {
    NetAddr server{};
    if (ParseNetAddr(address_, server)) owner_->OpenShell(server);
}

void ConnectionFrame::OpenFileSendSession() {
    NetAddr server{};
    if (ParseNetAddr(address_, server)) owner_->OpenFileSend(server);
}

wxMenu* DeskhubTrayIcon::CreatePopupMenu() {
    auto* menu = new wxMenu();
    const int toggleWindowId =
        menu->Append(wxID_ANY,
                ToWx(frame_.IsShown() ? ui::kTrayHideWindow : ui::kTrayShowWindow))
            ->GetId();
    const int toggleShareId =
        menu->Append(wxID_ANY, ToWx(frame_.Sharing() ? ui::kStopSharing : ui::kStartSharing))
            ->GetId();
    menu->AppendSeparator();
    const int quitId = menu->Append(wxID_ANY, ToWx(ui::kTrayQuit))->GetId();

    menu->Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { frame_.ToggleWindowFromTray(); }, toggleWindowId);
    menu->Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { frame_.OnShare(); }, toggleShareId);
    menu->Bind(
        wxEVT_MENU, [this](wxCommandEvent&) { frame_.QuitFromTray(); }, quitId);
    return menu;
}

void DeskhubTrayIcon::OnLeftDown(wxTaskBarIconEvent&) {
    frame_.ToggleWindowFromTray();
}

class DeskhubApp final : public wxApp {
public:
    bool OnInit() override {
        SetAppName("Deskhub");
        auto* frame = new MainFrame();
        frame->Show();
        return true;
    }

    int FilterEvent(wxEvent& event) override {
        if (event.GetEventType() == wxEVT_LEFT_DOWN || event.GetEventType() == wxEVT_LEFT_UP) {
            const auto* win = dynamic_cast<wxWindow*>(event.GetEventObject());
            const wxString name = win ? win->GetName() : wxString("?");
            const wxString label = win ? win->GetLabel().Left(24) : wxString();
            LOGI("[UI] Mouse %s on \"%s\" (%s).",
                event.GetEventType() == wxEVT_LEFT_DOWN ? "down" : "up",
                std::string(name.utf8_str()).c_str(), std::string(label.utf8_str()).c_str());
        }
        return -1;
    }
};

}

wxIMPLEMENT_APP_NO_MAIN(DeskhubApp);

int RunDeskhubApp() {
    return wxEntry(GetModuleHandleW(nullptr), nullptr, nullptr, SW_SHOWNORMAL);
}

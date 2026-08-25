#include <wx/wx.h>

#include <wx/dcbuffer.h>
#include <wx/dirdlg.h>
#include <wx/hyperlink.h>
#include <wx/init.h>
#include <wx/listctrl.h>
#include <wx/scrolwin.h>
#include <wx/clipbrd.h>
#include <wx/simplebook.h>
#include <wx/spinctrl.h>
#include <wx/taskbar.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "MainFrame.h"

#include "PasscodePrompt.h"
#include "SourcePickerDialog.h"
#include "FileSendWindow.h"
#include "TerminalWindow.h"
#include "Viewer.h"
#include "WxUi.h"
#include "deskhub/media/QualityPreset.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/net/BindAddress.h"
#include "deskhub/net/TrustStore.h"
#include "deskhub/session/host/ShareFlow.h"
#include "deskhub/net/PairedDevices.h"
#include "deskhub/ui/AutoShareGate.h"
#include "deskhub/ui/DeviceRows.h"
#include "deskhub/ui/HostRows.h"
#include "deskhub/ui/RecentDevices.h"
#include "deskhub/ui/Strings.h"
#include "deskhub/ui/UiSettings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/media/DisplayEnum.h"
#include "deskhubp/client/DeviceStatusPoller.h"
#include "deskhubp/client/LanScanner.h"
#include "deskhubp/net/NetInfo.h"
#include "deskhubp/net/UdpSocket.h"
#include "deskhubp/host/ShareDriver.h"
#include "deskhubp/host/SharingHost.h"
#include "deskhubp/client/SourceQueryAsync.h"
#include "deskhubp/host/ShareController.h"
#include "deskhubp/system/FileStore.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/Autostart.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/PairedDevicesFile.h"
#include "deskhubp/system/UiSettingsStore.h"

namespace {

namespace ui = deskhub::ui;

constexpr const char* kRecentDevicesFile = "recent-devices.txt";

constexpr int kHostTimerId = 1;
constexpr int kScanTimerId = 2;
constexpr int kClipTimerId = 3;
constexpr int kAutoShareTimerId = 4;
constexpr int kRescanDelayMs = int(deskhubp::kLanRescanSecs) * 1000;
constexpr int kPrimaryButtonH = 46;
constexpr int kConnectionWindowWidth = 460;
constexpr int kConnectionWindowCascade = 28;
constexpr int kListMinH = 130;
constexpr int kHostListMinH = 150;
constexpr int kBannerWrapWidth = 520;

enum Page { kPageHost = 0,
    kPageClient = 1,
    kPageDevices = 2,
    kPageSettings = 3,
    kPageCount = 4 };

const char* const kPageLabels[kPageCount] = {ui::kSidebarHost, ui::kSidebarClient,
    ui::kSidebarDevices, ui::kSidebarSettings};

const wxColour kSidebarBg(31, 41, 55);
const wxColour kSidebarHover(55, 65, 81);
const wxColour kAccent(37, 99, 235);
const wxColour kNavText(209, 213, 219);
const wxColour kSidebarFootnote(148, 163, 184);
const wxColour kOnline(0, 145, 60);
const wxColour kOffline(200, 40, 40);
const wxColour kWarning(202, 108, 8);
const wxColour kRowLine(229, 231, 235);
const wxColour kViewerRowBg(249, 250, 251);
const wxColour kBannerIdleBg(243, 244, 246);
const wxColour kBannerLiveBg(232, 250, 239);
const wxColour kBannerBusyBg(235, 243, 255);

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

struct HostColumn {
    const char* title;
    int width;
    long align;
    bool mono;
};

constexpr int kHostColumnCount = 8;
constexpr int kHostCellGap = 8;
constexpr int kHostActionWidth = 104;
constexpr int kHostAttachWidth = 104;
constexpr int kHostActionsWidth = kHostActionWidth + kHostCellGap + kHostAttachWidth;
constexpr int kHostRowHeight = 32;
constexpr int kHostRowBarWidth = 3;

const HostColumn kHostColumns[kHostColumnCount] = {{"Source", 168, wxALIGN_LEFT, false},
    {"Size", 88, wxALIGN_LEFT, false}, {"Viewers", 58, wxALIGN_RIGHT, true},
    {"Client", 132, wxALIGN_LEFT, false}, {"Capture", 60, wxALIGN_RIGHT, true},
    {"Send", 52, wxALIGN_RIGHT, true}, {"Mbps", 56, wxALIGN_RIGHT, true},
    {"RTT", 54, wxALIGN_RIGHT, true}};

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

void PaintButton(wxButton* button, const wxColour& background) {
    button->SetBackgroundColour(background);
    button->SetForegroundColour(*wxWHITE);
    button->SetFont(button->GetFont().Bold());
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

struct ProbeResult {
    bool online = false;
    uint32_t rttMs = 0;
};

void SetHintLabel(wxStaticText* hint, const wxString& text) {
    hint->SetLabel(text);
    hint->Wrap(hint->FromDIP(kHintWrapDip));
    hint->GetParent()->Layout();
}

wxSizer* MakeHeadingRow(wxWindow* parent, const char* heading, const wxString& action,
    std::function<void()> onClick) {
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(MakeHeading(parent, heading), wxSizerFlags().CentreVertical());
    row->AddStretchSpacer(1);

    auto* button = new wxButton(parent, wxID_ANY, action);
    button->SetMinSize(parent->FromDIP(wxSize(110, 30)));
    button->Bind(wxEVT_BUTTON,
        [onClick = std::move(onClick)](wxCommandEvent&) { onClick(); });
    row->Add(button, wxSizerFlags().CentreVertical());
    return row;
}

void CopyTextToClipboard(HWND owner, const wxString& text) {
    const std::wstring wide = text.ToStdWstring();
    if (wide.empty() || !OpenClipboard(owner)) return;
    EmptyClipboard();
    const size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        if (void* mem = GlobalLock(handle)) {
            std::memcpy(mem, wide.c_str(), bytes);
            GlobalUnlock(handle);
            if (SetClipboardData(CF_UNICODETEXT, handle)) handle = nullptr;
        }
        if (handle) GlobalFree(handle);
    }
    CloseClipboard();
}

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
    void RefreshPairedDevices();
    bool AskPairing(const PairingRequest& request);
    void ForgetSelectedDevice();
    void ForgetEveryDevice();
    static wxTextCtrl* MakePasscodeCtrl(wxWindow* parent);

    void SelectPage(int page);
    void RefreshDeviceList();
    void ApplyRowStatus(long row, const std::string& addr);
    void ApplyProbeToRows(const std::string& addr);
    void RecordProbe(const std::string& addr, bool online, uint32_t rttMs);
    const ProbeResult* ProbeFor(const std::string& addr) const;
    void StartPoller();
    void OnDeviceStatus(const deskhubp::DeviceStatus& status);

    void OnShare(ShareTrigger trigger = ShareTrigger::kUser);
    void StartTenants();
    void BeginAutoShare();
    void OnAutoShareTimer(wxTimerEvent& event);
    void ReportShareProblem(const wxString& text, const wxString& title);
    bool Sharing() const;
    bool ScreenSharing() const {
        return screenSharing_;
    }
    bool TerminalTicked() const;
    bool FilesTicked() const;
    std::filesystem::path TransferFolder() const;
    void StartHosting(const std::vector<ShareSource>& sources, const ShareOptions& options);
    void OnHostStarted(bool started, const std::string& error, uint16_t port,
        bool allowInput, const std::string& passcode);
    void StopHosting();
    void StartFileShare();
    void ChooseTransferFolder();
    void OpenFileSend(const NetAddr& server, const std::string& passcode);
    void ApplySharingBanner();
    void OnHostTimer(wxTimerEvent& event);
    void OnClipboardTimer(wxTimerEvent& event);
    void RefreshDisplayChoices();
    void OnDisplayChanged(wxDisplayChangedEvent& event);
    void UpdateHostRows(const std::vector<ShareSourceStatus>& rows);
    wxWindow* BuildHostTable(wxWindow* parent);
    wxButton* MakeRowAction(wxWindow* parent, const ui::HostRow& ref);
    wxButton* MakeRowAttach(wxWindow* parent, const ui::HostRow& ref);
    void RebuildHostTable();
    void ShowHostTable(bool sharing);
    void RelayoutHostPage();
    void StopDisplay(uint8_t sourceId);
    void KickViewer(uint8_t sourceId, const std::string& viewerAddr);
    void ApplyHostState(HostShareState state, const wxString& detail);
    void ShowIdleHostState();

    void StartConnect(const std::string& addr);
    void OpenShell(const NetAddr& server, const std::string& passcode);
    std::string ClientDeviceName() const;
    void SetClientControl(bool on);
    void ForgetConnection(ConnectionFrame* frame);
    void SetClientStatus(const wxString& text, const wxColour& colour);
    void ConnectWithPrompt(const std::string& addr, std::string passcode);
    void StartScan();
    void RescanNow();
    void RefreshDeviceStatus();
    void RefreshDevicesNow();
    void OnScanTimer(wxTimerEvent& event);
    void OnScanHit(const deskhubp::ScanHit& hit);
    void OnScanProgress(const deskhubp::ScanProgress& progress);
    void OnScanFinished(const deskhubp::ScanProgress& progress);
    void OnListClick(wxMouseEvent& event);
    void ConnectRow(long row);
    void OnSourcesReady(const std::string& addr, const std::string& passcode,
        const deskhubp::ConnectOutcome& outcome);
    void OpenConnectionWindow(const std::string& addr, const std::string& passcode,
        const deskhubp::ConnectOutcome& outcome);
    ConnectionFrame* ConnectionFor(const std::string& addr) const;
    void CloseEveryConnection();
    void DeselectAllRows();
    void SaveSettings();
    void PopulateBindChoice();
    void RebuildHostAddressRows();
    void SaveRecentDevices();
    void OnClose(wxCloseEvent& event);

    wxSimplebook* book_ = nullptr;
    wxScrolledWindow* hostPage_ = nullptr;
    NavItem* pageButtons_[kPageCount] = {};
    wxTextCtrl* addrCtrl_ = nullptr;
    wxTextCtrl* connectPortCtrl_ = nullptr;
    wxButton* connectBtn_ = nullptr;
    wxStaticText* clientStatus_ = nullptr;
    wxListCtrl* deviceList_ = nullptr;
    wxStaticText* deviceHint_ = nullptr;
    wxWindow* addressForm_ = nullptr;
    wxWindow* devicesPanel_ = nullptr;
    std::vector<ConnectionFrame*> connections_;
    wxListCtrl* pairedList_ = nullptr;
    wxStaticText* pairedHint_ = nullptr;
    wxCheckBox* allowPairingCtrl_ = nullptr;
    wxButton* forgetDeviceBtn_ = nullptr;
    std::vector<deskhub::PairedDevice> pairedDevices_;
    wxPanel* hostAddrPanel_ = nullptr;
    wxPanel* hostBanner_ = nullptr;
    wxWindow* hostBannerBar_ = nullptr;
    wxStaticText* hostStateLabel_ = nullptr;
    wxStaticText* hostStatusLabel_ = nullptr;
    wxStaticText* hostHint_ = nullptr;
    wxListCtrl* hostPicker_ = nullptr;
    wxWindow* hostTableHolder_ = nullptr;
    wxScrolledWindow* hostTable_ = nullptr;
    std::vector<HostRowView> hostRowViews_;
    wxButton* shareBtn_ = nullptr;
    wxTextCtrl* clientPasscodeCtrl_ = nullptr;
    wxTextCtrl* deviceNameCtrl_ = nullptr;
    wxSpinCtrl* fpsCtrl_ = nullptr;
    wxSpinCtrl* bitrateCtrl_ = nullptr;
    wxSpinCtrl* portCtrl_ = nullptr;
    wxChoice* qualityChoice_ = nullptr;
    wxCheckBox* allowInputCtrl_ = nullptr;
    wxTextCtrl* passcodeCtrl_ = nullptr;
    wxChoice* bindChoice_ = nullptr;
    wxCheckBox* autoShareCtrl_ = nullptr;
    wxCheckBox* autostartCtrl_ = nullptr;
    wxCheckBox* startHiddenCtrl_ = nullptr;
    wxCheckBox* shareAudioCtrl_ = nullptr;
    wxCheckBox* playAudioCtrl_ = nullptr;
    wxCheckBox* keepAwakeCtrl_ = nullptr;
    wxCheckBox* clipboardCtrl_ = nullptr;
    wxStaticText* transferDirLabel_ = nullptr;
    wxStaticText* hostFilesHint_ = nullptr;
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
    std::string sharePasscodeNote_;
    std::string shareBindWarning_;
    bool shareViewOnly_ = false;
    std::vector<ui::HostRow> hostRows_;
    std::vector<ui::RecentDevice> recent_;
    std::vector<deskhubp::ScanHit> scanned_;
    std::vector<ui::DeviceRow> deviceRows_;
    std::vector<std::string> scannedThisRound_;
    std::map<uint64_t, ProbeResult> probes_;
    deskhubp::SourceQueryAsync connectDriver_;
    deskhubp::DeviceStatusPoller poller_;
    deskhubp::LanScanner scanner_;
    deskhubp::ShareController share_;
    deskhubp::ShareDriver shareDriver_;
    wxTimer hostTimer_;
    wxTimer scanTimer_;
    wxTimer clipTimer_;
    wxTimer autoShareTimer_;
    ui::AutoShareGate autoShareGate_;
    ShareTrigger shareTrigger_ = ShareTrigger::kUser;
    bool hosting_ = false;
    bool hostStarting_ = false;
    bool prompting_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

class ConnectionFrame final : public wxFrame {
public:
    ConnectionFrame(MainFrame* owner, std::string address, std::string passcode,
        deskhub::HostCaps caps, std::vector<deskhub::SourceInfo> sources, bool control);

    const std::string& Address() const {
        return address_;
    }

    void ApplyProbe(const ProbeResult* probe);

private:
    void OpenDesktopSession();
    void OpenShellSession();
    void OpenFileSendSession();

    MainFrame* owner_ = nullptr;
    std::string address_;
    std::string passcode_;
    deskhub::HostCaps caps_{};
    std::vector<deskhub::SourceInfo> sources_;
    bool control_ = false;
    wxStaticText* stateLabel_ = nullptr;
    wxStaticText* pingLabel_ = nullptr;
};

MainFrame::MainFrame() : wxFrame(nullptr, wxID_ANY, ToWx(ui::kAppTitle)) {
    settings_ = deskhubp::LoadUiSettings();
    recent_ = ui::ParseRecentDevices(deskhubp::ReadAppDataFile(kRecentDevicesFile));

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
    SetMinClientSize(FromDIP(wxSize(1000, 640)));
    SetClientSize(FromDIP(wxSize(1240, 780)));
    Centre();

    hostTimer_.SetOwner(this, kHostTimerId);
    scanTimer_.SetOwner(this, kScanTimerId);
    clipTimer_.SetOwner(this, kClipTimerId);
    autoShareTimer_.SetOwner(this, kAutoShareTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnHostTimer, this, kHostTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnScanTimer, this, kScanTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnClipboardTimer, this, kClipTimerId);
    Bind(wxEVT_TIMER, &MainFrame::OnAutoShareTimer, this, kAutoShareTimerId);
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
    hooks.askPairing = [this](const PairingRequest& request) { return AskPairing(request); };
    hooks.onBannerChanged = [this] { ApplySharingBanner(); };
    hooks.onNothingLeftShared = [this] { StopHosting(); };
    share_.SetHooks(std::move(hooks));

    RefreshDeviceList();
    StartPoller();
    StartScan();
    SelectPage(kPageClient);
    ApplyTrayMode();

    if (settings_.autoShare) {
        SelectPage(kPageHost);
        CallAfter([this] { BeginAutoShare(); });
    } else {
        CallAfter([this] { StartTenants(); });
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
    sizer->Add(repoLink, wxSizerFlags().Border(wxLEFT | wxRIGHT, FromDIP(16)));

    auto* version = new wxStaticText(panel, wxID_ANY, ToWx(ui::VersionLine()));
    version->SetForegroundColour(kSidebarFootnote);
    sizer->Add(version,
        wxSizerFlags().Border(wxLEFT | wxRIGHT | wxBOTTOM | wxTOP, FromDIP(16)));

    panel->SetSizer(sizer);
    return panel;
}

wxWindow* MainFrame::BuildHostPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    hostPage_ = panel;
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    const wxSizerFlags pad = wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    sizer->Add(MakeHeading(panel, ui::kHostHeading), pad);
    sizer->Add(MakeHint(panel, ToWx(ui::kHostIpIntro)), pad);

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
    sizer->Add(netRow, pad);

    hostAddrPanel_ = new wxPanel(panel);
    sizer->Add(hostAddrPanel_,
        wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(14)));
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
    bannerRow->Add(bannerText, wxSizerFlags(1).Expand().Border(wxALL, FromDIP(10)));

    hostBanner_->SetSizer(bannerRow);
    sizer->Add(hostBanner_, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    hostPicker_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_NO_HEADER | wxLC_SINGLE_SEL);
    hostPicker_->InsertColumn(0, "Source", wxLIST_FORMAT_LEFT, FromDIP(560));
    hostPicker_->SetMinSize(FromDIP(wxSize(-1, kHostListMinH)));
    const auto onTick = [this](wxListEvent& event) {
        event.Skip();
        if (!Sharing()) ShowHostTable(false);
    };
    hostPicker_->Bind(wxEVT_LIST_ITEM_CHECKED, onTick);
    hostPicker_->Bind(wxEVT_LIST_ITEM_UNCHECKED, onTick);
    sizer->Add(hostPicker_,
        wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(14)));

    hostTableHolder_ = BuildHostTable(panel);
    hostTableHolder_->SetMinSize(FromDIP(wxSize(-1, kHostListMinH)));
    sizer->Add(hostTableHolder_,
        wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(14)));

    hostHint_ = MakeHint(panel, ToWx(ui::kPickSourcesHint));
    sizer->Add(hostHint_, pad);

    hostFilesHint_ = MakeHint(panel, wxString());
    sizer->Add(hostFilesHint_, pad);

    shareBtn_ = new wxButton(panel, wxID_ANY, wxString());
    shareBtn_->SetMinSize(FromDIP(wxSize(-1, kPrimaryButtonH)));
    shareBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OnShare(); });
    sizer->Add(shareBtn_, wxSizerFlags().Expand().Border(wxALL, FromDIP(14)));

    panel->SetSizer(sizer);
    panel->FitInside();
    ShowIdleHostState();
    RefreshDisplayChoices();
    return panel;
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
        grid->AddGrowableCol(1, 1);
        for (const auto& a : shown) {
            grid->Add(new wxStaticText(hostAddrPanel_, wxID_ANY, ToWx(a.name)),
                wxSizerFlags().CentreVertical());
            auto* ipText = new wxStaticText(hostAddrPanel_, wxID_ANY, ToWx(a.ip));
            ipText->SetFont(ipText->GetFont().Bold());
            grid->Add(ipText, wxSizerFlags().CentreVertical());
            auto* copy = new wxButton(hostAddrPanel_, wxID_ANY, "Copy");
            copy->SetMinSize(FromDIP(wxSize(84, 32)));
            const wxString ip = ToWx(a.ip);
            copy->Bind(wxEVT_BUTTON, [this, ip](wxCommandEvent&) {
                CopyTextToClipboard(HWND(GetHandle()), ip);
            });
            grid->Add(copy);
        }
        holder->Add(grid, wxSizerFlags(1).Expand());
    }
    hostAddrPanel_->SetSizer(holder);
    RelayoutHostPage();
}

wxTextCtrl* MainFrame::MakePasscodeCtrl(wxWindow* parent) {
    auto* ctrl = new wxTextCtrl(parent, wxID_ANY, wxString(), wxDefaultPosition,
        parent->FromDIP(wxSize(64, -1)), wxTE_PROCESS_ENTER,
        wxTextValidator(wxFILTER_DIGITS));
    ctrl->SetMaxLength(deskhub::kPasscodeDigits);
    return ctrl;
}

wxWindow* MainFrame::BuildClientPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    const wxSizerFlags pad = wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    sizer->Add(MakeHeading(panel, ui::kClientHeading), pad);

    auto connectNow = [this](wxCommandEvent&) {
        const uint16_t port =
            ui::PortOrDefault(std::string(connectPortCtrl_->GetValue().utf8_str()));
        StartConnect(ui::AddressWithPort(std::string(addrCtrl_->GetValue().utf8_str()), port));
    };

    auto* form = new wxPanel(panel);
    form->SetBackgroundColour(*wxWHITE);
    auto* formSizer = new wxBoxSizer(wxVERTICAL);
    addressForm_ = form;

    auto* grid = new wxFlexGridSizer(2, FromDIP(wxSize(12, 12)));

    grid->Add(new wxStaticText(form, wxID_ANY, ToWx(ui::kClientIpPrompt)),
        wxSizerFlags().CentreVertical());
    addrCtrl_ = new wxTextCtrl(form, wxID_ANY, wxString(), wxDefaultPosition,
        FromDIP(wxSize(260, -1)), wxTE_PROCESS_ENTER);
    addrCtrl_->SetName("address-field");
    addrCtrl_->SetHint(ToWx(ui::kClientIpPlaceholder));
    addrCtrl_->Bind(wxEVT_TEXT_ENTER, connectNow);
    grid->Add(addrCtrl_, wxSizerFlags().CentreVertical());

    grid->Add(new wxStaticText(form, wxID_ANY, ToWx(ui::kUdpPortLabel)),
        wxSizerFlags().CentreVertical());
    connectPortCtrl_ = new wxTextCtrl(form, wxID_ANY,
        ToWx(std::to_string(deskhub::kDeskhubPort)), wxDefaultPosition,
        FromDIP(wxSize(80, -1)), wxTE_PROCESS_ENTER);
    connectPortCtrl_->Bind(wxEVT_TEXT_ENTER, connectNow);
    grid->Add(connectPortCtrl_, wxSizerFlags().CentreVertical());

    grid->Add(new wxStaticText(form, wxID_ANY, ToWx(ui::kClientPasscodePrompt)),
        wxSizerFlags().CentreVertical());
    clientPasscodeCtrl_ = MakePasscodeCtrl(form);
    clientPasscodeCtrl_->SetName("passcode-field");
    clientPasscodeCtrl_->SetToolTip(ToWx(ui::kClientPasscodeHint));
    clientPasscodeCtrl_->Bind(wxEVT_TEXT_ENTER, connectNow);
    grid->Add(clientPasscodeCtrl_, wxSizerFlags().CentreVertical());

    grid->Add(new wxStaticText(form, wxID_ANY, ToWx(ui::kDeviceNameLabel)),
        wxSizerFlags().CentreVertical());
    const std::string initialName =
        settings_.deviceName.empty() ? deskhubp::LocalDeviceName() : settings_.deviceName;
    deviceNameCtrl_ = new wxTextCtrl(form, wxID_ANY, ToWx(initialName), wxDefaultPosition,
        FromDIP(wxSize(260, -1)), wxTE_PROCESS_ENTER);
    deviceNameCtrl_->SetName("name-field");
    deviceNameCtrl_->Bind(wxEVT_TEXT_ENTER, connectNow);
    grid->Add(deviceNameCtrl_, wxSizerFlags().CentreVertical());

    formSizer->Add(grid, wxSizerFlags().Border(wxTOP, FromDIP(16)));

    connectBtn_ = new wxButton(form, wxID_ANY, ToWx(ui::kConnectButton));
    connectBtn_->SetName("connect-button");
    connectBtn_->SetMinSize(FromDIP(wxSize(-1, kPrimaryButtonH)));
    PaintButton(connectBtn_, kAccent);
    connectBtn_->Bind(wxEVT_BUTTON, connectNow);
    formSizer->Add(connectBtn_, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));
    form->SetSizer(formSizer);
    sizer->Add(form, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT, FromDIP(16)));

    clientStatus_ = new wxStaticText(panel, wxID_ANY, wxString());
    clientStatus_->SetName("client-status");
    clientStatus_->SetForegroundColour(kMutedText);
    sizer->Add(clientStatus_, wxSizerFlags().Centre().Border(wxTOP, FromDIP(8)));

    auto* devices = new wxPanel(panel);
    devices->SetBackgroundColour(*wxWHITE);
    auto* devicesSizer = new wxBoxSizer(wxVERTICAL);
    devicesPanel_ = devices;

    devicesSizer->Add(MakeHeadingRow(devices, ui::kDevicesHeading, ToWx(ui::kRefreshNow),
                          [this] { RefreshDevicesNow(); }),
        wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));

    deviceList_ = new wxListCtrl(devices, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL);
    deviceList_->InsertColumn(0, "Device", wxLIST_FORMAT_LEFT, FromDIP(170));
    deviceList_->InsertColumn(1, ToWx(ui::kDeviceColumnWhere), wxLIST_FORMAT_LEFT, FromDIP(120));
    deviceList_->InsertColumn(2, "Status", wxLIST_FORMAT_LEFT, FromDIP(100));
    deviceList_->InsertColumn(3, "Ping", wxLIST_FORMAT_RIGHT, FromDIP(70));
    deviceList_->InsertColumn(4, "Last connected", wxLIST_FORMAT_LEFT, FromDIP(150));
    deviceList_->SetMinSize(FromDIP(wxSize(-1, kListMinH)));
    deviceList_->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& event) { OnListClick(event); });
    devicesSizer->Add(deviceList_, wxSizerFlags(1).Expand().Border(wxTOP, FromDIP(16)));

    deviceHint_ = MakeHint(devices, ToWx(ui::kLanDevicesEmpty));
    devicesSizer->Add(deviceHint_, wxSizerFlags().Border(wxTOP | wxBOTTOM, FromDIP(16)));
    devices->SetSizer(devicesSizer);
    sizer->Add(devices, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, FromDIP(16)));

    panel->SetSizer(sizer);
    panel->FitInside();
    return panel;
}

wxWindow* MainFrame::BuildDevicesPage(wxWindow* parent) {
    auto* panel = new wxScrolledWindow(parent);
    panel->SetBackgroundColour(*wxWHITE);
    panel->SetScrollRate(0, FromDIP(10));
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    const wxSizerFlags pad = wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    sizer->Add(MakeHeading(panel, ui::kPairedHeading), pad);
    sizer->Add(MakeHint(panel, ToWx(ui::kPairedHint)), pad);

    pairedList_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL);
    pairedList_->InsertColumn(0, ToWx(ui::kPairedColumnName), wxLIST_FORMAT_LEFT, FromDIP(200));
    pairedList_->InsertColumn(1, ToWx(ui::kPairedColumnKey), wxLIST_FORMAT_LEFT, FromDIP(130));
    pairedList_->InsertColumn(2, ToWx(ui::kPairedColumnPaired), wxLIST_FORMAT_LEFT, FromDIP(150));
    pairedList_->InsertColumn(3, ToWx(ui::kPairedColumnLastSeen), wxLIST_FORMAT_LEFT,
        FromDIP(150));
    pairedList_->SetMinSize(FromDIP(wxSize(-1, kListMinH)));
    sizer->Add(pairedList_, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT | wxTOP,
                                FromDIP(16)));

    pairedHint_ = MakeHint(panel, ToWx(ui::kPairedEmpty));
    sizer->Add(pairedHint_, pad);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    forgetDeviceBtn_ = new wxButton(panel, wxID_ANY, ToWx(ui::kPairedForget));
    forgetDeviceBtn_->SetName("forget-device");
    forgetDeviceBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ForgetSelectedDevice(); });
    buttons->Add(forgetDeviceBtn_);
    auto* forgetAll = new wxButton(panel, wxID_ANY, ToWx(ui::kPairedForgetAll));
    forgetAll->SetName("forget-all-devices");
    forgetAll->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ForgetEveryDevice(); });
    buttons->AddSpacer(FromDIP(8));
    buttons->Add(forgetAll);
    sizer->Add(buttons, pad);

    sizer->Add(MakeHint(panel, ToWx(ui::kPairedForgetNote)), pad);

    sizer->AddSpacer(FromDIP(12));
    allowPairingCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kAllowPairingLabel));
    allowPairingCtrl_->SetName("allow-pairing");
    allowPairingCtrl_->SetValue(settings_.allowNewPairings);
    allowPairingCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        settings_.allowNewPairings = allowPairingCtrl_->GetValue();
        deskhubp::SaveUiSettings(settings_);
    });
    sizer->Add(allowPairingCtrl_, pad);
    sizer->Add(MakeHint(panel, ToWx(ui::kAllowPairingHint)), pad);

    sizer->AddSpacer(FromDIP(12));
    sizer->Add(MakeSection(panel, ui::kThisMachineHeading), pad);
    const std::string name =
        settings_.deviceName.empty() ? deskhubp::LocalDeviceName() : settings_.deviceName;
    const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity(name);
    auto* keyText = new wxStaticText(panel, wxID_ANY,
        ToWx(identity.Valid() ? deskhub::FormatFingerprint(identity.fingerprint)
                              : std::string(ui::kShareNoHostIdentity)));
    keyText->SetName("own-fingerprint");
    sizer->Add(keyText, pad);
    sizer->Add(MakeHint(panel, ToWx(ui::kThisMachineHint)),
        wxSizerFlags().Border(wxALL, FromDIP(16)));

    panel->SetSizer(sizer);
    panel->FitInside();
    RefreshPairedDevices();
    return panel;
}

void MainFrame::RefreshPairedDevices() {
    if (pairedList_ == nullptr) return;
    pairedDevices_ = deskhubp::LoadPairedDevices().Devices();

    pairedList_->DeleteAllItems();
    for (size_t i = 0; i < pairedDevices_.size(); ++i) {
        const deskhub::PairedDevice& device = pairedDevices_[i];
        const long row = pairedList_->InsertItem(long(i),
            ToWx(device.name.empty() ? std::string("(unnamed)") : device.name));
        pairedList_->SetItem(row, 1, ToWx(deskhub::ShortFingerprint(device.fingerprint)));
        pairedList_->SetItem(row, 2, ToWx(FormatUnixMinute(device.pairedUnix)));
        pairedList_->SetItem(row, 3, ToWx(FormatUnixMinute(device.lastSeenUnix)));
    }
    SetHintLabel(pairedHint_, ToWx(ui::kPairedEmpty));
    pairedHint_->Show(pairedDevices_.empty());
    forgetDeviceBtn_->Enable(!pairedDevices_.empty());
}

bool MainFrame::AskPairing(const PairingRequest& request) {
    wxString body = ToWx(ui::PairingRequestBody(request.name,
        NetAddr::Unpack(request.addrPacked).ToString(), request.shortKey));
    wxMessageDialog dialog(this, body, ToWx(ui::kPairingRequestTitle),
        wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    dialog.SetYesNoLabels(ToWx(ui::kPairingAllow), ToWx(ui::kPairingDeny));
    const bool allowed = dialog.ShowModal() == wxID_YES;
    if (allowed) RefreshPairedDevices();
    return allowed;
}

void MainFrame::ForgetSelectedDevice() {
    const long row = pairedList_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || size_t(row) >= pairedDevices_.size()) return;
    deskhubp::ForgetPairedDevice(pairedDevices_[size_t(row)].fingerprint);
    RefreshPairedDevices();
}

void MainFrame::ForgetEveryDevice() {
    if (pairedDevices_.empty()) return;
    wxMessageDialog dialog(this, ToWx(ui::kPairedForgetAllPrompt), "Deskhub",
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (dialog.ShowModal() != wxID_YES) return;
    deskhubp::ForgetAllPairedDevices();
    RefreshPairedDevices();
}

wxWindow* MainFrame::BuildSettingsPage(wxWindow* parent) {
    auto* panel = new wxPanel(parent);
    panel->SetBackgroundColour(*wxWHITE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    const wxSizerFlags pad = wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    sizer->Add(MakeHeading(panel, ui::kSettingsHeading), pad);
    sizer->Add(MakeHint(panel, ToWx(ui::kSettingsHint)), pad);

    sizer->AddSpacer(FromDIP(8));
    sizer->Add(MakeSection(panel, ui::kSettingsSectionVideo), pad);
    auto* videoGrid = new wxFlexGridSizer(2, FromDIP(wxSize(14, 10)));

    videoGrid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kFpsLabel)),
        wxSizerFlags().CentreVertical());
    fpsCtrl_ = new wxSpinCtrl(panel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 1, int(ui::kMaxSettingsFps), int(settings_.fps));
    videoGrid->Add(fpsCtrl_);

    videoGrid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kBitrateLabel)),
        wxSizerFlags().CentreVertical());
    bitrateCtrl_ = new wxSpinCtrl(panel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 1, int(ui::kMaxSettingsBitrateMbps), int(settings_.bitrateMbps));
    videoGrid->Add(bitrateCtrl_);

    videoGrid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kQualityLabel)),
        wxSizerFlags().CentreVertical());
    qualityChoice_ = new wxChoice(panel, wxID_ANY);
    for (const auto& preset : deskhub::media::kQualityPresets)
        qualityChoice_->Append(ToWx(preset.label));
    qualityChoice_->SetSelection(int(deskhub::media::QualityPresetIndex(settings_.maxDim)));
    videoGrid->Add(qualityChoice_);

    sizer->Add(videoGrid, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    sizer->AddSpacer(FromDIP(12));
    sizer->Add(MakeSection(panel, ui::kSettingsSectionConnection), pad);
    auto* netGrid = new wxFlexGridSizer(2, FromDIP(wxSize(14, 10)));

    netGrid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kUdpPortLabel)),
        wxSizerFlags().CentreVertical());
    portCtrl_ = new wxSpinCtrl(panel, wxID_ANY, wxString(), wxDefaultPosition, wxDefaultSize,
        wxSP_ARROW_KEYS, 1, int(ui::kMaxSettingsPort), int(settings_.port));
    netGrid->Add(portCtrl_);

    sizer->Add(netGrid, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    sizer->AddSpacer(FromDIP(12));
    sizer->Add(MakeSection(panel, ui::kSettingsSectionSecurity), pad);
    auto* securityGrid = new wxFlexGridSizer(2, FromDIP(wxSize(14, 10)));
    securityGrid->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kPasscodeLabel)),
        wxSizerFlags().CentreVertical());
    passcodeCtrl_ = MakePasscodeCtrl(panel);
    passcodeCtrl_->SetValue(ToWx(settings_.passcode));
    securityGrid->Add(passcodeCtrl_);
    sizer->Add(securityGrid, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    sizer->Add(MakeHint(panel, ToWx(ui::kPasscodeHint)),
        wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    allowInputCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kAllowControlLabel));
    allowInputCtrl_->SetValue(settings_.allowInput);
    sizer->Add(allowInputCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    sizer->AddSpacer(FromDIP(12));
    sizer->Add(MakeSection(panel, ui::kSettingsSectionSession), pad);
    clipboardCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kClipboardSyncLabel));
    clipboardCtrl_->SetValue(settings_.clipboardSync);
    sizer->Add(clipboardCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    shareAudioCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kShareAudioLabel));
    shareAudioCtrl_->SetValue(settings_.shareAudio);
    sizer->Add(shareAudioCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    playAudioCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kPlayAudioLabel));
    playAudioCtrl_->SetValue(settings_.playAudio);
    sizer->Add(playAudioCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    keepAwakeCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kKeepAwakeLabel));
    keepAwakeCtrl_->SetValue(settings_.keepAwake);
    sizer->Add(keepAwakeCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    auto* folderRow = new wxBoxSizer(wxHORIZONTAL);
    folderRow->Add(new wxStaticText(panel, wxID_ANY, ToWx(ui::kTransferFolderLabel)),
        wxSizerFlags().CentreVertical());
    transferDirLabel_ = new wxStaticText(panel, wxID_ANY, ToWx(deskhubp::PathText(TransferFolder())),
        wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_MIDDLE);
    transferDirLabel_->SetForegroundColour(kMutedText);
    folderRow->Add(transferDirLabel_, wxSizerFlags(1).CentreVertical().Border(wxLEFT,
                                          FromDIP(8)));
    auto* folderBtn = new wxButton(panel, wxID_ANY, ToWx(ui::kTransferChooseButton));
    folderBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ChooseTransferFolder(); });
    folderRow->Add(folderBtn, wxSizerFlags().CentreVertical().Border(wxLEFT, FromDIP(8)));
    sizer->Add(folderRow, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    sizer->AddSpacer(FromDIP(12));
    sizer->Add(MakeSection(panel, ui::kSettingsSectionLaunch), pad);
    autostartCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kAutostartLabel));
    autostartCtrl_->SetValue(deskhubp::AutostartEnabled());
    sizer->Add(autostartCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    autoShareCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kAutoShareLabel));
    autoShareCtrl_->SetValue(settings_.autoShare);
    sizer->Add(autoShareCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));
    startHiddenCtrl_ = new wxCheckBox(panel, wxID_ANY, ToWx(ui::kCloseToTrayLabel));
    startHiddenCtrl_->SetValue(settings_.startHidden);
    sizer->Add(startHiddenCtrl_, wxSizerFlags().Border(wxLEFT | wxRIGHT | wxTOP, FromDIP(16)));

    fpsCtrl_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { SaveSettings(); });
    fpsCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { SaveSettings(); });
    bitrateCtrl_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { SaveSettings(); });
    bitrateCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { SaveSettings(); });
    portCtrl_->Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&) { SaveSettings(); });
    portCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { SaveSettings(); });
    qualityChoice_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { SaveSettings(); });
    allowInputCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    clipboardCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    keepAwakeCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    autoShareCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    autostartCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    startHiddenCtrl_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SaveSettings(); });
    passcodeCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { SaveSettings(); });

    panel->SetSizer(sizer);
    return panel;
}

void MainFrame::SelectPage(int page) {
    for (int i = 0; i < kPageCount; ++i) pageButtons_[i]->SetSelected(i == page);
    book_->ChangeSelection(size_t(page));
    if (page == kPageHost && !Sharing()) RefreshDisplayChoices();
    if (page == kPageDevices) RefreshPairedDevices();
}

void MainFrame::RefreshDisplayChoices() {
    std::map<std::string, bool> previousTicks;
    for (size_t i = 0; i < availableDisplays_.size(); ++i) {
        if (long(i) >= hostPicker_->GetItemCount()) break;
        previousTicks[availableDisplays_[i].name] = hostPicker_->IsItemChecked(long(i));
    }
    const bool terminalWasTicked = hostPicker_->GetItemCount() == 0 || TerminalTicked();
    const bool filesWasTicked = hostPicker_->GetItemCount() == 0 || FilesTicked();

    availableDisplays_ = deskhubp::ListDisplays();
    hostRows_.clear();
    RebuildHostTable();
    hostPicker_->DeleteAllItems();
    hostPicker_->EnableCheckBoxes(true);
    for (size_t i = 0; i < availableDisplays_.size(); ++i) {
        const ShareSource& source = availableDisplays_[i];
        const long row = hostPicker_->InsertItem(long(i),
            ToWx(deskhub::media::SourcePickerLabel(source.name, uint8_t(i), source.width,
                source.height)));
        const auto seen = previousTicks.find(source.name);
        hostPicker_->CheckItem(row, seen == previousTicks.end() || seen->second);
    }
    const long terminalRow = hostPicker_->InsertItem(long(availableDisplays_.size()),
        ToWx(ui::kTerminalPickerLabel));
    hostPicker_->CheckItem(terminalRow, terminalWasTicked);
    const long filesRow = hostPicker_->InsertItem(long(availableDisplays_.size()) + 1,
        ToWx(ui::kFilesPickerLabel));
    hostPicker_->CheckItem(filesRow, filesWasTicked);
    ShowHostTable(false);
}

bool MainFrame::TerminalTicked() const {
    const long row = long(availableDisplays_.size());
    if (row >= hostPicker_->GetItemCount()) return false;
    return hostPicker_->IsItemChecked(row);
}

bool MainFrame::FilesTicked() const {
    const long row = long(availableDisplays_.size()) + 1;
    if (row >= hostPicker_->GetItemCount()) return false;
    return hostPicker_->IsItemChecked(row);
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
    headerRow->AddSpacer(FromDIP(kHostRowBarWidth + kHostCellGap));
    for (const HostColumn& column : kHostColumns) {
        auto* title = new wxStaticText(header, wxID_ANY, ToWx(column.title),
            wxDefaultPosition, FromDIP(wxSize(column.width, -1)), column.align);
        title->SetForegroundColour(kMutedText);
        title->SetFont(title->GetFont().Bold().Scaled(0.85f));
        headerRow->Add(title, wxSizerFlags().CentreVertical().Border(wxRIGHT,
                                  FromDIP(kHostCellGap)));
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
        auto* stop = new wxButton(parent, wxID_ANY, ToWx(ui::kStopDisplayAction));
        stop->SetMinSize(FromDIP(wxSize(kHostActionWidth, 26)));
        PaintButton(stop, kOffline);
        stop->Bind(wxEVT_BUTTON,
            [this](wxCommandEvent&) { share_.StopFilesRow(screenSharing_); });
        stop->Show(!viewer);
        return stop;
    }
    const bool remoteRow = viewer && !IsAttachedLocally(ref);
    auto* button = new wxButton(parent, wxID_ANY,
        ToWx(remoteRow ? ui::kDisconnectViewerAction : ui::kStopDisplayAction));
    button->SetMinSize(FromDIP(wxSize(kHostActionWidth, 26)));
    PaintButton(button, remoteRow ? kWarning : kOffline);

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
    auto* button = new wxButton(parent, wxID_ANY, ToWx(ui::kAttachShellAction));
    button->SetMinSize(FromDIP(wxSize(kHostAttachWidth, 26)));
    PaintButton(button, kOffline);
    const uint32_t termId = ref.termId;
    button->Bind(wxEVT_BUTTON,
        [this, termId](wxCommandEvent&) { share_.StopAndAttachShell(termId); });
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
        view.panel->SetMinSize(FromDIP(wxSize(-1, kHostRowHeight)));

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        view.bar = new wxWindow(view.panel, wxID_ANY, wxDefaultPosition,
            FromDIP(wxSize(kHostRowBarWidth, -1)));
        row->Add(view.bar, wxSizerFlags().Expand());
        row->AddSpacer(FromDIP(kHostCellGap));

        for (int c = 0; c < kHostColumnCount; ++c) {
            const HostColumn& column = kHostColumns[c];
            view.cells[c] = new wxStaticText(view.panel, wxID_ANY, wxString(), wxDefaultPosition,
                FromDIP(wxSize(column.width, -1)), column.align);
            if (column.mono) view.cells[c]->SetFont(MonoFont(view.cells[c]));
            if (c == 0 && !ref.viewer)
                view.cells[c]->SetFont(view.cells[c]->GetFont().Bold());
            row->Add(view.cells[c], wxSizerFlags().CentreVertical().Border(wxRIGHT,
                                        FromDIP(kHostCellGap)));
        }
        row->Add(MakeRowAction(view.panel, ref), wxSizerFlags().CentreVertical());
        row->AddSpacer(FromDIP(kHostCellGap));
        if (CanAttachLocally(ref)) {
            row->Add(MakeRowAttach(view.panel, ref), wxSizerFlags().CentreVertical());
        } else {
            row->AddSpacer(FromDIP(kHostAttachWidth));
        }
        row->AddSpacer(FromDIP(kHostCellGap));
        view.panel->SetSizer(row);

        rows->Add(view.panel, wxSizerFlags().Expand());
        hostRowViews_.push_back(view);
    }

    hostTable_->FitInside();
    hostTable_->Layout();
}

void MainFrame::ShowHostTable(bool sharing) {
    hostPicker_->Show(!sharing);
    hostTableHolder_->Show(sharing);
    hostHint_->Show(!sharing);

    const bool showFolder = !sharing && FilesTicked();
    hostFilesHint_->SetLabel(
        ToWx(std::string(ui::kTransferFolderLabel) + " " + deskhubp::PathText(TransferFolder())));
    hostFilesHint_->Show(showFolder);
    RelayoutHostPage();
}

void MainFrame::RefreshDeviceList() {
    std::vector<std::string> scannedAddrs;
    scannedAddrs.reserve(scanned_.size());
    for (const deskhubp::ScanHit& hit : scanned_) scannedAddrs.push_back(hit.addr);
    deviceRows_ = ui::BuildDeviceRows(scannedAddrs, recent_);

    deviceList_->DeleteAllItems();
    for (size_t i = 0; i < deviceRows_.size(); ++i) {
        const ui::DeviceRow& device = deviceRows_[i];
        const long row = deviceList_->InsertItem(long(i), ToWx(device.addr));
        deviceList_->SetItem(row, 1, ToWx(ui::DeviceOriginLabel(device.origin)));
        deviceList_->SetItem(row, 4,
            device.lastConnectedUnix != 0 ? ToWx(FormatUnixMinute(device.lastConnectedUnix))
                                          : wxString("-"));
        ApplyRowStatus(row, device.addr);
    }
}

const ProbeResult* MainFrame::ProbeFor(const std::string& addr) const {
    uint64_t key = 0;
    if (!HostKeyOf(addr, key)) return nullptr;
    const auto it = probes_.find(key);
    return it == probes_.end() ? nullptr : &it->second;
}

void MainFrame::RecordProbe(const std::string& addr, bool online, uint32_t rttMs) {
    uint64_t key = 0;
    if (!HostKeyOf(addr, key)) return;
    probes_[key] = ProbeResult{online, rttMs};
}

void MainFrame::ApplyRowStatus(long row, const std::string& addr) {
    const ProbeResult* probe = ProbeFor(addr);
    if (!probe) {
        deviceList_->SetItem(row, 2, ToWx(ui::kStatusChecking));
        deviceList_->SetItem(row, 3, "-");
        deviceList_->SetItemTextColour(row, kMutedText);
        return;
    }
    deviceList_->SetItem(row, 2, ToWx(probe->online ? ui::kStatusOnline : ui::kStatusOffline));
    deviceList_->SetItem(row, 3, probe->online ? ToWx(ui::PingMs(probe->rttMs)) : wxString("-"));
    deviceList_->SetItemTextColour(row, probe->online ? kOnline : kOffline);
}

void MainFrame::ApplyProbeToRows(const std::string& addr) {
    uint64_t key = 0;
    if (!HostKeyOf(addr, key)) return;
    for (size_t i = 0; i < deviceRows_.size(); ++i)
        if (SameHost(deviceRows_[i].addr, key)) ApplyRowStatus(long(i), deviceRows_[i].addr);
}

void MainFrame::StartPoller() {
    poller_.SetAddresses(ui::AddressesOf(recent_));
    poller_.Start([this](const deskhubp::DeviceStatus& status) {
        CallAfter([this, status] { OnDeviceStatus(status); });
    });
}

void MainFrame::OnDeviceStatus(const deskhubp::DeviceStatus& status) {
    RecordProbe(status.addr, status.online, status.rttMs);
    ApplyProbeToRows(status.addr);
    for (ConnectionFrame* frame : connections_)
        if (ui::SameDeviceAddr(frame->Address(), status.addr))
            frame->ApplyProbe(ProbeFor(status.addr));
}

void MainFrame::ApplyHostState(HostShareState state, const wxString& detail) {
    const HostStateStyle style = StyleFor(state);

    hostStateLabel_->SetLabel(ToWx(style.label));
    hostStateLabel_->SetForegroundColour(style.tint);
    hostStateLabel_->SetBackgroundColour(style.background);
    hostStatusLabel_->SetLabel(detail);
    hostStatusLabel_->Wrap(FromDIP(kBannerWrapWidth));
    hostStatusLabel_->Show(!detail.empty());
    hostStatusLabel_->SetBackgroundColour(style.background);
    hostBannerBar_->SetBackgroundColour(style.tint);
    hostBanner_->SetBackgroundColour(style.background);
    hostBanner_->Layout();
    if (wxWindow* page = hostBanner_->GetParent()) page->Layout();
    hostBanner_->Refresh();

    const bool screen = screenSharing_ || state == HostShareState::kStarting;
    shareBtn_->SetLabel(ToWx(screen ? ui::kStopSharing : ui::kStartSharing));
    PaintButton(shareBtn_, screen ? kOffline : kAccent);
    shareBtn_->Refresh();

    bindChoice_->Enable(!screen);
    ShowHostTable(screen);
}

void MainFrame::ShowIdleHostState() {
    ApplyHostState(HostShareState::kIdle,
        ToWx(ui::UdpPortLine(uint16_t(settings_.port)) + "."));
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

void MainFrame::StartTenants() {
    if (hostStarting_ || Sharing()) return;
    const bool terminal = TerminalTicked();
    const bool files = FilesTicked();
    if (!terminal && !files) return;
    terminalRequested_ = terminal;
    filesRequested_ = files;
    StartHosting({}, deskhub::ShareOptionsOf(settings_, terminal, files));
}

void MainFrame::OnShare(ShareTrigger trigger) {
    if (hostStarting_) return;
    if (screenSharing_) {
        StopHosting();
        StartTenants();
        return;
    }
    if (Sharing()) StopHosting();
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
    for (size_t i = 0; i < availableDisplays_.size(); ++i) {
        if (long(i) >= hostPicker_->GetItemCount()) break;
        if (hostPicker_->IsItemChecked(long(i))) chosen.push_back(availableDisplays_[i]);
    }
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
    banner.passcodeNote = sharePasscodeNote_;
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
        share_.sharingHost(), sources, options,
        [alive = alive_](std::function<void()> fn) {
            if (!wxTheApp) return;
            wxTheApp->CallAfter([alive, fn = std::move(fn)] {
                if (*alive) fn();
            });
        },
        [this, port = options.port, allowInput = options.allowInput,
            passcode = options.passcode](bool started, const std::string& error) {
            OnHostStarted(started, error, port, allowInput, passcode);
        });
}

void MainFrame::OnHostStarted(bool started, const std::string& error, uint16_t port,
    bool allowInput, const std::string& passcode) {
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
    sharePasscodeNote_ = ui::PasscodeNote(passcode);
    shareViewOnly_ = !allowInput;
    shareBindWarning_ = share_.sharingHost().BindWarning();
    if (terminalRequested_) share_.StartTerminalShare();
    if (filesRequested_) StartFileShare();
    ApplySharingBanner();
    hostTimer_.Start(int(deskhubp::kShareStatusPollMs));
    if (settings_.clipboardSync) clipTimer_.Start(1000);
}

void MainFrame::OnClipboardTimer(wxTimerEvent&) {
    if (!hosting_) return;
    const wxLogNull quietWhileClipboardIsBusy;
    if (!pendingClipboard_) pendingClipboard_ = share_.sharingHost().TakeRemoteClipboard();
    if (pendingClipboard_) {
        if (!wxTheClipboard->Open()) return;
        const bool put =
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(*pendingClipboard_)));
        wxTheClipboard->Close();
        if (put) pendingClipboard_.reset();
        return;
    }
    if (!wxTheClipboard->Open()) return;
    if (wxTheClipboard->IsSupported(wxDF_UNICODETEXT)) {
        wxTextDataObject data;
        wxTheClipboard->GetData(data);
        const std::string text(data.GetText().utf8_str());
        if (!text.empty()) share_.sharingHost().OfferLocalClipboard(text);
    }
    wxTheClipboard->Close();
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
    sharePasscodeNote_.clear();
    shareBindWarning_.clear();
    shareViewOnly_ = false;
    hostStatus_.clear();
    ShowIdleHostState();
    RefreshDisplayChoices();
}

void MainFrame::OnHostTimer(wxTimerEvent&) {
    share_.DrainPairingRequests();
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
    clientStatus_->Wrap(FromDIP(kHintWrapDip));
    clientStatus_->GetParent()->Layout();
}

std::string MainFrame::ClientDeviceName() const {
    std::string name =
        ui::TruncateDeviceName(std::string(deviceNameCtrl_->GetValue().utf8_str()));
    if (name.empty()) name = deskhubp::LocalDeviceName();
    return name;
}

void MainFrame::OpenFileSend(const NetAddr& server, const std::string& passcode) {
    FileSendLaunch launch;
    launch.address = server.ToString();
    launch.passcode = passcode;
    launch.clientName = ClientDeviceName();

    std::thread([launch] { RunStandaloneFileSend(launch); }).detach();
}

void MainFrame::OpenShell(const NetAddr& server, const std::string& passcode) {
    TerminalLaunch launch;
    launch.address = server.ToString();
    launch.passcode = passcode;
    launch.clientName = ClientDeviceName();

    if (!OpenTerminalWindow(this, launch))
        SetClientStatus(ToWx(ui::kTerminalUnreachable), kOffline);
}

void MainFrame::StartConnect(const std::string& rawAddr) {
    LOGI("[UI] Connect requested for \"%s\".", rawAddr.c_str());
    SetClientStatus(wxString(), kMutedText);
    const std::string deviceName = ClientDeviceName();
    deviceNameCtrl_->ChangeValue(ToWx(deviceName));
    if (deviceName != settings_.deviceName) {
        settings_.deviceName = deviceName;
        deskhubp::SaveUiSettings(settings_);
    }
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

    const std::string passcode =
        ui::TrimAscii(std::string(clientPasscodeCtrl_->GetValue().utf8_str()));
    if (!passcode.empty() && !deskhub::IsValidPasscode(passcode)) {
        wxMessageBox(ToWx(ui::kPasscodeInvalid), "Deskhub", wxOK | wxICON_ERROR, this);
        clientPasscodeCtrl_->SetFocus();
        return;
    }

    const bool started = connectDriver_.QueryAsync(
        server, passcode,
        [alive = alive_](std::function<void()> fn) {
            if (!wxTheApp) return;
            wxTheApp->CallAfter([alive, fn = std::move(fn)] {
                if (*alive) fn();
            });
        },
        [this, addr, passcode](const deskhubp::ConnectOutcome& outcome) {
            OnSourcesReady(addr, passcode, outcome);
        });
    if (!started) {
        SetClientStatus(ToWx(ui::kQueryingSources), kMutedText);
        return;
    }
    connectBtn_->Disable();
    SetClientStatus(ToWx(ui::kQueryingSources), kMutedText);
}

void MainFrame::StartScan() {
    scannedThisRound_.clear();
    const bool started = scanner_.Start(
        uint16_t(settings_.port),
        [alive = alive_](std::function<void()> fn) {
            if (!wxTheApp) return;
            wxTheApp->CallAfter([alive, fn = std::move(fn)] {
                if (*alive) fn();
            });
        },
        [this](const deskhubp::ScanHit& hit) { OnScanHit(hit); },
        [this](const deskhubp::ScanProgress& progress) { OnScanProgress(progress); },
        [this](const deskhubp::ScanProgress& progress) { OnScanFinished(progress); });
    if (!started) scanTimer_.StartOnce(kRescanDelayMs);
}

void MainFrame::RescanNow() {
    scanTimer_.Stop();
    SetHintLabel(deviceHint_, ToWx(ui::kLanDevicesEmpty));
    StartScan();
}

void MainFrame::RefreshDeviceStatus() {
    for (const ui::RecentDevice& device : recent_) {
        uint64_t key = 0;
        if (HostKeyOf(device.addr, key)) probes_.erase(key);
    }
    RefreshDeviceList();
    poller_.RefreshNow();
}

void MainFrame::RefreshDevicesNow() {
    RefreshDeviceStatus();
    RescanNow();
}

void MainFrame::OnScanTimer(wxTimerEvent&) {
    StartScan();
}

void MainFrame::OnScanHit(const deskhubp::ScanHit& hit) {
    scannedThisRound_.push_back(hit.addr);
    RecordProbe(hit.addr, true, hit.rttMs);

    const auto known = std::find_if(scanned_.begin(), scanned_.end(),
        [&hit](const deskhubp::ScanHit& seen) { return seen.addr == hit.addr; });
    if (known == scanned_.end()) {
        scanned_.push_back(hit);
        RefreshDeviceList();
    } else {
        known->rttMs = hit.rttMs;
    }
    ApplyProbeToRows(hit.addr);
}

void MainFrame::OnScanProgress(const deskhubp::ScanProgress& progress) {
    SetHintLabel(deviceHint_,
        ToWx(ui::ScanningStatus(progress.probed, progress.total, uint16_t(settings_.port))));
}

void MainFrame::OnScanFinished(const deskhubp::ScanProgress& progress) {
    const auto gone = [this](const deskhubp::ScanHit& hit) {
        return std::find(scannedThisRound_.begin(), scannedThisRound_.end(), hit.addr) ==
               scannedThisRound_.end();
    };
    scanned_.erase(std::remove_if(scanned_.begin(), scanned_.end(), gone), scanned_.end());
    RefreshDeviceList();

    SetHintLabel(deviceHint_,
        ToWx(ui::LanDevicesNote(scanned_.size(), progress.total, deskhubp::kLanRescanSecs)));
    scanTimer_.StartOnce(kRescanDelayMs);
}

void MainFrame::OnListClick(wxMouseEvent& event) {
    event.Skip();
    int flags = 0;
    const long row = deviceList_->HitTest(event.GetPosition(), flags);
    LOGI("[UI] device list click: row %ld%s.", row, prompting_ ? " (prompt already open)" : "");

    if (row == wxNOT_FOUND || prompting_) return;

    prompting_ = true;
    CallAfter([this, row] {
        ConnectRow(row);
        prompting_ = false;
    });
}

void MainFrame::ConnectRow(long row) {
    if (row < 0 || size_t(row) >= deviceRows_.size()) return;
    const std::string addr = deviceRows_[size_t(row)].addr;
    ConnectWithPrompt(addr, ui::PasscodeForDevice(recent_, addr));
}

void MainFrame::ConnectWithPrompt(const std::string& addr, std::string passcode) {
    std::string target = addr;
    if (!ShowPasscodePrompt(this, target, passcode)) return;
    const uint16_t port = ui::AddressPort(target);
    addrCtrl_->ChangeValue(ToWx(ui::AddressHost(target)));
    connectPortCtrl_->ChangeValue(
        ToWx(std::to_string(port != 0 ? port : deskhub::kDeskhubPort)));
    clientPasscodeCtrl_->ChangeValue(ToWx(ui::TrimAscii(passcode)));
    StartConnect(target);
}

void MainFrame::OnSourcesReady(const std::string& addr, const std::string& passcode,
    const deskhubp::ConnectOutcome& outcome) {
    connectBtn_->Enable();
    SetClientStatus(wxString(), kMutedText);
    DeselectAllRows();

    if (!outcome.ok) {
        wxMessageBox(ToWx(ui::SourceQueryFailed(addr)), "Deskhub", wxOK | wxICON_ERROR, this);
        return;
    }

    ui::TouchRecentDevice(recent_, addr, NowUnixSeconds(), passcode);
    SaveRecentDevices();
    poller_.SetAddresses(ui::AddressesOf(recent_));
    RefreshDeviceList();

    OpenConnectionWindow(addr, passcode, outcome);
}

ConnectionFrame* MainFrame::ConnectionFor(const std::string& addr) const {
    for (ConnectionFrame* frame : connections_)
        if (ui::SameDeviceAddr(frame->Address(), addr)) return frame;
    return nullptr;
}

void MainFrame::OpenConnectionWindow(const std::string& addr, const std::string& passcode,
    const deskhubp::ConnectOutcome& outcome) {
    if (ConnectionFrame* open = ConnectionFor(addr)) {
        open->Raise();
        open->SetFocus();
        return;
    }

    auto* frame = new ConnectionFrame(this, addr, passcode, outcome.caps, outcome.sources,
        settings_.clientControl);
    const int cascade = FromDIP(kConnectionWindowCascade) * int(connections_.size());
    frame->Move(GetPosition() + wxPoint(FromDIP(48) + cascade, FromDIP(48) + cascade));
    connections_.push_back(frame);
    frame->ApplyProbe(ProbeFor(addr));
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

void MainFrame::DeselectAllRows() {
    for (long row = 0; row < deviceList_->GetItemCount(); ++row)
        deviceList_->SetItemState(row, 0, wxLIST_STATE_SELECTED);
}

void MainFrame::SaveSettings() {
    settings_.fps = uint32_t(fpsCtrl_->GetValue());
    settings_.bitrateMbps = uint32_t(bitrateCtrl_->GetValue());
    settings_.port = uint32_t(portCtrl_->GetValue());
    settings_.allowInput = allowInputCtrl_->GetValue();
    settings_.clipboardSync = clipboardCtrl_->GetValue();
    settings_.shareAudio = shareAudioCtrl_->GetValue();
    settings_.playAudio = playAudioCtrl_->GetValue();
    settings_.keepAwake = keepAwakeCtrl_->GetValue();
    const std::string passcode(passcodeCtrl_->GetValue().utf8_str());
    if (passcode.empty() || deskhub::IsValidPasscode(passcode)) settings_.passcode = passcode;
    const int quality = qualityChoice_->GetSelection();
    if (quality != wxNOT_FOUND)
        settings_.maxDim = deskhub::media::QualityPresetMaxDim(size_t(quality),
            settings_.maxDim);
    const int bindSel = bindChoice_->GetSelection();
    if (bindSel != wxNOT_FOUND && size_t(bindSel) < bindChoices_.size())
        settings_.bindIp = bindChoices_[size_t(bindSel)];
    settings_.autoShare = autoShareCtrl_->GetValue();
    settings_.startHidden = startHiddenCtrl_->GetValue();
    ApplyTrayMode();
    const bool autostart = autostartCtrl_->GetValue();
    if (autostart != settings_.autostart) {
        deskhubp::SetAutostartEnabled(autostart);
        settings_.autostart = deskhubp::AutostartEnabled();
        autostartCtrl_->SetValue(settings_.autostart);
    }
    deskhubp::SaveUiSettings(settings_);
    if (!Sharing()) ShowIdleHostState();
}

void MainFrame::SaveRecentDevices() {
    deskhubp::WriteAppDataFile(kRecentDevicesFile, ui::SerializeRecentDevices(recent_));
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
    CloseEveryConnection();
    share_.terminalHost().Stop();
    hostTimer_.Stop();
    clipTimer_.Stop();
    scanTimer_.Stop();
    autoShareTimer_.Stop();
    scanner_.Cancel();
    share_.sharingHost().Stop();
    shareDriver_.Join();
    poller_.Stop();
    event.Skip();
}

ConnectionFrame::ConnectionFrame(MainFrame* owner, std::string address, std::string passcode,
    deskhub::HostCaps caps, std::vector<deskhub::SourceInfo> sources, bool control)
    : wxFrame(nullptr, wxID_ANY, ToWx(address)), owner_(owner), address_(std::move(address)), passcode_(std::move(passcode)), caps_(caps), sources_(std::move(sources)), control_(control) {
    SetIcon(wxICON(deskhub_app_icon));
    SetBackgroundColour(*wxWHITE);

    auto* panel = new wxPanel(this);
    panel->SetBackgroundColour(*wxWHITE);
    auto* sizer = new wxBoxSizer(wxVERTICAL);

    auto* addressRow = new wxBoxSizer(wxHORIZONTAL);
    addressRow->Add(MakeSection(panel, address_.c_str()), wxSizerFlags(1).CentreVertical());
    auto* disconnectBtn = new wxButton(panel, wxID_ANY, ToWx(ui::kDisconnectButton));
    disconnectBtn->SetName("disconnect-button");
    PaintButton(disconnectBtn, kOffline);
    disconnectBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
    addressRow->Add(disconnectBtn, wxSizerFlags().CentreVertical());
    sizer->Add(addressRow, wxSizerFlags().Expand().Border(wxTOP, FromDIP(16)));

    auto* stateRow = new wxBoxSizer(wxHORIZONTAL);
    stateLabel_ = new wxStaticText(panel, wxID_ANY, ToWx(ui::kConnectedPickSession));
    stateLabel_->SetForegroundColour(kOnline);
    stateRow->Add(stateLabel_, wxSizerFlags(1).CentreVertical());
    pingLabel_ = new wxStaticText(panel, wxID_ANY, wxString());
    pingLabel_->SetForegroundColour(kOnline);
    stateRow->Add(pingLabel_, wxSizerFlags().CentreVertical());
    sizer->Add(stateRow, wxSizerFlags().Expand().Border(wxTOP, FromDIP(8)));

    auto* openDesktopBtn = new wxButton(panel, wxID_ANY, ToWx(ui::kOpenDesktopLabel));
    openDesktopBtn->SetName("open-desktop");
    openDesktopBtn->Enable(!sources_.empty());
    openDesktopBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenDesktopSession(); });
    sizer->Add(openDesktopBtn, wxSizerFlags().Expand().Border(wxTOP, FromDIP(12)));

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
    sizer->Add(controlRow, wxSizerFlags().Border(wxTOP, FromDIP(8)));

    auto* openShellBtn = new wxButton(panel, wxID_ANY, ToWx(ui::kOpenShellLabel));
    openShellBtn->SetName("open-shell");
    openShellBtn->Enable(caps_.terminal);
    openShellBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenShellSession(); });
    sizer->Add(openShellBtn, wxSizerFlags().Expand().Border(wxTOP, FromDIP(8)));

    auto* openFilesBtn = new wxButton(panel, wxID_ANY, ToWx(ui::kOpenFilesLabel));
    openFilesBtn->SetName("open-files");
    openFilesBtn->Enable(caps_.files);
    openFilesBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenFileSendSession(); });
    sizer->Add(openFilesBtn, wxSizerFlags().Expand().Border(wxTOP, FromDIP(8)));

    sizer->Add(MakeHint(panel, ToWx(ui::kMobileHostNote)),
        wxSizerFlags().Border(wxTOP | wxBOTTOM, FromDIP(8)));

    auto* pagePad = new wxBoxSizer(wxVERTICAL);
    pagePad->Add(sizer, wxSizerFlags(1).Expand().Border(wxLEFT | wxRIGHT, FromDIP(16)));
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

void ConnectionFrame::ApplyProbe(const ProbeResult* probe) {
    const bool online = probe && probe->online;
    const wxColour tint = probe && !probe->online ? kOffline : kOnline;
    stateLabel_->SetForegroundColour(tint);
    pingLabel_->SetForegroundColour(tint);
    pingLabel_->SetLabel(online ? ToWx(ui::PingMs(probe->rttMs)) : wxString());
    Layout();
}

void ConnectionFrame::OpenDesktopSession() {
    if (sources_.empty()) return;
    std::vector<deskhub::SourceInfo> picked;
    if (!ShowSourcePickerDialog(HWND(GetHandle()), sources_, picked)) return;
    std::thread([addr = address_, passcode = passcode_, picked = std::move(picked),
                    control = control_] { RunViewer(addr, picked, control, passcode); })
        .detach();
}

void ConnectionFrame::OpenShellSession() {
    NetAddr server{};
    if (ParseNetAddr(address_, server)) owner_->OpenShell(server, passcode_);
}

void ConnectionFrame::OpenFileSendSession() {
    NetAddr server{};
    if (ParseNetAddr(address_, server)) owner_->OpenFileSend(server, passcode_);
}

wxMenu* DeskhubTrayIcon::CreatePopupMenu() {
    auto* menu = new wxMenu();
    const int toggleWindowId =
        menu->Append(wxID_ANY,
                ToWx(frame_.IsShown() ? ui::kTrayHideWindow : ui::kTrayShowWindow))
            ->GetId();
    const int toggleShareId =
        menu->Append(wxID_ANY, ToWx(frame_.ScreenSharing() ? ui::kStopSharing : ui::kStartSharing))
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

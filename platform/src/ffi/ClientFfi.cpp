#include "deskhubp/ffi/ClientFfi.h"

#include "deskhub/input/Hotkeys.h"
#include "deskhub/input/PointerLockState.h"
#include "deskhub/input/Set1Scancodes.h"
#include "deskhub/input/VirtualKeys.h"
#include "deskhub/input/TrackpadCursor.h"
#include "deskhub/media/SourceLabel.h"
#include "deskhub/media/ViewerTitle.h"
#include "deskhub/media/ViewFit.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/client/ConnectFlow.h"
#include "deskhub/session/client/OpenViewers.h"
#include "deskhub/ui/AutoShareGate.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/diag/LogFile.h"
#include "deskhubp/ffi/FfiText.h"
#include "deskhubp/input/NativeKeyMap.h"
#include "deskhubp/client/SourceQuery.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

deskhub::ViewRect ToRect(DHViewRect r) {
    return {r.x, r.y, r.width, r.height};
}

deskhub::OpenViewerCount& OpenViewers() {
    static deskhub::OpenViewerCount count;
    return count;
}

template <class Fn>
DHPointerLockEffect ApplyPointerLock(DHPointerLock* state, Fn&& step) {
    if (!state) return DHPointerLockEffect{false, false};
    deskhub::PointerLockState s(state->locked);
    const deskhub::PointerLockEffect e = step(s);
    state->locked = s.locked();
    return DHPointerLockEffect{e.lockChanged, e.releaseHeldInput};
}

}

extern "C" {

const char* dh_string(DHStringId id) {
    switch (id) {
        case DHStrAppTitle: return deskhub::ui::kAppTitle;
        case DHStrHostIpIntro: return deskhub::ui::kHostIpIntro;
        case DHStrNoNetworkAddress: return deskhub::ui::kNoNetworkAddress;
        case DHStrClientIpPrompt: return deskhub::ui::kClientIpPrompt;
        case DHStrPickerTitle: return deskhub::ui::kPickerTitle;
        case DHStrPickerEachWindow: return deskhub::ui::kPickerEachWindow;
        case DHStrShareButton: return deskhub::ui::kShareButton;
        case DHStrSharingTitle: return deskhub::ui::kSharingTitle;
        case DHStrSharingSourcesIntro: return deskhub::ui::kSharingSourcesIntro;
        case DHStrSharingConnectHint: return deskhub::ui::kSharingConnectHint;
        case DHStrNothingShared: return deskhub::ui::kNothingShared;
        case DHStrStopSharing: return deskhub::ui::kStopSharing;
        case DHStrQueryingSources: return deskhub::ui::kQueryingSources;
        case DHStrViewerOpenFailed: return deskhub::ui::kViewerOpenFailed;
        case DHStrConnectionEndedTitle: return deskhub::ui::kConnectionEndedTitle;
        case DHStrDisconnected: return deskhub::ui::kDisconnected;
        case DHStrClientPasscodePrompt: return deskhub::ui::kClientPasscodePrompt;
        case DHStrClientPasscodeHint: return deskhub::ui::kClientPasscodeHint;
        case DHStrPasscodeInvalid: return deskhub::ui::kPasscodeInvalid;
        case DHStrPasscodeLabel: return deskhub::ui::kPasscodeLabel;
        case DHStrLanDevicesHeading: return deskhub::ui::kLanDevicesHeading;
        case DHStrRecentDevicesHeading: return deskhub::ui::kRecentDevicesHeading;
        case DHStrRecentDevicesHint: return deskhub::ui::kRecentDevicesHint;
        case DHStrRecentDevicesEmpty: return deskhub::ui::kRecentDevicesEmpty;
        case DHStrSidebarHost: return deskhub::ui::kSidebarHost;
        case DHStrSidebarClient: return deskhub::ui::kSidebarClient;
        case DHStrSidebarSettings: return deskhub::ui::kSidebarSettings;
        case DHStrHostHeading: return deskhub::ui::kHostHeading;
        case DHStrClientHeading: return deskhub::ui::kClientHeading;
        case DHStrSettingsHeading: return deskhub::ui::kSettingsHeading;
        case DHStrSettingsHint: return deskhub::ui::kSettingsHint;
        case DHStrClientSettingsHeading: return deskhub::ui::kClientSettingsHeading;
        case DHStrClientSettingsHint: return deskhub::ui::kClientSettingsHint;
        case DHStrUdpPortLabel: return deskhub::ui::kUdpPortLabel;
        case DHStrProjectUrl: return deskhub::ui::kProjectUrl;
        case DHStrProjectLinkLabel: return deskhub::ui::kProjectLinkLabel;
        case DHStrAllowControlLabel: return deskhub::ui::kAllowControlLabel;
        case DHStrRequestControlLabel: return deskhub::ui::kRequestControlLabel;
        case DHStrClientIpPlaceholder: return deskhub::ui::kClientIpPlaceholder;
        case DHStrConnectPromptTitle: return deskhub::ui::kConnectPromptTitle;
        case DHStrPickDisplaysHint: return deskhub::ui::kPickDisplaysHint;
        case DHStrNoDisplayTicked: return deskhub::ui::kNoDisplayTicked;
        case DHStrStopSelectedDisplay: return deskhub::ui::kStopSelectedDisplay;
        case DHStrDisconnectSelectedViewer: return deskhub::ui::kDisconnectSelectedViewer;
        case DHStrShareStateOn: return deskhub::ui::kShareStateOn;
        case DHStrShareStateOff: return deskhub::ui::kShareStateOff;
        case DHStrStartSharing: return deskhub::ui::kStartSharing;
        case DHStrStartingShare: return deskhub::ui::kStartingShare;
        case DHStrRefreshNow: return deskhub::ui::kRefreshNow;
        case DHStrStopDisplayAction: return deskhub::ui::kStopDisplayAction;
        case DHStrDisconnectViewerAction: return deskhub::ui::kDisconnectViewerAction;
        case DHStrLanDevicesHint: return deskhub::ui::kLanDevicesHint;
        case DHStrNotSharing: return deskhub::ui::kNotSharing;
        case DHStrLanDevicesEmpty: return deskhub::ui::kLanDevicesEmpty;
        case DHStrBroadcastMemoryLabel: return deskhub::ui::kBroadcastMemoryLabel;
        case DHStrBindInterfaceLabel: return deskhub::ui::kBindInterfaceLabel;
        case DHStrBindAllInterfaces: return deskhub::ui::kBindAllInterfaces;
        case DHStrAutostartLabel: return deskhub::ui::kAutostartLabel;
        case DHStrAutoShareLabel: return deskhub::ui::kAutoShareLabel;
        case DHStrClipboardSyncLabel: return deskhub::ui::kClipboardSyncLabel;
        case DHStrShareAudioLabel: return deskhub::ui::kShareAudioLabel;
        case DHStrPlayAudioLabel: return deskhub::ui::kPlayAudioLabel;
        case DHStrCloseToTrayLabel: return deskhub::ui::kCloseToTrayLabel;
        case DHStrTrayShowWindow: return deskhub::ui::kTrayShowWindow;
        case DHStrTrayHideWindow: return deskhub::ui::kTrayHideWindow;
        case DHStrTrayQuit: return deskhub::ui::kTrayQuit;
        case DHStrBindNotConnectedNote: return deskhub::ui::kBindNotConnectedNote;
        case DHStrSettingsSectionVideo: return deskhub::ui::kSettingsSectionVideo;
        case DHStrSettingsSectionConnection: return deskhub::ui::kSettingsSectionConnection;
        case DHStrSettingsSectionSecurity: return deskhub::ui::kSettingsSectionSecurity;
        case DHStrSettingsSectionSession: return deskhub::ui::kSettingsSectionSession;
        case DHStrSettingsSectionLaunch: return deskhub::ui::kSettingsSectionLaunch;
        case DHStrKeepAwakeLabel: return deskhub::ui::kKeepAwakeLabel;
        case DHStrPairingRequestTitle: return deskhub::ui::kPairingRequestTitle;
        case DHStrPairingAllow: return deskhub::ui::kPairingAllow;
        case DHStrPairingDeny: return deskhub::ui::kPairingDeny;
        case DHStrSidebarDevices: return deskhub::ui::kSidebarDevices;
        case DHStrPairedHeading: return deskhub::ui::kPairedHeading;
        case DHStrPairedHint: return deskhub::ui::kPairedHint;
        case DHStrPairedEmpty: return deskhub::ui::kPairedEmpty;
        case DHStrPairedForget: return deskhub::ui::kPairedForget;
        case DHStrPairedForgetAll: return deskhub::ui::kPairedForgetAll;
        case DHStrPairedForgetAllPrompt: return deskhub::ui::kPairedForgetAllPrompt;
        case DHStrAllowPairingLabel: return deskhub::ui::kAllowPairingLabel;
        case DHStrAllowPairingHint: return deskhub::ui::kAllowPairingHint;
        case DHStrThisMachineHeading: return deskhub::ui::kThisMachineHeading;
        case DHStrThisMachineHint: return deskhub::ui::kThisMachineHint;
        case DHStrPairedColumnName: return deskhub::ui::kPairedColumnName;
        case DHStrPairedColumnKey: return deskhub::ui::kPairedColumnKey;
        case DHStrPairedColumnPaired: return deskhub::ui::kPairedColumnPaired;
        case DHStrPairedColumnLastSeen: return deskhub::ui::kPairedColumnLastSeen;
        case DHStrTrustNewHostTitle: return deskhub::ui::kTrustNewHostTitle;
        case DHStrTrustNewHostBody: return deskhub::ui::kTrustNewHostBody;
        case DHStrTrustChangedTitle: return deskhub::ui::kTrustChangedTitle;
        case DHStrTrustChangedBody: return deskhub::ui::kTrustChangedBody;
        case DHStrTrustFingerprintLabel: return deskhub::ui::kTrustFingerprintLabel;
        case DHStrTrustAccept: return deskhub::ui::kTrustAccept;
        case DHStrTrustReject: return deskhub::ui::kTrustReject;
        case DHStrTerminalSourceName: return deskhub::ui::kTerminalSourceName;
        case DHStrTerminalPickerLabel: return deskhub::ui::kTerminalPickerLabel;
        case DHStrFilesSourceName: return deskhub::ui::kFilesSourceName;
        case DHStrFilesPickerLabel: return deskhub::ui::kFilesPickerLabel;
        case DHStrTransferHeading: return deskhub::ui::kTransferHeading;
        case DHStrTransferChooseButton: return deskhub::ui::kTransferChooseButton;
        case DHStrTransferCancelButton: return deskhub::ui::kTransferCancelButton;
        case DHStrTransferFolderLabel: return deskhub::ui::kTransferFolderLabel;
        case DHStrTransferSending: return deskhub::ui::kTransferSending;
        case DHStrTransferDone: return deskhub::ui::kTransferDone;
        case DHStrTransferHostNotTaking: return deskhub::ui::kTransferHostNotTaking;
        case DHStrTransferSendHeading: return deskhub::ui::kTransferSendHeading;
        case DHStrTransferNoneChosen: return deskhub::ui::kTransferNoneChosen;
        case DHStrTransferBusyNote: return deskhub::ui::kTransferBusyNote;
        case DHStrTransferTooManyFiles: return deskhub::ui::kTransferTooManyFiles;
        case DHStrOpenFilesLabel: return deskhub::ui::kOpenFilesLabel;
        case DHStrTransferSentHeading: return deskhub::ui::kTransferSentHeading;
        case DHStrTransferAcceptLabel: return deskhub::ui::kTransferAcceptLabel;
        case DHStrTransferBlocksScreenNote: return deskhub::ui::kTransferBlocksScreenNote;
        case DHStrTransferArrivedTitle: return deskhub::ui::kTransferArrivedTitle;
        case DHStrTransferStopTakingButton: return deskhub::ui::kTransferStopTakingButton;
        case DHStrConnectedPickSession: return deskhub::ui::kConnectedPickSession;
        case DHStrConnectFirstHint: return deskhub::ui::kConnectFirstHint;
        case DHStrDisconnectButton: return deskhub::ui::kDisconnectButton;
        case DHStrLinkQualityGood: return deskhub::ui::kLinkQualityGood;
        case DHStrLinkQualityFair: return deskhub::ui::kLinkQualityFair;
        case DHStrLinkQualityPoor: return deskhub::ui::kLinkQualityPoor;
        case DHStrLinkNoReading: return deskhub::ui::kLinkNoReading;
        case DHStrLinkReattaching: return deskhub::ui::kTerminalReattaching;
        case DHStrOpenChoiceGroup: return deskhub::ui::kOpenChoiceGroup;
        case DHStrOpenDesktopLabel: return deskhub::ui::kOpenDesktopLabel;
        case DHStrOpenShellLabel: return deskhub::ui::kOpenShellLabel;
        case DHStrOpenNothingTicked: return deskhub::ui::kOpenNothingTicked;
        case DHStrTerminalExtraKeysHint: return deskhub::ui::kTerminalExtraKeysHint;
        case DHStrPickSourcesHint: return deskhub::ui::kPickSourcesHint;
        case DHStrPairedForgetNote: return deskhub::ui::kPairedForgetNote;
        case DHStrOpenChoiceHint: return deskhub::ui::kOpenChoiceHint;
        case DHStrMobileHostNote: return deskhub::ui::kMobileHostNote;
        case DHStrHostHasNoTerminal: return deskhub::ui::kHostHasNoTerminal;
        case DHStrWaitingForDisplays: return deskhub::ui::kWaitingForDisplays;
        case DHStrNoDisplayFound: return deskhub::ui::kNoDisplayFound;
        case DHStrPasscodeHint: return deskhub::ui::kPasscodeHint;
        case DHStrDevicesHeading: return deskhub::ui::kDevicesHeading;
        case DHStrDeviceColumnWhere: return deskhub::ui::kDeviceColumnWhere;
        case DHStrDeviceNameLabel: return deskhub::ui::kDeviceNameLabel;
        case DHStrConnectButton: return deskhub::ui::kConnectButton;
        case DHStrCopyButton: return deskhub::ui::kCopyButton;
        case DHStrFpsLabel: return deskhub::ui::kFpsLabel;
        case DHStrBitrateLabel: return deskhub::ui::kBitrateLabel;
        case DHStrQualityLabel: return deskhub::ui::kQualityLabel;
        case DHStrAttachShellAction: return deskhub::ui::kAttachShellAction;
        case DHStrTerminalLocalWindowTitle: return deskhub::ui::kTerminalLocalWindowTitle;
        case DHStrTerminalAttachedHere: return deskhub::ui::kTerminalAttachedHere;
        case DHStrTerminalClosed: return deskhub::ui::kTerminalClosed;
        case DHStrShareClampWarning: {
            static const std::string warning = deskhub::ui::ShareClampWarning();
            return warning.c_str();
        }
        case DHStrUdpPortLine: {
            static const std::string line = deskhub::ui::UdpPortLine();
            return line.c_str();
        }
        case DHStrSessionEnded: return deskhub::ui::kSessionEnded;
        case DHStrShareStartFailed: return deskhub::ui::kShareStartFailed;
        case DHStrScreenRecordingRequired: return deskhub::ui::kScreenRecordingRequired;
        case DHStrInvalidAddressHint: {
            static const std::string hint = deskhub::ui::InvalidAddressHint();
            return hint.c_str();
        }
    }
    return "";
}

bool dh_parse_address(const char* address) {
    if (!address) return false;
    NetAddr server;
    return ParseNetAddr(address, server);
}

int dh_pairing_request_body(const char* name, const char* address, const char* shortKey,
    char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::PairingRequestBody(
            name ? name : "", address ? address : "", shortKey ? shortKey : ""));
    return int(std::strlen(out));
}

int dh_connecting_to(const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity), deskhub::ui::ConnectingTo(address ? address : ""));
    return int(std::strlen(out));
}

int dh_link_quality_text(DHLinkQuality quality, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::LinkQualityText(deskhub::LinkQuality(quality)));
    return int(std::strlen(out));
}

int dh_link_ping_text(bool haveRtt, uint32_t rttMs, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        haveRtt ? deskhub::ui::PingMs(rttMs) : std::string(deskhub::ui::kLinkNoReading));
    return int(std::strlen(out));
}

int dh_could_not_connect(const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::CouldNotConnectTo(address ? address : ""));
    return int(std::strlen(out));
}

int dh_source_query_failed(const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::SourceQueryFailed(address ? address : ""));
    return int(std::strlen(out));
}

int dh_source_query_empty(const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::SourceQueryEmpty(address ? address : ""));
    return int(std::strlen(out));
}

int dh_udp_port_line(uint32_t port, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity), deskhub::ui::UdpPortLine(uint16_t(port)));
    return int(std::strlen(out));
}

int dh_compose_address(const char* host, const char* portText, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const uint16_t port = deskhub::ui::PortOrDefault(portText ? portText : "");
    deskhubp::CopyToBuf(out, size_t(capacity), deskhub::ui::AddressWithPort(host ? host : "", port));
    return int(std::strlen(out));
}

int dh_address_host(const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity), deskhub::ui::AddressHost(address ? address : ""));
    return int(std::strlen(out));
}

uint32_t dh_address_port(const char* address) {
    return deskhub::ui::AddressPort(address ? address : "");
}

int dh_host_title(const char* address, uint32_t width, uint32_t height, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::HostTitle(address ? address : "", width, height));
    return int(std::strlen(out));
}

int dh_zoom_label(double zoom, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    std::snprintf(out, size_t(capacity), "%.1f\xC3\x97", zoom);
    return int(std::strlen(out));
}

bool dh_is_zoomed(double zoom) {
    return deskhub::IsZoomed(zoom);
}

bool dh_should_refit_viewer(uint32_t fitted_w, uint32_t fitted_h, uint32_t new_w,
    uint32_t new_h) {
    return deskhub::ShouldRefitViewer(fitted_w, fitted_h, new_w, new_h);
}

void dh_fit_viewer_window(uint32_t video_w, uint32_t video_h, uint32_t work_w, uint32_t work_h,
    uint32_t* out_w, uint32_t* out_h) {
    if (!out_w || !out_h) return;
    const uint32_t margin = uint32_t(deskhub::kViewerMarginPx);
    const uint32_t avail_w = work_w > margin ? work_w - margin : work_w;
    const uint32_t avail_h = work_h > margin ? work_h - margin : work_h;
    const deskhub::ViewSize fitted = deskhub::ScaleToFit(video_w, video_h, avail_w, avail_h);
    *out_w = fitted.width;
    *out_h = fitted.height;
}

int dh_viewer_base_title(const char* sourceName, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ViewerBaseTitle(sourceName ? sourceName : ""));
    return int(std::strlen(out));
}

bool dh_is_valid_passcode(const char* passcode) {
    return passcode && deskhub::IsValidPasscode(passcode);
}

int dh_passcode_digits(void) {
    return int(deskhub::kPasscodeDigits);
}

int dh_max_sources(void) {
    return int(deskhub::kMaxSources);
}

int dh_max_transfer_files(void) {
    return int(deskhub::kMaxTransferFiles);
}

uint32_t dh_auto_share_probe_ms(void) {
    return deskhub::ui::kAutoShareProbeMs;
}

DHAutoShareStep dh_auto_share_step(bool displays_ready, uint32_t waited_ms) {
    switch (deskhub::ui::NextAutoShareStep(displays_ready, waited_ms,
        deskhub::ui::kAutoShareProbeMs, deskhub::ui::kAutoShareGiveUpMs)) {
        case deskhub::ui::AutoShareStep::ShareNow: return DHAutoShareShareNow;
        case deskhub::ui::AutoShareStep::GiveUpWaiting: return DHAutoShareGiveUpWaiting;
        case deskhub::ui::AutoShareStep::KeepWaiting: break;
    }
    return DHAutoShareKeepWaiting;
}

int dh_list_sources(const char* address, DHSourceInfo* out, int capacity, const char* passcode,
    DHHostCaps* out_caps) {
    if (out_caps) *out_caps = DHHostCaps{false, false, false, false};
    if (!address || !out || capacity <= 0) return DH_SOURCE_QUERY_FAILED;

    NetAddr server;
    if (!ParseNetAddr(address, server)) {
        LOGE("[Bridge] Invalid address: %s", address);
        return DH_SOURCE_QUERY_FAILED;
    }

    std::vector<deskhub::SourceInfo> sources;
    deskhub::HostCaps caps{};
    if (!QuerySources(server, sources, passcode ? passcode : "", nullptr, &caps))
        return DH_SOURCE_QUERY_FAILED;
    if (out_caps)
        *out_caps = DHHostCaps{caps.acceptsInput, caps.terminal, caps.audio, caps.files};

    const int count = int(sources.size()) < capacity ? int(sources.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const deskhub::SourceInfo& src = sources[size_t(i)];
        out[i].sourceId = src.sourceId;
        out[i].width = src.width;
        out[i].height = src.height;
        deskhubp::CopyToBuf(out[i].name, sizeof(out[i].name), src.name);
        deskhubp::CopyToBuf(out[i].displayName, sizeof(out[i].displayName),
            deskhub::media::SourceName(src.name, src.sourceId));
        deskhubp::CopyToBuf(out[i].sizeLabel, sizeof(out[i].sizeLabel),
            deskhub::media::SourceSizeLabel(src.width, src.height));
        deskhubp::CopyToBuf(out[i].pickerLabel, sizeof(out[i].pickerLabel),
            deskhub::media::SourcePickerLabel(src.name, src.sourceId, src.width, src.height));
    }
    return count;
}

bool dh_host_has_terminal(const char* address, const char* passcode) {
    NetAddr server;
    if (!address || !ParseNetAddr(address, server)) return false;

    std::vector<deskhub::SourceInfo> sources;
    deskhub::HostCaps caps{};
    if (!QuerySources(server, sources, passcode ? passcode : "", nullptr, &caps)) return false;
    return caps.terminal;
}

bool dh_host_takes_files(const char* address, const char* passcode) {
    NetAddr server;
    if (!address || !ParseNetAddr(address, server)) return false;

    std::vector<deskhub::SourceInfo> sources;
    deskhub::HostCaps caps{};
    if (!QuerySources(server, sources, passcode ? passcode : "", nullptr, &caps)) return false;
    return caps.files;
}

bool dh_connect_decision(const DHSourceInfo* sources, int count, uint8_t* out_source_id) {
    std::vector<deskhub::SourceInfo> list;
    if (sources && count > 0) {
        list.resize(size_t(count));
        for (int i = 0; i < count; ++i) list[size_t(i)].sourceId = sources[i].sourceId;
    }
    const deskhub::ConnectDecision d = deskhub::DecideAfterSourceQuery(list);
    if (out_source_id) *out_source_id = d.sourceId;
    return d.showPicker;
}

DHConnectPlan dh_connect_plan(DHHostCaps caps, const DHSourceInfo* sources, int count,
    bool want_desktop, bool want_shell, bool want_files) {
    std::vector<deskhub::SourceInfo> list;
    if (sources && count > 0) {
        list.resize(size_t(count));
        for (int i = 0; i < count; ++i) list[size_t(i)].sourceId = sources[i].sourceId;
    }

    deskhub::HostCaps hostCaps;
    hostCaps.acceptsInput = caps.acceptsInput;
    hostCaps.terminal = caps.terminal;
    hostCaps.audio = caps.audio;
    hostCaps.files = caps.files;

    const deskhub::ConnectPlan planned = deskhub::PlanAfterConnect(hostCaps, list,
        deskhub::OpenChoice{want_desktop, want_shell, want_files});

    DHConnectPlan out{};
    out.openShell = planned.openShell;
    out.openFiles = planned.openFiles;
    out.openDesktop = planned.openDesktop;
    out.showPicker = planned.showPicker;
    out.sourceId = planned.sourceId;
    out.problem = int32_t(planned.problem);
    return out;
}

int dh_connect_problem_text(int32_t problem, const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ConnectProblemText(deskhub::ConnectProblem(problem), address ? address : ""));
    return int(std::strlen(out));
}

int dh_hotkeys(DHHotkey* out, int capacity) {
    if (!out || capacity <= 0) return 0;

    const std::span<const deskhub::Hotkey> table = deskhub::TouchHotkeys();
    const int count = int(table.size()) < capacity ? int(table.size()) : capacity;
    for (int i = 0; i < count; ++i) {
        const deskhub::Hotkey& h = table[size_t(i)];
        const size_t room = sizeof(out[i].label) - 1;
        const size_t n = std::strlen(h.label) < room ? std::strlen(h.label) : room;
        std::memcpy(out[i].label, h.label, n);
        out[i].label[n] = '\0';
        out[i].vk = h.vk;
        out[i].scan = h.scan;
        out[i].modVk = h.modVk;
        out[i].modScan = h.modScan;
    }
    return count;
}

DHViewRect dh_video_rect(double viewportW, double viewportH, double aspect, DHViewTransform t) {
    const deskhub::ViewRect r = deskhub::FitVideoRect(viewportW, viewportH, aspect,
        deskhub::ViewTransform{t.zoom, t.panX, t.panY});
    return DHViewRect{r.x, r.y, r.width, r.height};
}

DHViewTransform dh_apply_gesture(DHViewTransform cur, double factor, double centroidX,
    double centroidY, double panDeltaX, double panDeltaY, double viewportW, double viewportH,
    double aspect) {
    const deskhub::ViewTransform t = deskhub::ApplyGesture(
        deskhub::ViewTransform{cur.zoom, cur.panX, cur.panY}, factor, centroidX, centroidY,
        panDeltaX, panDeltaY, viewportW, viewportH, aspect);
    return DHViewTransform{t.zoom, t.panX, t.panY};
}

bool dh_normalize_pointer(double px, double py, DHViewRect rect, int32_t* nx, int32_t* ny) {
    if (!nx || !ny) return false;
    return deskhub::NormalizePointer(px, py, ToRect(rect), *nx, *ny);
}

int32_t dh_take_scroll_notches(double dragPoints, double* carry) {
    if (!carry) return 0;
    return deskhub::TakeScrollNotches(dragPoints, *carry);
}

int32_t dh_scroll_notches_from_lines(double lines) {
    return deskhub::ScrollNotchesFromLines(lines);
}

DHCursor dh_cursor_clamp(DHCursor cur, DHViewRect video, double viewportW, double viewportH) {
    const deskhub::TrackpadCursor out = deskhub::ClampToVisible({cur.x, cur.y}, ToRect(video),
        viewportW, viewportH);
    return DHCursor{out.x, out.y};
}

DHCursor dh_cursor_move(DHCursor cur, double dx, double dy, DHViewRect video, double viewportW,
    double viewportH) {
    const deskhub::TrackpadCursor out = deskhub::MoveCursorBy({cur.x, cur.y}, dx, dy,
        ToRect(video), viewportW, viewportH);
    return DHCursor{out.x, out.y};
}

bool dh_cursor_point(DHCursor cur, DHViewRect video, double* px, double* py) {
    if (!px || !py) return false;
    return deskhub::CursorScreenPoint({cur.x, cur.y}, ToRect(video), *px, *py);
}

bool dh_cursor_normalize(DHCursor cur, DHViewRect video, int32_t* nx, int32_t* ny) {
    if (!nx || !ny) return false;
    return deskhub::NormalizeCursor({cur.x, cur.y}, ToRect(video), *nx, *ny);
}

DHModifier dh_modifier_class(int32_t vk) {
    switch (deskhub::ModifierKeyOf(vk)) {
        case deskhub::ModifierKey::Shift: return DHModifierShift;
        case deskhub::ModifierKey::Control: return DHModifierControl;
        case deskhub::ModifierKey::Menu: return DHModifierOption;
        case deskhub::ModifierKey::Win: return DHModifierCommand;
        case deskhub::ModifierKey::CapsLock: return DHModifierCapsLock;
        case deskhub::ModifierKey::None: return DHModifierNone;
    }
    return DHModifierNone;
}

bool dh_native_key_to_vk(int32_t native_key_code, int32_t* out_vk, int32_t* out_scan) {
    if (!out_vk || !out_scan) return false;
    return deskhubp::NativeKeyToWin(native_key_code, *out_vk, *out_scan);
}

int32_t dh_vk_scancode(int32_t vk) {
    return deskhub::VkToSet1Scancode(vk);
}

bool dh_is_lock_toggle_vk(int32_t vk) {
    return vk == deskhub::kViewerLockToggleVk;
}

bool dh_is_escape_vk(int32_t vk) {
    return vk == deskhub::kVkEscape;
}

DHPointerLockEffect dh_pointer_toggle_lock(DHPointerLock* state) {
    return ApplyPointerLock(state, [](deskhub::PointerLockState& s) { return s.OnToggleLockKey(); });
}

DHPointerLockEffect dh_pointer_escape(DHPointerLock* state) {
    return ApplyPointerLock(state, [](deskhub::PointerLockState& s) { return s.OnEscape(); });
}

DHPointerLockEffect dh_pointer_focus_lost(DHPointerLock* state) {
    return ApplyPointerLock(state, [](deskhub::PointerLockState& s) { return s.OnFocusLost(); });
}

int dh_pointer_subtitle(DHPointerLock state, const char* statusLine, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    const deskhub::PointerLockState s(state.locked);
    deskhubp::CopyToBuf(out, size_t(capacity), s.SubtitleFor(statusLine ? statusLine : ""));
    return int(std::strlen(out));
}

int dh_view_only_subtitle(const char* statusLine, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ViewerViewOnlySubtitle(statusLine ? statusLine : ""));
    return int(std::strlen(out));
}

int dh_invalid_address_line(const char* address, char* out, int capacity) {
    if (!out || capacity <= 0) return 0;
    deskhubp::CopyToBuf(out, size_t(capacity),
        deskhub::ui::InvalidAddressLine(address ? address : ""));
    return int(std::strlen(out));
}

void dh_set_data_dir(const char* dir) {
    deskhubp::SetAppDataDir(dir ? std::string(dir) : std::string());
}

void dh_viewer_opened() {
    OpenViewers().Opened();
}

bool dh_viewer_closed() {
    return OpenViewers().Closed();
}
}

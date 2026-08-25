#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/LinkPulse.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#ifndef DESKHUB_VERSION
#define DESKHUB_VERSION "0.0-dev"
#endif

namespace deskhub::ui {

inline constexpr const char* kAppVersion = DESKHUB_VERSION;
inline constexpr const char* kProjectUrl = "https://github.com/manhpham90vn/Deskhub";
inline constexpr const char* kProjectLinkLabel = "GitHub";
inline constexpr const char* kAppTitle = "Deskhub - stream & remotely control a machine";
inline constexpr const char* kHostIpIntro =
    "Others connect to you using one of these IP addresses:";
inline constexpr const char* kNoNetworkAddress = "(no network address found)";
inline constexpr const char* kClientIpPrompt = "Host machine IP address:";
inline constexpr const char* kPickerTitle = "What do you want to view?";
inline constexpr const char* kPickerEachWindow = "Each one you pick opens its own window.";
inline constexpr const char* kShareButton = "Share...  (pick the display to share)";
inline constexpr const char* kSharingTitle = "Deskhub - sharing";
inline constexpr const char* kSharingSourcesIntro = "Sources currently being shared:";
inline constexpr const char* kSharingConnectHint =
    "Others connect by entering this machine's IP address.";
inline constexpr const char* kNothingShared = "(nothing is being shared)";
inline constexpr const char* kStopSharing = "Stop sharing";
inline constexpr const char* kShareStartFailed = "Could not start sharing";
inline constexpr const char* kQueryingSources =
    "Asking the other machine what it is sharing...";
inline constexpr const char* kViewerOpenFailed =
    "Could not open a viewing session - check the address and that the other machine is "
    "sharing.";
inline constexpr const char* kConnectionEndedTitle = "Connection ended";
inline constexpr const char* kDisconnected = "disconnected";
inline constexpr const char* kSessionEnded = "Session ended";
inline constexpr const char* kDisconnectButton = "Disconnect";
inline constexpr const char* kLinkQualityGood = "Good";
inline constexpr const char* kLinkQualityFair = "Fair";
inline constexpr const char* kLinkQualityPoor = "Poor";
inline constexpr const char* kLinkNoReading = "\xE2\x80\x94";

inline const char* LinkQualityText(LinkQuality quality) {
    switch (quality) {
        case LinkQuality::Good: return kLinkQualityGood;
        case LinkQuality::Fair: return kLinkQualityFair;
        case LinkQuality::Poor: return kLinkQualityPoor;
        case LinkQuality::Unknown: break;
    }
    return kLinkNoReading;
}
inline constexpr const char* kScreenRecordingRequired =
    "Screen Recording permission is required. Grant it in System Settings, then quit and "
    "reopen Deskhub.";
inline constexpr const char* kSidebarHost = "Host";
inline constexpr const char* kSidebarClient = "Client";
inline constexpr const char* kSidebarSettings = "Settings";
inline constexpr const char* kHostHeading = "Share this machine's screen";
inline constexpr const char* kClientHeading = "Connect to another machine";
inline constexpr const char* kSettingsHeading = "Share settings";
inline constexpr const char* kSettingsHint = "These apply the next time you start sharing.";
inline constexpr const char* kClientSettingsHeading = "Connection settings";
inline constexpr const char* kClientSettingsHint =
    "The device scan looks for sharing machines on this UDP port. Match it to the port in the "
    "host's Share settings.";
inline constexpr const char* kRecentDevicesHeading = "Recent devices";
inline constexpr const char* kRecentDevicesHint = "Click a device to connect to it again.";
inline constexpr const char* kRecentDevicesEmpty = "Devices you connect to will appear here.";
inline constexpr const char* kStatusOnline = "Online";
inline constexpr const char* kStatusOffline = "Offline";
inline constexpr const char* kStatusChecking = "Checking...";
inline constexpr const char* kNotSharing = "Not sharing.";
inline constexpr const char* kStartingShare = "Starting share...";
inline constexpr const char* kShareStateOn = "Sharing";
inline constexpr const char* kShareStateOff = "Not sharing";
inline constexpr const char* kStartSharing = "Start sharing";
inline constexpr const char* kBroadcastMemoryLabel = "Broadcast memory";
inline constexpr const char* kAllowControlLabel =
    "Viewers can control this machine (mouse and keyboard)";
inline constexpr const char* kRequestControlLabel =
    "Control the remote machine (untick to just watch)";
inline constexpr const char* kViewOnlyNote = "View-only: viewers can watch but not control.";
inline constexpr const char* kPickDisplaysHint = "Tick the displays to share, then press Share.";
inline constexpr const char* kPickSourcesHint =
    "Tick what to share \xE2\x80\x94 displays, the terminal, or both \xE2\x80\x94 then press "
    "Share.";
inline constexpr const char* kPortalConfirmNote =
    "The desktop confirms the screen capture in its own dialog the first time; that choice "
    "is remembered, so the dialog does not appear again.";
inline constexpr const char* kWaitingForShareDialog =
    "Waiting for the screen-sharing dialog\xE2\x80\xA6";
inline constexpr const char* kWaitingForDisplays =
    "No display yet \xE2\x80\x94 waiting for the desktop to finish starting before sharing"
    "\xE2\x80\xA6";
inline constexpr const char* kCaptureUnavailableTitle = "Screen capture is not available";
inline constexpr const char* kNoDisplayTicked = "Tick at least one display to share.";
inline constexpr const char* kStopSelectedDisplay = "Stop selected display";
inline constexpr const char* kDisconnectSelectedViewer = "Disconnect selected viewer";
inline constexpr const char* kStopDisplayAction = "Stop";
inline constexpr const char* kDisconnectViewerAction = "Disconnect";
inline constexpr const char* kPasscodeLabel = "Pairing passcode (4 digits, optional)";
inline constexpr const char* kPasscodeHint =
    "A machine types this once, the first time it connects, and is remembered by its key "
    "afterwards. Leave it empty and you will be asked here instead, each time a new machine "
    "wants in.";
inline constexpr const char* kClientPasscodePrompt = "Passcode (4 digits):";
inline constexpr const char* kClientPasscodeHint =
    "Read the 4-digit code off the host \xE2\x80\x94 or leave it empty to ask the person at "
    "that machine to let you in.";
inline constexpr const char* kDeviceNameLabel = "Your name";
inline constexpr const char* kConnectButton = "Connect";
inline constexpr const char* kCopyButton = "Copy";
inline constexpr const char* kFpsLabel = "FPS";
inline constexpr const char* kBitrateLabel = "Bitrate (Mbps)";
inline constexpr const char* kQualityLabel = "Quality";
inline constexpr const char* kNoDisplayFound = "No display found to share.";
inline constexpr const char* kClientIpPlaceholder = "192.168.1.10";
inline constexpr const char* kUdpPortLabel = "UDP port";
inline constexpr const char* kBindInterfaceLabel = "Share on network";
inline constexpr const char* kBindAllInterfaces = "All networks";
inline constexpr const char* kBindNotConnectedNote = "not connected";
inline constexpr const char* kSettingsSectionVideo = "Video";
inline constexpr const char* kSettingsSectionConnection = "Connection";
inline constexpr const char* kSettingsSectionSecurity = "Security";
inline constexpr const char* kSettingsSectionSession = "Session";
inline constexpr const char* kSettingsSectionLaunch = "Launch & background";
inline constexpr const char* kAutostartLabel = "Start Deskhub when you log in";
inline constexpr const char* kAutoShareLabel = "Start sharing when Deskhub opens";
inline constexpr const char* kClipboardSyncLabel =
    "Sync clipboard text with connected devices";
inline constexpr const char* kShareAudioLabel =
    "Share this device's sound with viewers";
inline constexpr const char* kPlayAudioLabel =
    "Play the sound of the device you are watching";
inline constexpr const char* kKeepAwakeLabel =
    "Keep this device awake while a session is active";
inline constexpr const char* kCloseToTrayLabel =
    "Keep running in the background (tray icon) when the window is closed";
inline constexpr const char* kTrayShowWindow = "Show Deskhub";
inline constexpr const char* kTrayHideWindow = "Hide window";
inline constexpr const char* kTrayQuit = "Quit Deskhub";
inline constexpr const char* kLanDevicesHeading = "Machines sharing on this network";
inline constexpr const char* kLanDevicesEmpty = "Looking for devices that are sharing\xE2\x80\xA6";
inline constexpr const char* kLanDevicesHint = "Click a device to connect to it.";
inline constexpr const char* kLanDevicesNoneSharing =
    "A machine appears here only while it is sharing \xE2\x80\x94 start the share on it, then "
    "check again.";
inline constexpr const char* kScanRescanNote = "Checking again shortly.";
inline constexpr const char* kRefreshNow = "Refresh now";
inline constexpr const char* kAuthWrongPasscode =
    "That passcode was not accepted \xE2\x80\x94 check the code on the machine you are "
    "connecting to.";
inline constexpr const char* kAuthNotPaired =
    "That machine does not recognise this one. It may have been forgotten there.";
inline constexpr const char* kAuthPairingDisabled =
    "That machine is not letting new machines pair with it right now.";
inline constexpr const char* kAuthRefused = "Somebody at that machine turned this one away.";
inline constexpr const char* kAuthTimedOut = "Nobody at that machine answered in time.";
inline constexpr const char* kAuthLocked =
    "Too many wrong passcodes \xE2\x80\x94 that machine is not taking attempts right now. "
    "Wait half a minute and try again.";

inline const char* AuthRefusalText(AuthResultCode code) {
    switch (code) {
        case AuthResultCode::WrongPasscode: return kAuthWrongPasscode;
        case AuthResultCode::PairingDisabled: return kAuthPairingDisabled;
        case AuthResultCode::Refused: return kAuthRefused;
        case AuthResultCode::TimedOut: return kAuthTimedOut;
        case AuthResultCode::Locked: return kAuthLocked;
        case AuthResultCode::Accepted: return "Connected.";
        case AuthResultCode::NotPaired: break;
    }
    return kAuthNotPaired;
}

inline constexpr const char* kPairingRequestTitle = "Let this machine in?";
inline constexpr const char* kPairingAllow = "Allow";
inline constexpr const char* kPairingDeny = "Deny";

inline std::string PairingRequestBody(std::string_view name, std::string_view address,
    std::string_view shortKey) {
    std::string out(name.empty() ? std::string("A machine") : std::string(name));
    out += " at ";
    out += address;
    out += " wants to connect to this one.\n\nIts key starts with ";
    out += shortKey;
    out +=
        "\n\nAllowing it pairs the two machines: it will be recognised by that key from now "
        "on and will not ask again. You can undo this on the Devices page.";
    return out;
}

inline constexpr const char* kSidebarDevices = "Devices";
inline constexpr const char* kPairedHeading = "Machines allowed to connect to this one";
inline constexpr const char* kPairedHint =
    "A machine gets on this list once, and after that it is recognised by its key \xE2\x80\x94 "
    "no passcode is asked for again.";
inline constexpr const char* kPairedEmpty = "(no machine has paired with this one yet)";
inline constexpr const char* kPairedForget = "Forget";
inline constexpr const char* kPairedForgetAll = "Forget every machine";
inline constexpr const char* kPairedForgetAllPrompt =
    "Every machine will have to pair again before it can connect. Continue?";
inline constexpr const char* kPairedForgetNote =
    "Changing the passcode does NOT turn these machines away \xE2\x80\x94 they no longer use it. "
    "Forgetting them is what does.";
inline constexpr const char* kAllowPairingLabel = "Let new machines pair with this one";
inline constexpr const char* kAllowPairingHint =
    "Turn this off once your own machines are paired: a passcode that leaks is then worth "
    "nothing, and the machines already on the list keep working.";
inline constexpr const char* kThisMachineHeading = "This machine's key";
inline constexpr const char* kThisMachineHint =
    "Read this out over the phone to whoever is connecting. It is the one thing a machine in "
    "the middle cannot fake.";
inline constexpr const char* kPairedColumnName = "Machine";
inline constexpr const char* kPairedColumnKey = "Key";
inline constexpr const char* kPairedColumnPaired = "Paired";
inline constexpr const char* kPairedColumnLastSeen = "Last seen";
inline constexpr const char* kDevicesHeading = "Devices";
inline constexpr const char* kDeviceColumnWhere = "Where";
inline constexpr const char* kDeviceOnThisNetwork = "On this network";
inline constexpr const char* kDeviceRecent = "Recent";
inline constexpr const char* kScanNoLocalNetwork =
    "This machine has no network address to scan from.";
inline constexpr const char* kConnectPromptTitle = "Connect to this device";
inline constexpr const char* kPasscodeInvalid =
    "The passcode must be exactly 4 digits (for example 0417).";

inline constexpr const char* kTerminalSourceName = "Terminal";
inline constexpr const char* kTerminalPickerLabel =
    "Terminal \xE2\x80\x94 a shell on this machine";
inline constexpr const char* kTerminalDetached = "(detached)";
inline constexpr const char* kTerminalLocalClient = "attached on this machine";
inline constexpr const char* kAttachShellAction = "Stop & attach";
inline constexpr const char* kTerminalLocalWindowTitle = "Terminal \xE2\x80\x94 this machine";
inline constexpr const char* kTerminalAttachedHere =
    "Attached to the shell on this machine.";
inline constexpr const char* kTerminalHostHeading = "Share this machine's terminal";
inline constexpr const char* kTerminalHostHint =
    "Anyone who knows the passcode gets a shell on this machine, running as you.";
inline constexpr const char* kTerminalShareButton = "Share terminal";
inline constexpr const char* kTerminalStopSharing = "Stop sharing the terminal";
inline constexpr const char* kTerminalSharingOff = "This machine is not sharing a terminal.";
inline constexpr const char* kTerminalNetworkLabel = "Share the terminal on:";
inline constexpr const char* kTerminalOpenSessionsHeading = "Shells open on this machine";
inline constexpr const char* kTerminalNoSessions = "(nobody has a shell open)";
inline constexpr const char* kShareNoQuicLibrary =
    "This build has no QUIC library, so it cannot share anything. Build one with "
    "scripts/build-quiche.sh, then build Deskhub again.";
inline constexpr const char* kShareNoHostIdentity =
    "This machine could not create the key it identifies itself with, so it cannot share.";
inline constexpr const char* kOpenChoiceGroup = "What to open on that machine";
inline constexpr const char* kConnectedPickSession =
    "Connected \xE2\x80\x94 choose what to open.";
inline constexpr const char* kConnectFirstHint =
    "Connect first \xE2\x80\x94 each button lights up only with what that machine shares.";
inline constexpr const char* kOpenDesktopLabel = "Remote desktop \xE2\x80\x94 view its screen";
inline constexpr const char* kOpenShellLabel = "Terminal \xE2\x80\x94 open a shell";
inline constexpr const char* kOpenNothingTicked =
    "Tick the remote desktop, a terminal, or both before connecting.";
inline constexpr const char* kOpenChoiceHint =
    "This applies to the Connect button and to the devices listed below.";
inline constexpr const char* kMobileHostNote =
    "A phone or tablet can only be watched: control and terminal do nothing on one.";
inline constexpr const char* kHostHasNoTerminal =
    "That machine is not sharing a terminal \xE2\x80\x94 a phone or tablet cannot.";
inline constexpr const char* kTerminalClientHeading = "Open a terminal on another machine";
inline constexpr const char* kTerminalClientHint =
    "This is separate from viewing a screen \xE2\x80\x94 you can do either, or both.";
inline constexpr const char* kTerminalConnecting = "Connecting\xE2\x80\xA6";
inline constexpr const char* kTerminalConnected = "Connected.";
inline constexpr const char* kTerminalClosed = "The shell has ended.";
inline constexpr const char* kTerminalWrongPasscode = "That passcode was not accepted.";
inline constexpr const char* kTerminalNotShared =
    "That machine is not sharing a terminal right now.";
inline constexpr const char* kTerminalTooManySessions =
    "That machine already has as many shells open as it allows.";
inline constexpr const char* kTerminalNoSuchSession =
    "The shell we were attached to is gone; open a new one.";
inline constexpr const char* kTerminalUnreachable = "Could not reach that machine.";
inline constexpr const char* kTerminalReattaching = "Connection lost \xE2\x80\x94 reattaching\xE2\x80\xA6";
inline constexpr const char* kTerminalReattached = "Reattached to the shell you had open.";

inline constexpr const char* kTransferConnecting = "Connecting\xE2\x80\xA6";
inline constexpr const char* kTransferSending = "Sending\xE2\x80\xA6";
inline constexpr const char* kTransferDone = "Every file arrived.";
inline constexpr const char* kTransferNotAccepting =
    "That machine is not taking files right now.";
inline constexpr const char* kTransferBusy = "That machine is already taking files from here.";
inline constexpr const char* kTransferTooManyFiles = "That is more files than one batch carries.";
inline constexpr const char* kTransferTooLarge = "That machine has no room for this much.";
inline constexpr const char* kTransferBadName = "One of those names cannot be stored.";
inline constexpr const char* kTransferWriteFailed = "That machine could not write the file.";
inline constexpr const char* kTransferCorrupt =
    "A file arrived damaged and was thrown away, so the batch stopped.";
inline constexpr const char* kTransferCancelled = "The transfer was stopped.";
inline constexpr const char* kTransferLinkLost = "The connection went before the files did.";
inline constexpr const char* kTransferReadFailed = "A file could not be read from this machine.";
inline constexpr const char* kTransferHostNotTaking =
    "That machine is not taking files \xE2\x80\x94 it was not started with file transfer on.";
inline constexpr const char* kFilesSourceName = "File transfer";
inline std::string NoDisplaySharedNote(bool terminal, bool files) {
    if (terminal && files) return "No display is shared - only the shell and file transfer.";
    if (files) return "No display is shared - only file transfer.";
    return "No display is shared - only the shell.";
}

inline constexpr const char* kOpenFilesLabel =
    "File transfer \xE2\x80\x94 send files to it";
inline constexpr const char* kFilesPickerLabel =
    "File transfer \xE2\x80\x94 files viewers send";
inline constexpr const char* kTransferHeading = "Files viewers send";
inline constexpr const char* kTransferSendHeading = "Send files to this machine";
inline constexpr const char* kTransferNoneChosen = "No file chosen yet.";
inline constexpr const char* kTransferSentHeading = "Sent from this window";
inline constexpr const char* kTransferBusyNote =
    "One batch at a time \xE2\x80\x94 wait for this one to finish.";
inline constexpr const char* kTransferChooseButton = "Choose files\xE2\x80\xA6";
inline constexpr const char* kTransferCancelButton = "Stop sending";
inline constexpr const char* kTransferAcceptLabel = "Take files viewers send";
inline constexpr const char* kTransferArrivedTitle = "Files received";
inline constexpr const char* kTransferStopTakingButton = "Stop taking files";
inline constexpr const char* kTransferFolderLabel = "Store them in";

inline std::string TransferFolderNote(std::string_view folder) {
    return "Files viewers send land in " + std::string(folder) + ".";
}

inline std::string TransferFolderUnusable(std::string_view folder) {
    return "Taking no files: they cannot be stored in " + std::string(folder) + ".";
}

inline const char* TransferReasonText(TransferReason reason) {
    switch (reason) {
        case TransferReason::Accepted: return kTransferDone;
        case TransferReason::NotAccepting: return kTransferNotAccepting;
        case TransferReason::Busy: return kTransferBusy;
        case TransferReason::TooManyFiles: return kTransferTooManyFiles;
        case TransferReason::TooLarge: return kTransferTooLarge;
        case TransferReason::BadName: return kTransferBadName;
        case TransferReason::WriteFailed: return kTransferWriteFailed;
        case TransferReason::Corrupt: return kTransferCorrupt;
        case TransferReason::Cancelled: return kTransferCancelled;
        case TransferReason::LinkLost: return kTransferLinkLost;
        case TransferReason::ReadFailed: return kTransferReadFailed;
    }
    return kTransferLinkLost;
}

inline std::string TransferProgressLine(std::string_view name, uint16_t index, uint16_t count,
    uint64_t bytes, uint64_t total) {
    std::string out = "[" + std::to_string(index + 1) + "/" + std::to_string(count) + "] ";
    out += name.empty() ? std::string("\xE2\x80\xA6") : std::string(name);
    if (total == 0) return out;
    out += "  " + std::to_string(bytes * 100 / total) + "%";
    return out;
}

inline constexpr const char* kTrustNewHostTitle = "Is this the right machine?";
inline constexpr const char* kTrustNewHostBody =
    "This is the first time this machine has been contacted. Check its fingerprint matches the "
    "one it shows, then decide whether to trust it.";
inline constexpr const char* kTrustChangedTitle = "This machine's key has changed";
inline constexpr const char* kTrustChangedBody =
    "The key does not match the one recorded the first time. Either the machine was reinstalled, "
    "or something is sitting between you and it. Do not continue unless you know why it changed.";
inline constexpr const char* kTrustFingerprintLabel = "Fingerprint:";
inline constexpr const char* kTrustAccept = "Trust this machine";
inline constexpr const char* kTrustReject = "Do not connect";
inline constexpr const char* kTrustedHostsHeading = "Machines you have trusted";
inline constexpr const char* kTrustedHostsEmpty = "(none yet)";
inline constexpr const char* kTrustForget = "Forget";

inline constexpr const char* kTerminalExtraKeysHint =
    "Ctrl and Alt latch: tap one, then a letter.";

inline const char* TerminalRefusalText(TermReason reason) {
    switch (reason) {
        case TermReason::WrongPasscode: return kTerminalWrongPasscode;
        case TermReason::TooManySessions: return kTerminalTooManySessions;
        case TermReason::NotShared: return kTerminalNotShared;
        case TermReason::NoSuchSession: return kTerminalNoSuchSession;
        case TermReason::Accepted: return kTerminalConnected;
    }
    return kTerminalUnreachable;
}

inline std::string TrimAscii(std::string_view s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(b, e - b + 1));
}

inline uint32_t ParsePositiveUint(std::string_view s, uint32_t fallback) {
    if (s.empty()) return fallback;
    uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return fallback;
        v = v * 10 + uint64_t(c - '0');
        if (v > 0xFFFFFFFFull) return fallback;
    }
    return v > 0 ? uint32_t(v) : fallback;
}

inline std::string ShareClampWarning() {
    const std::string cap = std::to_string(kMaxSources);
    return "This machine has more than " + cap + " displays. Only the first " + cap +
           " will be shared.";
}

inline std::string BindFallbackWarning(std::string_view requested) {
    return "Network " + std::string(requested) +
           " was not found - sharing on all networks instead.";
}

inline std::string ConnectingTo(std::string_view address) {
    return "Connecting to " + std::string(address) + "\xE2\x80\xA6";
}

inline std::string HostTitle(std::string_view address, uint32_t width, uint32_t height) {
    std::string title(address);
    if (!width || !height) return title;
    title += " \xE2\x80\x94 ";
    title += std::to_string(width);
    title += "\xC3\x97";
    title += std::to_string(height);
    return title;
}

inline bool SplitHostPort(std::string_view s, std::string& host, uint16_t& port) {
    const size_t colon = s.find(':');
    if (colon == std::string_view::npos) {
        if (s.empty()) return false;
        host = std::string(s);
        return true;
    }

    const std::string_view hostPart = s.substr(0, colon);
    const std::string_view portPart = s.substr(colon + 1);
    if (hostPart.empty() || portPart.empty()) return false;

    uint32_t value = 0;
    for (char c : portPart) {
        if (c < '0' || c > '9') return false;
        value = value * 10 + uint32_t(c - '0');
        if (value > 65535) return false;
    }
    if (value == 0) return false;

    host = std::string(hostPart);
    port = uint16_t(value);
    return true;
}

inline std::string VersionLine() {
    return std::string("Version ") + kAppVersion;
}

inline std::string UdpPortLine(uint16_t port) {
    return "UDP port " + std::to_string(port);
}

inline std::string UdpPortLine() {
    return UdpPortLine(kDeskhubPort);
}

inline std::string PortCell(uint16_t port) {
    return "port " + std::to_string(port);
}

inline std::string PingMs(uint32_t ms) {
    return std::to_string(ms) + " ms";
}

inline std::string LinkPingText(bool haveRtt, uint32_t rttUs) {
    if (!haveRtt) return kLinkNoReading;
    return PingMs((rttUs + 500) / 1000);
}

inline std::string SharingStatusLine(uint16_t port) {
    return "Sharing on UDP port " + std::to_string(port) +
           " - others can connect to this machine now.";
}

inline std::string ShareSummaryLine(bool screen, bool terminal, bool files, uint16_t port) {
    if (!screen && !terminal && !files) return {};
    std::string what;
    const auto add = [&what](const char* name) {
        if (!what.empty()) what += " \xC2\xB7 ";
        what += name;
    };
    if (screen) add("Screen");
    if (terminal) add("Terminal");
    if (files) add("Files");
    return what + " on UDP port " + std::to_string(port) + ".";
}

inline std::string ShareSummaryLine(bool screen, bool terminal, uint16_t port) {
    return ShareSummaryLine(screen, terminal, false, port);
}

inline std::string PasscodeNote(std::string_view passcode) {
    if (passcode.empty())
        return "No passcode set \xE2\x80\x94 you will be asked here before a new machine is "
               "let in.";
    return "A machine pairing for the first time needs passcode " + std::string(passcode) +
           " \xE2\x80\x94 or your approval here if it offers none.";
}

inline std::string CouldNotConnectTo(std::string_view address) {
    return "Could not connect to " + std::string(address) + ".";
}

inline std::string SourceQueryFailed(std::string_view address) {
    return "No reply from " + std::string(address) +
           " - check that the other machine is sharing and that the passcode matches.";
}

inline std::string SourceQueryEmpty(std::string_view address) {
    return std::string(address) +
           " replied without any sources - check the 4-digit passcode on the host, and that it "
           "is still sharing.";
}

inline std::string ScanningStatus(size_t probed, size_t total, uint16_t port) {
    return "Looking for hosts on " + UdpPortLine(port) + " - " + std::to_string(probed) + " of " +
           std::to_string(total) + " addresses checked" + "\xE2\x80\xA6";
}

inline std::string ScanRecheckNote(uint32_t seconds) {
    return "Checking again in " + std::to_string(seconds) + "s.";
}

inline std::string StatusRecheckNote(uint32_t seconds) {
    return "Status and ping recheck every " + std::to_string(seconds) + "s.";
}

inline std::string ScanFinishedStatus(size_t found, size_t total) {
    return std::to_string(found) + (found == 1 ? " device" : " devices") + " found after checking " +
           std::to_string(total) + " addresses.";
}

inline std::string LanDevicesNote(size_t found, size_t total, uint32_t rescanSecs) {
    if (total == 0) return kScanNoLocalNetwork;
    const char* detail = found > 0 ? kLanDevicesHint : kLanDevicesNoneSharing;
    return ScanFinishedStatus(found, total) + " " + detail + " " + ScanRecheckNote(rescanSecs);
}

inline std::string RecentDevicesNote(size_t deviceCount, uint32_t recheckSecs) {
    if (deviceCount == 0) return kRecentDevicesEmpty;
    return std::string(kRecentDevicesHint) + " " + StatusRecheckNote(recheckSecs);
}

inline uint16_t PortOrDefault(std::string_view typed, uint16_t fallback = kDeskhubPort) {
    const std::string trimmed = TrimAscii(typed);
    if (trimmed.empty() || trimmed.size() > 5) return fallback;
    uint32_t value = 0;
    for (char c : trimmed) {
        if (c < '0' || c > '9') return fallback;
        value = value * 10 + uint32_t(c - '0');
    }
    if (value < 1 || value > 65535) return fallback;
    return uint16_t(value);
}

inline std::string AddressWithPort(std::string_view typed, uint16_t port) {
    std::string trimmed = TrimAscii(typed);
    if (trimmed.empty() || trimmed.find(':') != std::string::npos) return trimmed;
    return trimmed + ":" + std::to_string(port);
}

inline std::string AddressHost(std::string_view address) {
    std::string trimmed = TrimAscii(address);
    std::string host;
    uint16_t port = 0;
    if (SplitHostPort(trimmed, host, port)) return host;
    return trimmed;
}

inline uint16_t AddressPort(std::string_view address) {
    std::string host;
    uint16_t port = 0;
    SplitHostPort(TrimAscii(address), host, port);
    return port;
}

inline std::string NormalizedDeviceAddr(std::string_view address) {
    const std::string trimmed = TrimAscii(address);
    if (trimmed.empty()) return {};
    const uint16_t port = AddressPort(trimmed);
    return AddressHost(trimmed) + ":" + std::to_string(port != 0 ? port : kDeskhubPort);
}

inline bool SameDeviceAddr(std::string_view left, std::string_view right) {
    const std::string wanted = NormalizedDeviceAddr(left);
    return !wanted.empty() && wanted == NormalizedDeviceAddr(right);
}

inline std::string InvalidAddressLine(std::string_view address) {
    return "Invalid address: \"" + std::string(address) + "\".";
}

inline std::string InvalidAddressHint() {
    const std::string port = std::to_string(kDeskhubPort);
    return "Enter the host's IP address, with an optional port (e.g., 192.168.1.10 or "
           "192.168.1.10:" +
           port + "). The default UDP port is " + port + ".";
}

}

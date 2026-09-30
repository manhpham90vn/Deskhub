#pragma once
#include "deskhub/protocol/Wire.h"

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
inline constexpr const char* kSharingTitle = "Deskhub - sharing";
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
inline constexpr const char* kScreenRecordingRequired =
    "Screen Recording permission is required. Grant it in System Settings, then quit and "
    "reopen Deskhub.";
inline constexpr const char* kSidebarHost = "Host";
inline constexpr const char* kSidebarClient = "Client";
inline constexpr const char* kSidebarSettings = "Settings";
inline constexpr const char* kHostHeading = "Share this machine's screen";
inline constexpr const char* kClientHeading = "Connect to another machine";
inline constexpr const char* kSettingsHint = "These apply the next time you start sharing.";
inline constexpr const char* kClientSettingsHeading = "Connection settings";
inline constexpr const char* kClientSettingsHint =
    "The device scan looks for sharing machines on this UDP port. Match it to the port in the "
    "host's Share settings.";
inline constexpr const char* kNotSharing = "Not sharing.";
inline constexpr const char* kStartingShare = "Starting share...";
inline constexpr const char* kShareStateOn = "Sharing";
inline constexpr const char* kShareStateOff = "Not sharing";
inline constexpr const char* kReceivingFilesState = "Receiving files";
inline constexpr const char* kStartSharing = "Start sharing";
inline constexpr const char* kBroadcastMemoryLabel = "Broadcast memory";
inline constexpr const char* kAllowControlLabel =
    "Viewers can control this machine (mouse and keyboard)";
inline constexpr const char* kRequestControlLabel =
    "Control the remote machine (untick to just watch)";
inline constexpr const char* kViewOnlyNote = "View-only: viewers can watch but not control.";
inline constexpr const char* kPickSourcesHint =
    "Tick what to share \xE2\x80\x94 displays, the terminal, file transfer \xE2\x80\x94 then "
    "press Share. Nothing is shared until you press it.";
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
inline constexpr const char* kStopDisplayAction = "Stop";
inline constexpr const char* kDisconnectViewerAction = "Disconnect";
inline constexpr const char* kDeviceNameLabel = "Device name";
inline constexpr const char* kDeviceNameHint =
    "The one name this device goes by: hosts see it when you connect, devices that connect "
    "here see it on this machine, and it labels the public keys you copy.";
inline constexpr const char* kConnectButton = "Connect";
inline constexpr const char* kCopyButton = "Copy";
inline constexpr const char* kCopiedButton = "Copied";
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
inline constexpr const char* kSettingsGeneralArea = "General";
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
inline constexpr const char* kLanDevicesEmpty = "Saved devices appear here after a connection.";
inline constexpr const char* kAuthNotPaired =
    "That machine has not allowed this device yet. Connect again so its owner sees the "
    "request under Connection requests and presses Approve, scan the QR code it shows, or "
    "send it this device's public key (Devices > Copy public key) to paste under Clients "
    "allowed to connect.";
inline constexpr const char* kAuthNotPairedCliHint =
    "From the command line: the host approves with `deskhub-cli access approve --fingerprint "
    "SHA256:...`, or run `deskhub-cli key public` here and pipe that line into "
    "`deskhub-cli access add --stdin` on the other machine.";
inline constexpr const char* kAuthAwaitingApproval =
    "The owner of that machine did not approve this device in time. Ask them to press "
    "Approve under Connection requests on their Host page, then connect again.";
inline constexpr const char* kAuthRefused =
    "That machine refused this connection. Ask its owner to check that this device's public "
    "key is still listed under Devices > Clients allowed to connect, then connect again.";
inline constexpr const char* kAuthTimedOut =
    "The other machine did not finish signing in in time. Check that it is still sharing "
    "and reachable, then connect again.";
inline constexpr const char* kAuthVersionMismatch =
    "That machine uses an incompatible authentication version. Update Deskhub on both machines.";
inline constexpr const char* kAuthBadSignature =
    "That machine could not verify this device's key. Connect again; if it keeps happening, "
    "remove this device's key on that machine and add its current public key again.";
inline constexpr const char* kAuthConfigError =
    "A Deskhub key list cannot be read. On the machine that shows this, open Devices and "
    "add the allowed clients or trusted hosts again - the damaged list is replaced on the "
    "next change.";
inline constexpr const char* kAuthUntrustedHost =
    "This host is not trusted yet. Check its fingerprint and trust it, or add its host key "
    "first.";
inline constexpr const char* kAuthLocalKeyUnavailable =
    "This device's key cannot be loaded. Check that Deskhub can read and write its data "
    "folder, then try again.";
inline constexpr const char* kInviteHostMismatch =
    "The machine that answered is not the one that made this QR code. Show the code again "
    "on the host and scan it once more.";
inline constexpr const char* kInviteInvalid =
    "That is not a Deskhub QR code or invite link.";

inline const char* AuthRefusalText(AuthResultCode code) {
    switch (code) {
        case AuthResultCode::Refused: return kAuthRefused;
        case AuthResultCode::TimedOut: return kAuthTimedOut;
        case AuthResultCode::VersionMismatch: return kAuthVersionMismatch;
        case AuthResultCode::BadSignature: return kAuthBadSignature;
        case AuthResultCode::ConfigError: return kAuthConfigError;
        case AuthResultCode::UntrustedHost: return kAuthUntrustedHost;
        case AuthResultCode::LocalKeyUnavailable: return kAuthLocalKeyUnavailable;
        case AuthResultCode::AwaitingApproval: return kAuthAwaitingApproval;
        case AuthResultCode::Accepted: return "Connected.";
        case AuthResultCode::NotPaired: break;
    }
    return kAuthNotPaired;
}

inline constexpr const char* kSidebarDevices = "Devices";
inline constexpr const char* kPairedHeading =
    "Devices allowed to connect to this machine";
inline constexpr const char* kPairedHint =
    "Only the devices listed here get in. A device joins the list when you approve its "
    "connection request, when it scans this machine's QR code, or when you paste its public "
    "key below.";
inline constexpr const char* kPairedEmpty =
    "No device can connect to this machine yet. Show the QR code while sharing, approve a "
    "connection request, or paste a device's public key above and press Allow.";
inline constexpr const char* kPairedForget =
    "Remove";
inline constexpr const char* kPairedForgetAll =
    "Remove every client";
inline constexpr const char* kPairedForgetAllPrompt =
    "All client keys will lose access. You can add them again later. Continue?";
inline constexpr const char* kPairedForgetNote =
    "Removing a key disconnects that device at once.";
inline constexpr const char* kThisMachineHeading = "This machine";
inline constexpr const char* kThisMachineHint =
    "Other machines know this one by this fingerprint, whether it hosts or connects. A device "
    "connecting here for the first time is shown it; ask its user to compare before they "
    "trust this machine. Copy public key gives the line another host's owner can paste to "
    "allow this machine.";
inline constexpr const char* kPairedColumnName =
    "Client";
inline constexpr const char* kPairedColumnKey = "Key";
inline constexpr const char* kDevicesHeading = "Devices";
inline constexpr const char* kSavedHostsHeading =
    "Trusted hosts";
inline constexpr const char* kSavedHostsHint =
    "A host is added here when you confirm its fingerprint on the first connection or scan "
    "its QR code. Trust follows the host's key, so it stays trusted when its address "
    "changes; the address shown is the last one it answered at.";
inline constexpr const char* kSavedHostsEmpty =
    "(no trusted hosts yet)";
inline constexpr const char* kHostNameLabel = "Name";
inline constexpr const char* kHostAddressLabel = "Address";
inline constexpr const char* kHostKeyLabel = "Host key";
inline constexpr const char* kHostLastAddressLabel = "Last address";
inline constexpr const char* kRemoveHostAction = "Remove";
inline constexpr const char* kAllowClientPlaceholder = "ssh-ed25519 AAAA\xE2\x80\xA6 laptop";
inline constexpr const char* kAllowClientAction = "Allow";
inline constexpr const char* kAllowClientInvalid =
    "That is not a supported Ed25519 or ECDSA P-256 public key, or it is already allowed.";
inline constexpr const char* kCopyPublicKeyAction = "Copy public key";
inline constexpr const char* kShowQrAction = "Show QR code";
inline constexpr const char* kHideQrAction = "Hide QR code";
inline constexpr const char* kScanQrAction = "Scan QR code";
inline constexpr const char* kQrHint =
    "Scan this with Deskhub on a phone or tablet, or paste the link into another Deskhub's "
    "address field. It lets one device in and stops working after five minutes or when it "
    "is hidden.";
inline constexpr const char* kQrUnavailable =
    "Could not make a QR code: this machine has no identity key or no network address.";
inline constexpr const char* kUnnamedClient = "(unnamed)";
inline constexpr const char* kAccessRequestsHeading = "Connection requests";
inline constexpr const char* kAccessRequestsEmpty =
    "No device is waiting. A device that connects without being allowed appears here for "
    "ten minutes.";
inline constexpr const char* kAccessRequestNotificationTitle = "Connection request";
inline constexpr const char* kApproveAction = "Approve";
inline constexpr const char* kDenyAction = "Deny";
inline constexpr const char* kCameraDenied =
    "Deskhub cannot use the camera. Allow camera access in the system settings, or paste "
    "the invite link into the address field instead.";
inline constexpr const char* kTrustNewHostTitle = "New host";
inline constexpr const char* kTrustNewHostAction = "Trust and connect";
inline constexpr const char* kCancelAction = "Cancel";

inline std::string NewHostKeyCliHint(std::string_view fingerprint) {
    return "Host key fingerprint: " + std::string(fingerprint) +
           "\nCompare it with the fingerprint on that machine's Devices page, then rerun with "
           "--accept-new-host-key.";
}

inline std::string PreviousOwnerWarning(std::string_view address, std::string_view label,
    std::string_view fingerprint) {
    if (fingerprint.empty()) return {};
    std::string owner = label.empty() ? std::string(fingerprint)
                                      : std::string(label) + " (" + std::string(fingerprint) + ")";
    return "Careful: " + std::string(address) + " used to belong to " + owner +
           ", which stays in Trusted hosts. This is a different machine.";
}

inline std::string TrustNewHostPrompt(std::string_view address, std::string_view fingerprint,
    std::string_view previousOwnerWarning = {}) {
    std::string prompt = "This device has not connected to " + std::string(address) +
                         " before. Its host key fingerprint is:\n\n" + std::string(fingerprint) +
                         "\n\nCompare it with the fingerprint shown on that machine's Devices "
                         "page. Trust it only if they match.";
    if (!previousOwnerWarning.empty()) prompt += "\n\n" + std::string(previousOwnerWarning);
    return prompt;
}

inline std::string AwaitingApprovalLine(std::string_view host) {
    return "Waiting for the owner of " + std::string(host) +
           " to approve this device\xE2\x80\xA6 They see it under Connection requests on their "
           "Host page.";
}

inline std::string AccessRequestNotificationBody(std::string_view name, std::string_view address) {
    const std::string who = name.empty() ? std::string(kUnnamedClient) : std::string(name);
    return who + " (" + std::string(address) +
           ") wants to connect to this machine. Approve or deny it on the Devices page.";
}

inline std::string AccessRequestCliLine(std::string_view name, std::string_view fingerprint,
    std::string_view address) {
    const std::string who = name.empty() ? std::string("An unnamed device") : std::string(name);
    return who + " (" + std::string(fingerprint) + ", " + std::string(address) +
           ") wants to connect. Approve with: deskhub-cli access approve --fingerprint " +
           std::string(fingerprint);
}

inline constexpr const char* kTerminalSourceName = "Terminal";
inline constexpr const char* kTerminalPickerLabel =
    "Terminal \xE2\x80\x94 a shell on this machine";
inline constexpr const char* kTerminalDetached = "(detached)";
inline constexpr const char* kTerminalLocalClient = "attached on this machine";
inline constexpr const char* kAttachShellAction = "Stop & attach";
inline constexpr const char* kTerminalLocalWindowTitle = "Terminal \xE2\x80\x94 this machine";
inline constexpr const char* kTerminalAttachedHere =
    "Attached to the shell on this machine.";
inline constexpr const char* kShareNoQuicLibrary =
    "This build has no QUIC library, so it cannot share anything. Build one with "
    "scripts/build-quiche.sh, then build Deskhub again.";
inline constexpr const char* kShareNoHostIdentity =
    "This machine could not create the key it identifies itself with, so it cannot share.";
inline constexpr const char* kConnectedPickSession =
    "Connected \xE2\x80\x94 choose what to open.";
inline constexpr const char* kOpenDesktopLabel = "Remote desktop \xE2\x80\x94 view its screen";
inline constexpr const char* kOpenShellLabel = "Terminal \xE2\x80\x94 open a shell";
inline constexpr const char* kMobileHostNote =
    "A phone or tablet can only be watched: control and terminal do nothing on one.";
inline constexpr const char* kTerminalConnecting = "Connecting\xE2\x80\xA6";
inline constexpr const char* kTerminalConnected = "Connected.";
inline constexpr const char* kTerminalClosed = "The shell has ended.";
inline constexpr const char* kTerminalNotShared =
    "That machine is not sharing a terminal right now.";
inline constexpr const char* kTerminalTooManySessions =
    "That machine already has as many shells open as it allows.";
inline constexpr const char* kTerminalNoSuchSession =
    "The shell we were attached to is gone; open a new one.";
inline constexpr const char* kTerminalUnreachable = "Could not reach that machine.";
inline constexpr const char* kTerminalReattaching = "Connection lost \xE2\x80\x94 reattaching\xE2\x80\xA6";
inline constexpr const char* kTerminalReattached = "Reattached to the shell you had open.";
inline constexpr const char* kTerminalPickSession =
    "Pick a shell to reattach, or open a new one.";
inline constexpr const char* kShellPickerTitle = "Shells this machine is keeping";
inline constexpr const char* kShellPickerEmpty = "That machine is keeping no shells.";
inline constexpr const char* kShellPickerResume = "Reattach";
inline constexpr const char* kShellPickerNew = "New shell";
inline constexpr const char* kShellPickerClose = "Close shell";
inline constexpr const char* kShellPickerCloseAsk =
    "Close that shell? Whatever it is running ends with it.";
inline constexpr const char* kTerminalInUse = "(in use)";
inline constexpr const char* kTerminalLocalElsewhere = "attached on that machine";
inline constexpr const char* kTerminalUnnamedClient = "an unnamed machine";

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
inline constexpr const char* kTransferSendHeading = "Send files to this machine";
inline constexpr const char* kTransferNoneChosen = "No file chosen yet.";
inline constexpr const char* kTransferSentHeading = "Sent from this window";
inline constexpr const char* kTransferChooseButton = "Choose files\xE2\x80\xA6";
inline constexpr const char* kOpenFolderAction = "Open folder";
inline constexpr const char* kTransferCancelButton = "Stop sending";
inline constexpr const char* kMobileTakesFilesNote =
    "Always on: files viewers send are taken even when the screen is not shared.";
inline constexpr const char* kTransferArrivedTitle = "Files received";
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

inline constexpr const char* kTerminalExtraKeysHint =
    "Ctrl and Alt latch: tap one, then a letter.";

inline const char* TerminalRefusalText(TermReason reason) {
    switch (reason) {
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

inline std::string CouldNotConnectTo(std::string_view address) {
    return "Could not connect to " + std::string(address) + ".";
}

inline std::string SourceQueryFailed(std::string_view address) {
    return "No reply from " + std::string(address) +
           " - check that the other machine is sharing and has authorized this device's key.";
}

inline std::string SourceQueryEmpty(std::string_view address) {
    return std::string(address) +
           " replied without any sources - it is not sharing a screen right now.";
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

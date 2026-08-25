#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/Strings.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace deskhub;

namespace {

bool Contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

void TestEveryLabelSaysSomething() {
    std::printf("[strings] no screen can end up showing an empty label...\n");
    const char* const labels[] = {ui::kAppTitle, ui::kHostIpIntro, ui::kNoNetworkAddress,
        ui::kClientIpPrompt, ui::kPickerTitle, ui::kPickerEachWindow, ui::kShareButton,
        ui::kSharingTitle, ui::kSharingSourcesIntro, ui::kSharingConnectHint, ui::kNothingShared,
        ui::kStopSharing, ui::kShareStartFailed, ui::kQueryingSources, ui::kViewerOpenFailed,
        ui::kConnectionEndedTitle, ui::kDisconnected, ui::kSessionEnded, ui::kSidebarHost,
        ui::kSidebarClient, ui::kSidebarSettings, ui::kHostHeading, ui::kClientHeading,
        ui::kSettingsHeading, ui::kSettingsHint, ui::kClientSettingsHeading,
        ui::kClientSettingsHint, ui::kRecentDevicesHeading,
        ui::kRecentDevicesHint, ui::kRecentDevicesEmpty, ui::kStatusOnline, ui::kStatusOffline,
        ui::kStatusChecking, ui::kNotSharing, ui::kStartingShare, ui::kShareStateOn,
        ui::kShareStateOff, ui::kStartSharing, ui::kPickDisplaysHint,
        ui::kNoDisplayTicked, ui::kNoDisplayFound, ui::kWaitingForDisplays,
        ui::kStopSelectedDisplay, ui::kDisconnectSelectedViewer,
        ui::kStopDisplayAction, ui::kDisconnectViewerAction,
        ui::kAllowControlLabel, ui::kViewOnlyNote, ui::kRequestControlLabel,
        ui::kPasscodeLabel, ui::kClientPasscodePrompt, ui::kClientPasscodeHint,
        ui::kClientIpPlaceholder, ui::kUdpPortLabel, ui::kPasscodeInvalid, ui::kLanDevicesHeading,
        ui::kLanDevicesEmpty, ui::kLanDevicesHint, ui::kLanDevicesNoneSharing,
        ui::kScanRescanNote, ui::kScanNoLocalNetwork,
        ui::kConnectPromptTitle, ui::kAppVersion, ui::kProjectUrl, ui::kProjectLinkLabel,
        ui::kRefreshNow, ui::kBindInterfaceLabel, ui::kBindAllInterfaces,
        ui::kBindNotConnectedNote, ui::kAutostartLabel,
        ui::kAutoShareLabel, ui::kClipboardSyncLabel, ui::kCloseToTrayLabel,
        ui::kTrayShowWindow, ui::kTrayHideWindow, ui::kTrayQuit,
        ui::kSettingsSectionVideo, ui::kSettingsSectionConnection, ui::kSettingsSectionSecurity,
        ui::kSettingsSectionSession, ui::kSettingsSectionLaunch,
        ui::kPairingRequestTitle, ui::kPairingAllow, ui::kPairingDeny, ui::kAuthLocked};
    for (const char* s : labels) Check(s && *s, "every shared UI string is non-empty");
    Check(Contains(ui::PasscodeNote("0417"), "0417"),
        "the sharing status quotes the passcode viewers must enter");
    Check(Contains(ui::PasscodeNote("0417"), "approval"),
        "and says a machine offering no code falls back to the owner's approval");
    Check(Contains(ui::PasscodeNote(""), "asked here"),
        "with no code set, it promises the owner will be asked instead");
    Check(Contains(ui::kClientPasscodeHint, "empty"),
        "the client hint says the field may be left empty");
}

void TestBindFallbackNamesTheMissingNetwork() {
    std::printf("[strings] the bind fallback warning names the address that vanished...\n");
    Check(Contains(ui::BindFallbackWarning("192.168.1.10"), "192.168.1.10"),
        "the user can see which network was not found");
    Check(Contains(ui::BindFallbackWarning("192.168.1.10"), "all networks"),
        "and learns that sharing fell back to every network");
}

void TestConnectingMentionsTheAddress() {
    std::printf("[strings] the connecting line repeats the address the user typed...\n");
    Check(Contains(ui::ConnectingTo("192.168.1.10"), "192.168.1.10"),
        "the user can see which machine is being dialled");
    Check(Contains(ui::ConnectingTo(""), "Connecting to"),
        "an empty address still produces a sentence, not a crash");
}

void TestQueryFailureExplainsWhatToCheck() {
    std::printf("[strings] the failed pre-connect check names the host and both causes...\n");
    const std::string msg = ui::SourceQueryFailed("192.168.1.10:47777");
    Check(Contains(msg, "192.168.1.10:47777"), "the user can see which machine stayed silent");
    Check(Contains(msg, "sharing"), "a host that is not sharing is the most likely cause");
    Check(Contains(msg, "passcode"), "a wrong passcode fails the exact same way");
    Check(Contains(ui::SourceQueryFailed(""), "No reply"),
        "an empty address still produces a sentence, not a crash");

    const std::string empty = ui::SourceQueryEmpty("192.168.1.10:47777");
    Check(Contains(empty, "192.168.1.10:47777"), "the user can see which machine held back");
    Check(Contains(empty, "passcode"),
        "a host that answers with nothing is guarding against a wrong passcode");
    Check(Contains(empty, "sharing"), "or it simply stopped sharing in the meantime");
}

void TestHostTitleOnlyShowsAKnownSize() {
    std::printf("[strings] the host title shows a size only once one is known...\n");
    Check(ui::HostTitle("192.168.1.10", 0, 0) == "192.168.1.10",
        "no size yet: the title is just the address, with no trailing separator");
    Check(ui::HostTitle("192.168.1.10", 1920, 0) == "192.168.1.10",
        "a half-known size counts as unknown");

    const std::string full = ui::HostTitle("192.168.1.10", 1920, 1080);
    Check(Contains(full, "192.168.1.10"), "the address stays in the title");
    Check(Contains(full, "1920") && Contains(full, "1080"), "both dimensions are shown");
}

void TestThePortIsNeverHardcodedTwice() {
    std::printf("[strings] the port the user is told about is the port we listen on...\n");
    const std::string port = std::to_string(kDeskhubPort);
    Check(Contains(ui::UdpPortLine(), port), "the port line quotes the protocol constant");
    Check(Contains(ui::UdpPortLine(50123), "50123"), "a custom port shows as configured");
    Check(ui::ShareSummaryLine(true, false, 50123) == "Screen on UDP port 50123.",
        "the banner names what is shared and where");
    Check(ui::ShareSummaryLine(true, true, 50123) == "Screen \xC2\xB7 Terminal on UDP port 50123.",
        "screen and terminal share the one port on one line");
    Check(ui::ShareSummaryLine(false, true, 50123) == "Terminal on UDP port 50123.",
        "a terminal-only share reads the same way");
    Check(ui::ShareSummaryLine(false, false, 50123).empty(),
        "and nothing shared is nothing said");
    Check(Contains(ui::SharingStatusLine(50123), "50123"),
        "the sharing banner quotes the port actually bound");
    Check(Contains(ui::InvalidAddressHint(), port), "so does the bad-address hint");
    Check(Contains(ui::InvalidAddressHint(), "192.168.1.10"),
        "the hint shows an example, so the user knows what shape to type");
}

void TestSplitHostPortHandlesEveryShape() {
    std::printf("[strings] host:port splitting honours the port and rejects junk...\n");
    std::string host;
    uint16_t port = kDeskhubPort;

    Check(ui::SplitHostPort("192.168.1.10", host, port) && host == "192.168.1.10" &&
              port == kDeskhubPort,
        "a bare host keeps the caller's default port");
    Check(ui::SplitHostPort("192.168.1.10:5000", host, port) && host == "192.168.1.10" &&
              port == 5000,
        "an explicit port is split out");
    Check(ui::SplitHostPort("h:1", host, port) && port == 1, "the lowest port is allowed");
    Check(ui::SplitHostPort("h:65535", host, port) && port == 65535, "so is the highest");

    const char* bad[] = {"", ":5000", "host:", "host:0", "host:65536", "host:1:2", "host:abc"};
    for (const char* s : bad) {
        Check(!ui::SplitHostPort(s, host, port), "malformed input is refused outright");
    }
}

void TestThePortFieldFallsBackToTheDefault() {
    std::printf("[strings] the port field accepts a real port and falls back otherwise...\n");
    Check(ui::PortOrDefault("50123") == 50123, "a plain port parses");
    Check(ui::PortOrDefault(" 50123 ") == 50123, "surrounding whitespace is forgiven");
    Check(ui::PortOrDefault("1") == 1 && ui::PortOrDefault("65535") == 65535,
        "both ends of the range are allowed");
    const char* bad[] = {"", "0", "65536", "999999", "47a77", "-1", " "};
    for (const char* s : bad) {
        Check(ui::PortOrDefault(s) == kDeskhubPort, "junk falls back to the default port");
        Check(ui::PortOrDefault(s, 50124) == 50124,
            "and to the caller's own fallback when it names one");
    }
}

void TestTheAddressAndPortFieldsComposeOneAddress() {
    std::printf("[strings] a bare host gains the field's port, an explicit one keeps its own...\n");
    Check(ui::AddressWithPort("192.168.1.10", 50123) == "192.168.1.10:50123",
        "the port field is appended to a bare host");
    Check(ui::AddressWithPort(" 192.168.1.10 ", 50123) == "192.168.1.10:50123",
        "the host is trimmed before composing");
    Check(ui::AddressWithPort("192.168.1.10:5000", 50123) == "192.168.1.10:5000",
        "a pasted host:port keeps its explicit port");
    Check(ui::AddressWithPort("", 50123).empty(), "an empty host stays empty for the caller");

    Check(ui::AddressHost("192.168.1.10:5000") == "192.168.1.10" &&
              ui::AddressPort("192.168.1.10:5000") == 5000,
        "a stored address splits back into the two fields");
    Check(ui::AddressHost("192.168.1.10") == "192.168.1.10" &&
              ui::AddressPort("192.168.1.10") == 0,
        "a bare host reports no port so the caller can show the default");
    Check(ui::AddressHost("host:bad") == "host:bad" && ui::AddressPort("host:bad") == 0,
        "a malformed address is left whole for the error path to quote");
}

void TestTrimStripsOnlyTheEdges() {
    std::printf("[strings] trimming eats surrounding whitespace and nothing else...\n");
    Check(ui::TrimAscii("  192.168.1.10\r\n") == "192.168.1.10", "edges go");
    Check(ui::TrimAscii("a b") == "a b", "inner spaces stay");
    Check(ui::TrimAscii(" \t\r\n").empty(), "all-whitespace collapses to empty");
    Check(ui::TrimAscii("").empty(), "empty stays empty");
}

void TestParsePositiveUintIsStrict() {
    std::printf("[strings] numeric entry fields fall back instead of guessing...\n");
    Check(ui::ParsePositiveUint("60", 30) == 60, "a plain number parses");
    Check(ui::ParsePositiveUint("0", 30) == 30, "zero is not a usable fps/bitrate");
    Check(ui::ParsePositiveUint("", 30) == 30, "empty falls back");
    Check(ui::ParsePositiveUint("6x", 30) == 30, "trailing junk falls back, not truncates");
    Check(ui::ParsePositiveUint("99999999999", 30) == 30, "overflow falls back");
}

void TestPingLabelQuotesTheMeasurement() {
    std::printf("[strings] the ping label shows the measured number with its unit...\n");
    Check(ui::PingMs(12) == "12 ms", "a normal rtt renders as-is");
    Check(ui::PingMs(0) == "0 ms", "zero still renders a full label");
}

void TestTheAboutLineNamesTheBuild() {
    std::printf("[strings] the sidebar shows which build this is and where it lives...\n");
    Check(Contains(ui::VersionLine(), ui::kAppVersion),
        "the version line quotes the version the build was stamped with");
    Check(std::string(ui::kProjectUrl).rfind("https://github.com/", 0) == 0,
        "the project link points at GitHub over https");
}

void TestScanStatusCountsWhatWasChecked() {
    std::printf("[strings] the scan status says how far it got and on which port...\n");
    const std::string running = ui::ScanningStatus(64, 253, 50123);
    Check(Contains(running, "64") && Contains(running, "253"),
        "the user can see the sweep is progressing");
    Check(Contains(running, "50123"), "and which port is being knocked on");
    Check(Contains(ui::ScanFinishedStatus(1, 253), "1 device"),
        "a single hit is not reported as devices");
    Check(Contains(ui::ScanFinishedStatus(0, 253), "0 devices"),
        "an empty network still reports how much was checked");
}

void TestRecheckNotesQuoteTheRealInterval() {
    std::printf("[strings] the user is told how often a list refreshes itself...\n");
    Check(Contains(ui::ScanRecheckNote(45), "45"), "the rescan note quotes the caller's delay");
    Check(Contains(ui::StatusRecheckNote(30), "30"),
        "the status note quotes the poller's own round gap");
    Check(!Contains(ui::ScanRecheckNote(45), "shortly"),
        "no vague wording survives where a number is available");
}

void TestDeviceListNotesReadTheSameOnEveryClient() {
    std::printf("[strings] the two device lists explain themselves the same way everywhere...\n");
    Check(ui::LanDevicesNote(0, 0, 45) == ui::kScanNoLocalNetwork,
        "a machine with no local network says so instead of reporting 0 of 0");

    const std::string empty = ui::LanDevicesNote(0, 253, 45);
    Check(Contains(empty, "0 devices") && Contains(empty, "45"),
        "an empty sweep still says how much was checked and when it retries");
    Check(!Contains(empty, ui::kLanDevicesHint),
        "there is nothing to click, so no click hint is offered");
    Check(Contains(empty, ui::kLanDevicesNoneSharing),
        "an empty sweep says why a machine would be missing, not just that none was found");

    const std::string found = ui::LanDevicesNote(2, 253, 45);
    Check(Contains(found, ui::kLanDevicesHint) && Contains(found, "45"),
        "once devices are listed the user is told to click one, and when the list refreshes");
    Check(!Contains(found, ui::kLanDevicesNoneSharing),
        "the missing-machine explanation is dropped once the list has something in it");

    Check(ui::RecentDevicesNote(0, 30) == ui::kRecentDevicesEmpty,
        "an empty history explains what will fill it");
    const std::string recent = ui::RecentDevicesNote(3, 30);
    Check(Contains(recent, ui::kRecentDevicesHint) && Contains(recent, "30"),
        "a populated history says it is clickable and how often status is rechecked");
}

void TestTerminalRefusalsNameTheirOwnCause() {
    std::printf("[strings] a share that cannot start says which of the two causes it is...\n");
    Check(std::string(ui::kShareNoQuicLibrary) != std::string(ui::kShareNoHostIdentity),
        "a build without QUIC and a machine without a key are not the same failure");
    Check(Contains(ui::kShareNoQuicLibrary, "build-quiche"),
        "a build without QUIC names the script that produces one");
    Check(Contains(ui::kShareNoHostIdentity, "key"),
        "a machine that cannot make an identity says what it failed to create");
    Check(!Contains(ui::kShareNoQuicLibrary, "system cannot"),
        "does not blame the operating system for a missing build step");
}

void TestEveryAuthVerdictReadsAsItsOwnMessage() {
    std::printf("[strings] each auth verdict has its own words, and lockout names itself...\n");
    std::vector<std::string> seen;
    for (uint8_t c = 0; c <= uint8_t(AuthResultCode::Locked); ++c) {
        const std::string text = ui::AuthRefusalText(AuthResultCode(c));
        Check(!text.empty(), "every verdict has words to show");
        for (const std::string& other : seen)
            Check(text != other, "and no two verdicts share them");
        seen.push_back(text);
    }
    Check(ui::AuthRefusalText(AuthResultCode::Locked) == std::string(ui::kAuthLocked),
        "a lockout shows the lockout text, not the generic refusal");
}

void TestClampWarningQuotesTheProtocolLimit() {
    std::printf("[strings] the too-many-displays warning quotes the real limit...\n");
    Check(Contains(ui::ShareClampWarning(), std::to_string(kMaxSources)),
        "the number the user reads is the number the protocol enforces");
}

void TestOneHostIsOneAddressHoweverItIsSpelled() {
    std::printf("[strings] the default port is implied, so both spellings name one host...\n");
    Check(ui::NormalizedDeviceAddr("192.168.1.60") == "192.168.1.60:" +
                                                          std::to_string(kDeskhubPort),
        "a bare address gains the default port");
    Check(ui::NormalizedDeviceAddr("  192.168.1.60:5000  ") == "192.168.1.60:5000",
        "an explicit port survives, surrounding space does not");
    Check(ui::NormalizedDeviceAddr("").empty(), "an empty address normalizes to nothing");

    Check(ui::SameDeviceAddr("192.168.1.60", "192.168.1.60:" + std::to_string(kDeskhubPort)),
        "the scanned spelling and the typed one are the same host");
    Check(!ui::SameDeviceAddr("192.168.1.60", "192.168.1.60:5000"),
        "another port is another host");
    Check(!ui::SameDeviceAddr("192.168.1.60", "192.168.1.61"), "another IP is another host");
    Check(!ui::SameDeviceAddr("", ""), "two empty addresses match nothing, not each other");
}

}

void RunStringsTests() {
    TestEveryLabelSaysSomething();
    TestConnectingMentionsTheAddress();
    TestQueryFailureExplainsWhatToCheck();
    TestHostTitleOnlyShowsAKnownSize();
    TestThePortIsNeverHardcodedTwice();
    TestSplitHostPortHandlesEveryShape();
    TestThePortFieldFallsBackToTheDefault();
    TestTheAddressAndPortFieldsComposeOneAddress();
    TestTrimStripsOnlyTheEdges();
    TestParsePositiveUintIsStrict();
    TestPingLabelQuotesTheMeasurement();
    TestTheAboutLineNamesTheBuild();
    TestScanStatusCountsWhatWasChecked();
    TestRecheckNotesQuoteTheRealInterval();
    TestDeviceListNotesReadTheSameOnEveryClient();
    TestTerminalRefusalsNameTheirOwnCause();
    TestEveryAuthVerdictReadsAsItsOwnMessage();
    TestClampWarningQuotesTheProtocolLimit();
    TestBindFallbackNamesTheMissingNetwork();
    TestOneHostIsOneAddressHoweverItIsSpelled();
}

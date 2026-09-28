#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/UiSettings.h"

#include <cstdio>
#include <string>

using namespace deskhub;

namespace {

void TestRoundTrip() {
    std::printf("[settings] saved settings come back exactly as chosen...\n");
    ui::UiSettings s;
    s.fps = 30;
    s.bitrateMbps = 50;
    s.maxDim = 2560;
    s.port = 50123;
    s.allowInput = false;
    s.clientControl = false;
    s.deviceName = "Anh's laptop";
    s.bindIp = "192.168.1.10";
    s.autostart = true;
    s.autoShare = true;
    s.clipboardSync = true;
    s.startHidden = true;
    s.keepAwake = false;
    Check(ui::ParseUiSettings(ui::SerializeUiSettings(s)) == s,
        "serialize then parse is identity");
}

void TestBindIpPersistence() {
    std::printf("[settings] the bind address persists only when it is a real IPv4...\n");
    Check(ui::ParseUiSettings("").bindIp.empty(), "all interfaces by default");
    Check(ui::ParseUiSettings("bind_ip=192.168.1.10").bindIp == "192.168.1.10",
        "a real address is kept");
    Check(ui::ParseUiSettings("bind_ip=").bindIp.empty(), "an empty value means all interfaces");
    Check(ui::ParseUiSettings("bind_ip=not-an-ip").bindIp.empty(), "junk is dropped");
    Check(ui::ParseUiSettings("bind_ip=192.168.1.999").bindIp.empty(),
        "an out-of-range octet is dropped");

    ui::UiSettings s;
    s.bindIp = "garbage";
    Check(ui::ParseUiSettings(ui::SerializeUiSettings(s)).bindIp.empty(),
        "an invalid in-memory address is not written out");
}

void TestBehaviorTogglesPersist() {
    std::printf("[settings] the launch and clipboard toggles default off and round-trip...\n");
    const ui::UiSettings defaults;
    Check(!defaults.autostart && !defaults.autoShare && !defaults.clipboardSync &&
              !defaults.startHidden,
        "every new toggle defaults off");
    Check(ui::ParseUiSettings("autostart=1").autostart, "autostart round-trips on");
    Check(!ui::ParseUiSettings("autostart=0").autostart, "and off");
    Check(ui::ParseUiSettings("auto_share=1").autoShare, "auto-share round-trips on");
    Check(ui::ParseUiSettings("clipboard_sync=1").clipboardSync,
        "clipboard sync round-trips on");
    Check(ui::ParseUiSettings("start_hidden=1").startHidden, "start hidden round-trips on");
    Check(!ui::ParseUiSettings("autostart=x").autostart,
        "junk in a toggle falls back to off");
    Check(defaults.keepAwake, "keep awake defaults on");
    Check(!ui::ParseUiSettings("keep_awake=0").keepAwake, "keep awake round-trips off");
    Check(ui::ParseUiSettings("keep_awake=1").keepAwake, "and on");
    Check(ui::ParseUiSettings("keep_awake=x").keepAwake,
        "junk in keep awake falls back to on");
}

void TestDeviceNamePersistence() {
    std::printf("[settings] the device name persists, trimmed to its limits...\n");
    Check(ui::ParseUiSettings("").deviceName.empty(), "no name by default");
    Check(ui::ParseUiSettings("name=Ph\xC3\xB2ng kh\xC3\xA1"
                              "ch")
                  .deviceName ==
              "Ph\xC3\xB2ng kh\xC3\xA1"
              "ch",
        "a hand-written UTF-8 name is kept");

    std::string longName;
    while (longName.size() < kMaxClientNameBytes + 20) longName += "\xE1\xBA\xA1";
    const std::string parsed = ui::ParseUiSettings("name=" + longName).deviceName;
    Check(parsed.size() <= kMaxClientNameBytes && parsed.size() % 3 == 0,
        "an over-long name is cut on a UTF-8 boundary");

    Check(ui::TruncateDeviceName("a\x01"
                                 "b\x7f"
                                 "c") == "abc",
        "control characters never reach the file or the wire");
    Check(ui::TruncateDeviceName("") == "", "an empty name stays empty");

    ui::UiSettings s;
    s.deviceName = "desk\x02top";
    Check(ui::ParseUiSettings(ui::SerializeUiSettings(s)).deviceName == "desktop",
        "a dirty in-memory name is cleaned on the way out");
}

void TestLegacySettingsAreDiscarded() {
    std::printf("[settings] retired authentication fields are discarded...\n");
    ui::UiSettings s;
    s.passcode = "0417";
    const std::string text = ui::SerializeUiSettings(s);
    Check(text.find("passcode=") == std::string::npos,
        "a passcode is not serialized");
    Check(text.find("allow_new_pairings=") == std::string::npos,
        "the old pairing switch is not serialized");
    const ui::UiSettings legacy = ui::ParseUiSettings(
        "passcode=0417\nallow_new_pairings=0\nname=Older host\n");
    Check(legacy.passcode.empty(), "retired values from an old file are ignored");
    Check(legacy.deviceName == "Older host", "other settings remain available");
}

void TestDefaultsMatchShareDefaults() {
    std::printf("[settings] a missing or empty file yields the share defaults...\n");
    const ui::UiSettings defaults;
    Check(defaults.fps == 60 && defaults.bitrateMbps == 20 && defaults.maxDim == 1920,
        "defaults are the long-standing share defaults");
    Check(defaults.port == kDeskhubPort, "the default port is the protocol default");
    Check(defaults.allowInput, "remote control is allowed unless the user turns it off");
    Check(ui::ParseUiSettings("") == defaults, "empty text is all defaults");
}

void TestNativeQualityIsPreserved() {
    std::printf("[settings] the Native preset (max_dim=0) survives a round trip...\n");
    ui::UiSettings s;
    s.maxDim = 0;
    Check(ui::ParseUiSettings(ui::SerializeUiSettings(s)).maxDim == 0,
        "zero means native resolution, not a parse failure");
}

void TestGarbageFallsBackPerKey() {
    std::printf("[settings] each bad value falls back alone, good ones still apply...\n");
    const ui::UiSettings s = ui::ParseUiSettings(
        "fps=abc\n"
        "bitrate_mbps=50\n"
        "max_dim=999999\n"
        "unknown_key=7\n"
        "no equals sign here\n");
    Check(s.fps == 60, "junk fps falls back to the default");
    Check(s.bitrateMbps == 50, "the valid bitrate is kept");
    Check(s.maxDim == 1920, "an absurd dimension falls back to the default");
}

void TestBoundsAreEnforced() {
    std::printf("[settings] zero and overflow values cannot smuggle in...\n");
    Check(ui::ParseUiSettings("fps=0").fps == 60, "fps zero is rejected");
    Check(ui::ParseUiSettings("fps=241").fps == 60, "fps above the cap is rejected");
    Check(ui::ParseUiSettings("bitrate_mbps=0").bitrateMbps == 20, "bitrate zero is rejected");
    Check(ui::ParseUiSettings("port=0").port == kDeskhubPort, "port zero is rejected");
    Check(ui::ParseUiSettings("port=65536").port == kDeskhubPort,
        "a port past 16 bits is rejected");
    Check(ui::ParseUiSettings("port=50123").port == 50123, "a real custom port is kept");
    Check(!ui::ParseUiSettings("allow_input=0").allowInput, "view-only mode round-trips");
    Check(ui::ParseUiSettings("allow_input=1").allowInput, "so does control mode");
    Check(!ui::ParseUiSettings("client_control=0").clientControl,
        "the client-side view-only choice round-trips too");
    Check(ui::ParseUiSettings("").clientControl, "and defaults to controlling");
    Check(ui::ParseUiSettings("allow_input=x").allowInput,
        "junk falls back to allowing control, the long-standing behaviour");
    Check(ui::ParseUiSettings("fps = 30 ").fps == 30, "spaces around key and value are fine");
}

void TestTerminalSharePersistence() {
    std::printf("[settings] the terminal is a source of its own, on the shared network...\n");
    ui::UiSettings s;
    s.bindIp = "192.168.1.10";
    const ui::UiSettings back = ui::ParseUiSettings(ui::SerializeUiSettings(s));
    Check(back.bindIp == "192.168.1.10",
        "the terminal shares on the one network the host picked");
    Check(back == s, "nothing else changed on the way through");

    Check(ui::SerializeUiSettings(s).find("terminal_share") == std::string::npos,
        "which source is ticked is not a saved setting: the terminal starts ticked every time, "
        "like the displays do");

    const std::string fourX =
        "fps=30\n"
        "bitrate_mbps=15\n"
        "max_dim=1280\n"
        "port=47777\n"
        "allow_input=1\n"
        "client_control=1\n"
        "passcode=\n"
        "name=Old machine\n"
        "bind_ip=192.168.1.10\n"
        "autostart=0\n"
        "auto_share=1\n"
        "clipboard_sync=1\n"
        "start_hidden=0\n"
        "keep_awake=1\n"
        "terminal_port=47778\n";
    const ui::UiSettings old = ui::ParseUiSettings(fourX);
    Check(old.fps == 30 && old.autoShare && old.deviceName == "Old machine",
        "an older settings file still reads back everything it used to hold");
    Check(old.transferDir.empty(), "and the fields it never heard of come out at their defaults");
    Check(ui::SerializeUiSettings(old).find("terminal_port") == std::string::npos,
        "the retired terminal-port key is dropped rather than written back");
}

}

void RunUiSettingsTests() {
    TestRoundTrip();
    TestBindIpPersistence();
    TestBehaviorTogglesPersist();
    TestDeviceNamePersistence();
    TestLegacySettingsAreDiscarded();
    TestDefaultsMatchShareDefaults();
    TestNativeQualityIsPreserved();
    TestGarbageFallsBackPerKey();
    TestBoundsAreEnforced();
    TestTerminalSharePersistence();
}

#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/UnattendedAccess.h"

#include <cstdio>

using namespace deskhub;

namespace {

ui::UiSettings NoisySettings() {
    ui::UiSettings settings;
    settings.unattended = true;
    settings.fps = 90;
    settings.port = 50123;
    settings.allowInput = false;
    settings.shareAudio = true;
    settings.playAudio = true;
    settings.clipboardSync = true;
    settings.autostart = true;
    settings.autoShare = true;
    settings.startHidden = true;
    settings.deviceName = "Office PC";
    return settings;
}

void TestEnabledConfigCarriesEverythingTheHostNeeds() {
    std::printf("[unattended] an enabled config carries the key, the list and the settings...\n");
    const UnattendedConfig config =
        ui::UnattendedConfigOf(NoisySettings(), "-----KEY-----", "ssh-ed25519 AAAA laptop\n");
    Check(config.enabled, "the config is enabled when the setting is on");
    Check(config.hostKeyPem == "-----KEY-----", "the key text is passed through untouched");
    Check(config.authorizedKeys == "ssh-ed25519 AAAA laptop\n",
        "the allowed list is passed through untouched");
    const ui::UiSettings forHost = ui::ParseUiSettings(config.settings);
    Check(forHost.unattended && forHost.fps == 90 && forHost.port == 50123 &&
              !forHost.allowInput && forHost.deviceName == "Office PC",
        "the sharing settings reach the host as chosen");
}

void TestEnabledConfigStripsWhatTheHostCannotDo() {
    std::printf("[unattended] sound, clipboard and launch settings are switched off for the host...\n");
    const UnattendedConfig config = ui::UnattendedConfigOf(NoisySettings(), "k", "a");
    const ui::UiSettings forHost = ui::ParseUiSettings(config.settings);
    Check(!forHost.shareAudio && !forHost.playAudio && !forHost.clipboardSync,
        "the host shares neither sound nor clipboard");
    Check(!forHost.autostart && !forHost.autoShare && !forHost.startHidden,
        "the app's launch behaviour does not travel to the host");
}

void TestDisabledConfigCarriesNothing() {
    std::printf("[unattended] a disabled config carries no key or list...\n");
    ui::UiSettings settings = NoisySettings();
    settings.unattended = false;
    const UnattendedConfig config = ui::UnattendedConfigOf(settings, "secret", "list");
    Check(!config.enabled, "the config is disabled when the setting is off");
    Check(config.hostKeyPem.empty() && config.authorizedKeys.empty() && config.settings.empty(),
        "nothing of the machine travels when the setting is off");
}

}

void RunUnattendedAccessTests() {
    TestEnabledConfigCarriesEverythingTheHostNeeds();
    TestEnabledConfigStripsWhatTheHostCannotDo();
    TestDisabledConfigCarriesNothing();
}

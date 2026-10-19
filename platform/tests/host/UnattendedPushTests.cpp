#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/UiSettings.h"
#include "deskhubp/host/UnattendedPush.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"

#include <cstdio>
#include <optional>
#include <string>

namespace {

void TestDisabledCarriesNothing() {
    std::printf("[push] switching the feature off sends no key and no list...\n");
    deskhub::ui::UiSettings settings;
    settings.unattended = false;
    const auto config = deskhubp::BuildUnattendedConfig(settings);
    Check(config.has_value() && !config->enabled, "an off switch still builds");
    Check(config && config->hostKeyPem.empty() && config->authorizedKeys.empty() &&
              config->settings.empty(),
        "and carries nothing the daemon could keep");
}

void TestEnabledCopiesTheMachineIdentity() {
    std::printf("[push] switching it on copies this machine's key, list and settings...\n");
    const IsolatedAppData isolated("deskhub-push-");
    Check(ForgetHostIdentity(), "the isolated folder starts without a key");
    deskhub::ui::UiSettings settings;
    settings.unattended = true;
    settings.shareAudio = true;
    settings.fps = 24;
    const auto config = deskhubp::BuildUnattendedConfig(settings);
    Check(config.has_value() == deskhubp::QuicAvailable(),
        "a configuration needs a machine key, which only a QUIC build can make");
    if (!config) return;
    const deskhubp::HostIdentity identity = deskhubp::LoadHostIdentity();
    Check(config->enabled && config->hostKeyPem == identity.keyPem,
        "the key is the one this machine answers with");
    Check(deskhubp::RememberAuthorizedKey(deskhubp::IdentityPublicKeyText(identity)),
        "a client key is allowed");
    const auto again = deskhubp::BuildUnattendedConfig(settings);
    Check(again && again->authorizedKeys.find("ecdsa-sha2-nistp256") != std::string::npos,
        "the allowed list travels as the file text");
    const deskhub::ui::UiSettings forHost = deskhub::ui::ParseUiSettings(again->settings);
    Check(forHost.fps == 24 && !forHost.shareAudio && forHost.unattended,
        "the settings are the sharing choices with sound and launch options off");
    Check(RevokeAllClientKeys(), "the isolated list is cleared again");
}

void TestUnattendedHostSharesQuietly() {
    std::printf("[push] the host before sign-in shares no sound, clipboard, terminal or files...\n");
    deskhub::ui::UiSettings settings;
    settings.shareAudio = true;
    settings.clipboardSync = true;
    settings.fps = 24;
    settings.deviceName = "front desk";
    const deskhub::media::ShareOptions options = deskhubp::UnattendedShareOptions(settings);
    Check(!options.audio && !options.clipboardSync, "sound and the clipboard stay off");
    Check(!options.terminal && !options.files, "the terminal and file transfer stay off");
    Check(options.fps == 24 && options.deviceName == "front desk",
        "the sharing choices and the name carry over");
    settings.deviceName.clear();
    Check(!deskhubp::UnattendedShareOptions(settings).deviceName.empty(),
        "an unnamed machine falls back to its own name");
}

}

void RunUnattendedPushTests() {
    TestDisabledCarriesNothing();
    TestEnabledCopiesTheMachineIdentity();
    TestUnattendedHostSharesQuietly();
}

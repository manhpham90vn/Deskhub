#include "deskhub/ui/UnattendedAccess.h"

#include <utility>

namespace deskhub::ui {

namespace {

UiSettings SettingsForUnattendedHost(UiSettings settings) {
    settings.shareAudio = false;
    settings.playAudio = false;
    settings.clipboardSync = false;
    settings.autostart = false;
    settings.autoShare = false;
    settings.startHidden = false;
    return settings;
}

}

UnattendedConfig UnattendedConfigOf(const UiSettings& settings, std::string hostKeyPem,
    std::string authorizedKeys) {
    UnattendedConfig config;
    config.enabled = settings.unattended;
    if (!config.enabled) return config;
    config.hostKeyPem = std::move(hostKeyPem);
    config.authorizedKeys = std::move(authorizedKeys);
    config.settings = SerializeUiSettings(SettingsForUnattendedHost(settings));
    return config;
}

}

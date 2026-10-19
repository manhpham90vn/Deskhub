#include "deskhubp/host/UnattendedPush.h"

#include "deskhub/session/host/ShareFlow.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/DeviceName.h"
#include "deskhubp/system/HostIdentity.h"

namespace deskhubp {

std::optional<deskhub::UnattendedConfig> BuildUnattendedConfig(
    const deskhub::ui::UiSettings& settings) {
    deskhub::UnattendedConfig config;
    config.enabled = settings.unattended;
    if (!config.enabled) return config;
    const HostIdentity identity = LoadOrCreateHostIdentity();
    if (!identity.Valid()) return std::nullopt;
    config.hostKeyPem = identity.keyPem;
    config.authorizedKeys = ReadAppDataFile(kAuthorizedKeysFileName);
    config.settings =
        deskhub::ui::SerializeUiSettings(deskhub::ui::UnattendedHostSettings(settings));
    return config;
}

deskhub::media::ShareOptions UnattendedShareOptions(const deskhub::ui::UiSettings& settings) {
    deskhub::media::ShareOptions options = deskhub::ShareOptionsOf(settings, false, false);
    options.audio = false;
    options.clipboardSync = false;
    if (options.deviceName.empty()) options.deviceName = LocalDeviceName();
    return options;
}

}

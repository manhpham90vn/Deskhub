#include "deskhubp/host/UnattendedPush.h"

#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
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

}

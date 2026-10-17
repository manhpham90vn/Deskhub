#include "secure/UnattendedAccess.h"

#include <string>
#include <utility>

#include "deskhub/ui/UnattendedAccess.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/AuthorizedKeysFile.h"
#include "deskhubp/system/HostIdentity.h"
#include "secure/ServiceControlPipe.h"

bool PushUnattendedAccess(const deskhub::ui::UiSettings& settings) {
    std::string hostKeyPem;
    std::string authorizedKeys;
    if (settings.unattended) {
        const deskhubp::HostIdentity identity = deskhubp::LoadOrCreateHostIdentity();
        if (!identity.Valid()) return false;
        hostKeyPem = identity.keyPem;
        authorizedKeys = deskhubp::ReadAppDataFile(deskhubp::kAuthorizedKeysFileName);
    }
    return SendUnattendedConfig(
        deskhub::ui::UnattendedConfigOf(settings, std::move(hostKeyPem), std::move(authorizedKeys)));
}

void SyncUnattendedAccess(const deskhub::ui::UiSettings& settings) {
    if (settings.unattended) PushUnattendedAccess(settings);
}

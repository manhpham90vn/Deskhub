#include "secure/UnattendedAccess.h"

#include <optional>

#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhubp/host/UnattendedPush.h"
#include "secure/ServiceControlPipe.h"

bool PushUnattendedAccess(const deskhub::ui::UiSettings& settings) {
    const std::optional<deskhub::UnattendedConfig> config = deskhubp::BuildUnattendedConfig(settings);
    return config && SendUnattendedConfig(*config);
}

void SyncUnattendedAccess(const deskhub::ui::UiSettings& settings) {
    if (settings.unattended) PushUnattendedAccess(settings);
}

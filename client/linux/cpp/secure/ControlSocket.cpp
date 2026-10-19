#include "secure/ControlSocket.h"

#include <filesystem>
#include <optional>
#include <system_error>

#include "deskhubp/host/UnattendedClient.h"
#include "deskhubp/host/UnattendedPush.h"
#include "secure/UnattendedNames.h"

namespace {

deskhubp::UnattendedDaemonAddress DaemonAddress() {
    return deskhubp::UnattendedDaemonAddress{kUnattendedSocketPath, {}};
}

}

bool UnattendedDaemonInstalled() {
    std::error_code ignored;
    return std::filesystem::is_regular_file(kUnattendedDaemonExe, ignored) &&
           std::filesystem::is_regular_file(kUnattendedDaemonUnit, ignored);
}

std::shared_ptr<void> TakeoverUnattendedHost() {
    if (!UnattendedDaemonInstalled()) return nullptr;
    return deskhubp::TakeoverUnattendedHost(DaemonAddress());
}

bool PushUnattendedAccess(const deskhub::ui::UiSettings& settings) {
    const std::optional<deskhub::UnattendedConfig> config =
        deskhubp::BuildUnattendedConfig(settings);
    return config && deskhubp::SendUnattendedConfig(DaemonAddress(), *config);
}

void SyncUnattendedAccess(const deskhub::ui::UiSettings& settings) {
    if (settings.unattended && UnattendedDaemonInstalled()) PushUnattendedAccess(settings);
}

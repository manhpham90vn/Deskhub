#include "Unattended.h"

#include <unistd.h>

#include "Output.h"

#include "deskhub/ui/Strings.h"
#include "secure/AdminGroup.h"
#include "secure/ControlSocket.h"

namespace deskhubcli {
namespace {

bool RunByAdministrator() {
    return IsAdministrator(uint32_t(getuid()));
}

}

std::string_view UnattendedAccessBlocker() {
    if (!UnattendedDaemonInstalled()) return deskhub::ui::kUnattendedServiceMissingNote;
    if (!RunByAdministrator()) return deskhub::ui::kUnattendedNeedsAdminGroup;
    return {};
}

bool PushUnattended(const deskhub::ui::UiSettings& settings) {
    const bool accepted = PushUnattendedAccess(settings);
    if (!accepted && !RunByAdministrator()) PrintError(deskhub::ui::kUnattendedNeedsAdminGroup);
    return accepted;
}

void SyncUnattended(const deskhub::ui::UiSettings& settings) {
    if (!settings.unattended || !UnattendedDaemonInstalled()) return;
    if (PushUnattendedAccess(settings)) return;
    PrintError(deskhub::ui::kUnattendedSyncFailedNote);
    if (!RunByAdministrator()) PrintError(deskhub::ui::kUnattendedNeedsAdminGroup);
}

}

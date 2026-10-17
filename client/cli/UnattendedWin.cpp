#include "Unattended.h"

#include "Output.h"

#include "deskhub/ui/Strings.h"
#include "secure/ServiceControlPipe.h"
#include "secure/UnattendedAccess.h"

namespace deskhubcli {

std::string_view UnattendedAccessBlocker() {
    if (SecureDesktopServiceInstalled()) return {};
    return deskhub::ui::kUnattendedServiceMissingNote;
}

bool PushUnattended(const deskhub::ui::UiSettings& settings) {
    const bool accepted = PushUnattendedAccess(settings);
    if (!accepted) PrintError(deskhub::ui::kUnattendedNeedsAdminTerminal);
    return accepted;
}

void SyncUnattended(const deskhub::ui::UiSettings& settings) {
    if (!settings.unattended) return;
    if (PushUnattendedAccess(settings)) return;
    PrintError(deskhub::ui::kUnattendedSyncFailedNote);
    PrintError(deskhub::ui::kUnattendedNeedsAdminTerminal);
}

}

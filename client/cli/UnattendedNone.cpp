#include "Unattended.h"

#include "deskhub/ui/Strings.h"

namespace deskhubcli {

std::string_view UnattendedAccessBlocker() {
    return deskhub::ui::kUnattendedAppOnlyNote;
}

bool PushUnattended(const deskhub::ui::UiSettings& settings) {
    return !settings.unattended;
}

void SyncUnattended(const deskhub::ui::UiSettings&) {
}

}

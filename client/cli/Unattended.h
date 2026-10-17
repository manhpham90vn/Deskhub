#pragma once
#include <string_view>

#include "deskhub/ui/UiSettings.h"

namespace deskhubcli {

std::string_view UnattendedAccessBlocker();
bool PushUnattended(const deskhub::ui::UiSettings& settings);
void SyncUnattended(const deskhub::ui::UiSettings& settings);

}

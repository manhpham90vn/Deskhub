#pragma once
#include <string>

#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhub/ui/UiSettings.h"

namespace deskhub::ui {

UnattendedConfig UnattendedConfigOf(const UiSettings& settings, std::string hostKeyPem,
    std::string authorizedKeys);

}

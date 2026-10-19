#pragma once
#include "deskhub/media/ShareTypes.h"
#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhub/ui/UiSettings.h"

#include <optional>

namespace deskhubp {

std::optional<deskhub::UnattendedConfig> BuildUnattendedConfig(
    const deskhub::ui::UiSettings& settings);

deskhub::media::ShareOptions UnattendedShareOptions(const deskhub::ui::UiSettings& settings);

}

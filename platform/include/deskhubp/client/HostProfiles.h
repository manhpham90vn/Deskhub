#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/ui/HostProfiles.h"

#include <optional>
#include <string_view>

namespace deskhubp {

std::optional<deskhub::Fingerprint> ParseHostKeyText(std::string_view text);
deskhub::ui::HostProfileError SaveHostProfile(deskhub::ui::HostProfileMode mode,
    const deskhub::ui::HostProfileRequest& request);
deskhub::ui::HostProfileError RemoveHostProfile(std::string_view alias);
deskhub::ui::HostProfileError TrustNewHost(std::string_view address,
    const deskhub::Fingerprint& fingerprint);

}

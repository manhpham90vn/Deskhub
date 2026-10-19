#include "deskhubp/host/UnattendedClient.h"

namespace deskhubp {

std::shared_ptr<void> TakeoverUnattendedHost(const UnattendedDaemonAddress&) {
    return nullptr;
}

bool SendUnattendedConfig(const UnattendedDaemonAddress&, const deskhub::UnattendedConfig&) {
    return false;
}

std::optional<deskhub::UnattendedStatus> QueryUnattendedStatus(const UnattendedDaemonAddress&) {
    return std::nullopt;
}

}

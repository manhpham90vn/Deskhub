#include "PeerRights.h"

#include <filesystem>
#include <system_error>

#include "secure/AdminGroup.h"
#include "secure/UnattendedNames.h"

namespace {

bool RunsDeskhub(const deskhubp::LocalPeer& peer) {
    std::error_code ignored;
    const std::filesystem::path exe = std::filesystem::weakly_canonical(peer.exePath, ignored);
    return exe == kUnattendedAppExe || exe == kUnattendedCliExe;
}

}

deskhubp::UnattendedPeerRights PeerRightsOf(const deskhubp::LocalPeer& peer) {
    if (!RunsDeskhub(peer)) return deskhubp::UnattendedPeerRights::None;
    return IsAdministrator(peer.uid) ? deskhubp::UnattendedPeerRights::Configure
                                     : deskhubp::UnattendedPeerRights::Hold;
}

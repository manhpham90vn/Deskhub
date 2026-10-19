#pragma once
#include "deskhubp/host/UnattendedDaemon.h"
#include "deskhubp/net/LocalSocket.h"

deskhubp::UnattendedPeerRights PeerRightsOf(const deskhubp::LocalPeer& peer);

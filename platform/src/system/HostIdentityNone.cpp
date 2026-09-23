#include "deskhubp/system/HostIdentity.h"

#include "deskhubp/diag/Log.h"

namespace deskhubp {

namespace {

void WarnOnce() {
    static bool warned = false;
    if (warned) return;
    warned = true;
    LOGW("host identity: this build has no QUIC library - run 'make bootstrap' to build quiche");
}

}

bool QuicAvailable() {
    return false;
}

std::optional<deskhub::Fingerprint> FingerprintOfCertDer(std::span<const uint8_t>) {
    WarnOnce();
    return std::nullopt;
}

HostIdentity LoadHostIdentity() {
    WarnOnce();
    return {};
}

HostIdentity LoadOrCreateHostIdentity(std::string_view) {
    WarnOnce();
    return {};
}

}

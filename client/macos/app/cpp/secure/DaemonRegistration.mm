#import <ServiceManagement/ServiceManagement.h>

#include "secure/DaemonRegistration.h"

#include <sys/xattr.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "deskhubp/diag/Log.h"
#include "secure/ControlSocket.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr const char* kQuarantineAttribute = "com.apple.quarantine";

SMAppService* DaemonService() {
    return [SMAppService daemonServiceWithPlistName:@(UnattendedDaemonPlistName().c_str())];
}

void ReleaseFromQuarantine(const std::string& path) {
    if (path.empty()) return;
    if (removexattr(path.c_str(), kQuarantineAttribute, 0) == 0 || errno == ENOATTR) return;
    LOGW("[Secure] Could not clear the quarantine flag on %s: %s. Gatekeeper will have to "
         "verify it online when launchd starts it at boot, before the network is up, and "
         "a failed check there blocks the whole app until it is allowed in System Settings.",
        path.c_str(), std::strerror(errno));
}

void ReleaseDaemonFromQuarantine() {
    ReleaseFromQuarantine(UnattendedDaemonPath());
    ReleaseFromQuarantine(UnattendedDaemonPlistPath());
    ReleaseFromQuarantine(BundledCliPath());
}

}

bool RegisterUnattendedDaemon(bool on) {
    if (on) ReleaseDaemonFromQuarantine();
    SMAppService* service = DaemonService();
    NSError* error = nil;
    const bool ok = on ? [service registerAndReturnError:&error]
                       : [service unregisterAndReturnError:&error];
    if (!ok && error)
        LOGW("[Secure] %s of the Deskhub daemon failed: %s", on ? "registration" : "removal",
            error.localizedDescription.UTF8String);
    if (!on && !ok && error.code == kSMErrorJobNotFound) return true;
    return ok;
}

DaemonStatus UnattendedDaemonStatus() {
    switch (DaemonService().status) {
        case SMAppServiceStatusEnabled: return DaemonStatus::Enabled;
        case SMAppServiceStatusRequiresApproval: return DaemonStatus::RequiresApproval;
        case SMAppServiceStatusNotFound: return DaemonStatus::NotFound;
        case SMAppServiceStatusNotRegistered: break;
    }
    return DaemonStatus::NotRegistered;
}

void OpenLoginItemsSettings() {
    [SMAppService openSystemSettingsLoginItems];
}

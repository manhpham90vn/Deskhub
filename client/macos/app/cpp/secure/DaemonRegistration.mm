#import <ServiceManagement/ServiceManagement.h>

#include "secure/DaemonRegistration.h"

#include "deskhubp/diag/Log.h"
#include "secure/UnattendedNames.h"

namespace {

SMAppService* DaemonService() {
    return [SMAppService daemonServiceWithPlistName:@(UnattendedDaemonPlistName().c_str())];
}

}

bool RegisterUnattendedDaemon(bool on) {
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

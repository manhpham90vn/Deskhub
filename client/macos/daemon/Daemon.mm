#import <Foundation/Foundation.h>

#include "Daemon.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>

#include <sys/stat.h>
#include <unistd.h>

#include "LoginWindowAgent.h"
#include "PeerTrust.h"
#include "deskhub/session/host/UnattendedControl.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/UnattendedDaemon.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/MachineFolder.h"
#include "secure/UnattendedNames.h"

namespace {

constexpr mode_t kSocketDirMode = 0755;
constexpr uint32_t kSocketMode = 0666;

std::atomic<bool> g_stop{false};

void OnSignal(int) {
    g_stop.store(true, std::memory_order_release);
}

bool PrepareSocketDir() {
    if (mkdir(kUnattendedSocketDir, kSocketDirMode) != 0 && errno != EEXIST) return false;
    return chmod(kUnattendedSocketDir, kSocketDirMode) == 0;
}

bool UpdateLoginWindowAgent(bool enabled) {
    if (!enabled) {
        RemoveLoginWindowAgent();
        return true;
    }
    if (WriteLoginWindowAgent()) return true;
    LOGE("[Daemon] Could not write the login window agent into %s.", kLaunchAgentsDir);
    return false;
}

deskhubp::UnattendedDaemonHooks MacHooks() {
    deskhubp::UnattendedDaemonHooks hooks;
    hooks.rightsOf = [](const deskhubp::LocalPeer& peer) {
        return TrustedAppPeer(peer) ? deskhubp::UnattendedPeerRights::Configure
                                    : deskhubp::UnattendedPeerRights::None;
    };
    hooks.afterConfigure = UpdateLoginWindowAgent;
    hooks.status = [] {
        return deskhub::ParseUnattendedStatus(
            deskhubp::ReadAppDataFile(deskhubp::kUnattendedStatusFile));
    };
    return hooks;
}

}

int RunDaemon() {
    if (geteuid() != 0) {
        LOGE("[Daemon] DeskhubDaemon must run as root; launchd starts it from the Deskhub app "
             "once you allow Deskhub to run in the background.");
        return 1;
    }
    if (!deskhubp::EnterMachineFolder(kUnattendedMachineFolder)) {
        LOGE("[Daemon] Could not enter %s.", kUnattendedMachineFolder);
        return 1;
    }
    if (!PrepareSocketDir()) {
        LOGE("[Daemon] Could not prepare %s.", kUnattendedSocketDir);
        return 1;
    }
    std::signal(SIGTERM, OnSignal);
    std::signal(SIGINT, OnSignal);
    const bool served =
        deskhubp::ServeUnattendedControl(kUnattendedSocketPath, kSocketMode, MacHooks(), g_stop);
    unlink(kUnattendedSocketPath);
    if (!served) return 1;
    LOGI("[Daemon] Stopped.");
    return 0;
}

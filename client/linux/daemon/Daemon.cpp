#include "Daemon.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

#include "HostSupervisor.h"
#include "PeerRights.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/host/UnattendedDaemon.h"
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
    return chown(kUnattendedSocketDir, 0, 0) == 0 && chmod(kUnattendedSocketDir, kSocketDirMode) == 0;
}

deskhubp::UnattendedDaemonHooks LinuxHooks(HostSupervisor& supervisor) {
    deskhubp::UnattendedDaemonHooks hooks;
    hooks.rightsOf = PeerRightsOf;
    hooks.afterConfigure = [&supervisor](bool) {
        supervisor.ConfigChanged();
        return true;
    };
    hooks.holdChanged = [&supervisor](bool held) { supervisor.SetHeld(held); };
    return hooks;
}

}

int RunDaemon() {
    if (geteuid() != 0) {
        LOGE("[Daemon] deskhubd must run as root; systemd starts it as deskhubd.service.");
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
    unlink(kUnattendedSocketPath);
    std::signal(SIGTERM, OnSignal);
    std::signal(SIGINT, OnSignal);

    HostSupervisor supervisor;
    std::thread supervising([&supervisor] { supervisor.Run(g_stop); });
    const bool served = deskhubp::ServeUnattendedControl(kUnattendedSocketPath, kSocketMode,
        LinuxHooks(supervisor), g_stop);
    g_stop.store(true, std::memory_order_release);
    supervisor.ConfigChanged();
    supervising.join();
    supervisor.Shutdown();
    unlink(kUnattendedSocketPath);
    if (!served) return 1;
    LOGI("[Daemon] Stopped.");
    return 0;
}

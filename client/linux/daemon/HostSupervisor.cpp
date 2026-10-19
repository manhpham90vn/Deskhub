#include "HostSupervisor.h"

#include <array>
#include <chrono>
#include <csignal>
#include <thread>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "capture/KmsDevice.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/AppDataFile.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/HostIdentity.h"
#include "deskhubp/system/MachineFolder.h"
#include "deskhubp/system/UiSettingsStore.h"
#include "secure/UnattendedNames.h"

extern char** environ;

namespace {

constexpr auto kPoll = std::chrono::milliseconds(500);
constexpr auto kExitPoll = std::chrono::milliseconds(50);
constexpr auto kGracefulExit = std::chrono::seconds(5);
constexpr uint64_t kRestartBackoffUs = 2'000'000;

std::string DescribeExit(int status) {
    if (WIFEXITED(status)) return "exit code " + std::to_string(WEXITSTATUS(status));
    if (WIFSIGNALED(status)) return "signal " + std::to_string(WTERMSIG(status));
    return "status " + std::to_string(status);
}

bool WaitForExit(pid_t pid, std::chrono::milliseconds budget, int& status) {
    const auto deadline = std::chrono::steady_clock::now() + budget;
    while (std::chrono::steady_clock::now() < deadline) {
        if (waitpid(pid, &status, WNOHANG) == pid) return true;
        std::this_thread::sleep_for(kExitPoll);
    }
    return false;
}

}

HostSupervisor::Snapshot HostSupervisor::ReadSnapshot() {
    return Snapshot{deskhubp::ReadAppDataFile(deskhubp::kHostKeyFileName),
        deskhubp::ReadAppDataFile(deskhubp::kUiSettingsFileName)};
}

void HostSupervisor::Run(const std::atomic<bool>& stop) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!stop.load(std::memory_order_acquire)) {
        StepLocked();
        wake_.wait_for(lock, kPoll);
    }
}

void HostSupervisor::SetHeld(bool held) {
    const std::lock_guard<std::mutex> lock(mutex_);
    held_ = held;
    if (held) HandOverLocked("the app is sharing");
    wake_.notify_all();
}

void HostSupervisor::ConfigChanged() {
    const std::lock_guard<std::mutex> lock(mutex_);
    wake_.notify_all();
}

void HostSupervisor::Shutdown() {
    const std::lock_guard<std::mutex> lock(mutex_);
    StopChildLocked("the daemon is stopping");
}

void HostSupervisor::StepLocked() {
    ReapLocked();
    const bool enabled = deskhubp::UnattendedEnabled(kUnattendedMachineFolder);
    if (child_ > 0 && !enabled) StopChildLocked("access before sign-in was switched off");
    if (child_ > 0 && ReadSnapshot() != runningWith_)
        HandOverLocked("the key or settings changed");
    if (child_ <= 0 && enabled && ShouldStartLocked()) SpawnLocked();
}

bool HostSupervisor::ShouldStartLocked() const {
    if (held_) return false;
    if (NowUs() - lastExitUs_ < kRestartBackoffUs) return false;
    return !ListKmsDisplays().empty();
}

void HostSupervisor::SpawnLocked() {
    std::array<char, 4096> self{};
    const ssize_t n = readlink("/proc/self/exe", self.data(), self.size() - 1);
    if (n <= 0) {
        LOGE("[Daemon] Cannot find its own executable to start the unattended host.");
        lastExitUs_ = NowUs();
        return;
    }
    std::string hostArg = kUnattendedHostArg;
    std::array<char*, 3> argv = {self.data(), hostArg.data(), nullptr};
    pid_t pid = -1;
    if (posix_spawn(&pid, self.data(), nullptr, nullptr, argv.data(), environ) != 0) {
        LOGE("[Daemon] Could not start the unattended host.");
        lastExitUs_ = NowUs();
        return;
    }
    child_ = pid;
    runningWith_ = ReadSnapshot();
    LOGI("[Daemon] Started the unattended host (pid %d).", int(pid));
}

void HostSupervisor::ReapLocked() {
    if (child_ <= 0) return;
    int status = 0;
    if (waitpid(child_, &status, WNOHANG) != child_) return;
    LOGI("[Daemon] The unattended host (pid %d) ended: %s.", int(child_),
        DescribeExit(status).c_str());
    child_ = -1;
    lastExitUs_ = NowUs();
}

void HostSupervisor::HandOverLocked(const char* why) {
    if (child_ <= 0) return;
    kill(child_, SIGKILL);
    int status = 0;
    waitpid(child_, &status, 0);
    LOGI("[Daemon] Handed over from the unattended host (pid %d) without ending its viewers' "
         "sessions, so they redial: %s.",
        int(child_), why);
    child_ = -1;
    lastExitUs_ = NowUs();
}

void HostSupervisor::StopChildLocked(const char* why) {
    if (child_ <= 0) return;
    kill(child_, SIGTERM);
    int status = 0;
    if (!WaitForExit(child_, kGracefulExit, status)) {
        LOGW("[Daemon] The unattended host (pid %d) did not stop in time; killing it.",
            int(child_));
        kill(child_, SIGKILL);
        waitpid(child_, &status, 0);
    }
    LOGI("[Daemon] Stopped the unattended host (pid %d): %s.", int(child_), why);
    child_ = -1;
    lastExitUs_ = NowUs();
}

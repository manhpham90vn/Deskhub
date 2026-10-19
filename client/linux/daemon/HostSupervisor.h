#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

#include <sys/types.h>

class HostSupervisor {
public:
    void Run(const std::atomic<bool>& stop);
    void SetHeld(bool held);
    void ConfigChanged();
    void Shutdown();

private:
    struct Snapshot {
        std::string hostKey;
        std::string settings;

        bool operator==(const Snapshot&) const = default;
    };

    static Snapshot ReadSnapshot();

    void StepLocked();
    bool ShouldStartLocked() const;
    void SpawnLocked();
    void ReapLocked();
    void HandOverLocked(const char* why);
    void StopChildLocked(const char* why);

    std::mutex mutex_;
    std::condition_variable wake_;
    pid_t child_ = -1;
    bool held_ = false;
    uint64_t lastExitUs_ = 0;
    Snapshot runningWith_;
};

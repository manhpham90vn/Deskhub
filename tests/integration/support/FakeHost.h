#pragma once
#include "support/TestSupport.h"

#include "deskhubp/host/HostEngine.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace deskhubp {
class FileHost;
class TerminalHost;
}

namespace fake {

deskhub::media::ShareSource Source(const char* name, uint32_t width, uint32_t height,
    uint64_t targetId);

class SharingHost {
public:
    ~SharingHost() {
        Stop();
    }

    bool Start(const std::vector<deskhub::media::ShareSource>& sources, uint16_t port,
        uint32_t fps = 30, uint32_t maxDim = 1920, bool allowInput = true, bool audio = false);

    void Stop() {
        StopTone();
        engine_.Stop();
    }

    deskhubp::SessionTransport& socket() {
        return engine_.socket();
    }

    void SetFiles(deskhubp::FileHost* files) {
        engine_.SetFiles(files);
    }

    void SetTerminal(deskhubp::TerminalHost* terminal) {
        engine_.SetTerminal(terminal);
    }

    bool audioRunning() {
        return engine_.audioRunning();
    }

    bool running() {
        return engine_.running();
    }

    std::vector<deskhub::media::ShareSourceStatus> Status() {
        return engine_.Status();
    }

    std::string LastError() {
        return engine_.LastError();
    }

private:
    void StartTone(const deskhub::media::AudioFormat& format,
        const std::function<void(std::span<const int16_t>)>& offer);
    void StopTone();

    deskhubp::HostEngine engine_;
    std::thread tone_;
    std::atomic<bool> toneStop_{false};
};

}

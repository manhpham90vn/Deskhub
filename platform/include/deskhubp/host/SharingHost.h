#pragma once
#include "deskhubp/host/HostEngine.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using ShareSource = deskhub::media::ShareSource;
using ShareOptions = deskhub::media::ShareOptions;
using ShareSourceStatus = deskhub::media::ShareSourceStatus;

class SharingHost {
public:
    SharingHost() = default;
    SharingHost(const SharingHost&) = delete;
    SharingHost& operator=(const SharingHost&) = delete;

    bool Start(const std::vector<ShareSource>& sources, const ShareOptions& opt);

    void Stop() {
        engine_.Stop();
        hold_.reset();
    }

    void OfferAudio(std::span<const int16_t> pcm) {
        engine_.OfferAudio(pcm);
    }

    bool audioRunning() const {
        return engine_.audioRunning();
    }

    void StopSource(uint8_t sourceId) {
        engine_.RequestStopSource(sourceId);
    }

    void KickViewer(uint8_t sourceId, uint64_t addrPacked) {
        engine_.RequestKickViewer(sourceId, addrPacked);
    }

    bool running() const {
        return engine_.running();
    }

    std::vector<ShareSourceStatus> Status() {
        return engine_.Status();
    }

    std::string LastError() {
        return engine_.LastError();
    }

    std::string BindWarning() {
        return engine_.BindWarning();
    }

    void OfferLocalClipboard(std::string text) {
        engine_.OfferLocalClipboard(std::move(text));
    }

    std::optional<std::string> TakeRemoteClipboard() {
        return engine_.TakeRemoteClipboard();
    }

    deskhubp::SessionTransport& Socket() {
        return engine_.socket();
    }

    void SetTerminal(deskhubp::TerminalHost* terminal) {
        engine_.SetTerminal(terminal);
    }

    void SetFiles(deskhubp::FileHost* files) {
        engine_.SetFiles(files);
    }

protected:
    bool StartEngine(const std::vector<ShareSource>& sources, const ShareOptions& opt,
        deskhubp::HostEnginePolicy policy) {
        const bool started = engine_.Start(sources, opt, std::move(policy));
        if (!started) hold_.reset();
        return started;
    }

    void HoldWhileSharing(std::shared_ptr<void> hold) {
        hold_ = std::move(hold);
    }

private:
    deskhubp::HostEngine engine_;
    std::shared_ptr<void> hold_;
};

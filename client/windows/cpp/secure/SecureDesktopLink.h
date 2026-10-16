#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <span>
#include <thread>

#include "deskhub/session/host/SecureDesktopWire.h"
#include "secure/OverlappedPipe.h"

class SecureDesktopLink {
public:
    using FrameSink = std::function<void(uint32_t width, uint32_t height,
        std::span<const uint8_t> bgra)>;
    using StateSink = std::function<void(bool secure)>;

    SecureDesktopLink() = default;
    ~SecureDesktopLink();
    SecureDesktopLink(const SecureDesktopLink&) = delete;
    SecureDesktopLink& operator=(const SecureDesktopLink&) = delete;

    bool Start(const deskhub::SecureDesktopWatch& watch, FrameSink onFrame, StateSink onState);
    void Stop();

    bool Secure() const {
        return secure_.load(std::memory_order_acquire);
    }

    bool Forward(const INPUT& input);

private:
    bool ConnectToService();
    bool ReceiveAgentPipe();
    void ReadLoop();
    void Dispatch(const deskhub::SecureDesktopMessage& msg);

    OverlappedPipe control_;
    OverlappedPipe agent_;
    std::thread reader_;
    FrameSink onFrame_;
    StateSink onState_;
    std::atomic<bool> secure_{false};
};

#pragma once
#include "deskhub/session/client/ScreenClient.h"

#include <cstdint>
#include <functional>
#include <span>

namespace deskhubp {

uint32_t MakeClientId(uint8_t sourceId);

using ScreenViewerRecv = std::function<int(uint8_t* buf, size_t cap)>;

struct ScreenViewerLoopHooks {
    std::function<bool()> stopped;
    std::function<void(deskhub::ScreenClient&, uint64_t nowUs)> afterFrames;
    std::function<void(deskhub::ScreenClient&, uint64_t nowUs)> beforeTick;
    std::function<void(bool streaming)> onPhase;
    std::function<void()> onSocketError;
    std::function<bool()> onSessionDead;
};

void RunScreenViewerLoop(const ScreenViewerRecv& recv, deskhub::ScreenClient& screen,
    const ScreenViewerLoopHooks& hooks);

}

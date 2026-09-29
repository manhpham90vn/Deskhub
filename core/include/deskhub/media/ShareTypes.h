#pragma once
#include "deskhub/media/ShareSource.h"
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deskhub::media {

struct ShareOptions {
    uint32_t fps = 60;
    uint32_t bitrateMbps = 20;
    uint32_t maxDim = 1920;
    bool allowInput = true;

    int32_t desktopX = 0, desktopY = 0;
    uint32_t desktopW = 0, desktopH = 0;

    uint16_t port = kDeskhubPort;
    std::string bindIp{};
    std::string deviceName{};
    bool clipboardSync = false;
    bool terminal = false;
    bool files = false;
    bool audio = false;
};

struct ShareSourceStatus {
    uint8_t sourceId = 0;
    std::string name{};
    uint32_t width = 0, height = 0;
    bool viewerConnected = false;
    uint32_t viewerCount = 0;
    std::string viewerAddr{};
    std::vector<std::string> viewerAddrs{};
    std::vector<std::string> viewerNames{};
    double captureFps = 0, sendFps = 0, sendKbps = 0;
    uint32_t rttMs = 0;
    bool zeroCopy = false;
};

}

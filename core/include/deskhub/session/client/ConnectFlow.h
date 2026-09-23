#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <span>

namespace deskhub {

struct ConnectDecision {
    bool showPicker = false;
    uint8_t sourceId = 0;
};

inline ConnectDecision DecideAfterSourceQuery(std::span<const SourceInfo> sources) {
    ConnectDecision d;
    if (sources.size() > 1) {
        d.showPicker = true;
        return d;
    }
    if (!sources.empty()) d.sourceId = sources.front().sourceId;
    return d;
}

}

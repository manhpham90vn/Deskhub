#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace deskhub {

class SourceListResponder {
public:
    void SetSources(std::span<const SourceInfo> sources) {
        sources_.assign(sources.begin(), sources.end());
    }

    void SetCaps(HostCaps caps) {
        caps_ = caps;
    }

    void SetHostName(std::string name) {
        hostName_ = std::move(name);
    }

    size_t Reply(std::span<uint8_t> out, std::span<const uint8_t> pkt, bool trusted = false) const;

private:
    std::vector<SourceInfo> sources_;
    HostCaps caps_{};
    std::string hostName_;
};

}

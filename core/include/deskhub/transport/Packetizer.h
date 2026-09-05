#pragma once
#include "deskhub/protocol/Wire.h"
#include "deskhub/transport/FecScheme.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace deskhub {

class Packetizer {
public:
    using SendFn = std::function<void(std::span<const uint8_t>)>;

    Packetizer();

    void SetSessionId(uint32_t id) {
        sessionId_ = id;
    }
    uint32_t sessionId() const {
        return sessionId_;
    }

    void SetFecEnabled(bool on) {
        fec_ = on;
    }
    bool fecEnabled() const {
        return fec_;
    }

    bool SetFecScheme(std::string_view name);

    bool SetFecParityPerGroup(size_t count) {
        return scheme_->SetParityPerGroup(count);
    }

    size_t fecParityPerGroup() const {
        return scheme_->ParityPerGroup();
    }
    std::string_view fecScheme() const {
        return scheme_->Name();
    }

    void SetFecGroups(size_t groups) {
        groups_ = groups < kMaxSignalledFecGroups ? groups : kMaxSignalledFecGroups;
    }
    size_t fecGroups() const {
        return groups_;
    }

    size_t SendFrame(std::span<const uint8_t> nal, uint32_t frameId, uint64_t timestampUs,
        bool idr, const SendFn& send);

    static constexpr size_t kParityStride = kFecLenPrefix + kMaxVideoPayload;

private:
    uint32_t sessionId_ = 0;
    bool fec_ = false;
    size_t groups_ = 0;
    std::unique_ptr<FecScheme> scheme_;
    uint8_t buf_[kMaxDatagram] = {};
    std::vector<std::span<const uint8_t>> members_;
    std::vector<std::span<const uint8_t>> parityOut_;
};

}

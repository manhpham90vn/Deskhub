#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <span>

namespace deskhub {

inline constexpr uint64_t kInputRepeatIntervalUs = 25'000;
inline constexpr uint32_t kInputRepeatCount = 2;
inline constexpr size_t kInputRedundancy = 8;
inline constexpr size_t kInputBatchMax = 24;

class InputSender {
public:
    using SendFn = std::function<void(std::span<const uint8_t>)>;

    void SetSessionId(uint32_t id) {
        sessionId_ = id;
    }

    void Queue(const InputEvent& e);

    size_t Flush(uint64_t nowUs, const SendFn& send);

    void Reset();

    bool pending() const {
        return unsent_ > 0;
    }

private:
    size_t SendRange(size_t from, size_t to, const SendFn& send);

    std::deque<InputEvent> history_;
    uint32_t nextSeq_ = 0;
    uint32_t firstSeq_ = 0;
    size_t unsent_ = 0;
    uint32_t repeatsLeft_ = 0;
    uint64_t lastSendUs_ = 0;
    uint32_t sessionId_ = 0;
    uint8_t buf_[kMaxDatagram] = {};
};

}

#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deskhub {

inline constexpr uint32_t kClipboardResendCount = 2;
inline constexpr uint64_t kClipboardResendIntervalUs = 60'000;

class ClipboardSync {
public:
    using SendFn = std::function<void(std::span<const uint8_t>)>;

    void SetSessionId(uint32_t id) {
        sessionId_ = id;
    }

    bool OfferLocal(std::string_view text);

    size_t Flush(uint64_t nowUs, const SendFn& send);

    bool Accept(const ClipboardChunkView& chunk);

    std::optional<std::string> TakeCompleted();

    void Reset();

private:
    size_t SendAllChunks(const SendFn& send);
    static uint64_t HashText(std::string_view text);

    std::string outgoing_{};
    uint32_t revision_ = 0;
    uint32_t sendsLeft_ = 0;
    uint64_t lastSendUs_ = 0;
    uint64_t lastSentHash_ = 0;
    uint64_t lastAppliedHash_ = 0;
    bool haveSentHash_ = false;
    bool haveAppliedHash_ = false;

    uint32_t inRevision_ = 0;
    uint32_t lastTakenRevision_ = 0;
    bool haveTakenRevision_ = false;
    std::vector<std::vector<uint8_t>> inChunks_{};
    size_t inReceived_ = 0;
    std::optional<std::string> completed_{};

    uint32_t sessionId_ = 0;
    uint8_t buf_[kMaxDatagram] = {};
};

}

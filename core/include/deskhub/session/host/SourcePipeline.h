#pragma once
#include "deskhub/control/StreamSize.h"
#include "deskhub/media/ShareTypes.h"
#include "deskhub/media/VideoTypes.h"
#include "deskhub/protocol/Wire.h"
#include "deskhub/session/host/SourcePipelineState.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deskhub {

inline constexpr uint64_t kIdrFlushIdleUs = 200'000;
inline constexpr uint64_t kKeepaliveIdleUs = 500'000;
inline constexpr uint64_t kKeepaliveIntervalUs = 500'000;
inline constexpr uint32_t kReconfigSends = 3;
inline constexpr uint64_t kReconfigResendIntervalUs = 100'000;

SourcePipelineState* RouteDatagram(std::span<SourcePipelineState* const> live,
    const CommonHeader& header, std::span<const uint8_t> pkt);

struct AcceptedDatagram {
    bool parsed = false;
    SourcePipelineState* target = nullptr;
};

AcceptedDatagram AcceptDatagram(std::span<SourcePipelineState* const> live,
    std::span<const uint8_t> pkt, uint64_t packedFrom, uint64_t nowUs);

struct OfferUpdate {
    bool sizeChanged = false;
    bool qualityChanged = false;
    bool sendReconfig = false;
    Reconfig reconfig{};
};

OfferUpdate RefreshOffer(SourcePipelineState& st, uint8_t fallbackFps);

std::optional<Reconfig> TakeReconfigToSend(SourcePipelineState& st, uint64_t nowUs);

StreamSize RetargetStream(SourcePipelineState& st, uint32_t maxDim);

struct FrameAdmission {
    bool drop = false;
    bool rebuildEncoder = false;
    StreamSize encode;
    std::string sizeNote;
    std::string pauseNote;
};

FrameAdmission AdmitCapturedFrame(SourcePipelineState& st, uint32_t nativeW, uint32_t nativeH,
    uint32_t maxDim);

media::EncoderConfig MakeEncoderConfig(const SourcePipelineState& st, StreamSize encode,
    uint32_t fallbackFps);

enum class FlushReason : uint8_t { None,
    ForceIdr,
    Keepalive };

FlushReason DueForFlush(const SourcePipelineState& st, uint64_t nowUs);

FlushReason TakeFlushReason(SourcePipelineState& st, uint64_t nowUs);

struct NegotiationHooks {
    std::function<StreamSize(uint16_t clientW, uint16_t clientH)> resolveSize;
};

struct NegotiationResult {
    bool accepted = false;
    StreamSize size;
    int rungCount = 0;
};

NegotiationResult BeginNegotiation(SourcePipelineState& st, const Hello& hello, uint8_t maxFps,
    const NegotiationHooks& hooks);

struct StatusExtras {
    std::string viewerAddr;
    std::vector<std::string> viewerAddrs;
    std::vector<std::string> viewerNames;
    bool zeroCopy = false;
};

media::ShareSourceStatus MakeSourceStatus(const SourcePipelineState& st,
    const StatusExtras& extras);

SourceInfo MakeSourceInfo(const SourcePipelineState& st);

}

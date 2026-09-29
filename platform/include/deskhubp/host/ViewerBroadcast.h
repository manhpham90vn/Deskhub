#pragma once
#include "deskhub/media/ShareTypes.h"
#include "deskhub/session/host/SourceListResponder.h"
#include "deskhub/session/host/SourcePipelineState.h"
#include "deskhubp/net/SessionTransport.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace deskhubp {

using SourcePredicate = std::function<bool(const deskhub::SourcePipelineState&)>;

size_t SendToViewers(const deskhub::SourcePipelineState& st, SessionTransport& sock,
    std::span<const uint8_t> datagram);

std::string ViewerAddrList(const deskhub::SourcePipelineState& st);

void SendEncodedFrame(deskhub::SourcePipelineState& st, SessionTransport& sock,
    std::span<const uint8_t> frame, uint64_t timestampUs, bool keyframe);

size_t SendAudioFrame(deskhub::SourcePipelineState& st, SessionTransport& sock,
    std::span<const uint8_t> opusFrame, uint32_t seq, uint64_t timestampUs);

void LogListeningAddresses(uint16_t port, const std::string& boundIp = {});

std::vector<deskhub::SourcePipelineState*> SelectLiveSources(
    std::span<deskhub::SourcePipelineState* const> pipes, const SourcePredicate& closed,
    const std::function<bool()>& aborted,
    const std::function<void(deskhub::SourcePipelineState&)>& shutdown);

void EndScreenHostSession(deskhub::SourcePipelineState& st, SessionTransport& sock);

struct SourceStatusHooks {
    SourcePredicate closed;
    SourcePredicate zeroCopy;
};

std::vector<deskhub::media::ShareSourceStatus> PublishSourceStatus(
    std::span<deskhub::SourcePipelineState* const> live, deskhub::SourceListResponder& sourceList,
    const SourceStatusHooks& hooks);

void LogTransferTotals(std::span<deskhub::SourcePipelineState* const> pipes);

}

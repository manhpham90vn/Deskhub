#include "deskhub/session/LinkPulse.h"

namespace deskhub {

void LinkPulse::Reset() {
    *this = LinkPulse{};
}

void LinkPulse::Tick(uint64_t nowUs) {
    const uint64_t lastUs = lastTickUs_;
    lastTickUs_ = nowUs;
    if (lastUs == 0 || nowUs <= lastUs) return;
    if (lastPongUs_ == 0 || lastPongUs_ >= nowUs) return;
    const uint64_t sinceUs = nowUs - lastUs;
    if (sinceUs <= kLinkWatchStepUs) return;
    const uint64_t unwatchedUs = sinceUs - kLinkWatchStepUs;
    const uint64_t silentUs = nowUs - lastPongUs_;
    lastPongUs_ = unwatchedUs >= silentUs ? nowUs : lastPongUs_ + unwatchedUs;
}

bool LinkPulse::PingDue(uint64_t nowUs) const {
    return lastPingUs_ == 0 || nowUs - lastPingUs_ >= kLinkPingIntervalUs;
}

PingPong LinkPulse::MakePing(uint64_t nowUs) {
    const PingPong ping{nextPingId_++, nowUs};
    sent_[ping.pingId % kLinkPingWindow] = SentPing{ping.pingId, nowUs, false};
    lastPingUs_ = nowUs;
    return ping;
}

void LinkPulse::OnPong(const PingPong& pong, uint64_t nowUs) {
    SentPing& slot = sent_[pong.pingId % kLinkPingWindow];
    if (slot.pingId != pong.pingId || slot.answered) return;
    if (slot.sentUs != pong.sendTimeUs || nowUs < slot.sentUs) return;
    slot.answered = true;
    lastPongUs_ = nowUs;
    answeredOnce_ = true;
}

bool LinkPulse::Stalled(uint64_t nowUs) const {
    return answeredOnce_ && nowUs - lastPongUs_ > kLinkStallAfterUs;
}

}

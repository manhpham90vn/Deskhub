#include "deskhub/session/LinkPulse.h"

namespace deskhub {

LinkQuality ClassifyLinkQuality(bool haveRtt, uint32_t rttUs, uint8_t lossPct) {
    if (!haveRtt) return LinkQuality::Unknown;
    if (rttUs >= kLinkFairMaxRttUs || lossPct > kLinkFairMaxLossPct) return LinkQuality::Poor;
    if (rttUs < kLinkGoodMaxRttUs && lossPct == 0) return LinkQuality::Good;
    return LinkQuality::Fair;
}

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

bool LinkPulse::OnPong(const PingPong& pong, uint64_t nowUs) {
    SentPing& slot = sent_[pong.pingId % kLinkPingWindow];
    if (slot.pingId != pong.pingId || slot.answered) return false;
    if (slot.sentUs != pong.sendTimeUs || nowUs < slot.sentUs) return false;
    slot.answered = true;
    lastPongUs_ = nowUs;
    const uint64_t sample = nowUs - slot.sentUs;
    smoothedRttUs_ = haveRtt_ ? (smoothedRttUs_ * 7 + sample) / 8 : sample;
    haveRtt_ = true;
    return true;
}

bool LinkPulse::Stalled(uint64_t nowUs) const {
    return haveRtt_ && nowUs - lastPongUs_ > kLinkStallAfterUs;
}

LinkPulseView LinkPulse::View(uint64_t nowUs) const {
    uint32_t decided = 0;
    uint32_t lost = 0;
    for (const SentPing& ping : sent_) {
        if (ping.pingId == 0) continue;
        if (ping.answered) {
            ++decided;
            continue;
        }
        if (nowUs - ping.sentUs < kLinkPingLostAfterUs) continue;
        ++decided;
        ++lost;
    }

    LinkPulseView view;
    view.haveRtt = haveRtt_;
    view.rttUs = uint32_t(smoothedRttUs_);
    view.lossPct = decided ? uint8_t(lost * 100 / decided) : uint8_t(0);
    view.quality = Stalled(nowUs) ? LinkQuality::Poor
                                  : ClassifyLinkQuality(haveRtt_, view.rttUs, view.lossPct);
    return view;
}

}

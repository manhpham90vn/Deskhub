#include "deskhub/media/RecoveryPolicy.h"

namespace deskhub::media {

const char* RecoveryActionName(RecoveryAction action) {
    switch (action) {
        case RecoveryAction::None: return "none";
        case RecoveryAction::Keyframe: return "keyframe";
        case RecoveryAction::InvalidateReference: return "invalidate_ref";
        case RecoveryAction::IntraRefresh: return "intra_refresh";
    }
    return "?";
}

void RecoveryPolicy::Reset() {
    newestLongTerm_ = 0;
    haveLongTerm_ = false;
    refreshFramesLeft_ = 0;
    recoveryPending_ = false;
}

bool RecoveryPolicy::ShouldMarkLongTerm(uint32_t frameId) const {
    if (!caps_.longTermReference) return false;
    return frameId % kLongTermIntervalFrames == 0;
}

void RecoveryPolicy::NoteEncoded(uint32_t frameId, bool idr, bool markedLongTerm) {
    if (refreshFramesLeft_) --refreshFramesLeft_;
    recoveryPending_ = false;

    if (!caps_.longTermReference) return;
    if (!idr && !markedLongTerm) return;
    if (haveLongTerm_ && frameId <= newestLongTerm_) return;
    newestLongTerm_ = frameId;
    haveLongTerm_ = true;
}

RecoveryDecision RecoveryPolicy::OnReferenceLost(uint32_t lostFrameId) {
    RecoveryDecision decision;

    if (refreshFramesLeft_ && !recoveryPending_) {
        decision.action = RecoveryAction::None;
        return decision;
    }

    const bool canInvalidate = caps_.longTermReference && haveLongTerm_ &&
                               newestLongTerm_ < lostFrameId && !recoveryPending_;
    if (canInvalidate) {
        decision.action = RecoveryAction::InvalidateReference;
        decision.referenceFrameId = newestLongTerm_;
        recoveryPending_ = true;
        return decision;
    }

    if (caps_.intraRefresh && !refreshFramesLeft_ && !recoveryPending_) {
        decision.action = RecoveryAction::IntraRefresh;
        refreshFramesLeft_ = kIntraRefreshFrames;
        recoveryPending_ = true;
        return decision;
    }

    decision.action = RecoveryAction::Keyframe;
    haveLongTerm_ = false;
    refreshFramesLeft_ = 0;
    recoveryPending_ = true;
    return decision;
}

}

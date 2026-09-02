#pragma once
#include <cstdint>

namespace deskhub::media {

enum class RecoveryAction : uint8_t {
    None,
    Keyframe,
    InvalidateReference,
    IntraRefresh,
};

const char* RecoveryActionName(RecoveryAction action);

struct EncoderRecoveryCaps {
    bool longTermReference = false;
    bool intraRefresh = false;
};

struct RecoveryDecision {
    RecoveryAction action = RecoveryAction::Keyframe;
    uint32_t referenceFrameId = 0;
};

class RecoveryPolicy {
public:
    static constexpr uint32_t kLongTermIntervalFrames = 30;
    static constexpr uint32_t kIntraRefreshFrames = 30;

    explicit RecoveryPolicy(EncoderRecoveryCaps caps = {}) : caps_(caps) {}

    void SetCaps(EncoderRecoveryCaps caps) {
        caps_ = caps;
        Reset();
    }

    const EncoderRecoveryCaps& caps() const {
        return caps_;
    }

    bool ShouldMarkLongTerm(uint32_t frameId) const;

    void NoteEncoded(uint32_t frameId, bool idr, bool markedLongTerm);

    RecoveryDecision OnReferenceLost(uint32_t lostFrameId);

    bool refreshing() const {
        return refreshFramesLeft_ != 0;
    }

    bool haveLongTerm() const {
        return haveLongTerm_;
    }

    uint32_t newestLongTerm() const {
        return newestLongTerm_;
    }

    void Reset();

private:
    EncoderRecoveryCaps caps_{};
    uint32_t newestLongTerm_ = 0;
    bool haveLongTerm_ = false;
    uint32_t refreshFramesLeft_ = 0;
    bool recoveryPending_ = false;
};

}

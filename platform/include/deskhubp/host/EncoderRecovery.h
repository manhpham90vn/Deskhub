#pragma once
#include "deskhub/media/RecoveryPolicy.h"
#include "deskhub/media/VideoContract.h"
#include "deskhub/session/host/SourcePipelineState.h"
#include "deskhubp/diag/Log.h"

#include <cstdint>

namespace deskhubp {

template <class Encoder>
bool PrepareRecovery(deskhub::SourcePipelineState& st, Encoder& encoder, uint32_t frameId,
    bool idr) {
    const uint32_t firstInvalidFrame = st.invalidateBeforeFrame.exchange(0);
    if (firstInvalidFrame) {
        bool applied = false;
        if constexpr (deskhub::media::ReferenceInvalidatingEncoder<Encoder>)
            applied = encoder.InvalidateReference(firstInvalidFrame);
        if (!applied) {
            LOGW(
                "[Host][%s] The encoder kept no reference older than frame %u, so recovery "
                "falls back to an IDR.",
                st.name.c_str(), firstInvalidFrame);
            idr = true;
        }
    }

    const uint32_t refreshFrames = st.wantIntraRefresh.exchange(false)
                                       ? deskhub::media::RecoveryPolicy::kIntraRefreshFrames
                                       : 0;
    if (refreshFrames) {
        bool applied = false;
        if constexpr (deskhub::media::IntraRefreshEncoder<Encoder>)
            applied = encoder.BeginIntraRefresh(refreshFrames);
        if (!applied) {
            LOGW(
                "[Host][%s] The encoder would not start an intra refresh, so recovery falls "
                "back to an IDR.",
                st.name.c_str());
            idr = true;
        }
    }

    if constexpr (deskhub::media::ReferenceInvalidatingEncoder<Encoder>) {
        if (idr || st.recovery.ShouldMarkLongTerm(frameId))
            encoder.MarkLongTermReference(frameId);
    }
    return idr;
}

}

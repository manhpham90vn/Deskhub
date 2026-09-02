#pragma once
#include "deskhub/control/CongestionControl.h"
#include "deskhub/media/RecoveryPolicy.h"
#include "deskhub/control/QualityLadder.h"
#include "deskhub/diag/ShareDiag.h"
#include "deskhub/session/ClipboardSync.h"
#include "deskhub/session/host/ScreenHostSession.h"
#include "deskhub/transport/Pacer.h"
#include "deskhub/transport/Packetizer.h"
#include "deskhub/transport/RetransmitCache.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>

namespace deskhub {

struct SourcePipelineState {
    SourcePipelineState(uint32_t startBps, uint32_t minBps, diag::ShareDiagCaps caps = {})
        : curBitrateBps(startBps),
          rate(MakeCongestionControl(kDefaultCongestionControl, startBps, minBps)),
          diag(caps) {}

    bool SetCongestionControl(std::string_view control, uint32_t startBps, uint32_t minBps) {
        std::unique_ptr<CongestionControl> made =
            MakeCongestionControl(control, startBps, minBps);
        if (!made) return false;
        rate = std::move(made);
        return true;
    }

    virtual ~SourcePipelineState() = default;

    SourcePipelineState(const SourcePipelineState&) = delete;
    SourcePipelineState& operator=(const SourcePipelineState&) = delete;

    uint8_t sourceId = 0;
    std::string name;

    std::unique_ptr<ScreenHostSession> session;
    StreamParams offer;
    Packetizer packetizer;
    Pacer pacer;
    ClipboardSync clipOut;

    RetransmitCache retxCache;
    std::mutex retxMutex;

    std::atomic<uint32_t> srcW{0}, srcH{0};
    std::atomic<uint32_t> curFps{0};
    std::atomic<uint32_t> curBitrateBps{0};

    std::atomic<uint32_t> nativeW{0}, nativeH{0};
    std::atomic<uint32_t> wantW{0}, wantH{0};
    uint32_t cliW = 0, cliH = 0;

    std::atomic<bool> sizeChanged{false};
    std::atomic<bool> qualityChanged{false};
    std::atomic<bool> wantFec{true};
    std::atomic<uint32_t> wantFecParity{1};
    std::atomic<uint32_t> fecParityPin{0};
    std::atomic<bool> fecArmedAlways{false};
    std::atomic<bool> fecArmedNever{false};
    media::RecoveryPolicy recovery{};
    std::atomic<uint32_t> invalidateBeforeFrame{0};
    std::atomic<bool> wantIntraRefresh{false};
    std::atomic<bool> netReady{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> paused{false};
    std::atomic<bool> forceIdr{false};
    bool shutdownDone = false;

    std::atomic<uint64_t> replyPacked{0};
    std::atomic<uint64_t> bytesSent{0}, framesSent{0};
    std::atomic<uint32_t> captured{0};
    std::atomic<uint32_t> nextFrameId{0};

    std::atomic<uint32_t> uiRttMs{0};
    std::atomic<uint32_t> uiLossPct{0}, uiRecvKbps{0};
    std::atomic<bool> haveFeedback{false};

    std::atomic<uint64_t> lastFrameUs{0};
    uint64_t lastKeepaliveUs = 0;

    diag::WindowMax frameAgeMs;
    std::unique_ptr<QualityLadder> ladder;
    QualityStep step;
    std::unique_ptr<CongestionControl> rate;

    diag::SourceRate statRate;
    diag::SourceRate::Window statWindow;
    diag::SourceDiag diag;
};

inline size_t ViewerCountOf(const SourcePipelineState& st) {
    return st.session ? st.session->viewerCount() : 0;
}

inline size_t SnapshotViewerAddrs(const SourcePipelineState& st, std::span<uint64_t> out) {
    return st.session ? st.session->SnapshotViewerAddrs(out) : 0;
}

inline size_t SnapshotAudioViewerAddrs(const SourcePipelineState& st,
    std::span<uint64_t> out) {
    return st.session ? st.session->SnapshotAudioViewerAddrs(out) : 0;
}

inline size_t SnapshotViewerInfos(const SourcePipelineState& st, std::span<ViewerInfo> out) {
    return st.session ? st.session->SnapshotViewerInfos(out) : 0;
}

}

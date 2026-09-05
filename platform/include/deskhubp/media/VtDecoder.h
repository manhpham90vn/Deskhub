#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "deskhub/control/VideoPacer.h"
#include "deskhub/media/PresentCounters.h"
#include "deskhub/media/VideoContract.h"
#include <vector>

class VtDecoder {
public:
    VtDecoder() = default;
    ~VtDecoder();
    VtDecoder(const VtDecoder&) = delete;
    VtDecoder& operator=(const VtDecoder&) = delete;

    bool Init(void* layer, int width, int height);
    void Shutdown();
    bool IsOpen() const {
        return layer_ != nullptr;
    }

    void SetPacing(bool adaptiveLead, uint64_t displayIntervalUs) {
        pacer_.SetAdaptiveLead(adaptiveLead);
        pacer_.SetDisplayIntervalUs(displayIntervalUs);
    }

    bool SetClockOffset(std::string_view name) {
        return pacer_.SetClockOffset(name);
    }

    bool Decode(const uint8_t* nal, size_t len, uint64_t ptsUs);

    uint32_t TakeRenderedCount() {
        return counters_.TakeRenderedCount();
    }

    uint32_t TakeCongestionDrops() {
        return counters_.TakeCongestionDrops();
    }

    uint64_t lastRenderedPtsUs() const {
        return counters_.lastRenderedPtsUs();
    }

    uint32_t TakePresentDelayMs() {
        return counters_.TakePresentDelayMs();
    }

    uint64_t lastRenderedAtUs() const {
        return counters_.lastRenderedAtUs();
    }

private:
    static constexpr uint32_t kPacedStallDrops = 30;

    bool EnsurePacedTimebase(uint64_t ptsUs, uint64_t nowUs);
    void DisablePacing();

    void* layer_ = nullptr;
    void* formatDesc_ = nullptr;
    void* timebase_ = nullptr;
    deskhub::VideoPacer pacer_;
    bool timebaseRunning_ = false;
    bool paceDisabled_ = false;
    uint32_t pacedCongestionRun_ = 0;
    deskhub::media::PresentCounters counters_;

    uint8_t sps_[256] = {};
    uint8_t pps_[256] = {};
    size_t spsLen_ = 0;
    size_t ppsLen_ = 0;

    std::vector<uint8_t> avcc_;
};

static_assert(deskhub::media::EngineDecoder<VtDecoder, void*>,
    "VtDecoder must decode, restart in place, and bind to the layer it is handed at Init");
static_assert(deskhub::media::RenderCountingDecoder<VtDecoder>,
    "VtDecoder counts presented frames itself — the enqueue is async so Decode cannot count them");
static_assert(deskhub::media::CongestionAwareDecoder<VtDecoder>,
    "VtDecoder must report frames swallowed by the display layer — that is where disp_drop comes from");
static_assert(deskhub::media::PresentTimingDecoder<VtDecoder>,
    "VtDecoder paces presentation, so e2e must be measured at the scheduled display time");

#include "deskhubp/audio/AudioCapture.h"

#include <utility>

namespace deskhubp {

struct AudioCapture::Impl {};

AudioCapture::AudioCapture() = default;
AudioCapture::~AudioCapture() = default;

bool AudioCapture::Start(const deskhub::media::AudioFormat&, FrameHandler onFrame) {
    const FrameHandler discarded = std::move(onFrame);
    return false;
}

void AudioCapture::Stop() {}

bool AudioCapture::Running() const {
    return false;
}

const char* AudioCapture::BackendName() {
    return "none";
}

}

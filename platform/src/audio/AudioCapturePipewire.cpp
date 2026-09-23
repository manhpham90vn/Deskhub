#include "PwStream.h"

#include "deskhubp/audio/AudioCapture.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace deskhubp {

struct AudioCapture::Impl {
    deskhub::media::AudioFormat format{};
    FrameHandler onFrame;

    std::vector<int16_t> staging;
    size_t staged = 0;

    void Feed(const int16_t* samples, size_t count) {
        while (count > 0) {
            const size_t room = staging.size() - staged;
            const size_t take = std::min(room, count);
            std::memcpy(staging.data() + staged, samples, take * sizeof(int16_t));
            staged += take;
            samples += take;
            count -= take;
            if (staged < staging.size()) return;
            if (onFrame) onFrame(staging);
            staged = 0;
        }
    }

    static void OnProcess(void* userdata);
    static void OnStateChanged(void* userdata, pw_stream_state old, pw_stream_state now,
        const char* error);
    static const pw_stream_events kEvents;

    PwStream pw;
};

void AudioCapture::Impl::OnProcess(void* userdata) {
    auto* impl = static_cast<AudioCapture::Impl*>(userdata);
    pw_buffer* b = pw_stream_dequeue_buffer(impl->pw.stream());
    if (b == nullptr) return;

    const spa_data& d = b->buffer->datas[0];
    if (d.data != nullptr && d.chunk->size > 0) {
        const auto* samples = static_cast<const int16_t*>(d.data);
        const size_t offset = d.chunk->offset / sizeof(int16_t);
        impl->Feed(samples + offset, d.chunk->size / sizeof(int16_t));
    }
    pw_stream_queue_buffer(impl->pw.stream(), b);
}

void AudioCapture::Impl::OnStateChanged(void* userdata, pw_stream_state old, pw_stream_state now,
    const char* error) {
    (void)userdata;
    if (now == PW_STREAM_STATE_ERROR) LogPwStreamError("capture_error", old, error);
}

const pw_stream_events AudioCapture::Impl::kEvents =
    MakePwStreamEvents(&AudioCapture::Impl::OnProcess, &AudioCapture::Impl::OnStateChanged);

AudioCapture::AudioCapture() = default;

AudioCapture::~AudioCapture() {
    Stop();
}

bool AudioCapture::Start(const deskhub::media::AudioFormat& format, FrameHandler onFrame) {
    Stop();
    if (!deskhub::media::IsSupportedAudioFormat(format) || !onFrame) return false;

    auto impl = std::make_unique<Impl>();
    impl->format = format;
    impl->onFrame = std::move(onFrame);
    impl->staging.assign(format.interleavedSamples(), 0);

    const PwStreamOptions options{"deskhub-audio-capture", "Deskhub share", "deskhub-share",
        "Capture", true, PW_DIRECTION_INPUT, "capture_open_fail", "capture_connect_fail", false};
    if (!impl->pw.Open(options, Impl::kEvents, impl.get(), format)) return false;

    impl_ = std::move(impl);
    LOGI("[audio] evt=capture_open source=default sink monitor rate=%u ch=%u", format.sampleRate,
        format.channels);
    return true;
}

void AudioCapture::Stop() {
    if (!impl_) return;
    impl_->pw.Close();
    impl_.reset();
}

bool AudioCapture::Running() const {
    return impl_ != nullptr;
}

const char* AudioCapture::BackendName() {
    return "pipewire";
}

}

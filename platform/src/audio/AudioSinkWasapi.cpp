#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "deskhub/media/PcmRing.h"
#include "deskhubp/audio/AudioSink.h"
#include "deskhubp/diag/Log.h"

#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <objbase.h>
#include <wrl/client.h>

#include <atomic>
#include <thread>

namespace deskhubp {

namespace {

constexpr REFERENCE_TIME kBufferDuration = 400'000;
constexpr DWORD kRenderWaitMs = 200;

struct ComScope {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComScope() {
        if (SUCCEEDED(hr)) CoUninitialize();
    }
};

WAVEFORMATEX MakeWaveFormat(const deskhub::media::AudioFormat& format) {
    WAVEFORMATEX wf{};
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = WORD(format.channels);
    wf.nSamplesPerSec = format.sampleRate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = WORD(wf.nChannels * wf.wBitsPerSample / 8);
    wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
    wf.cbSize = 0;
    return wf;
}

}

struct AudioSink::Impl {
    deskhub::media::AudioFormat format{};

    deskhub::media::PcmRing ring;

    std::thread thread;
    std::atomic<bool> quit{false};
    std::atomic<bool> opened{false};

    HANDLE bufferReady = nullptr;
    HANDLE ready = nullptr;

    void Run();
};

void AudioSink::Impl::Run() {
    ComScope com;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    Microsoft::WRL::ComPtr<IMMDevice> device;
    if (SUCCEEDED(hr)) hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

    Microsoft::WRL::ComPtr<IAudioClient> client;
    if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);

    const WAVEFORMATEX wf = MakeWaveFormat(format);
    if (SUCCEEDED(hr))
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            kBufferDuration, 0, &wf, nullptr);
    if (SUCCEEDED(hr)) hr = client->SetEventHandle(bufferReady);

    UINT32 bufferFrames = 0;
    if (SUCCEEDED(hr)) hr = client->GetBufferSize(&bufferFrames);

    Microsoft::WRL::ComPtr<IAudioRenderClient> render;
    if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&render));
    if (SUCCEEDED(hr)) hr = client->Start();

    if (FAILED(hr)) {
        LOGE("[audio] evt=sink_open_fail backend=wasapi hr=0x%08lx", static_cast<unsigned long>(hr));
        SetEvent(ready);
        return;
    }

    opened.store(true, std::memory_order_release);
    SetEvent(ready);

    while (!quit.load(std::memory_order_acquire)) {
        if (WaitForSingleObject(bufferReady, kRenderWaitMs) != WAIT_OBJECT_0) continue;

        UINT32 padding = 0;
        if (FAILED(client->GetCurrentPadding(&padding))) break;
        const UINT32 free = bufferFrames - padding;
        if (!free) continue;

        BYTE* raw = nullptr;
        if (FAILED(render->GetBuffer(free, &raw))) break;

        auto* out = reinterpret_cast<int16_t*>(raw);
        const size_t wanted = size_t(free) * format.channels;
        ring.TakeOrSilence(out, wanted);
        render->ReleaseBuffer(free, 0);
    }

    client->Stop();
}

AudioSink::AudioSink() = default;

AudioSink::~AudioSink() {
    Close();
}

bool AudioSink::Open(const deskhub::media::AudioFormat& format) {
    Close();
    if (!deskhub::media::IsSupportedAudioFormat(format)) return false;

    auto impl = std::make_unique<Impl>();
    impl->format = format;
    impl->ring.Open(format);
    impl->bufferReady = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    impl->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (impl->bufferReady == nullptr || impl->ready == nullptr) {
        if (impl->bufferReady != nullptr) CloseHandle(impl->bufferReady);
        if (impl->ready != nullptr) CloseHandle(impl->ready);
        return false;
    }

    Impl* raw = impl.get();
    impl->thread = std::thread([raw] { raw->Run(); });
    WaitForSingleObject(impl->ready, INFINITE);

    if (!impl->opened.load(std::memory_order_acquire)) {
        impl->quit.store(true, std::memory_order_release);
        impl->thread.join();
        CloseHandle(impl->bufferReady);
        CloseHandle(impl->ready);
        return false;
    }

    impl_ = std::move(impl);
    LOGI("[audio] evt=sink_open backend=wasapi rate=%u ch=%u", format.sampleRate,
        format.channels);
    return true;
}

void AudioSink::Close() {
    if (!impl_) return;
    impl_->quit.store(true, std::memory_order_release);
    SetEvent(impl_->bufferReady);
    if (impl_->thread.joinable()) impl_->thread.join();
    CloseHandle(impl_->bufferReady);
    CloseHandle(impl_->ready);
    impl_.reset();
}

bool AudioSink::Write(std::span<const int16_t> pcm) {
    if (!impl_ || pcm.empty()) return false;
    if (pcm.size() != impl_->format.interleavedSamples()) return false;
    impl_->ring.Put(pcm);
    return true;
}

bool AudioSink::IsOpen() const {
    return impl_ != nullptr;
}

size_t AudioSink::framesQueued() const {
    return impl_ ? impl_->ring.framesQueued() : 0;
}

uint64_t AudioSink::framesDropped() const {
    return impl_ ? impl_->ring.framesDropped() : 0;
}

uint64_t AudioSink::framesStarved() const {
    return impl_ ? impl_->ring.framesStarved() : 0;
}

const char* AudioSink::BackendName() {
    return "wasapi";
}

}

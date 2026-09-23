#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "deskhubp/audio/AudioCapture.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

#include <windows.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <objbase.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace deskhubp {

namespace {

constexpr REFERENCE_TIME kBufferDuration = 2'000'000;
constexpr DWORD kPollMs = 5;

struct ComScope {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComScope() {
        if (SUCCEEDED(hr)) CoUninitialize();
    }
};

bool IsFloatFormat(const WAVEFORMATEX& wf) {
    if (wf.wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wf.wFormatTag != WAVE_FORMAT_EXTENSIBLE) return false;
    const auto& ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(wf);
    return ext.SubFormat.Data1 == WAVE_FORMAT_IEEE_FLOAT;
}

int16_t ClampToPcm(float sample) {
    const float scaled = sample * 32767.0f;
    if (scaled >= 32767.0f) return 32767;
    if (scaled <= -32768.0f) return -32768;
    return int16_t(scaled);
}

}

struct AudioCapture::Impl {
    deskhub::media::AudioFormat format{};
    AudioCapture::FrameHandler onFrame;

    std::vector<int16_t> staging;
    size_t staged = 0;

    std::thread thread;
    std::atomic<bool> quit{false};
    std::atomic<bool> opened{false};
    bool emittedAny = false;
    HANDLE ready = nullptr;

    void Emit() {
        if (onFrame) onFrame(staging);
        emittedAny = true;
        staged = 0;
    }

    void FeedInterleaved(const void* data, size_t deviceFrames, const WAVEFORMATEX& wf,
        bool silent) {
        const size_t deviceChannels = wf.nChannels;
        const bool isFloat = IsFloatFormat(wf);
        const auto* asFloat = static_cast<const float*>(data);
        const auto* asPcm = static_cast<const int16_t*>(data);

        for (size_t f = 0; f < deviceFrames; ++f) {
            for (uint32_t c = 0; c < format.channels; ++c) {
                const size_t at = f * deviceChannels + std::min<size_t>(c, deviceChannels - 1);
                int16_t sample = 0;
                if (!silent) sample = isFloat ? ClampToPcm(asFloat[at]) : asPcm[at];
                staging[staged++] = sample;
                if (staged == staging.size()) Emit();
            }
        }
    }

    void PadWithSilence() {
        std::fill(staging.begin() + std::ptrdiff_t(staged), staging.end(), int16_t(0));
        staged = staging.size();
        Emit();
    }

    void Run();
};

void AudioCapture::Impl::Run() {
    ComScope com;

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator));
    Microsoft::WRL::ComPtr<IMMDevice> device;
    if (SUCCEEDED(hr)) hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);

    Microsoft::WRL::ComPtr<IAudioClient> client;
    if (SUCCEEDED(hr)) hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);

    WAVEFORMATEX* mix = nullptr;
    if (SUCCEEDED(hr)) hr = client->GetMixFormat(&mix);
    if (FAILED(hr) || mix == nullptr) {
        LOGE("[audio] evt=capture_open_fail backend=wasapi hr=0x%08lx",
            static_cast<unsigned long>(hr));
        SetEvent(ready);
        return;
    }

    const WAVEFORMATEX wf = *mix;
    if (wf.nSamplesPerSec != format.sampleRate) {
        LOGW("[audio] evt=capture_rate_mismatch device=%lu want=%u note=sharing without sound",
            static_cast<unsigned long>(wf.nSamplesPerSec), format.sampleRate);
        CoTaskMemFree(mix);
        SetEvent(ready);
        return;
    }

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
        kBufferDuration, 0, mix, nullptr);
    CoTaskMemFree(mix);

    Microsoft::WRL::ComPtr<IAudioCaptureClient> capture;
    if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&capture));
    if (SUCCEEDED(hr)) hr = client->Start();
    if (FAILED(hr)) {
        LOGE("[audio] evt=capture_start_fail backend=wasapi hr=0x%08lx",
            static_cast<unsigned long>(hr));
        SetEvent(ready);
        return;
    }

    opened.store(true, std::memory_order_release);
    SetEvent(ready);
    LOGI("[audio] evt=capture_open backend=wasapi source=default render loopback rate=%u ch=%u",
        format.sampleRate, format.channels);

    const uint64_t frameUs = uint64_t(format.samplesPerFrame) * 1'000'000 / format.sampleRate;
    uint64_t nextDueUs = NowUs() + frameUs;

    while (!quit.load(std::memory_order_acquire)) {
        UINT32 available = 0;
        while (SUCCEEDED(capture->GetNextPacketSize(&available)) && available > 0) {
            BYTE* data = nullptr;
            UINT32 deviceFrames = 0;
            DWORD flags = 0;
            if (FAILED(capture->GetBuffer(&data, &deviceFrames, &flags, nullptr, nullptr))) break;
            FeedInterleaved(data, deviceFrames, wf, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0);
            capture->ReleaseBuffer(deviceFrames);
        }

        const uint64_t nowUs = NowUs();
        if (nowUs >= nextDueUs) {
            if (staged > 0 || emittedAny) PadWithSilence();
            nextDueUs = nowUs + frameUs;
        } else {
            Sleep(kPollMs);
        }
    }

    client->Stop();
}

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
    impl->ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (impl->ready == nullptr) return false;

    Impl* raw = impl.get();
    impl->thread = std::thread([raw] { raw->Run(); });
    WaitForSingleObject(impl->ready, INFINITE);

    if (!impl->opened.load(std::memory_order_acquire)) {
        impl->quit.store(true, std::memory_order_release);
        impl->thread.join();
        CloseHandle(impl->ready);
        return false;
    }

    impl_ = std::move(impl);
    return true;
}

void AudioCapture::Stop() {
    if (!impl_) return;
    impl_->quit.store(true, std::memory_order_release);
    if (impl_->thread.joinable()) impl_->thread.join();
    CloseHandle(impl_->ready);
    impl_.reset();
}

bool AudioCapture::Running() const {
    return impl_ != nullptr;
}

const char* AudioCapture::BackendName() {
    return "wasapi";
}

}

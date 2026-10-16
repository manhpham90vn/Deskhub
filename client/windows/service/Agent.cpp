#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Agent.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cwchar>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "DuplicationCapture.h"
#include "deskhub/session/host/SecureDesktopWire.h"
#include "deskhubp/diag/Log.h"
#include "secure/OverlappedPipe.h"

#pragma comment(lib, "user32.lib")

namespace {

constexpr auto kDesktopPoll = std::chrono::milliseconds(100);
constexpr UINT kAcquireTimeoutMs = 100;
constexpr DWORD kIdleSleepMs = 100;
constexpr DWORD kRetryOpenMs = 250;
constexpr uint64_t kMinFrameIntervalMs = 100;
constexpr size_t kReadChunk = 64 * 1024;
constexpr size_t kMaxQueuedInputs = 4096;

std::string DesktopName(HDESK desk) {
    wchar_t name[256] = {};
    DWORD needed = 0;
    if (!GetUserObjectInformationW(desk, UOI_NAME, name, sizeof(name) - sizeof(wchar_t), &needed))
        return {};
    std::string ascii;
    for (const wchar_t* p = name; *p; ++p) ascii.push_back(*p < 0x80 ? char(*p) : '?');
    return ascii;
}

class ThreadDesktop {
public:
    bool Follow() {
        HDESK input = OpenInputDesktop(0, FALSE, GENERIC_ALL);
        if (!input) return false;
        const std::string name = DesktopName(input);
        if (current_ && name == name_) {
            CloseDesktop(input);
            return false;
        }
        if (!SetThreadDesktop(input)) {
            LOGW("[Agent] SetThreadDesktop(%s) failed: %lu", name.c_str(), GetLastError());
            CloseDesktop(input);
            return false;
        }
        if (current_) CloseDesktop(current_);
        current_ = input;
        name_ = name;
        return true;
    }

    const std::string& Name() const {
        return name_;
    }

private:
    HDESK current_ = nullptr;
    std::string name_;
};

uint64_t NowMs() {
    return GetTickCount64();
}

INPUT ToNativeInput(const deskhub::SecureDesktopInput& in) {
    INPUT native{};
    if (in.kind == deskhub::SecureDesktopInputKind::Keyboard) {
        native.type = INPUT_KEYBOARD;
        native.ki.wVk = in.vk;
        native.ki.wScan = in.scan;
        native.ki.dwFlags = in.flags;
        return native;
    }
    native.type = INPUT_MOUSE;
    native.mi.dx = in.dx;
    native.mi.dy = in.dy;
    native.mi.mouseData = in.data;
    native.mi.dwFlags = in.flags;
    return native;
}

struct Watcher {
    deskhub::SecureDesktopWatch watch;
    std::atomic<bool> stop{false};
    std::thread thread;
};

class Agent {
public:
    explicit Agent(HANDLE pipe) : pipe_(pipe) {}

    int Run() {
        if (!pipe_.Valid()) {
            LOGE("[Agent] The pipe handle from the service is not usable.");
            return 1;
        }
        LOGI("[Agent] Started; following the input desktop.");
        std::thread desktop([this] { DesktopLoop(); });
        ReadLoop();
        stop_.store(true);
        inputCv_.notify_all();
        desktop.join();
        StopAllWatchers();
        LOGI("[Agent] Host disconnected; exiting.");
        return 0;
    }

private:
    void Send(const std::vector<uint8_t>& message) {
        if (!message.empty() && !pipe_.WriteAll(message)) pipe_.Close();
    }

    void DesktopLoop() {
        ThreadDesktop desk;
        bool reported = false;
        while (!stop_.load()) {
            if (desk.Follow()) {
                const bool secure = deskhub::IsSecureDesktopName(desk.Name());
                LOGI("[Agent] Input desktop is now \"%s\".", desk.Name().c_str());
                if (!reported || secure != secure_.load()) {
                    secure_.store(secure);
                    reported = true;
                    Send(deskhub::EncodeSecureDesktopState(secure));
                }
            }
            for (const deskhub::SecureDesktopInput& in : TakeInputs()) {
                INPUT native = ToNativeInput(in);
                SendInput(1, &native, sizeof(INPUT));
            }
        }
    }

    std::deque<deskhub::SecureDesktopInput> TakeInputs() {
        std::unique_lock<std::mutex> lock(inputMutex_);
        inputCv_.wait_for(lock, kDesktopPoll, [this] { return stop_.load() || !inputs_.empty(); });
        std::deque<deskhub::SecureDesktopInput> taken;
        taken.swap(inputs_);
        return taken;
    }

    void QueueInput(const deskhub::SecureDesktopInput& in) {
        {
            const std::lock_guard<std::mutex> lock(inputMutex_);
            if (inputs_.size() >= kMaxQueuedInputs) return;
            inputs_.push_back(in);
        }
        inputCv_.notify_one();
    }

    void CaptureLoop(Watcher& w) {
        ThreadDesktop desk;
        DuplicationCapture capture;
        const RECT rect{w.watch.left, w.watch.top, LONG(w.watch.left + int64_t(w.watch.width)),
            LONG(w.watch.top + int64_t(w.watch.height))};
        uint64_t lastSentMs = 0;
        bool pending = false;
        while (!w.stop.load() && !stop_.load()) {
            if (!secure_.load()) {
                capture.Close();
                pending = false;
                Sleep(kIdleSleepMs);
                continue;
            }
            if ((desk.Follow() || !capture.IsOpen()) && !capture.Open(rect)) {
                Sleep(kRetryOpenMs);
                continue;
            }
            const DuplicationCapture::Result result = capture.Next(kAcquireTimeoutMs);
            if (result == DuplicationCapture::Result::Lost) continue;
            if (result == DuplicationCapture::Result::Frame) pending = true;
            if (!pending || NowMs() - lastSentMs < kMinFrameIntervalMs) continue;
            Send(deskhub::EncodeSecureDesktopFrame(w.watch.sourceId, capture.Width(),
                capture.Height(), capture.Pixels()));
            pending = false;
            lastSentMs = NowMs();
        }
    }

    void StartWatcher(const deskhub::SecureDesktopWatch& watch) {
        StopWatcher(watch.sourceId);
        auto w = std::make_unique<Watcher>();
        w->watch = watch;
        Watcher* raw = w.get();
        w->thread = std::thread([this, raw] { CaptureLoop(*raw); });
        watchers_[watch.sourceId] = std::move(w);
        LOGI("[Agent] Watching source %u at (%d,%d) %ux%u.", unsigned(watch.sourceId), watch.left,
            watch.top, watch.width, watch.height);
    }

    void StopWatcher(uint8_t sourceId) {
        const auto it = watchers_.find(sourceId);
        if (it == watchers_.end()) return;
        it->second->stop.store(true);
        it->second->thread.join();
        watchers_.erase(it);
    }

    void StopAllWatchers() {
        for (auto& [id, w] : watchers_) w->stop.store(true);
        for (auto& [id, w] : watchers_) w->thread.join();
        watchers_.clear();
    }

    void Handle(const deskhub::SecureDesktopMessage& msg) {
        switch (msg.type) {
            case deskhub::SecureDesktopMsg::Watch:
                if (const auto watch = deskhub::DecodeSecureDesktopWatch(msg.payload))
                    StartWatcher(*watch);
                return;
            case deskhub::SecureDesktopMsg::Input:
                if (const auto in = deskhub::DecodeSecureDesktopInput(msg.payload)) QueueInput(*in);
                return;
            default: LOGW("[Agent] Ignored message type %u from the host.", unsigned(msg.type));
        }
    }

    void ReadLoop() {
        deskhub::SecureDesktopStream stream;
        deskhub::SecureDesktopMessage msg;
        std::vector<uint8_t> buf(kReadChunk);
        size_t got = 0;
        while (pipe_.ReadSome(buf, got)) {
            stream.Append(std::span<const uint8_t>(buf.data(), got));
            while (stream.Next(msg)) Handle(msg);
            if (stream.Failed()) {
                LOGE("[Agent] The host sent a malformed message; closing.");
                return;
            }
        }
    }

    OverlappedPipe pipe_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> secure_{false};
    std::mutex inputMutex_;
    std::condition_variable inputCv_;
    std::deque<deskhub::SecureDesktopInput> inputs_;
    std::map<uint8_t, std::unique_ptr<Watcher>> watchers_;
};

}

int RunSecureDesktopAgent(const wchar_t* pipeHandleArg) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    wchar_t* end = nullptr;
    const unsigned long long value = std::wcstoull(pipeHandleArg, &end, 10);
    if (!end || *end != L'\0' || value == 0) {
        LOGE("[Agent] Expected a pipe handle number, got \"%ls\".", pipeHandleArg);
        return 1;
    }
    Agent agent(reinterpret_cast<HANDLE>(uintptr_t(value)));
    return agent.Run();
}

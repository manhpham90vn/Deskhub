#include "deskhubp/client/TerminalViewer.h"

#include "deskhub/session/LinkRecovery.h"
#include "deskhub/session/TerminalSession.h"
#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"
#include "deskhubp/system/Random.h"

#include <utility>

namespace deskhubp {

namespace {

constexpr uint32_t kPollWaitMs = 5;
constexpr size_t kMaxOutboxBytes = size_t{256} * 1024;

}

TerminalViewer::~TerminalViewer() {
    try {
        Stop();
    } catch (...) {
        outbox_.clear();
    }
}

std::string TerminalViewer::Message() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return message_;
}

std::string TerminalViewer::Fingerprint() const {
    return link_.FingerprintText();
}

void TerminalViewer::SetState(TerminalViewerState state, std::string_view message) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        message_.assign(message);
    }
    state_.store(state, std::memory_order_release);
    if (cb_.onState) cb_.onState(state, message);
}

TerminalSnapshot TerminalViewer::Snapshot(size_t scrollOffset) const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return deskhub::term::SnapshotScreen(screen_, scrollOffset);
}

bool TerminalViewer::Start(const TerminalViewerConfig& config,
    TerminalViewerCallbacks callbacks) {
    if (Running()) return false;
    if (!QuicAvailable()) {
        SetState(TerminalViewerState::Failed, deskhub::ui::kTerminalUnreachable);
        return false;
    }

    config_ = config;
    cb_ = std::move(callbacks);
    outbox_.clear();
    outboxBytes_ = 0;
    lostAtUs_ = 0;
    resumeRetryAtUs_ = 0;
    resumeAttempts_ = 0;
    {
        const std::lock_guard<std::mutex> lock(commandMutex_);
        commands_.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        screen_ = deskhub::term::Screen(deskhub::ClampTermSize(config_.size));
    }

    deskhub::TerminalClientCallbacks hooks;
    hooks.send = [this](std::span<const uint8_t> message) { SendRecord(message); };
    hooks.onOutput = [this](std::span<const uint8_t> bytes) {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            screen_.Write(bytes);
        }
        const std::string reply = [this] {
            const std::lock_guard<std::mutex> lock(mutex_);
            return screen_.TakeResponse();
        }();
        if (!reply.empty()) SendBytes(reply);
        if (cb_.onOutput) cb_.onOutput(bytes);
        if (cb_.onRedraw) cb_.onRedraw();
    };
    hooks.onOpened = [this](const deskhub::TermOpenAck& ack) {
        lostAtUs_ = 0;
        resumeRetryAtUs_ = 0;
        resumeAttempts_ = 0;
        SetState(TerminalViewerState::Live,
            ack.resumed ? deskhub::ui::kTerminalReattached : deskhub::ui::kTerminalConnected);
    };
    hooks.onRefused = [this](deskhub::TermReason reason) {
        if (lostAtUs_ != 0 && client_->CanReattach()) {
            SetState(TerminalViewerState::Reattaching, deskhub::ui::kTerminalReattaching);
            resumeRetryAtUs_ = NowUs() + deskhub::ReconnectDelayUs(resumeAttempts_, RandomU32());
            ++resumeAttempts_;
            return;
        }
        SetState(TerminalViewerState::Refused, deskhub::ui::TerminalRefusalText(reason));
    };
    hooks.onExit = [this](int32_t) {
        SetState(TerminalViewerState::Ended, deskhub::ui::kTerminalClosed);
    };
    client_ = std::make_unique<deskhub::TerminalClient>(std::move(hooks));

    if (!channel_) channel_ = link_.Open({deskhub::Chan::Terminal});

    HostLinkConfig linkConfig;
    linkConfig.host = config_.host;
    linkConfig.hostLabel = config_.hostLabel;
    linkConfig.passcode = config_.passcode;
    linkConfig.clientName = config_.clientName;
    linkConfig.recoverLink = true;
    linkConfig.recoverGraceUs = deskhub::kTerminalReattachGraceUs;

    HostLinkCallbacks linkHooks;
    linkHooks.onState = [this](HostLinkState state, std::string_view message) {
        OnLinkState(state, message);
    };
    linkHooks.onTrustAsked = [this](deskhub::TrustVerdict verdict, std::string_view fingerprint) {
        if (cb_.onTrustAsked) cb_.onTrustAsked(verdict, fingerprint);
    };
    linkHooks.onReady = [this](bool resumed) {
        Post([this, resumed] { HandleLinkReady(resumed); });
        channel_->Kick();
    };
    linkHooks.onLinkLost = [this] {
        Post([this] {
            lostAtUs_ = NowUs();
            resumeAttempts_ = 0;
            client_->LinkLost();
        });
        channel_->Kick();
    };

    stop_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    SetState(TerminalViewerState::Connecting, deskhub::ui::kTerminalConnecting);
    if (!link_.Start(linkConfig, std::move(linkHooks))) {
        SetState(TerminalViewerState::Failed, deskhub::ui::kTerminalUnreachable);
        running_.store(false, std::memory_order_release);
        return false;
    }
    thread_ = std::thread([this] { Loop(); });
    return true;
}

void TerminalViewer::OnLinkState(HostLinkState state, std::string_view message) {
    switch (state) {
        case HostLinkState::Connecting:
            SetState(TerminalViewerState::Connecting, deskhub::ui::kTerminalConnecting);
            return;
        case HostLinkState::Deciding:
            SetState(TerminalViewerState::Deciding, deskhub::ui::kTrustChangedBody);
            return;
        case HostLinkState::Authing:
            SetState(TerminalViewerState::Opening, deskhub::ui::kTerminalConnecting);
            return;
        case HostLinkState::Recovering:
            SetState(TerminalViewerState::Reattaching, deskhub::ui::kTerminalReattaching);
            return;
        case HostLinkState::Refused: SetState(TerminalViewerState::Refused, message); return;
        case HostLinkState::Failed:
            SetState(TerminalViewerState::Failed, deskhub::ui::kTerminalUnreachable);
            return;
        case HostLinkState::Ended: SetState(TerminalViewerState::Ended, message); return;
        default: return;
    }
}

void TerminalViewer::HandleLinkReady(bool resumed) {
    if (resumed && client_->CanReattach()) {
        client_->Reattach();
        return;
    }
    client_->Open(std::string(), config_.size, config_.clientName);
}

void TerminalViewer::RetryResume(uint64_t nowUs) {
    if (resumeRetryAtUs_ == 0 || nowUs < resumeRetryAtUs_) return;
    resumeRetryAtUs_ = 0;
    if (lostAtUs_ == 0) return;
    if (!deskhub::ReconnectStillWorthTrying(nowUs - lostAtUs_,
            deskhub::kTerminalReattachGraceUs)) {
        LOGW("terminal: gave up reattaching to %s after %u attempts",
            config_.host.ToString().c_str(), resumeAttempts_);
        SetState(TerminalViewerState::Failed, deskhub::ui::kTerminalUnreachable);
        return;
    }
    if (link_.State() == HostLinkState::Ready && client_->CanReattach()) {
        client_->Reattach();
        return;
    }
    resumeRetryAtUs_ = nowUs + deskhub::ReconnectDelayUs(resumeAttempts_, RandomU32());
}

void TerminalViewer::Stop() {
    if (Running()) {
        if (State() == TerminalViewerState::Live) {
            Post([this] { client_->Close(); });
        }
        stop_.store(true, std::memory_order_release);
        if (channel_) channel_->Kick();
        if (thread_.joinable()) thread_.join();
    }
    stop_.store(true, std::memory_order_release);
    link_.Stop();
    running_.store(false, std::memory_order_release);
    client_.reset();
    const std::lock_guard<std::mutex> lock(commandMutex_);
    commands_.clear();
}

void TerminalViewer::Post(std::function<void()> command) {
    const std::lock_guard<std::mutex> lock(commandMutex_);
    commands_.push_back(std::move(command));
}

void TerminalViewer::RunCommands() {
    std::vector<std::function<void()>> todo;
    {
        const std::lock_guard<std::mutex> lock(commandMutex_);
        todo.swap(commands_);
    }
    for (const std::function<void()>& command : todo) command();
}

void TerminalViewer::Loop() {
    while (!stop_.load(std::memory_order_acquire)) {
        RunCommands();
        while (const auto message = channel_->Poll()) client_->HandleMessage(*message);
        FlushOutbox();
        RetryResume(NowUs());
        channel_->WaitWork(kPollWaitMs);
    }
    RunCommands();
    FlushOutbox();
}

void TerminalViewer::AcceptFingerprint() {
    if (State() != TerminalViewerState::Deciding) return;
    SetState(TerminalViewerState::Opening, deskhub::ui::kTerminalConnecting);
    link_.AcceptFingerprint();
}

void TerminalViewer::RejectFingerprint() {
    link_.RejectFingerprint();
}

void TerminalViewer::SendRecord(std::span<const uint8_t> message) {
    if (message.empty()) return;
    if (outboxBytes_ + message.size() > kMaxOutboxBytes) return;
    outboxBytes_ += message.size();
    outbox_.emplace_back(message.begin(), message.end());
    FlushOutbox();
}

void TerminalViewer::FlushOutbox() {
    if (link_.State() != HostLinkState::Ready) return;
    while (!outbox_.empty()) {
        if (!link_.SendRecordOn(kQuicControlStream, outbox_.front())) return;
        outboxBytes_ -= outbox_.front().size();
        outbox_.pop_front();
    }
}

void TerminalViewer::SendBytes(const std::string& bytes) {
    if (bytes.empty() || !Running()) return;
    const auto payload = std::make_shared<const std::string>(bytes);
    Post([this, payload] {
        client_->SendInput(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(payload->data()), payload->size()));
    });
    if (channel_) channel_->Kick();
}

deskhub::term::TerminalModes TerminalViewer::CurrentModes() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return screen_.Modes();
}

void TerminalViewer::SendKey(const deskhub::term::TermKeyEvent& key) {
    SendBytes(deskhub::term::EncodeKey(key, CurrentModes()));
}

void TerminalViewer::SendText(std::string_view text) {
    SendBytes(deskhub::term::EncodeText(text, CurrentModes()));
}

void TerminalViewer::Paste(std::string_view text) {
    SendBytes(deskhub::term::EncodePaste(text, CurrentModes()));
}

void TerminalViewer::Resize(deskhub::TermSize size) {
    const deskhub::TermSize clamped = deskhub::ClampTermSize(size);
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (clamped == screen_.Size()) return;
        screen_.Resize(clamped);
    }
    if (!Running()) {
        config_.size = clamped;
        return;
    }
    Post([this, clamped] {
        config_.size = clamped;
        client_->Resize(clamped);
    });
    if (channel_) channel_->Kick();
    if (cb_.onRedraw) cb_.onRedraw();
}

}

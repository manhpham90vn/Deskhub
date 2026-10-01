#include "deskhubp/client/FileTransferClient.h"

#include "deskhub/ui/Strings.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Clock.h"

#include <utility>

namespace deskhubp {

namespace {

constexpr uint64_t kOfferTimeoutUs = 10'000'000;
constexpr uint32_t kIdleWaitMs = 20;
constexpr uint32_t kPumpWaitMs = 5;

}

FileTransferClient::~FileTransferClient() {
    Stop();
}

bool FileTransferClient::Start(const FileTransferClientConfig& config,
    FileTransferClientCallbacks callbacks) {
    if (running_.load(std::memory_order_acquire)) return false;
    if (thread_.joinable()) thread_.join();
    if (config.files.empty()) return false;

    config_ = config;
    cb_ = std::move(callbacks);
    stop_.store(false, std::memory_order_release);
    reason_.store(deskhub::TransferReason::Accepted, std::memory_order_release);
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        progress_ = deskhub::TransferProgress{};
    }
    if (!channel_) channel_ = link_.Open({deskhub::Chan::File});

    FileUploadCallbacks uploadHooks;
    uploadHooks.send = [this](std::span<const uint8_t> message) {
        return link_.SendRecordOn(kQuicFileStream, message);
    };
    uploadHooks.onProgress = [this](const deskhub::TransferProgress& progress) {
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            progress_ = progress;
        }
        if (cb_.onProgress) cb_.onProgress(progress);
    };
    upload_ = std::make_unique<FileUpload>(std::move(uploadHooks));

    HostLinkConfig linkConfig;
    linkConfig.host = config_.host;
    linkConfig.hostLabel = config_.hostLabel;
    linkConfig.clientName = config_.clientName;
    linkConfig.acceptNewHostKey = config_.acceptNewHostKey;

    HostLinkCallbacks hooks;
    hooks.onState = [this](HostLinkState state, std::string_view message) {
        OnLinkState(state, message);
    };
    hooks.onStreamBroken = [this](uint64_t streamId) {
        if (streamId == kQuicFileStream && upload_) upload_->LinkLost();
    };

    running_.store(true, std::memory_order_release);
    SetState(FileTransferClientState::Connecting, deskhub::ui::kTransferConnecting);
    if (!link_.Start(linkConfig, std::move(hooks))) {
        SetState(FileTransferClientState::Failed, deskhub::ui::kTerminalUnreachable);
        running_.store(false, std::memory_order_release);
        return false;
    }
    thread_ = std::thread([this] { Loop(); });
    return true;
}

void FileTransferClient::Cancel() {
    if (upload_) upload_->Cancel();
    stop_.store(true, std::memory_order_release);
    if (channel_) channel_->Kick();
}

void FileTransferClient::Stop() {
    stop_.store(true, std::memory_order_release);
    if (channel_) channel_->Kick();
    if (thread_.joinable()) thread_.join();
    link_.Stop();
    running_.store(false, std::memory_order_release);
}

bool FileTransferClient::Finished() const {
    const FileTransferClientState state = State();
    return state == FileTransferClientState::Done ||
           state == FileTransferClientState::Refused ||
           state == FileTransferClientState::Failed;
}

std::string FileTransferClient::Message() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return message_;
}

std::string FileTransferClient::FingerprintText() const {
    return link_.FingerprintText();
}

deskhub::TransferProgress FileTransferClient::Progress() const {
    const std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

deskhub::ui::TransferView FileTransferClient::View() const {
    const FileTransferClientState state = State();
    deskhub::ui::TransferView view;
    view.active = state == FileTransferClientState::Connecting ||
                  state == FileTransferClientState::Sending;
    view.done = state == FileTransferClientState::Done;
    view.failed = state == FileTransferClientState::Refused ||
                  state == FileTransferClientState::Failed;

    const std::lock_guard<std::mutex> lock(mutex_);
    view.fileIndex = progress_.fileIndex;
    view.fileCount = progress_.fileCount;
    view.bytes = progress_.batchBytes;
    view.total = progress_.batchSize;
    view.name = progress_.name;
    view.message = message_;
    return view;
}

void FileTransferClient::SetState(FileTransferClientState state, std::string_view message) {
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        message_.assign(message);
    }
    state_.store(state, std::memory_order_release);
    if (cb_.onState) cb_.onState(state, message);
}

void FileTransferClient::OnLinkState(HostLinkState state, std::string_view message) {
    const FileTransferClientState current = State();
    const bool beforeUpload = current == FileTransferClientState::Connecting;
    switch (state) {
        case HostLinkState::AwaitingApproval:
            if (beforeUpload) SetState(FileTransferClientState::Connecting, message);
            return;
        case HostLinkState::Refused:
            if (beforeUpload) SetState(FileTransferClientState::Refused, message);
            return;
        case HostLinkState::Failed:
        case HostLinkState::Ended:
            if (beforeUpload) SetState(FileTransferClientState::Failed, message);
            return;
        default: return;
    }
}

void FileTransferClient::Loop() {
    while (!stop_.load(std::memory_order_acquire)) {
        if (link_.State() == HostLinkState::Ready) break;
        if (link_.Settled()) {
            running_.store(false, std::memory_order_release);
            return;
        }
        channel_->WaitWork(kIdleWaitMs);
    }
    if (!stop_.load(std::memory_order_acquire)) ServeUpload();
    running_.store(false, std::memory_order_release);
}

void FileTransferClient::ServeUpload() {
    if (!upload_->Begin(config_.files)) {
        reason_.store(deskhub::TransferReason::ReadFailed, std::memory_order_release);
        SetState(FileTransferClientState::Failed, upload_->LastError());
        return;
    }
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        progress_ = upload_->Progress();
    }
    SetState(FileTransferClientState::Sending, deskhub::ui::kTransferSending);

    const uint64_t offerDeadlineUs = NowUs() + kOfferTimeoutUs;
    bool unanswered = false;
    while (!stop_.load(std::memory_order_acquire)) {
        while (const auto message = channel_->Poll()) upload_->HandleMessage(*message);
        upload_->Pump();
        if (!upload_->Busy()) break;
        if (link_.State() != HostLinkState::Ready) {
            upload_->LinkLost();
            break;
        }
        if (upload_->State() == deskhub::FileSenderState::Offering && NowUs() > offerDeadlineUs) {
            unanswered = true;
            upload_->Cancel();
            break;
        }
        channel_->WaitWork(kPumpWaitMs);
    }
    if (upload_->Busy()) upload_->LinkLost();

    if (unanswered) {
        reason_.store(deskhub::TransferReason::NotAccepting, std::memory_order_release);
        SetState(FileTransferClientState::Refused, deskhub::ui::kTransferHostNotTaking);
        return;
    }

    const deskhub::FileSenderState last = upload_->State();
    const deskhub::TransferReason reason = upload_->Reason();
    reason_.store(reason, std::memory_order_release);

    if (last == deskhub::FileSenderState::Done)
        SetState(FileTransferClientState::Done, deskhub::ui::kTransferDone);
    else if (last == deskhub::FileSenderState::Refused)
        SetState(FileTransferClientState::Refused, deskhub::ui::TransferReasonText(reason));
    else
        SetState(FileTransferClientState::Failed, deskhub::ui::TransferReasonText(reason));
}

}

#pragma once
#include "deskhub/net/TrustStore.h"
#include "deskhub/session/client/FileSender.h"
#include "deskhub/session/FileTransfer.h"
#include "deskhub/ui/TransferView.h"
#include "deskhubp/client/FileUpload.h"
#include "deskhubp/client/HostLink.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace deskhubp {

enum class FileTransferClientState : uint8_t {
    Idle = 0,
    Connecting = 1,
    Sending = 2,
    Done = 3,
    Refused = 4,
    Failed = 5,
};

struct FileTransferClientConfig {
    NetAddr host{};
    std::string hostLabel{};
    std::string clientName{};
    bool acceptNewHostKey = false;
    std::vector<std::filesystem::path> files{};
};

struct FileTransferClientCallbacks {
    std::function<void(const deskhub::TransferProgress&)> onProgress;
    std::function<void(FileTransferClientState, std::string_view message)> onState;
};

class FileTransferClient {
public:
    FileTransferClient() = default;
    ~FileTransferClient();
    FileTransferClient(const FileTransferClient&) = delete;
    FileTransferClient& operator=(const FileTransferClient&) = delete;

    bool Start(const FileTransferClientConfig& config, FileTransferClientCallbacks callbacks);
    void Cancel();
    void Stop();

    FileTransferClientState State() const {
        return state_.load(std::memory_order_acquire);
    }
    bool Running() const {
        return running_.load(std::memory_order_acquire);
    }
    bool Finished() const;
    deskhub::TransferReason Reason() const {
        return reason_.load(std::memory_order_acquire);
    }
    std::string Message() const;
    std::string FingerprintText() const;
    deskhub::TransferProgress Progress() const;
    deskhub::ui::TransferView View() const;

private:
    void Loop();
    void ServeUpload();
    void OnLinkState(HostLinkState state, std::string_view message);
    void SetState(FileTransferClientState state, std::string_view message);

    FileTransferClientConfig config_{};
    FileTransferClientCallbacks cb_{};
    HostLink link_{};
    std::shared_ptr<HostLinkChannel> channel_{};
    std::unique_ptr<FileUpload> upload_{};
    std::string message_{};
    deskhub::TransferProgress progress_{};

    mutable std::mutex mutex_{};
    std::thread thread_{};
    std::atomic<FileTransferClientState> state_{FileTransferClientState::Idle};
    std::atomic<deskhub::TransferReason> reason_{deskhub::TransferReason::Accepted};
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_{false};
};

}

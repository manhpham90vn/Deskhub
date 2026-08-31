#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace deskhubp {

inline constexpr const char* kTransferDirName = "Deskhub";
inline constexpr const char* kTransferPartSuffix = ".deskhub-part";

inline constexpr size_t kMaxQueuedWriteBytes = 1u << 20;

std::filesystem::path DefaultTransferDir();

inline std::string PathText(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

class FileStore {
public:
    FileStore() = default;
    ~FileStore();
    FileStore(const FileStore&) = delete;
    FileStore& operator=(const FileStore&) = delete;

    bool SetDirectory(const std::filesystem::path& dir);

    const std::filesystem::path& Directory() const {
        return dir_;
    }

    void ShareBacklogWith(std::atomic<size_t>* counter) {
        backlog_ = counter;
    }

    uint64_t FreeBytes() const;

    std::string Open(uint16_t index, const std::string& safeName, uint64_t size);
    bool Write(uint16_t index, std::span<const uint8_t> data);
    bool Close(uint16_t index, bool keep);
    void CloseAll();

private:
    struct Slot {
        std::string name{};
        std::filesystem::path part{};
        std::filesystem::path target{};
        std::ofstream out{};
    };

    struct WriteJob {
        uint16_t index = 0;
        std::vector<uint8_t> data{};
    };

    bool Claimed(const std::string& name) const;
    void WriterThread();
    void StartWriter();
    void StopWriter();
    void Post(WriteJob job);
    void Settle();
    void CountBacklog(size_t bytes, bool added);
    bool WriteNow(uint16_t index, std::span<const uint8_t> data);

    std::filesystem::path dir_{};
    std::map<uint16_t, Slot> open_{};

    std::thread writer_{};
    mutable std::mutex mutex_{};
    std::condition_variable roomCv_{};
    std::condition_variable workCv_{};
    std::condition_variable idleCv_{};
    std::deque<WriteJob> queue_{};
    size_t queuedBytes_ = 0;
    bool writing_ = false;
    bool stopping_ = false;
    bool writeFailed_ = false;
    std::atomic<size_t>* backlog_ = nullptr;
};

}

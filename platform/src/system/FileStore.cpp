#include "deskhubp/system/FileStore.h"

#include "deskhub/transfer/SafeName.h"
#include "deskhubp/diag/Log.h"
#include "deskhubp/system/Environment.h"

#include <system_error>
#include <utility>

namespace deskhubp {

namespace {

std::filesystem::path HomePath() {
#ifdef _WIN32
    const std::string home = EnvValue("USERPROFILE");
#else
    const std::string home = EnvValue("HOME");
#endif
    if (home.empty()) return {};
    const std::u8string wide(home.begin(), home.end());
    return std::filesystem::path(wide);
}

std::filesystem::path Utf8Path(const std::string& text) {
    const std::u8string wide(text.begin(), text.end());
    return std::filesystem::path(wide);
}

std::string Utf8Of(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

}

std::filesystem::path DefaultTransferDir() {
    const std::filesystem::path home = HomePath();
    if (home.empty()) return {};
    return home / kTransferDirName;
}

FileStore::~FileStore() {
    try {
        CloseAll();
    } catch (...) {
        open_.clear();
    }
    StopWriter();
}

void FileStore::CountBacklog(size_t bytes, bool added) {
    if (backlog_ == nullptr || bytes == 0) return;
    if (added)
        backlog_->fetch_add(bytes, std::memory_order_relaxed);
    else
        backlog_->fetch_sub(bytes, std::memory_order_relaxed);
}

void FileStore::StartWriter() {
    if (writer_.joinable()) return;
    stopping_ = false;
    writer_ = std::thread([this] { WriterThread(); });
}

void FileStore::StopWriter() {
    if (!writer_.joinable()) return;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    workCv_.notify_all();
    roomCv_.notify_all();
    writer_.join();
}

void FileStore::WriterThread() {
    for (;;) {
        WriteJob job;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            workCv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            if (queue_.empty()) {
                if (stopping_) return;
                continue;
            }
            job = std::move(queue_.front());
            queue_.pop_front();
            writing_ = true;
        }

        const bool ok = WriteNow(job.index, std::span<const uint8_t>(job.data));

        {
            const std::lock_guard<std::mutex> lock(mutex_);
            queuedBytes_ -= job.data.size();
            writing_ = false;
            if (!ok) writeFailed_ = true;
        }
        CountBacklog(job.data.size(), false);
        roomCv_.notify_all();
        idleCv_.notify_all();
    }
}

void FileStore::Post(WriteJob job) {
    const size_t bytes = job.data.size();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        roomCv_.wait(lock,
            [this, bytes] { return stopping_ || queuedBytes_ + bytes <= kMaxQueuedWriteBytes; });
        if (stopping_) return;
        queuedBytes_ += bytes;
        queue_.push_back(std::move(job));
    }
    CountBacklog(bytes, true);
    workCv_.notify_one();
}

void FileStore::Settle() {
    if (!writer_.joinable()) return;
    std::unique_lock<std::mutex> lock(mutex_);
    idleCv_.wait(lock, [this] { return queue_.empty() && !writing_; });
}

bool FileStore::SetDirectory(const std::filesystem::path& dir) {
    if (dir.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (!std::filesystem::is_directory(dir, ec)) {
        LOGE("transfer: %s is not a directory files can be stored in", Utf8Of(dir).c_str());
        return false;
    }
    dir_ = dir;
    return true;
}

uint64_t FileStore::FreeBytes() const {
    if (dir_.empty()) return 0;
    std::error_code ec;
    const std::filesystem::space_info space = std::filesystem::space(dir_, ec);
    if (ec) return 0;
    return uint64_t(space.available);
}

bool FileStore::Claimed(const std::string& name) const {
    std::error_code ec;
    if (std::filesystem::exists(dir_ / Utf8Path(name), ec)) return true;
    for (const auto& [index, slot] : open_)
        if (slot.name == name) return true;
    return false;
}

std::string FileStore::Open(uint16_t index, const std::string& safeName, uint64_t size) {
    if (dir_.empty()) return {};
    Close(index, false);
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        writeFailed_ = false;
    }

    std::string name = deskhub::UniqueFileName(safeName,
        [this](const std::string& candidate) { return Claimed(candidate); });
    if (name.empty()) {
        LOGW("transfer: no free name left beside %s", safeName.c_str());
        return {};
    }

    if (size > FreeBytes()) {
        LOGW("transfer: %s needs more room than %s has left", name.c_str(),
            Utf8Of(dir_).c_str());
        return {};
    }

    Slot slot;
    slot.name = name;
    slot.target = dir_ / Utf8Path(name);
    slot.part = dir_ / Utf8Path(name + kTransferPartSuffix);
    slot.out.open(slot.part, std::ios::binary | std::ios::trunc);
    if (!slot.out) {
        LOGE("transfer: could not open %s for writing", Utf8Of(slot.part).c_str());
        return {};
    }

    open_.emplace(index, std::move(slot));
    StartWriter();
    return name;
}

bool FileStore::WriteNow(uint16_t index, std::span<const uint8_t> data) {
    const auto at = open_.find(index);
    if (at == open_.end()) return false;
    at->second.out.write(reinterpret_cast<const char*>(data.data()),
        std::streamsize(data.size()));
    return bool(at->second.out);
}

bool FileStore::Write(uint16_t index, std::span<const uint8_t> data) {
    if (open_.find(index) == open_.end()) return false;
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (writeFailed_) return false;
    }
    WriteJob job;
    job.index = index;
    job.data.assign(data.begin(), data.end());
    Post(std::move(job));
    return true;
}

bool FileStore::Close(uint16_t index, bool keep) {
    Settle();
    const auto at = open_.find(index);
    if (at == open_.end()) return !keep;
    Slot slot = std::move(at->second);
    open_.erase(at);

    slot.out.flush();
    const bool sound = bool(slot.out);
    slot.out.close();

    std::error_code ec;
    if (!keep || !sound) {
        std::filesystem::remove(slot.part, ec);
        return !keep;
    }

    std::filesystem::path target = slot.target;
    if (std::filesystem::exists(target, ec)) {
        const std::string fresh = deskhub::UniqueFileName(slot.name,
            [this](const std::string& candidate) { return Claimed(candidate); });
        if (fresh.empty()) {
            std::filesystem::remove(slot.part, ec);
            return false;
        }
        target = dir_ / Utf8Path(fresh);
    }

    std::filesystem::rename(slot.part, target, ec);
    if (ec) {
        LOGE(
            "transfer: could not put %s in place, so the part file is dropped and the batch "
            "fails; the sender is told the write failed rather than being left to believe a "
            "file that never landed arrived",
            Utf8Of(target).c_str());
        std::filesystem::remove(slot.part, ec);
        return false;
    }
    return true;
}

void FileStore::CloseAll() {
    while (!open_.empty()) Close(open_.begin()->first, false);
}

}

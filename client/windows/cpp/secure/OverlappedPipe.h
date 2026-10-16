#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>

class OverlappedPipe {
public:
    explicit OverlappedPipe(HANDLE handle = INVALID_HANDLE_VALUE);
    ~OverlappedPipe();
    OverlappedPipe(const OverlappedPipe&) = delete;
    OverlappedPipe& operator=(const OverlappedPipe&) = delete;

    void Reset(HANDLE handle);
    bool Valid() const;
    HANDLE Handle() const;

    bool ReadSome(std::span<uint8_t> into, size_t& got);
    bool WriteAll(std::span<const uint8_t> bytes);
    void Close();

private:
    bool WaitIo(HANDLE event, OVERLAPPED& ov, DWORD& transferred);

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE readEvent_ = nullptr;
    HANDLE writeEvent_ = nullptr;
    HANDLE closeEvent_ = nullptr;
    std::mutex writeMutex_;
};

std::wstring ProcessImagePath(HANDLE process);
std::wstring DirectoryOf(const std::wstring& path);
bool SamePath(const std::wstring& a, const std::wstring& b);

#include "secure/OverlappedPipe.h"

#include <vector>

OverlappedPipe::OverlappedPipe(HANDLE handle)
    : readEvent_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      writeEvent_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      closeEvent_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {
    Reset(handle);
}

OverlappedPipe::~OverlappedPipe() {
    Close();
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    if (readEvent_) CloseHandle(readEvent_);
    if (writeEvent_) CloseHandle(writeEvent_);
    if (closeEvent_) CloseHandle(closeEvent_);
}

void OverlappedPipe::Reset(HANDLE handle) {
    const std::lock_guard<std::mutex> lock(writeMutex_);
    if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    handle_ = handle;
    if (closeEvent_) ResetEvent(closeEvent_);
}

bool OverlappedPipe::Valid() const {
    return handle_ != INVALID_HANDLE_VALUE && readEvent_ && writeEvent_ && closeEvent_;
}

HANDLE OverlappedPipe::Handle() const {
    return handle_;
}

bool OverlappedPipe::WaitIo(HANDLE event, OVERLAPPED& ov, DWORD& transferred) {
    const HANDLE waits[] = {event, closeEvent_};
    const DWORD which = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
    if (which != WAIT_OBJECT_0) CancelIoEx(handle_, &ov);
    const BOOL done = GetOverlappedResult(handle_, &ov, &transferred, TRUE);
    return done && which == WAIT_OBJECT_0;
}

bool OverlappedPipe::ReadSome(std::span<uint8_t> into, size_t& got) {
    got = 0;
    if (!Valid() || into.empty()) return false;
    OVERLAPPED ov{};
    ov.hEvent = readEvent_;
    ResetEvent(readEvent_);
    DWORD n = 0;
    const DWORD want = DWORD(into.size() > MAXDWORD ? MAXDWORD : into.size());
    if (!ReadFile(handle_, into.data(), want, &n, &ov)) {
        if (GetLastError() != ERROR_IO_PENDING) return false;
        if (!WaitIo(readEvent_, ov, n)) return false;
    } else if (!GetOverlappedResult(handle_, &ov, &n, TRUE)) {
        return false;
    }
    got = n;
    return n > 0;
}

bool OverlappedPipe::WriteAll(std::span<const uint8_t> bytes) {
    const std::lock_guard<std::mutex> lock(writeMutex_);
    while (!bytes.empty()) {
        if (!Valid()) return false;
        OVERLAPPED ov{};
        ov.hEvent = writeEvent_;
        ResetEvent(writeEvent_);
        DWORD n = 0;
        const DWORD want = DWORD(bytes.size() > (1u << 20) ? (1u << 20) : bytes.size());
        if (!WriteFile(handle_, bytes.data(), want, &n, &ov)) {
            if (GetLastError() != ERROR_IO_PENDING) return false;
            if (!WaitIo(writeEvent_, ov, n)) return false;
        } else if (!GetOverlappedResult(handle_, &ov, &n, TRUE)) {
            return false;
        }
        if (n == 0) return false;
        bytes = bytes.subspan(n);
    }
    return true;
}

void OverlappedPipe::Close() {
    if (closeEvent_) SetEvent(closeEvent_);
    if (handle_ != INVALID_HANDLE_VALUE) CancelIoEx(handle_, nullptr);
}

std::wstring ProcessImagePath(HANDLE process) {
    if (!process) return {};
    std::vector<wchar_t> buf(32768);
    DWORD size = DWORD(buf.size());
    if (!QueryFullProcessImageNameW(process, 0, buf.data(), &size)) return {};
    return std::wstring(buf.data(), size);
}

std::wstring DirectoryOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

bool SamePath(const std::wstring& a, const std::wstring& b) {
    if (a.empty() || b.empty()) return false;
    return CompareStringOrdinal(a.c_str(), int(a.size()), b.c_str(), int(b.size()), TRUE) ==
           CSTR_EQUAL;
}

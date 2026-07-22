// =============================================================================
// DiagLog.cpp — cài đặt việc đổi hướng log sang file.
//
// VÌ SAO FREOPEN CHỨ KHÔNG TỰ MỞ MỘT FILE RIÊNG ĐỂ GHI
//   Log rải khắp chương trình bằng std::printf/std::wprintf trên stdout (AgentLoop,
//   ClientLoop, MfEncoder, ...). Thay hết bằng một hàm log riêng là sửa hàng trăm
//   chỗ và dễ sót. freopen tóm đúng cái stdout ấy — cùng một đường mà `> file` của
//   cmd đi, tức là con đường đã biết chắc cho ra UTF-8 đọc được.
//
// HAI CHI TIẾT DỄ SAI
//   1. freopen ĐẶT LẠI chế độ buffer. main.cpp đã setvbuf(_IONBF) để log ra ngay
//      cả khi bị redirect, nhưng thiết lập đó áp lên stream CŨ; không gọi lại thì
//      stream mới quay về full-buffer 4KB và một cú crash sẽ nuốt mất đoạn đuôi —
//      đúng đoạn cần đọc nhất.
//   2. Trả lại stdout phải qua _dup/_dup2 ở tầng FD, không phải freopen("CON").
//      Instance admin không chắc có console để mở lại, còn fd gốc thì luôn còn.
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "DiagLog.h"

#include <windows.h>

#include <cstdio>
#include <io.h>

namespace {

// Thư mục chứa exe. Log nằm cạnh exe để người dùng gửi kèm khỏi phải đi tìm.
std::wstring ExeDir() {
    wchar_t path[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return std::wstring();
    std::wstring s(path, n);
    const size_t slash = s.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : s.substr(0, slash + 1);
}

} // namespace

bool DiagLogRedirect::Start(DiagRole role) {
    if (active_) return true;

    SYSTEMTIME t{};
    GetLocalTime(&t);

    // Giờ ĐỊA PHƯƠNG chứ không phải UTC: người dùng đối chiếu log với "lúc nãy nó
    // giật khoảng 8 rưỡi", và họ đọc giờ trên đồng hồ máy mình.
    wchar_t name[64];
    swprintf(name, 64, L"diag-%ls-%04u%02u%02u-%02u%02u%02u.log",
        role == DiagRole::Agent ? L"agent" : L"client",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

    const std::wstring full = ExeDir() + name;

    // Giữ fd gốc TRƯỚC khi freopen — sau freopen thì stdout cũ không còn đường về.
    savedOut_ = _dup(_fileno(stdout));

    if (!_wfreopen(full.c_str(), L"w", stdout)) {
        if (savedOut_ >= 0) {
            _close(savedOut_);
            savedOut_ = -1;
        }
        return false;
    }
    setvbuf(stdout, nullptr, _IONBF, 0); // freopen vừa xoá mất thiết lập của main()

    // stderr gộp chung một file: thứ tự các dòng giữa hai luồng mới đọc được, và
    // người dùng chỉ phải gửi một file.
    savedErr_ = _dup(_fileno(stderr));
    _dup2(_fileno(stdout), _fileno(stderr));
    setvbuf(stderr, nullptr, _IONBF, 0);

    path_ = full;
    active_ = true;

    std::printf("[DiagLog] %ls role=%ls started %04u-%02u-%02u %02u:%02u:%02u\n",
        name, role == DiagRole::Agent ? L"agent" : L"client",
        t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return true;
}

void DiagLogRedirect::Stop() {
    if (!active_) return;
    std::fflush(stdout);
    std::fflush(stderr);

    if (savedErr_ >= 0) {
        _dup2(savedErr_, _fileno(stderr));
        _close(savedErr_);
        savedErr_ = -1;
    }
    if (savedOut_ >= 0) {
        _dup2(savedOut_, _fileno(stdout));
        _close(savedOut_);
        savedOut_ = -1;
    }
    active_ = false;
}

DiagLogRedirect::~DiagLogRedirect() {
    Stop();
}

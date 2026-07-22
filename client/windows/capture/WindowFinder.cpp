// =============================================================================
// WindowFinder.cpp — cài đặt việc quét cửa sổ bằng EnumWindows.
//
// KHUÔN MẪU CALLBACK CỦA WIN32
//   EnumWindows gọi lại một hàm C thuần cho TỪNG cửa sổ top-level, và cách duy nhất
//   mang dữ liệu vào/ra là con trỏ nhét trong LPARAM. Vì thế mỗi hàm công khai ở
//   cuối file đều theo cùng một khuôn: dựng một struct chứa kết quả, ép nó thành
//   LPARAM, rồi đọc lại sau khi quét xong.
//   Trả TRUE từ callback nghĩa là "tiếp tục quét" — trả FALSE sẽ dừng sớm, nên mọi
//   nhánh loại trừ dưới đây đều `return TRUE`, không phải FALSE.
//
// BỘ LỌC — mỗi dòng loại một loại rác cụ thể, theo thứ tự từ rẻ tới đắt:
//   !IsWindowVisible   — cửa sổ ẩn.
//   GW_OWNER != null   — cửa sổ con/hộp thoại thuộc sở hữu cửa sổ khác, không phải
//                        cửa sổ chính của ứng dụng.
//   IsCloaked          — UWP đã bị hệ thống treo: vẫn báo "visible" nhưng DWM che
//                        đi, bắt hình sẽ ra khung đen.
//   tiêu đề rỗng       — gần như luôn là cửa sổ hạ tầng ẩn, không phải thứ người
//                        dùng nhận ra được trong danh sách.
//   pid == mình        — không cho chọn chính cửa sổ của chương trình này (bắt hình
//                        cửa sổ đang hiển thị chính nó tạo ra hiệu ứng gương vô hạn).
//   kích thước ≤ 0     — cửa sổ chưa dựng xong hoặc đã thu về 0.
//
//   Phép kiểm tra tên exe (ExeNameOfWindow) đắt nhất vì phải mở handle tiến trình,
//   nên nó luôn đứng SAU các phép lọc rẻ.
//
// LIÊN QUAN: capture/WindowFinder.h (hai đường vào + lý do lọc)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "capture/WindowFinder.h"

#include <dwmapi.h>

#include <algorithm>
#include <cwctype>

#pragma comment(lib, "dwmapi.lib")

namespace {

// Trạng thái mang qua EnumWindows cho FindWindowByProcessName: vừa là đầu vào
// (targetExe) vừa là chỗ tích luỹ kết quả tốt nhất tìm được (found/bestArea).
struct WindowSearch {
    std::wstring targetExe;
    HWND found = nullptr;
    LONG bestArea = 0;
};

// "C:\Games\Foo\Game.EXE" -> "game.exe". Hạ chữ thường để so sánh không phân biệt
// hoa thường (người dùng gõ tên exe bằng tay, và Windows vốn không phân biệt).
std::wstring BaseNameLower(const std::wstring& path) {
    size_t slash = path.find_last_of(L"\\/");
    std::wstring name = (slash == std::wstring::npos) ? path : path.substr(slash + 1);
    for (auto& c : name) c = (wchar_t)towlower(c);
    return name;
}

// Tên exe (chữ thường) của process sở hữu cửa sổ; rỗng nếu không đọc được.
std::wstring ExeNameOfWindow(HWND hwnd, DWORD* outPid = nullptr) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (outPid) *outPid = pid;
    if (pid == 0) return {};

    // PROCESS_QUERY_LIMITED_INFORMATION chứ không phải PROCESS_QUERY_INFORMATION:
    // quyền hẹp hơn nên mở được cả tiến trình chạy ở mức toàn vẹn cao hơn — nếu
    // không, mọi ứng dụng chạy với quyền admin sẽ biến mất khỏi danh sách.
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return {};

    wchar_t path[MAX_PATH] = {};
    DWORD len = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(proc, 0, path, &len)) name = BaseNameLower(path);
    CloseHandle(proc);
    return name;
}

// UWP/app bị treo vẫn "visible" nhưng bị DWM cloak - không capture được.
bool IsCloaked(HWND hwnd) {
    DWORD cloaked = 0;
    return SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked != 0;
}

// Callback cho FindWindowByProcessName. Một tiến trình có thể có nhiều cửa sổ
// top-level (cửa sổ chính, cửa sổ splash, cửa sổ ẩn), nên không dừng ở cái đầu tiên
// khớp tên mà quét hết rồi giữ cái LỚN NHẤT — cửa sổ chính hầu như luôn là nó.
BOOL CALLBACK EnumProc(HWND hwnd, LPARAM lparam) {
    auto* search = reinterpret_cast<WindowSearch*>(lparam);

    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;

    if (ExeNameOfWindow(hwnd) != search->targetExe) return TRUE;

    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) return TRUE;
    LONG area = (rc.right - rc.left) * (rc.bottom - rc.top);
    if (area > search->bestArea) {
        search->bestArea = area;
        search->found = hwnd;
    }
    return TRUE;
}

// Callback cho ListCapturableWindows. Bộ lọc đầy đủ hơn EnumProc (thêm IsCloaked,
// tiêu đề, pid) vì kết quả đi thẳng ra trước mắt người dùng — xem đầu file.
BOOL CALLBACK ListProc(HWND hwnd, LPARAM lparam) {
    auto* out = reinterpret_cast<std::vector<WindowInfo>*>(lparam);

    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    if (IsCloaked(hwnd)) return TRUE;

    wchar_t title[256] = {};
    if (GetWindowTextW(hwnd, title, 256) == 0) return TRUE;

    DWORD pid = 0;
    std::wstring exe = ExeNameOfWindow(hwnd, &pid);
    if (exe.empty() || pid == GetCurrentProcessId()) return TRUE;

    RECT rc{};
    if (!GetClientRect(hwnd, &rc)) return TRUE;
    const LONG w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return TRUE;

    WindowInfo info;
    info.hwnd = hwnd;
    info.exeName = std::move(exe);
    info.title = title;
    info.width = (uint32_t)w;
    info.height = (uint32_t)h;
    info.minimized = IsIconic(hwnd) != FALSE;
    out->push_back(std::move(info));
    return TRUE;
}

} // namespace

HWND FindWindowByProcessName(const std::wstring& exeName) {
    WindowSearch search;
    search.targetExe = BaseNameLower(exeName);
    EnumWindows(EnumProc, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

std::vector<WindowInfo> ListCapturableWindows() {
    std::vector<WindowInfo> windows;
    EnumWindows(ListProc, reinterpret_cast<LPARAM>(&windows));
    // Lớn nhất lên đầu. Ép u64 trước khi nhân: 4K là ~8.3 triệu điểm ảnh, vẫn vừa
    // u32, nhưng màn hình rộng nhiều monitor thì tích có thể vượt — nhân ở u64 thì
    // không phải lo trường hợp nào cả.
    std::sort(windows.begin(), windows.end(), [](const WindowInfo& a, const WindowInfo& b) {
        return (uint64_t)a.width * a.height > (uint64_t)b.width * b.height;
    });
    return windows;
}

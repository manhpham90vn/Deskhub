#pragma once
// =============================================================================
// WindowFinder.h — tìm và liệt kê cửa sổ có thể chọn làm nguồn stream.
//
// NHIỆM VỤ
//   Hai đường vào, phục vụ hai cách người dùng chỉ định nguồn:
//     FindWindowByProcessName() — dòng lệnh: `client.exe notepad.exe`.
//     ListCapturableWindows()   — giao diện: hiện danh sách cho người dùng bấm chọn.
//
// VỊ TRÍ TRONG LUỒNG
//   **WindowFinder** → WindowCapture → IVideoEncoder → Packetizer → UDP
//   Đây là bước đầu tiên: từ một cái tên hoặc một cú bấm chuột ra được HWND.
//
// VÌ SAO "CAPTURABLE" CẦN LỌC NHIỀU ĐẾN THẾ
//   EnumWindows trả về hàng trăm HWND, phần lớn không phải thứ người dùng nghĩ là
//   "cửa sổ": cửa sổ công cụ ẩn, cửa sổ con thuộc sở hữu cửa sổ khác, ứng dụng UWP
//   đã bị treo nhưng vẫn báo là hiển thị. Đưa hết vào danh sách thì nó dài vô dụng
//   và đầy mục chọn vào là hỏng. Bộ lọc cụ thể nằm ở WindowFinder.cpp.
//
// SẮP XẾP THEO DIỆN TÍCH GIẢM DẦN
//   Cửa sổ người dùng muốn chia sẻ hầu như luôn là cửa sổ lớn nhất (game, trình
//   duyệt), nên để nó lên đầu danh sách là đoán đúng trong đa số trường hợp.
//
// LIÊN QUAN: capture/WindowCapture.h (bước tiếp theo),
//            ui/WindowPickerDialog.h (hiển thị danh sách)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>

// Trả về HWND cửa sổ hiển thị lớn nhất thuộc process có tên exe khớp `exeName`
// (so sánh không phân biệt hoa thường, chỉ phần tên file). nullptr nếu không thấy.
HWND FindWindowByProcessName(const std::wstring& exeName);

// Một cửa sổ có thể chọn làm nguồn stream.
struct WindowInfo {
    HWND         hwnd = nullptr;
    std::wstring exeName;   // tên file exe, chữ thường
    std::wstring title;
    uint32_t     width = 0, height = 0;   // kích thước client
    bool         minimized = false;       // đang thu nhỏ -> cần restore trước khi capture
};

// Liệt kê các cửa sổ top-level capture được: hiển thị, không owner, có tiêu đề,
// không bị DWM cloak (UWP ẩn), không thuộc chính process này.
// Sắp xếp theo diện tích giảm dần (cửa sổ game thường lớn nhất).
std::vector<WindowInfo> ListCapturableWindows();

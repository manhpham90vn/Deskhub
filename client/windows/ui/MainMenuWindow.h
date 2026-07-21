#pragma once
// =============================================================================
// MainMenuWindow.h — màn hình chính kiểu AnyDesk (GĐ5). Điểm vào của người dùng.
//
// NHIỆM VỤ
//   Cửa sổ đầu tiên hiện lên khi chạy chương trình. Từ đây rẽ thành hai vai:
//     CHIA SẺ  — hiện IP máy này, bấm nút → WindowPickerDialog → RunAgent().
//     KẾT NỐI  — gõ IP máy kia → RunClient().
//   Kèm ô chỉnh Port/FPS/Bitrate, trước đây chỉ đặt được bằng tham số dòng lệnh
//   (--port/--fps/--bitrate).
//
// VỊ TRÍ TRONG LUỒNG NGƯỜI DÙNG
//   main() → **MainMenuWindow** ─┬─ WindowPickerDialog → AgentLoop  (vai host)
//                               └─ SourcePickerDialog → ClientLoop (vai client)
//
// VÌ SAO GUI THUẦN, BỎ HẲN CLI
//   Người dùng đích không mở terminal. Toàn bộ tham số từng chỉ có trên dòng lệnh
//   giờ đều có ô nhập tương ứng ở đây, nên không còn đường nào bắt buộc phải gõ
//   lệnh. Các cờ dòng lệnh vẫn còn cho việc gỡ lỗi — xem main.cpp.
//
// LIÊN QUAN: main.cpp (người gọi), ui/WindowPickerDialog.h, AgentLoop.h,
//            ClientLoop.h, net/NetInfo.h (danh sách IP hiển thị)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

int RunMainMenuWindow();

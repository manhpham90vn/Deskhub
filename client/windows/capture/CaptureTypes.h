#pragma once
// =============================================================================
// CaptureTypes.h — kiểu dữ liệu ở ranh giới giữa tầng bắt hình và tầng tiêu thụ.
//
// NHIỆM VỤ
//   Định nghĩa FrameInfo: hình dạng của "một khung hình vừa bắt được". Đây là hợp
//   đồng giữa WindowCapture (bên sản xuất) và encoder / bộ ghi ảnh debug (bên tiêu
//   thụ), tách riêng ra để hai bên không phải include lẫn nhau.
//
// VÌ SAO CỐ Ý KHÔNG PHỤ THUỘC winrt
//   Windows.Graphics.Capture là API WinRT, kéo theo cả một bộ header C++/WinRT rất
//   nặng và đòi bật coroutine. Encoder không cần biết frame đến từ đâu — nó chỉ cần
//   một texture D3D11. Giữ file này ở mức D3D11/COM thuần nghĩa là encoder biên
//   dịch nhanh hơn, và sau này đổi sang nguồn bắt hình khác (Desktop Duplication
//   chẳng hạn) thì bên tiêu thụ không phải sửa gì.
//
// ⚠ QUY TẮC VÒNG ĐỜI QUAN TRỌNG NHẤT
//   `texture` CHỈ hợp lệ trong phạm vi lời gọi callback. Nó thuộc về frame pool của
//   WGC và sẽ được tái sử dụng cho khung hình sau ngay khi callback trả về. Bên tiêu
//   thụ phải encode hoặc copy NGAY; giữ con trỏ lại để dùng sau là đọc phải dữ liệu
//   của một khung hình khác — lỗi không gây crash, chỉ cho ra hình sai, nên rất khó
//   lần ra nếu không biết trước quy tắc này.
//
// LIÊN QUAN: capture/WindowCapture.h (bên sản xuất), encode/IVideoEncoder.h,
//            capture/BmpWriter.h (bên tiêu thụ)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <cstdint>

// Một frame vừa bắt được. `texture` nằm trong VRAM (BGRA) và CHỈ hợp lệ trong
// phạm vi lời gọi callback - consumer phải encode/copy ngay, không được giữ lại.
struct FrameInfo {
    ID3D11Texture2D* texture;      // BGRA8, còn sống trong VRAM khi callback chạy
    uint32_t         width;
    uint32_t         height;
    uint64_t         timestampUs;  // thời điểm capture (SystemRelativeTime), micro giây
    uint64_t         frameId;      // số thứ tự tăng dần, bắt đầu từ 0
};

#pragma once
// =============================================================================
// WindowCapture.h — bắt hình một cửa sổ HOẶC một màn hình bằng Windows Graphics
// Capture (WGC). Đầu nguồn của toàn bộ luồng video.
//
// NHIỆM VỤ
//   Biến một HWND/HMONITOR thành dòng texture D3D11 chảy đều đặn ra callback.
//   (Tên lớp giữ từ GĐ0 khi mới chỉ có đường cửa sổ; WGC dùng chung một
//   GraphicsCaptureItem cho cả hai nên phần còn lại của lớp không phân biệt.)
//
// VỊ TRÍ TRONG LUỒNG DỮ LIỆU
//   WindowFinder → **WindowCapture** → IVideoEncoder → Packetizer → Pacer → UDP
//
// BA QUYẾT ĐỊNH THIẾT KẾ
//   1. THEO SỰ KIỆN, KHÔNG POLLING. WGC bắn FrameArrived mỗi khi nội dung đổi. Hỏi
//      vòng theo nhịp cố định vừa thêm độ trễ (trung bình nửa chu kỳ) vừa đốt CPU
//      khi màn hình đứng yên. Đổi lại: callback chạy trên thread của WGC, không
//      phải thread ta kiểm soát — xem cảnh báo bên dưới.
//
//   2. PIMPL ĐỂ GIẤU winrt. Toàn bộ C++/WinRT nằm trong .cpp. Header này chỉ lộ
//      D3D11/COM thuần, nên encoder tiêu thụ được mà không phải kéo theo bộ header
//      WinRT rất nặng (xem thêm CaptureTypes.h).
//
//   3. CHIA SẺ D3D11 DEVICE ra ngoài qua Device()/Context(). Encoder dùng chung
//      device thì texture không bao giờ phải rời VRAM — xem GpuSelect.h.
//
// ⚠ CALLBACK CHẠY TRÊN THREAD-POOL CỦA WGC
//   Hai hệ quả bắt buộc phải nhớ:
//     - Phải xử lý NHANH. Giữ chỗ trong frame pool lâu là làm nghẽn cả đường bắt
//       hình. Tuyệt đối không ngủ trong đó (xem Pacer.h — bài học phải trả giá).
//     - Không giữ FrameInfo::texture sau khi callback trả về (xem CaptureTypes.h).
//
// LIÊN QUAN: capture/CaptureTypes.h (FrameInfo), capture/GpuSelect.h (device dùng
//            chung), capture/WindowFinder.h (nguồn HWND), AgentLoop.cpp
// =============================================================================
#include "capture/CaptureTypes.h"
#include <functional>
#include <memory>

namespace capture {

// Khởi tạo runtime WinRT (MTA) cho luồng hiện tại. Gọi một lần lúc khởi động,
// trước khi tạo WindowCapture.
void InitRuntime();

}  // namespace capture

// Nguồn cần bắt: đúng MỘT trong hai trường khác nullptr.
struct CaptureTarget {
    HWND     hwnd = nullptr;
    HMONITOR monitor = nullptr;

    bool valid() const { return (hwnd != nullptr) != (monitor != nullptr); }
    static CaptureTarget Window(HWND h) { return CaptureTarget{h, nullptr}; }
    static CaptureTarget Monitor(HMONITOR m) { return CaptureTarget{nullptr, m}; }
};

class WindowCapture {
public:
    // Callback được gọi trên luồng của thread-pool WGC mỗi khi có frame mới.
    // Phải xử lý nhanh (encode/copy) và KHÔNG giữ FrameInfo::texture sau khi trả về.
    using FrameHandler = std::function<void(const FrameInfo&)>;

    WindowCapture();
    ~WindowCapture();
    WindowCapture(const WindowCapture&) = delete;
    WindowCapture& operator=(const WindowCapture&) = delete;

    // Bắt đầu bắt hình `target` (cửa sổ hoặc màn hình), gọi `onFrame` cho mỗi frame.
    // `device`: D3D11 device dùng chung (từ GpuSelect). Nếu nullptr, tự tạo device mặc định.
    // Dùng chung device với encoder để texture không phải copy chéo GPU.
    bool Start(const CaptureTarget& target, ID3D11Device* device, FrameHandler onFrame);
    void Stop();

    // True khi nguồn mục tiêu đã đóng (cửa sổ đóng / màn hình bị ngắt).
    bool Closed() const;

    // D3D11 device/context dùng cho capture - chia sẻ cho encoder (COM thuần).
    ID3D11Device*        Device() const;
    ID3D11DeviceContext* Context() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

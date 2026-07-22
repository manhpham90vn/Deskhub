#pragma once
// =============================================================================
// GpuSelect.h — chọn GPU và tạo D3D11 device dùng chung cho cả chương trình.
//
// NHIỆM VỤ
//   Duyệt các adapter đồ hoạ theo một chuỗi ưu tiên do người gọi đưa vào (thường
//   là NVIDIA → Intel → AMD) và tạo D3D11 device trên cái đầu tiên dùng được.
//   Không có gì phù hợp thì rớt về WARP — bộ dựng hình bằng phần mềm.
//
// VÌ SAO PHẢI CHỌN, KHÔNG LẤY BỪA CÁI ĐẦU TIÊN
//   Máy host có thể có GPU rời hỗ trợ NVENC (encoder phần cứng nhanh nhất), máy
//   client có thể chỉ có iGPU Intel với Quick Sync. Laptop thường có CẢ HAI, và
//   adapter đầu tiên DXGI trả về không nhất thiết là cái ta muốn. Chọn sai nghĩa là
//   mất đường encoder phần cứng và tụt xuống encoder phần mềm chậm hơn nhiều lần.
//
// ⚠ MỘT DEVICE DÙNG CHUNG CHO CẢ CAPTURE LẪN ENCODE
//   Đây là ràng buộc kiến trúc quan trọng nhất của file này. Texture D3D11 thuộc về
//   đúng cái device tạo ra nó; nếu capture chạy trên GPU A còn encoder trên GPU B
//   thì mỗi khung hình phải đi vòng qua bộ nhớ hệ thống để copy chéo — đủ để giết
//   toàn bộ ngân sách độ trễ. Cả hai dùng chung device trả về ở đây thì khung hình
//   không bao giờ rời VRAM.
//
// VỀ WARP
//   Đường lùi cuối cùng, chạy hoàn toàn trên CPU. Chậm, nhưng chạy được ở mọi nơi —
//   kể cả trong máy ảo không có GPU ảo hoá. `hardware = false` để người gọi biết mà
//   hạ kỳ vọng (và để in cảnh báo cho người dùng).
//
// LIÊN QUAN: capture/WindowCapture.h, encode/EncoderFactory.cpp (hai bên dùng chung
//            device), decode/Renderer.h
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <vector>

enum class GpuVendor { Nvidia,
    Intel,
    Amd,
    Microsoft /*WARP - software*/,
    Unknown };

const wchar_t* GpuVendorName(GpuVendor v);

struct GpuChoice {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::wstring description; // tên adapter
    GpuVendor vendor = GpuVendor::Unknown;
    bool hardware = false; // false = WARP/software
};

// Tạo D3D11 device trên GPU đầu tiên khớp `preference` (theo thứ tự). Nếu không adapter
// phần cứng nào dùng được, rớt về WARP (software). Trả về false nếu thất bại hoàn toàn.
bool CreateBestDevice(const std::vector<GpuVendor>& preference, GpuChoice& out);

// =============================================================================
// GpuSelect.cpp — cài đặt việc quét adapter DXGI và tạo device.
//
// CẤU TRÚC: HAI VÒNG LẶP LỒNG NHAU
//   Vòng ngoài đi theo THỨ TỰ ƯU TIÊN người gọi đưa vào; vòng trong quét mọi adapter
//   của máy tìm cái khớp vendor đang xét. Nghĩa là ta quét lại toàn bộ danh sách
//   adapter cho từng vendor — kém hiệu quả về lý thuyết, nhưng máy có nhiều nhất
//   vài adapter và hàm này chạy đúng một lần lúc khởi động.
//   Cách này giữ được điều quan trọng: ưu tiên do người gọi quyết định, không phải
//   do thứ tự DXGI liệt kê.
//
// VÌ SAO NHẬN DIỆN VENDOR BẰNG PCI ID
//   Chuỗi mô tả adapter không đáng tin (khác nhau giữa các đời driver, có bản dịch).
//   Mã nhà sản xuất PCI thì cố định vĩnh viễn: 0x10DE NVIDIA, 0x8086 Intel,
//   0x1002 AMD, 0x1414 là adapter phần mềm của Microsoft.
//
// LIÊN QUAN: capture/GpuSelect.h (vì sao phải chọn + ràng buộc dùng chung device)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "capture/GpuSelect.h"

#include <dxgi1_2.h>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

const wchar_t* GpuVendorName(GpuVendor v) {
    switch (v) {
        case GpuVendor::Nvidia: return L"NVIDIA";
        case GpuVendor::Intel: return L"Intel";
        case GpuVendor::Amd: return L"AMD";
        case GpuVendor::Microsoft: return L"Microsoft (WARP/software)";
        default: return L"Unknown";
    }
}

// Mã nhà sản xuất PCI — cố định vĩnh viễn, xem ghi chú đầu file.
static GpuVendor VendorFromId(UINT vendorId) {
    switch (vendorId) {
        case 0x10DE: return GpuVendor::Nvidia;
        case 0x8086: return GpuVendor::Intel;
        case 0x1002: return GpuVendor::Amd;
        case 0x1414: return GpuVendor::Microsoft;
        default: return GpuVendor::Unknown;
    }
}

// Cố gắng tạo device trên một adapter cụ thể (nullptr = WARP).
static bool TryCreateOnAdapter(IDXGIAdapter1* adapter, D3D_DRIVER_TYPE driverType,
    GpuChoice& out) {
    // Hai cờ này đều BẮT BUỘC, thiếu cái nào cũng hỏng ở tận tầng trên:
    //   BGRA_SUPPORT  — Windows.Graphics.Capture giao frame ở định dạng BGRA.
    //   VIDEO_SUPPORT — mở đường cho Media Foundation dùng encoder/decoder phần
    //                   cứng trên chính device này.
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
    // Xếp từ cao xuống thấp: D3D11CreateDevice lấy mức đầu tiên adapter đáp ứng
    // được. 10_1 là sàn — thấp hơn nữa thì không chạy nổi đường video phần cứng.
    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1};

    HRESULT hr = D3D11CreateDevice(
        adapter, driverType, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        out.device.ReleaseAndGetAddressOf(), nullptr,
        out.context.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

bool CreateBestDevice(const std::vector<GpuVendor>& preference, GpuChoice& out) {
    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        std::printf("CreateDXGIFactory1 failed.\n");
        return false;
    }

    // Thử từng vendor theo thứ tự ưu tiên; với mỗi vendor, quét các adapter.
    for (GpuVendor want : preference) {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            // Bỏ adapter phần mềm ở vòng này — WARP là đường lùi cuối cùng, chỉ
            // dùng khi đã quét hết phần cứng mà không được gì.
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
            if (VendorFromId(desc.VendorId) != want) continue;

            // Truyền adapter tường minh thì driverType PHẢI là UNKNOWN — đưa
            // HARDWARE kèm adapter cụ thể sẽ khiến D3D11CreateDevice trả E_INVALIDARG.
            // Đây là một trong những quy tắc dễ vấp nhất của API này.
            if (TryCreateOnAdapter(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, out)) {
                out.description = desc.Description;
                out.vendor = want;
                out.hardware = true;
                return true;
            }
        }
    }

    // Rớt về WARP (software) - "CPU".
    std::printf("No preferred hardware GPU found; falling back to WARP (software).\n");
    if (TryCreateOnAdapter(nullptr, D3D_DRIVER_TYPE_WARP, out)) {
        out.description = L"WARP (software rasterizer)";
        out.vendor = GpuVendor::Microsoft;
        out.hardware = false;
        return true;
    }
    return false;
}

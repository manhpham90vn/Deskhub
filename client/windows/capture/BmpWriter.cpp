// =============================================================================
// BmpWriter.cpp — cài đặt: kéo texture từ VRAM về CPU rồi ghi ra file BMP.
//
// HAI BƯỚC TÁCH BẠCH
//   CopyToCpu() — VRAM → std::vector<uint8_t> (phần D3D11).
//   WriteBmp()  — vector → file (phần định dạng file, không biết gì về D3D).
//
// VÌ SAO PHẢI CÓ "STAGING TEXTURE"
//   Texture do WGC giao ra nằm trong VRAM và CPU không đọc thẳng được. D3D11 bắt
//   phải tạo một texture trung gian với Usage = STAGING và cờ CPU_ACCESS_READ, copy
//   sang đó, rồi mới Map để đọc. Đây là con đường DUY NHẤT đưa dữ liệu ảnh từ GPU
//   về CPU trong D3D11.
//
// RowPitch — CẠM BẪY KINH ĐIỂN
//   Sau khi Map, các hàng ảnh KHÔNG nằm liền nhau: GPU căn mỗi hàng theo một bước
//   nhảy riêng (mapped.RowPitch) thường lớn hơn width*4 vì lý do căn biên. Chép một
//   phát cả khối bằng memcpy sẽ ra ảnh xô lệch chéo — phải chép TỪNG HÀNG và nhảy
//   theo RowPitch ở nguồn, còn ở đích thì đi liền theo rowSize.
//
// LIÊN QUAN: capture/BmpWriter.h (vì sao chậm, vì sao chọn BMP)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "capture/BmpWriter.h"

#include <wrl/client.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using Microsoft::WRL::ComPtr;

static bool CopyToCpu(ID3D11Device* device, ID3D11DeviceContext* context,
                      ID3D11Texture2D* src, std::vector<uint8_t>& outBgra,
                      UINT& outW, UINT& outH) {
    D3D11_TEXTURE2D_DESC desc{};
    src->GetDesc(&desc);
    outW = desc.Width;
    outH = desc.Height;

    // Chép mô tả của texture gốc rồi sửa bốn trường thành dạng staging. Giữ nguyên
    // Width/Height/Format để CopyResource chấp nhận — nó đòi hai texture khớp hệt
    // nhau về kích thước và định dạng.
    D3D11_TEXTURE2D_DESC sd = desc;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.BindFlags = 0;      // staging không gắn được vào pipeline
    sd.MiscFlags = 0;      // bỏ cờ chia sẻ của texture gốc nếu có
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&sd, nullptr, staging.GetAddressOf()))) return false;

    context->CopyResource(staging.Get(), src);

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;

    outBgra.resize(static_cast<size_t>(desc.Width) * desc.Height * 4);
    const uint8_t* srcRow = static_cast<const uint8_t*>(mapped.pData);
    uint8_t* dstRow = outBgra.data();
    const size_t   rowSize = static_cast<size_t>(desc.Width) * 4;

    // Chép từng hàng: nguồn nhảy theo RowPitch (có đệm), đích đi liền nhau.
    // Xem ghi chú về RowPitch ở đầu file.
    for (UINT y = 0; y < desc.Height; ++y) {
        std::memcpy(dstRow, srcRow, rowSize);
        srcRow += mapped.RowPitch;
        dstRow += rowSize;
    }
    context->Unmap(staging.Get(), 0);
    return true;
}

static bool WriteBmp(const std::string& path, const std::vector<uint8_t>& bgra,
                     UINT width, UINT height) {
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{};
    const DWORD pixelBytes = static_cast<DWORD>(width) * height * 4;

    fh.bfType = 0x4D42; // 'BM' — chữ ký nhận dạng file BMP
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + pixelBytes;

    ih.biSize = sizeof(ih);
    ih.biWidth = static_cast<LONG>(width);
    // Chiều cao ÂM = ảnh xếp từ trên xuống. BMP mặc định lưu ngược từ dưới lên; dấu
    // âm là cách nói "dữ liệu của tôi theo thứ tự tự nhiên", khớp với cách texture
    // nằm trong bộ nhớ nên khỏi phải lật ảnh.
    ih.biHeight = -static_cast<LONG>(height);
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = pixelBytes;

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(&fh, sizeof(fh), 1, f);
    std::fwrite(&ih, sizeof(ih), 1, f);
    std::fwrite(bgra.data(), 1, bgra.size(), f);
    std::fclose(f);
    return true;
}

bool SaveTextureToBmp(ID3D11Device* device, ID3D11DeviceContext* context,
                      ID3D11Texture2D* src, const std::string& path) {
    std::vector<uint8_t> pixels;
    UINT w = 0, h = 0;
    if (!CopyToCpu(device, context, src, pixels, w, h)) return false;
    return WriteBmp(path, pixels, w, h);
}

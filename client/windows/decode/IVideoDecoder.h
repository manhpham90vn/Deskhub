#pragma once
// =============================================================================
// IVideoDecoder.h — giao diện trừu tượng cho backend giải mã video.
//
// NHIỆM VỤ
//   Đối xứng với IVideoEncoder: vào là NAL Annex-B, ra là texture trong VRAM.
//   Hiện chỉ có một bản cài đặt (Media Foundation), nhưng giữ giao diện để sau này
//   thêm NVDEC không phải sửa nơi gọi.
//
// VỊ TRÍ TRONG LUỒNG DỮ LIỆU
//   UDP ~~~> Reassembler → **IVideoDecoder** → Renderer → màn hình
//   Giao diện này được dựng từ GĐ2 khi còn chạy loopback (encoder và decoder trong
//   CÙNG một process, không qua mạng). Sang GĐ3 nguồn NAL đổi từ loopback sang
//   mạng, và giao diện không phải đổi gì — đó là điều nó được thiết kế để làm.
//
// ⚠ HAI TRƯỜNG PHẢI ĐI CÙNG NHAU: texture VÀ subresource
//   Decoder không cấp một texture riêng cho mỗi frame. Nó dùng một TEXTURE-ARRAY
//   làm pool, và mỗi frame là một lát (array slice) trong đó. Nghĩa là `texture`
//   giống nhau qua nhiều frame liên tiếp, chỉ `subresource` đổi. Dùng texture mà
//   quên subresource sẽ vẽ ra một frame khác — hình sai chứ không crash, nên rất
//   khó lần ra.
//
// ⚠ VÒNG ĐỜI: chỉ hợp lệ trong phạm vi callback
//   Cùng quy tắc như FrameInfo bên capture (xem CaptureTypes.h). Lát texture sẽ
//   được decoder dùng lại cho frame sau ngay khi callback trả về — render hoặc
//   copy ngay, không giữ con trỏ lại.
//
// LIÊN QUAN: decode/MfDecoder.h (bản cài đặt), decode/Renderer.h (bên tiêu thụ),
//            encode/IVideoEncoder.h (đối xứng, và nguồn của enum Codec)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <cstdint>
#include <functional>
#include <memory>

#include "encode/IVideoEncoder.h" // dùng lại enum Codec

struct DecoderConfig {
    Codec codec = Codec::H264;
    uint32_t width = 0; // kích thước gợi ý; decoder tự đọc lại từ SPS khi stream đổi
    uint32_t height = 0;
    uint32_t fps = 60;
};

// Một frame vừa giải mã xong. `texture` là NV12 trong VRAM, thường là một phần tử
// của texture-array trong pool của decoder -> phải dùng kèm `subresource`.
// CHỈ hợp lệ trong phạm vi callback - render/copy ngay, không giữ lại.
struct DecodedFrame {
    ID3D11Texture2D* texture; // NV12, còn sống trong VRAM khi callback chạy
    UINT subresource;         // array slice trong pool của decoder
    uint32_t width;
    uint32_t height;
    uint64_t timestampUs; // timestamp truyền xuyên suốt từ capture
};

class IVideoDecoder {
public:
    // Callback mỗi khi giải mã xong một frame. Chạy trên luồng gọi Decode().
    using FrameHandler = std::function<void(const DecodedFrame&)>;

    virtual ~IVideoDecoder() = default;

    // Khởi tạo trên device dùng chung (cùng device với encoder ở loopback;
    // sang GD3 là device riêng của client). false nếu backend không dùng được.
    virtual bool Init(ID3D11Device* device, const DecoderConfig& cfg,
        FrameHandler onFrame) = 0;

    // Nạp một gói NAL Annex-B (1 frame nén). Frame giải mã xong trả về qua onFrame
    // (có thể 0 hoặc nhiều frame mỗi lần gọi, tùy decoder giữ trễ bao nhiêu).
    virtual bool Decode(const uint8_t* data, size_t size, uint64_t timestampUs) = 0;

    virtual const wchar_t* BackendName() const = 0;
};

// Factory: hiện tại chỉ có backend Media Foundation (D3D11VA hardware decode).
std::unique_ptr<IVideoDecoder> CreateDecoder(ID3D11Device* device, const DecoderConfig& cfg,
    IVideoDecoder::FrameHandler onFrame);

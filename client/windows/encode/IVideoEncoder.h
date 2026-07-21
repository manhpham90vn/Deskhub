#pragma once
// =============================================================================
// IVideoEncoder.h — giao diện trừu tượng cho mọi backend nén video.
//
// NHIỆM VỤ
//   Tách phần gọi encoder khỏi backend cụ thể (NVENC / Media Foundation / sau này
//   có thể thêm cái khác). Nhờ vậy AgentLoop viết một lần và chạy được trên máy
//   NVIDIA, Intel, AMD, hay thậm chí không có GPU — đúng chuỗi ưu tiên phần cứng
//   mà GpuSelect dựng ra.
//
// VỊ TRÍ TRONG LUỒNG DỮ LIỆU
//   WindowCapture → **IVideoEncoder** → Packetizer → Pacer → UDP
//   Vào là texture D3D11 trong VRAM, ra là NAL Annex-B qua callback onPacket.
//
// HAI ĐƯỜNG RA, DÙNG ĐƯỢC CÙNG LÚC
//   outputPath — ghi file .h264/.mp4, dùng để kiểm chứng bằng ffplay (di sản GĐ1,
//                vẫn giữ vì nó là cách gỡ lỗi nhanh nhất khi hình ra sai).
//   onPacket   — trả NAL cho tầng mạng. Đây là đường thật của streaming.
//   Để rỗng cái nào thì tắt đường đó.
//
// ⚠ QUY TẮC VÒNG ĐỜI CỦA onPacket
//   Callback chạy ĐỒNG BỘ trên chính thread gọi Encode(), và `data` chỉ hợp lệ
//   trong phạm vi lời gọi. Hai hệ quả:
//     - Phải copy hoặc tiêu thụ ngay, không giữ con trỏ lại.
//     - Không được làm việc chậm trong đó. Encode() thường được gọi từ callback
//       FrameArrived của WGC, nên ngủ ở đây là làm đứng cả đường bắt hình — xem
//       Pacer.h về đúng lỗi này và cái giá của nó.
//
// VÌ SAO CÓ CẢ width/height LẪN srcWidth/srcHeight
//   H.264 dùng NV12, lấy mẫu chroma theo khối 2×2, nên kích thước NÉN bắt buộc phải
//   CHẴN. Nhưng cửa sổ người dùng chọn có thể rộng hoặc cao lẻ. Khi đó ta nén ở
//   kích thước chẵn nhỏ hơn một điểm ảnh, còn texture đưa vào vẫn giữ kích thước
//   lẻ thật — video processor cần biết CẢ HAI để cắt cho đúng thay vì co giãn méo.
//   Bằng nhau thì để srcWidth/srcHeight = 0.
//
// LIÊN QUAN: encode/NvencEncoder.h, encode/MfEncoder.h (hai bản cài đặt),
//            encode/EncoderFactory.cpp (chọn cái nào), capture/CaptureTypes.h
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

enum class Codec { H264, HEVC };
enum class RateControl { CBR, VBR };

// Nhận một gói NAL Annex-B vừa nén xong (1 frame). Chạy trên luồng gọi Encode().
// `data` chỉ hợp lệ trong phạm vi callback - copy/tiêu thụ ngay.
using PacketHandler = std::function<void(const uint8_t* data, size_t size,
                                         uint64_t timestampUs, bool keyframe)>;

struct EncoderConfig {
    Codec        codec = Codec::H264;
    uint32_t     width = 0;   // kích thước NÉN - phải chẵn (NV12 lấy mẫu chroma 2x2)
    uint32_t     height = 0;
    // Kích thước texture đầu vào THẬT, nếu khác `width`/`height` (0 = bằng nhau).
    // Cửa sổ rộng/cao lẻ phải nén ở kích thước chẵn nhỏ hơn, nhưng texture đưa vào
    // vẫn là kích thước lẻ - video processor cần biết cả hai để cắt cho đúng.
    uint32_t     srcWidth = 0;
    uint32_t     srcHeight = 0;
    uint32_t     fps = 60;
    uint32_t     bitrateBps = 20'000'000;
    RateControl  rc = RateControl::CBR;
    bool         lowLatency = true;
    std::wstring outputPath = L"output.mp4";  // rỗng = không ghi file
    // GD2+: đường NAL trong process (loopback) / lên mạng (GD3). Cả NVENC lẫn MF
    // (Encoder MFT thẳng, không qua SinkWriter) đều hỗ trợ.
    PacketHandler onPacket;
};

class IVideoEncoder {
public:
    virtual ~IVideoEncoder() = default;

    // Khởi tạo trên device ĐÃ CHỌN (chia sẻ với capture). Trả về false nếu backend không dùng được.
    virtual bool Init(ID3D11Device* device, const EncoderConfig& cfg) = 0;

    // Nén một frame VRAM. `timestampUs` từ capture. `forceKeyframe` xin IDR (dùng khi mất gói).
    virtual bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) = 0;

    // GD5: đổi bitrate mục tiêu giữa chừng (congestion control theo FEEDBACK).
    // Không dựng lại encoder — chuỗi inter-frame giữ nguyên, không cần IDR.
    // false = backend không đổi được, caller cứ chạy tiếp với bitrate cũ.
    virtual bool SetBitrate(uint32_t bitrateBps) = 0;

    // Flush + finalize (ghi xong file / đóng stream).
    virtual void Finish() = 0;

    virtual const wchar_t* BackendName() const = 0;
};

// Factory: thử các backend theo thứ tự, trả về cái đầu tiên Init thành công.
// Hiện tại: Media Foundation (tự chọn HW theo device: NVENC/QSV, hoặc software).
std::unique_ptr<IVideoEncoder> CreateEncoder(ID3D11Device* device, const EncoderConfig& cfg);

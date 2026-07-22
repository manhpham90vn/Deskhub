#pragma once
// =============================================================================
// NvencEncoder.h — backend nén video bằng NVIDIA NVENC (Video Codec SDK).
//
// NHIỆM VỤ
//   Bản cài đặt IVideoEncoder cho máy có card NVIDIA. Đây là lựa chọn ƯU TIÊN của
//   EncoderFactory vì nó cho độ trễ thấp nhất trong các đường có sẵn.
//
// VÌ SAO NVENC ĐỨNG ĐẦU CHUỖI
//   - Preset ULTRA_LOW_LATENCY được thiết kế đúng cho bài toán này (game streaming),
//     không phải cho lưu trữ hay phát lại.
//   - Xuất thẳng NAL Annex-B rời, đúng thứ Packetizer cần — không phải bóc khỏi
//     container nào cả.
//   - Có FORCEIDR chuẩn và đáng tin, tức là phục hồi được khi client mất gói.
//   - Đăng ký texture D3D11 trực tiếp: khung hình không rời VRAM trên cả đường từ
//     capture tới encoder (điều kiện là cùng device — xem GpuSelect.h).
//
// NẠP DLL ĐỘNG, KHÔNG LIÊN KẾT TĨNH
//   nvEncodeAPI64.dll đi kèm DRIVER chứ không kèm chương trình, nên không thể liên
//   kết .lib lúc dịch — máy không có NVIDIA sẽ không chạy nổi file exe. Nạp động
//   bằng LoadLibrary nghĩa là một binary chạy được ở mọi nơi: không có DLL thì
//   Init() trả false và factory lặng lẽ rớt sang Media Foundation.
//   Vì thế dự án chỉ cần header của SDK, không cần thư viện.
//
// PIMPL
//   Toàn bộ kiểu dữ liệu của NVENC SDK nằm trong .cpp, header này sạch — cùng lý do
//   như WindowCapture giấu winrt.
//
// LIÊN QUAN: encode/IVideoEncoder.h (giao diện), encode/MfEncoder.h (đường lùi),
//            encode/EncoderFactory.cpp
// =============================================================================
#include "encode/IVideoEncoder.h"

class NvencEncoder : public IVideoEncoder {
public:
    NvencEncoder();
    ~NvencEncoder() override;

    bool Init(ID3D11Device* device, const EncoderConfig& cfg) override;
    bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) override;
    bool SetBitrate(uint32_t bitrateBps) override;
    void Finish() override;
    const wchar_t* BackendName() const override {
        return L"NVENC (NVIDIA)";
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

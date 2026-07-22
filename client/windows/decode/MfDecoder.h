#pragma once
// =============================================================================
// MfDecoder.h — backend giải mã bằng Media Foundation (decoder MFT + D3D11VA).
//
// NHIỆM VỤ
//   Bản cài đặt IVideoDecoder duy nhất hiện có. Chạy trên mọi máy Windows.
//
// VÌ SAO MEDIA FOUNDATION CHO ĐƯỜNG GIẢI MÃ
//   - Có sẵn trong Windows SDK, không cần SDK hãng thứ ba (khác NVDEC, vốn đòi
//     card NVIDIA — mà phía client thì ta không chọn được máy).
//   - MFT của Microsoft là D3D11-aware: gắn DXGI device manager vào là nó giải mã
//     bằng phần cứng qua D3D11VA và trả texture NV12 ngay trong VRAM, đi thẳng
//     sang Renderer không qua CPU.
//   - Có MF_LOW_LATENCY để bắt MFT trả frame ngay thay vì giữ lại vài cái.
//
// VÌ SAO DÙNG MFT TRẦN, KHÔNG QUA Source Reader
//   Source Reader dễ dùng hơn nhưng nó đọc FILE CONTAINER. Đầu vào của ta là NAL
//   thô đến từ mạng, không có container nào cả. Cùng lý do như MfEncoder không
//   dùng SinkWriter.
//
// ĐƠN GIẢN HƠN MfEncoder MỘT BẬC
//   File này chỉ chạy MFT ĐỒNG BỘ (MFTEnumEx lọc bằng MFT_ENUM_FLAG_SYNCMFT), nên
//   KHÔNG có nhánh bất đồng bộ với vòng lặp sự kiện như bên encoder. Đó là lý do
//   nó ngắn hơn nhiều dù làm việc đối xứng.
//
// LIÊN QUAN: decode/IVideoDecoder.h (giao diện), decode/Renderer.h (bên tiêu thụ),
//            encode/MfEncoder.h (đối chiếu: bên đó phải xử lý cả async)
// =============================================================================
#include "decode/IVideoDecoder.h"

class MfDecoder : public IVideoDecoder {
public:
    MfDecoder();
    ~MfDecoder() override;

    bool Init(ID3D11Device* device, const DecoderConfig& cfg,
        FrameHandler onFrame) override;
    bool Decode(const uint8_t* data, size_t size, uint64_t timestampUs) override;
    const wchar_t* BackendName() const override {
        return L"Media Foundation (D3D11VA)";
    }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

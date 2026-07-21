#pragma once
// =============================================================================
// MfEncoder.h — backend nén video bằng Media Foundation Transform (MFT).
//
// NHIỆM VỤ
//   Bản cài đặt IVideoEncoder cho MỌI máy Windows khác — Intel QSV, AMD VCE, hoặc
//   encoder phần mềm nếu không có gì. Đây là đường lùi của EncoderFactory sau NVENC,
//   và cũng là đường bảo đảm chương trình luôn chạy được.
//
// VÌ SAO DÙNG THẲNG MFT, KHÔNG QUA IMFSinkWriter
//   SinkWriter là API dễ dùng hơn nhiều, nhưng nó gói đầu ra vào CONTAINER (.mp4).
//   Ta cần NAL Annex-B THÔ để Packetizer cắt gửi UDP — bóc ngược khỏi mp4 vừa thừa
//   vừa thêm độ trễ. Dùng MFT trần đổi lấy sự phức tạp ở .cpp nhưng cho đúng thứ cần.
//
// BỐN ĐIỂM ĐÁNG BIẾT TRƯỚC KHI ĐỌC .cpp
//   1. MFTEnumEx tự tìm MFT phù hợp với device D3D11 đang dùng, ưu tiên phần cứng
//      nhờ cờ SORTANDFILTER. Không phải chỉ định hãng nào.
//   2. Đầu vào là NV12, còn capture giao ra BGRA — phải chuyển màu bằng D3D11 Video
//      Processor, chạy trên GPU nên không đồng bộ hoá với CPU.
//   3. MFT phần cứng thường BẤT ĐỒNG BỘ: không gọi ProcessInput/ProcessOutput thẳng
//      được mà phải unlock rồi chờ sự kiện qua IMFMediaEventGenerator. Đây là khác
//      biệt lớn nhất so với NVENC và là phần rối nhất của .cpp.
//   4. SPS/PPS lấy từ MF_MT_MPEG_SEQUENCE_HEADER của kiểu đầu ra và tự chèn trước
//      mỗi IDR — tương đương repeatSPSPPS của NVENC. Không có nó thì client vào
//      giữa chừng (hoặc vừa mất gói) sẽ không có tham số để bắt đầu giải mã.
//
// LIÊN QUAN: encode/IVideoEncoder.h (giao diện), encode/NvencEncoder.h (đường ưu
//            tiên hơn), encode/EncoderFactory.cpp
// =============================================================================
#include "encode/IVideoEncoder.h"

class MfEncoder : public IVideoEncoder {
public:
    MfEncoder();
    ~MfEncoder() override;

    bool Init(ID3D11Device* device, const EncoderConfig& cfg) override;
    bool Encode(ID3D11Texture2D* frame, uint64_t timestampUs, bool forceKeyframe) override;
    bool SetBitrate(uint32_t bitrateBps) override;
    void Finish() override;
    const wchar_t* BackendName() const override { return L"Media Foundation (HW/SW auto)"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

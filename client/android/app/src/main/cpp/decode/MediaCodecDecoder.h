#pragma once
// =============================================================================
// MediaCodecDecoder.h — giải mã H.264 bằng bộ giải mã phần cứng của Android.
//
// NHIỆM VỤ
//   Nhận frame H.264 Annex-B đã ghép đủ (từ Reassembler) và đưa nó lên màn hình.
//   Đối ứng Android của MfDecoder bên Windows.
//
// KHÁC BIỆT CĂN BẢN SO VỚI BẢN WINDOWS — VÀ NÓ CÓ LỢI
//   Codec được configure THẲNG với ANativeWindow của Surface. Nghĩa là frame đi từ
//   bộ giải mã phần cứng ra màn hình qua hardware composer, KHÔNG qua CPU, không
//   qua bộ nhớ chung, và không cần lớp Renderer riêng như bên Windows.
//   Hệ quả trực tiếp: AMediaCodec_releaseOutputBuffer(..., true) chính là thao tác
//   "render" — không có bước vẽ nào khác trong toàn bộ app.
//
// MÔ HÌNH HÀNG ĐỢI CỦA MediaCodec
//   MediaCodec làm việc theo kiểu hai hàng đợi, KHÔNG phải gọi-là-có-kết-quả:
//     nạp vào  — mượn input buffer → chép dữ liệu → trả lại kèm PTS;
//     lấy ra   — hỏi output buffer đã xong chưa → có thì render → trả lại.
//   Hai chiều KHÔNG khớp một-đổi-một: nạp một frame có thể chưa ra gì, hoặc ra vài
//   frame cùng lúc. Vì thế Decode() luôn kết thúc bằng DrainOutput() để vét sạch
//   những gì đã sẵn sàng, thay vì giả định "nạp một, lấy một".
//
// MÔ HÌNH LUỒNG
//   Dùng trên MỘT thread (thread Decode). Init/Shutdown cũng phải trên đúng thread
//   đó — AMediaCodec không an toàn đa luồng, và Surface thì thuộc về main thread
//   nên việc bàn giao nó phải qua cơ chế bắt tay ở ClientLoop::SetWindow.
//
// LIÊN QUAN: ClientLoop.h (chủ sở hữu + luồng Decode),
//            client/windows/decode/MfDecoder.h (bản song song trên Media Foundation)
// =============================================================================
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>

#include <cstddef>
#include <cstdint>

class MediaCodecDecoder {
public:
    MediaCodecDecoder() = default;
    ~MediaCodecDecoder();
    MediaCodecDecoder(const MediaCodecDecoder&) = delete;
    MediaCodecDecoder& operator=(const MediaCodecDecoder&) = delete;

    // `window` phải sống lâu hơn decoder (chủ sở hữu là main thread của app).
    bool Init(ANativeWindow* window, int width, int height);
    void Shutdown();
    bool IsOpen() const {
        return codec_ != nullptr;
    }

    // Nạp một frame Annex-B đã ghép đủ và vẽ các frame đã sẵn sàng.
    // false = lỗi codec -> caller dựng lại decoder và xin IDR.
    bool Decode(const uint8_t* nal, size_t len, uint64_t ptsUs);

    // Số frame đã thực sự đưa lên màn hình kể từ lần gọi trước.
    uint32_t TakeRenderedCount();

    // PTS (đồng hồ host) của frame vừa đưa lên màn hình gần nhất — mốc để tính trễ
    // e2e THẬT (tính lúc nạp vào codec sẽ bỏ sót cả phần decode + hiển thị).
    // 0 = chưa render frame nào.
    uint64_t lastRenderedPtsUs() const {
        return lastRenderedPtsUs_;
    }

private:
    // Rút mọi output đang sẵn sàng và render. false = lỗi.
    bool DrainOutput();

    AMediaCodec* codec_ = nullptr;
    bool sentCsd_ = false; // đã nạp SPS/PPS dưới cờ CODEC_CONFIG chưa
    uint32_t rendered_ = 0;
    uint64_t lastRenderedPtsUs_ = 0;
};

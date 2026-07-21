// =============================================================================
// EncoderFactory.cpp — chọn backend encoder theo chuỗi ưu tiên.
//
// NHIỆM VỤ
//   Trả về encoder đầu tiên khởi tạo được trên device đang dùng. Người gọi không
//   cần biết máy có GPU gì.
//
// CHIẾN LƯỢC: THỬ RỒI RỚT DẦN
//   1. NVENC — độ trễ thấp nhất, nhưng chỉ chạy trên card NVIDIA.
//   2. Media Foundation — có mặt trên mọi máy Windows, tự chọn phần cứng theo
//      device (Intel QSV, AMD VCE) hoặc rớt tiếp xuống encoder phần mềm.
//
//   Không cần hỏi trước "máy này có GPU gì" — cứ gọi Init() và để nó thất bại. Đây
//   là cách đáng tin hơn hẳn việc đoán theo tên adapter: NVENC có thể vắng mặt ngay
//   trên máy NVIDIA (driver quá cũ, hoặc dòng card bị cắt tính năng), và chỉ có
//   chính lời gọi Init mới biết chắc.
//
//   Thứ tự này khớp với chuỗi ưu tiên phần cứng ở GpuSelect (NVIDIA → Intel → CPU),
//   nhưng hai bên độc lập nhau: GpuSelect chọn nơi TÍNH, factory chọn cách NÉN.
//
// LIÊN QUAN: encode/IVideoEncoder.h (giao diện), capture/GpuSelect.h (chuỗi ưu
//            tiên song song), AgentLoop.cpp (người gọi)
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "encode/IVideoEncoder.h"
#include "encode/NvencEncoder.h"
#include "encode/MfEncoder.h"

#include <cstdio>

std::unique_ptr<IVideoEncoder> CreateEncoder(ID3D11Device* device, const EncoderConfig& cfg) {
    // 1. NVENC trước.
    {
        auto enc = std::make_unique<NvencEncoder>();
        if (enc->Init(device, cfg)) {
            std::printf("[Encoder] Using backend: %ls\n", enc->BackendName());
            return enc;
        }
        std::printf("[Encoder] NVENC unavailable, trying Media Foundation...\n");
    }
    // 2. Media Foundation (fallback).
    {
        auto enc = std::make_unique<MfEncoder>();
        if (enc->Init(device, cfg)) {
            std::printf("[Encoder] Using backend: %ls\n", enc->BackendName());
            return enc;
        }
    }
    std::printf("[Encoder] Failed to initialize any backend.\n");
    return nullptr;
}

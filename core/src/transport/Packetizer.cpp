#include "rgc/transport/Packetizer.h"

#include <cstring>

namespace rgc {

size_t Packetizer::SendFrame(std::span<const uint8_t> nal, uint32_t frameId,
                             uint64_t timestampUs, bool idr, const SendFn& send) {
    if (nal.empty() || !send) return 0;
    const size_t count = (nal.size() + kMaxVideoPayload - 1) / kMaxVideoPayload;
    if (count > 0xFFFF) return 0;
    // groupIndex là u8: quá 256 nhóm thì không đánh số được → gửi trần, không FEC.
    const bool fec = fec_ && (count + kFecGroupSize - 1) / kFecGroupSize <= 256;

    VideoHeader vh;
    vh.frameId     = frameId;
    vh.timestampUs = timestampUs;
    vh.pktCount    = uint16_t(count);

    FecHeader fh;
    fh.frameId     = frameId;
    fh.timestampUs = timestampUs;
    fh.pktCount    = uint16_t(count);

    // Parity đi SAU cả nhóm: gửi trước thì nó tới trước gói dữ liệu và bên nhận phải
    // giữ chỗ chờ; gửi sau thì lúc nó tới ta đã biết chính xác còn thiếu gói nào.
    auto flushParity = [&](size_t groupIdx) -> bool {
        fh.groupIndex = uint8_t(groupIdx);
        const size_t n = BuildFecPacket(buf_, sessionId_, fh, idr,
                                        std::span<const uint8_t>(parity_, sizeof(parity_)));
        if (!n) return false;
        send(std::span<const uint8_t>(buf_, n));
        return true;
    };

    if (fec) std::memset(parity_, 0, sizeof(parity_));

    for (size_t i = 0; i < count; ++i) {
        const size_t off = i * kMaxVideoPayload;
        const size_t len = (nal.size() - off < kMaxVideoPayload) ? nal.size() - off
                                                                 : kMaxVideoPayload;
        vh.pktIndex = uint16_t(i);
        const bool frameEnd = (i + 1 == count);
        const size_t n = BuildVideoPacket(buf_, sessionId_, vh, idr, frameEnd,
                                          nal.subspan(off, len));
        if (!n) return 0;
        send(std::span<const uint8_t>(buf_, n));

        if (!fec) continue;
        // XOR cả độ dài lẫn dữ liệu. Chỉ gói cuối frame ngắn hơn kMaxVideoPayload,
        // nhưng bên nhận không đoán được độ dài gói THIẾU nếu không có lenXor.
        parity_[0] ^= uint8_t(len >> 8);
        parity_[1] ^= uint8_t(len & 0xFF);
        for (size_t b = 0; b < len; ++b)
            parity_[kFecLenPrefix + b] ^= nal[off + b];

        const bool groupEnd = ((i + 1) % kFecGroupSize == 0) || frameEnd;
        if (groupEnd) {
            if (!flushParity(i / kFecGroupSize)) return 0;
            std::memset(parity_, 0, sizeof(parity_));
        }
    }
    return count;
}

} // namespace rgc

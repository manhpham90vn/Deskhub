// =============================================================================
// Reassembler.cpp — cài đặt việc ghép mảnh, khôi phục FEC và chính sách bỏ frame.
//
// BỐ CỤC (theo dòng chảy của một gói qua lớp này)
//   Slot()       — tìm/tạo chỗ ghép cho một frameId, và chặn gói quá muộn.
//   Push()       — nhận mảnh dữ liệu.
//   PushFec()    — nhận gói parity.
//   TryRecover() — thử dựng lại mảnh thiếu bằng XOR ngược.
//   PopReady()   — quyết định: trả frame ra, hay bỏ nó, hay chờ thêm.
//   Drop()       — bỏ một frame, cập nhật thống kê và mốc chặn.
//
// KHÁI NIỆM "BARRIER" (barrierId_)
//   Mốc frameId mà mọi frame ≤ nó đã được phát ra hoặc đã bị bỏ. Gói đến sau mốc
//   này là gói lạc quá muộn — nhận vào chỉ tổ dựng lại một frame đã lỡ, và nếu
//   phát ra sẽ đưa decoder đi lùi thời gian. Nên chúng bị bỏ ngay tại Slot().
//
// CẤU TRÚC DỮ LIỆU
//   pending_ là std::map (cây đỏ-đen) chứ không phải unordered_map, vì lớp này cần
//   duyệt theo THỨ TỰ frameId liên tục: pending_.begin() luôn là frame cũ nhất,
//   tức là frame kế tiếp phải phát. Với tối đa 4 phần tử thì chi phí cây không đáng kể.
//
// VỀ THỜI GIAN
//   Không gọi đồng hồ hệ thống ở bất cứ đâu; `nowUs` do người gọi bơm vào. Đó là
//   lý do CoreTests kiểm chứng được cả đường timeout mà không phải sleep thật.
//
// LIÊN QUAN: rgc/transport/Reassembler.h (thiết kế + chính sách), Packetizer.cpp
// =============================================================================
#include "rgc/transport/Reassembler.h"

#include <iterator>

namespace rgc {

// Cổng vào chung của Push và PushFec: mọi gói đều phải qua đây để lấy chỗ ghép.
// Trả nullptr nghĩa là "bỏ gói này" — quá muộn, hoặc không khớp frame đang ghép.
Reassembler::Pending* Reassembler::Slot(uint32_t id, uint16_t pktCount,
                                        uint64_t timestampUs, uint64_t nowUs) {
    if (haveBarrier_ && id <= barrierId_) return nullptr; // gói muộn của frame đã phát/bỏ

    auto it = pending_.find(id);
    if (it == pending_.end()) {
        // Frame mới trong khi đã đầy chỗ: frame già nhất đang chặn hàng — bỏ nó.
        while (pending_.size() >= kMaxPendingFrames)
            Drop(pending_.begin(), true);
        it = pending_.emplace(id, Pending{}).first;
        Pending& f = it->second;
        f.pktCount = pktCount;
        f.pieces.resize(pktCount);
        f.timestampUs = timestampUs;
        f.firstSeenUs = nowUs;
    }
    if (it->second.pktCount != pktCount) return nullptr; // gói hỏng / không khớp frame
    return &it->second;
}

// Nhận một mảnh dữ liệu. Đếm packetsReceived TRƯỚC mọi kiểm tra, kể cả với gói
// trùng hay quá muộn: đây là mẫu số của tỉ lệ mất gói, bỏ sót gói nào ở đây sẽ làm
// tỉ lệ báo cáo cho host lệch đi.
void Reassembler::Push(const VideoPacketView& pkt, uint64_t nowUs) {
    ++stats_.packetsReceived;
    if (pkt.payload.empty()) return; // Packetizer không bao giờ phát mảnh rỗng

    Pending* fp = Slot(pkt.hdr.frameId, pkt.hdr.pktCount, pkt.hdr.timestampUs, nowUs);
    if (!fp) return;
    Pending& f = *fp;
    if (pkt.hdr.pktIndex >= f.pktCount) return; // gói hỏng
    auto& slot = f.pieces[pkt.hdr.pktIndex];
    if (!slot.empty()) return; // trùng
    slot.assign(pkt.payload.begin(), pkt.payload.end());
    f.bytes += slot.size();
    f.idr = f.idr || pkt.idr;
    ++f.received;

    // Gói này có thể là mảnh cuối nhóm cần để parity (đã tới trước) dùng được.
    TryRecover(f, uint8_t(pkt.hdr.pktIndex / kFecGroupSize));
}

// Nhận một gói parity. Đếm riêng vào fecReceived chứ KHÔNG cộng vào
// packetsReceived — trộn parity vào mẫu số sẽ làm tỉ lệ mất gói tụt xuống đúng vào
// lúc FEC đang bật, tức là đúng lúc đường truyền đang có vấn đề cần báo cáo.
void Reassembler::PushFec(const FecPacketView& pkt, uint64_t nowUs) {
    ++stats_.fecReceived;
    if (pkt.parity.size() < kFecLenPrefix) return;

    Pending* fp = Slot(pkt.hdr.frameId, pkt.hdr.pktCount, pkt.hdr.timestampUs, nowUs);
    if (!fp) return;
    Pending& f = *fp;
    f.idr = f.idr || pkt.idr;

    auto& slot = f.parity[pkt.hdr.groupIndex];
    if (!slot.empty()) return; // trùng
    slot.assign(pkt.parity.begin(), pkt.parity.end());
    TryRecover(f, pkt.hdr.groupIndex);
}

// Thử dựng lại mảnh thiếu của một nhóm FEC.
//
// Nguyên lý: parity = m₁ ⊕ m₂ ⊕ … ⊕ mₙ. XOR có tính chất tự nghịch đảo, nên nếu
// thiếu mảnh mₖ thì mₖ = parity ⊕ (mọi mảnh còn lại). Đúng một ẩn số thì giải được;
// hai ẩn trở lên thì một phương trình là không đủ.
//
// Gọi từ CẢ HAI đường (Push và PushFec) vì không biết trước cái nào tới sau: mảnh
// dữ liệu cuối cùng có thể tới sau parity, hoặc ngược lại. Gọi thừa là vô hại —
// hàm tự thoát ngay khi điều kiện chưa đủ.
bool Reassembler::TryRecover(Pending& f, uint8_t group) {
    auto pit = f.parity.find(group);
    if (pit == f.parity.end()) return false;
    const std::vector<uint8_t>& par = pit->second;

    const size_t first = size_t(group) * kFecGroupSize;
    if (first >= f.pktCount) return false;
    size_t last = first + kFecGroupSize;
    if (last > f.pktCount) last = f.pktCount;

    // Parity XOR chỉ gỡ được MỘT ẩn số. Không thiếu gói nào thì thôi, thiếu ≥2 thì chịu.
    size_t missing = 0, missingIdx = 0;
    for (size_t i = first; i < last; ++i)
        if (f.pieces[i].empty()) { ++missing; missingIdx = i; }
    if (missing != 1) return false;

    // XOR ngược: parity ^ (mọi mảnh đã có) = mảnh thiếu, kèm 2 byte độ dài đứng đầu.
    std::vector<uint8_t> rec(par);
    for (size_t i = first; i < last; ++i) {
        if (i == missingIdx) continue;
        const auto& p = f.pieces[i];
        rec[0] ^= uint8_t(p.size() >> 8);
        rec[1] ^= uint8_t(p.size() & 0xFF);
        for (size_t b = 0; b < p.size(); ++b) rec[kFecLenPrefix + b] ^= p[b];
    }

    const size_t len = (size_t(rec[0]) << 8) | rec[1];
    // Độ dài dựng ra phải hợp lệ: parity hỏng/không cùng frame sẽ cho số vô nghĩa,
    // nhét bừa vào NAL còn tệ hơn bỏ frame.
    if (len == 0 || len > kMaxVideoPayload || kFecLenPrefix + len > rec.size()) return false;

    f.pieces[missingIdx].assign(rec.begin() + kFecLenPrefix,
                                rec.begin() + kFecLenPrefix + len);
    f.bytes += len;
    ++f.received;
    ++stats_.packetsRecovered;
    return true;
}

// Trái tim của chính sách. Mỗi vòng lặp chỉ xét frame CŨ NHẤT (đầu hàng) và quyết
// một trong ba điều:
//   - đủ mảnh → ghép lại, trả ra, dời barrier;
//   - hết hy vọng (quá hạn, hoặc đã bị hai frame mới hơn vượt mặt) → bỏ, xét tiếp;
//   - còn hy vọng → dừng, trả nullopt.
// Không bao giờ nhảy qua đầu hàng để trả frame sau, kể cả khi frame sau đã đủ:
// decoder H.264 cần đúng thứ tự.
std::optional<Reassembler::Frame> Reassembler::PopReady(uint64_t nowUs) {
    while (!pending_.empty()) {
        auto head = pending_.begin();
        Pending& f = head->second;

        if (f.Complete()) {
            if (waitingForIdr_ && !f.idr) { // nuốt non-IDR khi chờ IDR
                Drop(head, false);
                continue;
            }
            // Nối các mảnh theo đúng thứ tự pktIndex thành NAL Annex-B liền mạch —
            // đúng chuỗi byte mà encoder đã đẻ ra ở đầu kia. reserve trước theo
            // f.bytes để chỉ cấp phát một lần.
            Frame out;
            out.frameId     = head->first;
            out.timestampUs = f.timestampUs;
            out.idr         = f.idr;
            out.nal.reserve(f.bytes);
            for (const auto& p : f.pieces)
                out.nal.insert(out.nal.end(), p.begin(), p.end());
            haveBarrier_ = true;
            barrierId_   = head->first;
            waitingForIdr_ = false;
            ++stats_.framesCompleted;
            pending_.erase(head);
            return out;
        }

        // Đầu hàng thiếu mảnh: bỏ nếu quá hạn hoặc bị ≥2 frame hoàn chỉnh mới hơn vượt mặt.
        size_t newerComplete = 0;
        for (auto n = std::next(head); n != pending_.end(); ++n)
            if (n->second.Complete()) ++newerComplete;
        if (nowUs - f.firstSeenUs > 2 * frameIntervalUs_ || newerComplete >= 2) {
            Drop(head, true);
            continue;
        }
        return std::nullopt; // còn hy vọng mảnh thiếu đang trên đường tới
    }
    return std::nullopt;
}

bool Reassembler::TakeLossEvent() {
    const bool e = lossEvent_;
    lossEvent_ = false;
    return e;
}

// Bỏ một frame khỏi hàng chờ. `loss` phân biệt hai tình huống rất khác nhau:
//   true  — frame thiếu mảnh vì MẤT GÓI THẬT. Tính vào thống kê mất mát, bật cờ
//           xin keyframe, và chuyển sang trạng thái chờ IDR.
//   false — frame LÀNH LẶN nhưng bị nuốt vì đang chờ IDR. Không phải lỗi đường
//           truyền, chỉ tính vào framesSkipped.
// Dù theo đường nào, barrier vẫn được dời tới để gói lạc của frame này bị chặn.
void Reassembler::Drop(PendingMap::iterator it, bool loss) {
    if (loss) {
        ++stats_.framesDropped;
        stats_.packetsLost += uint64_t(it->second.pktCount - it->second.received);

        // Đếm chùm mất liên tiếp (xem Stats::lossRuns). Chỉ chạy trên frame BỎ ĐI nên
        // không nằm trên đường nóng.
        const Pending& f = it->second;
        size_t run = 0;
        for (size_t i = 0; i <= f.pktCount; ++i) {
            const bool gone = i < f.pktCount && f.pieces[i].empty();
            if (gone) {
                ++run;
            } else if (run) {
                size_t b = 0;
                if (run <= 3)       b = run - 1;   // 1, 2, 3 tách riêng: chùm ngắn là
                else if (run < 8)   b = 3;         // thứ FEC hiện tại còn có cửa cứu
                else if (run < 16)  b = 4;
                else if (run < 32)  b = 5;
                else                b = 6;
                ++stats_.lossRuns[b];
                if (run > stats_.lossRunMax) stats_.lossRunMax = run;
                run = 0;
            }
        }

        lossEvent_ = true;
        ++stats_.lossEvents;
        waitingForIdr_ = true; // frame sau tham chiếu frame vừa mất → phải chờ IDR
    } else {
        ++stats_.framesSkipped;
    }
    if (!haveBarrier_ || it->first > barrierId_) {
        haveBarrier_ = true;
        barrierId_   = it->first;
    }
    pending_.erase(it);
}

} // namespace rgc

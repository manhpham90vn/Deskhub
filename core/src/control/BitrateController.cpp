#include "rgc/control/BitrateController.h"

namespace rgc {

BitrateDecision BitrateController::Update(const Feedback& fb, uint64_t nowUs) {
    BitrateDecision d;

    // --- FEC ---
    // FEC tốn 1/kFecGroupSize băng thông nên chỉ bật khi đang thực sự mất gói.
    // Tắt CHẬM hơn bật (5 giây sạch): mất gói thường đến theo cụm, tắt ngay sau
    // một giây yên là vừa tắt xong đã phải bật lại.
    const bool fecBefore = fec_;
    if (fb.lossPct >= 1) {
        cleanSeconds_ = 0;
        fec_ = true;
    } else if (++cleanSeconds_ >= 5) {
        fec_ = false;
    }
    d.fecEnabled = fec_;
    d.fecToggled = (fec_ != fecBefore);

    // --- Bitrate ---
    uint32_t next = cur_;
    if (fb.lossPct >= 5) {
        next = cur_ - cur_ / 4;         // ×0.75 — mất nhiều, lùi mạnh
        lastDecreaseUs_ = nowUs;
    } else if (fb.lossPct >= 2) {
        next = cur_ - cur_ / 10;        // ×0.90 — chớm nghẽn, lùi nhẹ
        lastDecreaseUs_ = nowUs;
    } else if (fb.lossPct <= 1 && nowUs - lastDecreaseUs_ > 2'000'000) {
        // Nới +5% TRẦN mỗi giây, và chỉ sau 2 giây không phải tụt: nới ngay sau khi
        // vừa tụt thì chỉ dao động quanh mức nghẽn chứ không hội tụ.
        next = cur_ + max_ / 20;
    }

    if (next > max_) next = max_;
    if (next < min_) next = min_;

    // Bỏ qua thay đổi vụn (<2%): mỗi lần đổi là một lần đàm phán lại rate control
    // của encoder, không đáng cho vài chục kbps.
    const uint32_t delta = next > cur_ ? next - cur_ : cur_ - next;
    d.changeBitrate = (next != cur_) && (delta >= cur_ / 50);
    d.bitrateBps = d.changeBitrate ? next : cur_;
    return d;
}

} // namespace rgc

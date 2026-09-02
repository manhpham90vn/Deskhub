#include "FecSchemes.h"

#include "FecShared.h"

#include <cstdint>
#include <vector>

namespace deskhub::fec {
namespace {

class XorFecScheme final : public FecScheme {
public:
    std::string_view Name() const override {
        return "xor";
    }

    size_t ParityPerGroup() const override {
        return 1;
    }

    bool SetParityPerGroup(size_t count) override {
        return count == 1;
    }

    size_t MaxRecoverablePerGroup() const override {
        return 1;
    }

    size_t Encode(std::span<const std::span<const uint8_t>> group,
        std::span<std::span<const uint8_t>> out) override {
        if (out.empty()) return 0;
        const size_t longest = LongestMember(group);
        if (longest == 0) return 0;

        scratch_.assign(kFecLenPrefix + longest, 0);
        for (const std::span<const uint8_t>& member : group) AccumulateMember(scratch_, member);
        out[0] = scratch_;
        return 1;
    }

    size_t Recover(std::span<const FecSlot> group,
        std::span<const std::span<const uint8_t>> parity, std::span<FecRecovery> out) override {
        if (out.empty() || parity.empty() || parity[0].size() < kFecLenPrefix) return 0;

        size_t missing = 0, missingSlot = 0;
        for (size_t i = 0; i < group.size(); ++i)
            if (!group[i].present) {
                ++missing;
                missingSlot = i;
            }
        if (missing != 1) return 0;

        scratch_.assign(parity[0].begin(), parity[0].end());
        for (size_t i = 0; i < group.size(); ++i) {
            if (i == missingSlot) continue;
            if (kFecLenPrefix + group[i].bytes.size() > scratch_.size()) return 0;
            AccumulateMember(scratch_, group[i].bytes);
        }

        const size_t len = LengthPrefixOf(scratch_);
        if (len == 0 || len > kMaxVideoPayload || kFecLenPrefix + len > scratch_.size()) return 0;

        out[0].slot = missingSlot;
        out[0].bytes = std::span<const uint8_t>(scratch_).subspan(kFecLenPrefix, len);
        return 1;
    }

private:
    std::vector<uint8_t> scratch_;
};

}

std::unique_ptr<FecScheme> MakeXor() {
    return std::make_unique<XorFecScheme>();
}

}

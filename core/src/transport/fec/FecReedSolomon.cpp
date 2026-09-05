#include "FecSchemes.h"

#include "FecShared.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace deskhub::fec {
namespace {

class Gf256 {
public:
    Gf256() {
        uint16_t x = 1;
        for (size_t i = 0; i < 255; ++i) {
            exp_[i] = uint8_t(x);
            log_[x] = uint8_t(i);
            x <<= 1;
            if (x & 0x100) x ^= 0x11D;
        }
        for (size_t i = 255; i < 512; ++i) exp_[i] = exp_[i - 255];
        log_[0] = 0;
    }

    uint8_t Mul(uint8_t a, uint8_t b) const {
        if (a == 0 || b == 0) return 0;
        return exp_[size_t(log_[a]) + size_t(log_[b])];
    }

    uint8_t Inv(uint8_t a) const {
        return exp_[255 - size_t(log_[a])];
    }

private:
    uint8_t exp_[512] = {};
    uint8_t log_[256] = {};
};

const Gf256& Field() {
    static const Gf256 field;
    return field;
}

inline constexpr size_t kMaxRsData = 128;
inline constexpr size_t kMaxRsParity = 64;
inline constexpr uint8_t kRsDataOffset = 128;

class ReedSolomonFecScheme final : public FecScheme {
public:
    std::string_view Name() const override {
        return "rs";
    }

    size_t ParityPerGroup() const override {
        return parity_;
    }

    bool SetParityPerGroup(size_t count) override {
        if (count == 0 || count > kMaxRsParity) return false;
        parity_ = count;
        return true;
    }

    size_t MaxRecoverablePerGroup() const override {
        return parity_;
    }

    size_t Encode(std::span<const std::span<const uint8_t>> group,
        std::span<std::span<const uint8_t>> out) override {
        const size_t rows = std::min(parity_, out.size());
        if (rows == 0 || group.empty() || group.size() > kMaxRsData) return 0;
        const size_t longest = LongestMember(group);
        if (longest == 0) return 0;

        const size_t width = kFecLenPrefix + longest;
        scratch_.assign(rows * width, 0);
        symbol_.assign(width, 0);

        const Gf256& gf = Field();
        for (size_t j = 0; j < group.size(); ++j) {
            std::fill(symbol_.begin(), symbol_.end(), uint8_t(0));
            AccumulateMember(symbol_, group[j]);
            for (size_t i = 0; i < rows; ++i) {
                const uint8_t coefficient = Coefficient(gf, i, j);
                uint8_t* row = scratch_.data() + i * width;
                for (size_t b = 0; b < width; ++b) row[b] ^= gf.Mul(coefficient, symbol_[b]);
            }
        }

        for (size_t i = 0; i < rows; ++i)
            out[i] = std::span<const uint8_t>(scratch_).subspan(i * width, width);
        return rows;
    }

    size_t Recover(std::span<const FecSlot> group,
        std::span<const std::span<const uint8_t>> parity, std::span<FecRecovery> out) override {
        if (out.empty() || group.empty() || group.size() > kMaxRsData) return 0;

        missing_.clear();
        for (size_t i = 0; i < group.size(); ++i)
            if (!group[i].present) missing_.push_back(i);
        if (missing_.empty() || missing_.size() > out.size()) return 0;

        size_t width = 0;
        usable_.clear();
        for (size_t i = 0; i < parity.size() && usable_.size() < missing_.size(); ++i) {
            if (parity[i].size() < kFecLenPrefix) continue;
            if (width == 0) width = parity[i].size();
            if (parity[i].size() != width) continue;
            usable_.push_back(i);
        }
        if (usable_.size() < missing_.size() || width == 0) return 0;
        for (const FecSlot& slot : group)
            if (slot.present && kFecLenPrefix + slot.bytes.size() > width) return 0;

        const size_t unknowns = missing_.size();
        const Gf256& gf = Field();

        rhs_.assign(unknowns * width, 0);
        symbol_.assign(width, 0);
        for (size_t a = 0; a < unknowns; ++a) {
            uint8_t* target = rhs_.data() + a * width;
            const std::span<const uint8_t> row = parity[usable_[a]];
            for (size_t b = 0; b < width; ++b) target[b] = row[b];
        }
        for (size_t j = 0; j < group.size(); ++j) {
            if (!group[j].present) continue;
            std::fill(symbol_.begin(), symbol_.end(), uint8_t(0));
            AccumulateMember(symbol_, group[j].bytes);
            for (size_t a = 0; a < unknowns; ++a) {
                const uint8_t coefficient = Coefficient(gf, usable_[a], j);
                uint8_t* target = rhs_.data() + a * width;
                for (size_t b = 0; b < width; ++b) target[b] ^= gf.Mul(coefficient, symbol_[b]);
            }
        }

        matrix_.assign(unknowns * unknowns, 0);
        for (size_t a = 0; a < unknowns; ++a)
            for (size_t b = 0; b < unknowns; ++b)
                matrix_[a * unknowns + b] = Coefficient(gf, usable_[a], missing_[b]);

        if (!SolveInPlace(gf, unknowns, width)) return 0;

        size_t recovered = 0;
        for (size_t b = 0; b < unknowns; ++b) {
            const std::span<const uint8_t> symbol(rhs_.data() + b * width, width);
            const size_t len = LengthPrefixOf(symbol);
            if (len == 0 || len > kMaxVideoPayload || kFecLenPrefix + len > width) return 0;
            out[recovered].slot = missing_[b];
            out[recovered].bytes = symbol.subspan(kFecLenPrefix, len);
            ++recovered;
        }
        return recovered;
    }

private:
    static uint8_t Coefficient(const Gf256& gf, size_t parityRow, size_t dataColumn) {
        return gf.Inv(uint8_t(parityRow) ^ uint8_t(kRsDataOffset + dataColumn));
    }

    bool SolveInPlace(const Gf256& gf, size_t n, size_t width) {
        for (size_t col = 0; col < n; ++col) {
            size_t pivot = col;
            while (pivot < n && matrix_[pivot * n + col] == 0) ++pivot;
            if (pivot == n) return false;
            if (pivot != col) {
                for (size_t b = 0; b < n; ++b)
                    std::swap(matrix_[col * n + b], matrix_[pivot * n + b]);
                for (size_t b = 0; b < width; ++b)
                    std::swap(rhs_[col * width + b], rhs_[pivot * width + b]);
            }

            const uint8_t inverse = gf.Inv(matrix_[col * n + col]);
            for (size_t b = 0; b < n; ++b)
                matrix_[col * n + b] = gf.Mul(matrix_[col * n + b], inverse);
            for (size_t b = 0; b < width; ++b)
                rhs_[col * width + b] = gf.Mul(rhs_[col * width + b], inverse);

            for (size_t row = 0; row < n; ++row) {
                if (row == col) continue;
                const uint8_t factor = matrix_[row * n + col];
                if (factor == 0) continue;
                for (size_t b = 0; b < n; ++b)
                    matrix_[row * n + b] ^= gf.Mul(factor, matrix_[col * n + b]);
                for (size_t b = 0; b < width; ++b)
                    rhs_[row * width + b] ^= gf.Mul(factor, rhs_[col * width + b]);
            }
        }
        return true;
    }

    size_t parity_ = 2;
    std::vector<uint8_t> scratch_;
    std::vector<uint8_t> symbol_;
    std::vector<uint8_t> rhs_;
    std::vector<uint8_t> matrix_;
    std::vector<size_t> missing_;
    std::vector<size_t> usable_;
};

}

std::unique_ptr<FecScheme> MakeReedSolomon() {
    return std::make_unique<ReedSolomonFecScheme>();
}

}

#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace deskhub {

inline constexpr size_t kMaxFecRecoveryPerGroup = 64;

struct FecSlot {
    std::span<const uint8_t> bytes{};
    bool present = false;
};

struct FecRecovery {
    size_t slot = 0;
    std::span<const uint8_t> bytes{};
};

class FecScheme {
public:
    virtual ~FecScheme() = default;

    virtual std::string_view Name() const = 0;

    virtual size_t ParityPerGroup() const = 0;

    virtual bool SetParityPerGroup(size_t count) = 0;

    virtual size_t MaxRecoverablePerGroup() const = 0;

    virtual size_t Encode(std::span<const std::span<const uint8_t>> group,
        std::span<std::span<const uint8_t>> out) = 0;

    virtual size_t Recover(std::span<const FecSlot> group,
        std::span<const std::span<const uint8_t>> parity, std::span<FecRecovery> out) = 0;
};

inline constexpr std::string_view kDefaultFecScheme = "xor";

std::unique_ptr<FecScheme> MakeFecScheme(std::string_view name);

std::span<const std::string_view> FecSchemeNames();

bool IsFecSchemeName(std::string_view name);

}

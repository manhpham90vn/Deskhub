#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace deskhub {

inline constexpr int kQrMinVersion = 1;
inline constexpr int kQrMaxVersion = 40;

struct QrCode {
    int size = 0;
    std::vector<uint8_t> modules{};

    bool Dark(int x, int y) const;
};

std::optional<QrCode> EncodeQr(std::string_view text);

std::string RenderQrText(const QrCode& code);

}

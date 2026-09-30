#include "deskhub/net/Ipv4.h"

namespace deskhub {

std::optional<uint32_t> ParseIPv4(std::string_view text) {
    uint32_t ip = 0;
    size_t pos = 0;
    for (int octet = 0; octet < 4; ++octet) {
        if (octet > 0) {
            if (pos >= text.size() || text[pos] != '.') return std::nullopt;
            ++pos;
        }
        uint32_t value = 0;
        size_t digits = 0;
        while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
            value = value * 10 + uint32_t(text[pos] - '0');
            ++digits;
            ++pos;
            if (digits > 3 || value > 255) return std::nullopt;
        }
        if (digits == 0) return std::nullopt;
        ip = (ip << 8) | value;
    }
    if (pos != text.size()) return std::nullopt;
    return ip;
}

std::string FormatIPv4(uint32_t ip) {
    return std::to_string((ip >> 24) & 0xFF) + "." + std::to_string((ip >> 16) & 0xFF) + "." +
           std::to_string((ip >> 8) & 0xFF) + "." + std::to_string(ip & 0xFF);
}

}

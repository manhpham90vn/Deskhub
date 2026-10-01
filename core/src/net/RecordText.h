#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace deskhub::detail {

inline std::string Trim(std::string_view s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(b, e - b + 1));
}

inline bool ParseUnixTime(std::string_view s, int64_t& out) {
    if (s.empty() || s.size() > 19) return false;
    int64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        const int digit = c - '0';
        if (v > (std::numeric_limits<int64_t>::max() - digit) / 10) return false;
        v = v * 10 + digit;
    }
    out = v;
    return true;
}

inline std::string SanitizeText(std::string_view text, size_t maxBytes) {
    std::string out;
    for (char c : text) {
        const uint8_t u = uint8_t(c);
        if (u >= 0x20 && u != 0x7F) out.push_back(c);
        if (out.size() == maxBytes) break;
    }
    return Trim(out);
}

}

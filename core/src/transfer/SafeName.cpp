#include "deskhub/transfer/SafeName.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace deskhub {

namespace {

inline constexpr std::string_view kIllegalOnWindows = "<>:\"|?*";
inline constexpr size_t kMaxPreservedExtension = 32;

constexpr std::array<std::string_view, 6> kReservedStems = {
    "con", "prn", "aux", "nul", "conin$", "conout$"};
constexpr std::array<std::string_view, 2> kReservedPrefixes = {"com", "lpt"};
constexpr size_t kReservedPrefixLength = 3;
constexpr std::array<std::string_view, 3> kSuperscriptDigits = {
    "\xC2\xB9", "\xC2\xB2", "\xC2\xB3"};

std::string_view BaseName(std::string_view name) {
    const size_t cut = name.find_last_of("/\\");
    return cut == std::string_view::npos ? name : name.substr(cut + 1);
}

std::string ScrubBytes(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        const uint8_t u = uint8_t(c);
        if (u < 0x20 || u == 0x7F) continue;
        out.push_back(kIllegalOnWindows.find(c) == std::string_view::npos ? c : '_');
    }
    return out;
}

void StripTrailingDotsAndSpaces(std::string& name) {
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
}

std::string LowerAscii(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c); });
    return out;
}

size_t StemLength(std::string_view name) {
    const size_t dot = name.find('.');
    return dot == std::string_view::npos ? name.size() : dot;
}

bool IsPortNumber(std::string_view text) {
    if (text.size() == 1) return text[0] >= '0' && text[0] <= '9';
    return std::find(kSuperscriptDigits.begin(), kSuperscriptDigits.end(), text) !=
           kSuperscriptDigits.end();
}

bool IsReservedDeviceName(std::string_view name) {
    std::string stem = LowerAscii(name.substr(0, StemLength(name)));
    while (!stem.empty() && stem.back() == ' ') stem.pop_back();
    if (std::find(kReservedStems.begin(), kReservedStems.end(), stem) != kReservedStems.end())
        return true;
    if (stem.size() <= kReservedPrefixLength) return false;
    const std::string_view head(stem.data(), kReservedPrefixLength);
    if (std::find(kReservedPrefixes.begin(), kReservedPrefixes.end(), head) ==
        kReservedPrefixes.end())
        return false;
    return IsPortNumber(std::string_view(stem).substr(kReservedPrefixLength));
}

size_t Utf8TruncLen(std::string_view text, size_t limit) {
    if (text.size() <= limit) return text.size();
    size_t n = limit;
    while (n > 0 && (uint8_t(text[n]) & 0xC0) == 0x80) --n;
    return n;
}

std::string_view ExtensionOf(std::string_view name) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0) return {};
    const std::string_view ext = name.substr(dot);
    return ext.size() <= kMaxPreservedExtension ? ext : std::string_view{};
}

std::string Shorten(std::string_view name) {
    if (name.size() <= kMaxSafeNameBytes) return std::string(name);
    const std::string_view ext = ExtensionOf(name);
    const std::string_view stem = name.substr(0, name.size() - ext.size());
    const size_t room = kMaxSafeNameBytes - ext.size();
    const size_t keep = Utf8TruncLen(stem, room);
    if (keep == 0) return std::string(name.substr(0, Utf8TruncLen(name, kMaxSafeNameBytes)));
    return std::string(stem.substr(0, keep)) + std::string(ext);
}

}

std::string SafeFileName(std::string_view name) {
    std::string out = ScrubBytes(BaseName(name));
    StripTrailingDotsAndSpaces(out);
    if (out.empty() || out == "." || out == "..") return {};
    if (IsReservedDeviceName(out)) out.insert(out.begin(), '_');
    out = Shorten(out);
    StripTrailingDotsAndSpaces(out);
    if (out.empty() || out == "." || out == "..") return {};
    return out;
}

std::string UniqueFileName(std::string_view safeName,
    const std::function<bool(const std::string&)>& taken) {
    std::string first(safeName);
    if (first.empty() || !taken || !taken(first)) return first;

    const std::string_view ext = ExtensionOf(first);
    const std::string_view stem = std::string_view(first).substr(0, first.size() - ext.size());
    for (size_t n = 2; n <= kMaxSafeNameAttempts; ++n) {
        const std::string suffix = " (" + std::to_string(n) + ")";
        const size_t room = kMaxSafeNameBytes - ext.size() - suffix.size();
        std::string candidate = std::string(stem.substr(0, Utf8TruncLen(stem, room))) + suffix +
                                std::string(ext);
        if (!taken(candidate)) return candidate;
    }
    return {};
}

}

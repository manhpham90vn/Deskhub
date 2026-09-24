#include "deskhub/ui/Theme.h"

#include <array>
#include <cstddef>
#include <cstdio>

namespace deskhub::ui {

namespace {

struct ThemeEntry {
    ThemeColor color;
    Rgb light;
    Rgb dark;
};

constexpr Rgb Hex(uint32_t packed) {
    return Rgb{uint8_t(packed >> 16), uint8_t(packed >> 8), uint8_t(packed)};
}

constexpr ThemeEntry Same(ThemeColor color, uint32_t packed) {
    return ThemeEntry{color, Hex(packed), Hex(packed)};
}

constexpr ThemeEntry Pair(ThemeColor color, uint32_t light, uint32_t dark) {
    return ThemeEntry{color, Hex(light), Hex(dark)};
}

constexpr std::array<ThemeEntry, size_t(ThemeColor::Count)> kTheme{{
    Same(ThemeColor::Sidebar, 0x1f2937),
    Same(ThemeColor::SidebarHover, 0x374151),
    Same(ThemeColor::NavText, 0xd1d5db),
    Same(ThemeColor::Footnote, 0x94a3b8),
    Same(ThemeColor::Accent, 0x2563eb),
    Same(ThemeColor::AccentPressed, 0x1d4ed8),
    Pair(ThemeColor::AccentDisabled, 0x93c5fd, 0x1e3a8a),
    Pair(ThemeColor::Heading, 0x111827, 0xe5e7eb),
    Pair(ThemeColor::Muted, 0x6b7280, 0x9ca3af),
    Pair(ThemeColor::Disabled, 0x9ca3af, 0x6b7280),
    Pair(ThemeColor::Online, 0x00913c, 0x4ade80),
    Pair(ThemeColor::Offline, 0xc82828, 0xf87171),
    Pair(ThemeColor::OfflinePressed, 0xa51f1f, 0xdc2626),
    Pair(ThemeColor::Warning, 0xca6c08, 0xfbbf24),
    Pair(ThemeColor::WarningPressed, 0xa85a06, 0xd97706),
    Pair(ThemeColor::Page, 0xffffff, 0x000000),
    Pair(ThemeColor::Border, 0xd1d5db, 0x4b5563),
    Pair(ThemeColor::Control, 0xf9fafb, 0x1f2937),
    Pair(ThemeColor::ControlHover, 0xf3f4f6, 0x374151),
    Pair(ThemeColor::ControlPressed, 0xe5e7eb, 0x4b5563),
    Pair(ThemeColor::RowLine, 0xe5e7eb, 0x374151),
    Pair(ThemeColor::ViewerRow, 0xf9fafb, 0x111827),
    Pair(ThemeColor::PanelIdle, 0xf3f4f6, 0x1f2937),
    Pair(ThemeColor::PanelBusy, 0xebf3ff, 0x172554),
    Pair(ThemeColor::PanelLive, 0xe8faef, 0x052e16),
    Pair(ThemeColor::PasscodeCard, 0xeff4ff, 0x111827),
}};

constexpr bool EveryColorSitsAtItsOwnIndex() {
    for (size_t i = 0; i < kTheme.size(); ++i)
        if (size_t(kTheme[i].color) != i) return false;
    return true;
}

static_assert(EveryColorSitsAtItsOwnIndex(),
    "kTheme must list every ThemeColor once, in the order the enum declares them");

}

Rgb ThemeRgb(ThemeColor color, ThemeMode mode) {
    const size_t index = size_t(color);
    if (index >= kTheme.size()) return Rgb{};
    return mode == ThemeMode::Dark ? kTheme[index].dark : kTheme[index].light;
}

uint32_t PackRgb(Rgb color) {
    return uint32_t(color.r) << 16 | uint32_t(color.g) << 8 | uint32_t(color.b);
}

std::string CssHex(Rgb color) {
    char text[8] = {};
    std::snprintf(text, sizeof(text), "#%02x%02x%02x", color.r, color.g, color.b);
    return text;
}

}

#pragma once
#include <cstdint>
#include <string>

namespace deskhub::ui {

struct Rgb {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;

    bool operator==(const Rgb&) const = default;
};

enum class ThemeColor : uint8_t {
    Sidebar,
    SidebarHover,
    NavText,
    Footnote,
    Accent,
    AccentPressed,
    AccentDisabled,
    Heading,
    Muted,
    Disabled,
    Online,
    Offline,
    OfflinePressed,
    Warning,
    WarningPressed,
    Page,
    Border,
    Control,
    ControlHover,
    ControlPressed,
    RowLine,
    ViewerRow,
    PanelIdle,
    PanelBusy,
    PanelLive,
    PasscodeCard,
    Count,
};

enum class ThemeMode : uint8_t {
    Light,
    Dark,
};

Rgb ThemeRgb(ThemeColor color, ThemeMode mode);

uint32_t PackRgb(Rgb color);

std::string CssHex(Rgb color);

}

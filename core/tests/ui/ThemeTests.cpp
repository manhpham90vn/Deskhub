#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace deskhub;

namespace {

constexpr double kReadableBodyContrast = 4.5;
constexpr double kReadableHeadingContrast = 7.0;

double Linear(uint8_t channel) {
    const double c = channel / 255.0;
    return c <= 0.03928 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double Luminance(ui::Rgb color) {
    return 0.2126 * Linear(color.r) + 0.7152 * Linear(color.g) + 0.0722 * Linear(color.b);
}

double Contrast(ui::Rgb a, ui::Rgb b) {
    const double la = Luminance(a);
    const double lb = Luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

double TextContrast(ui::ThemeColor text, ui::ThemeMode mode) {
    return Contrast(ui::ThemeRgb(text, mode), ui::ThemeRgb(ui::ThemeColor::Page, mode));
}

void TestTextStaysReadableInBothModes() {
    std::printf("[theme] headings and hints stay readable on the page, light and dark...\n");
    for (const ui::ThemeMode mode : {ui::ThemeMode::Light, ui::ThemeMode::Dark}) {
        Check(TextContrast(ui::ThemeColor::Heading, mode) >= kReadableHeadingContrast,
            "a heading clears the enhanced contrast bar");
        Check(TextContrast(ui::ThemeColor::Muted, mode) >= kReadableBodyContrast,
            "a muted hint still clears the body-text contrast bar");
    }
}

void TestDarkModeFlipsTheTextNotTheBrand() {
    std::printf("[theme] dark mode swaps the text colours but keeps the brand colours...\n");
    Check(ui::ThemeRgb(ui::ThemeColor::Heading, ui::ThemeMode::Light) !=
              ui::ThemeRgb(ui::ThemeColor::Heading, ui::ThemeMode::Dark),
        "headings change with the appearance");
    Check(ui::ThemeRgb(ui::ThemeColor::Accent, ui::ThemeMode::Light) ==
              ui::ThemeRgb(ui::ThemeColor::Accent, ui::ThemeMode::Dark),
        "the accent is the same blue in both");
    Check(ui::ThemeRgb(ui::ThemeColor::Sidebar, ui::ThemeMode::Light) ==
              ui::ThemeRgb(ui::ThemeColor::Sidebar, ui::ThemeMode::Dark),
        "the sidebar is dark in both");
}

void TestColoursTravelAsHexAndPackedValues() {
    std::printf("[theme] a colour reaches CSS and the FFI unchanged...\n");
    const ui::Rgb accent = ui::ThemeRgb(ui::ThemeColor::Accent, ui::ThemeMode::Light);
    Check(ui::CssHex(accent) == "#2563eb", "CSS gets a lowercase #rrggbb");
    Check(ui::PackRgb(accent) == 0x2563EBu, "the FFI gets 0xRRGGBB");
    Check(ui::CssHex(ui::Rgb{0, 0, 0}) == "#000000", "leading zeros are kept");
}

void TestAnUnknownColourIsBlackNotACrash() {
    std::printf("[theme] a colour past the end of the table reads as black...\n");
    Check(ui::ThemeRgb(ui::ThemeColor::Count, ui::ThemeMode::Light) == ui::Rgb{},
        "out of range answers black instead of reading past the table");
}

}

void RunThemeTests() {
    TestTextStaysReadableInBothModes();
    TestDarkModeFlipsTheTextNotTheBrand();
    TestColoursTravelAsHexAndPackedValues();
    TestAnUnknownColourIsBlackNotACrash();
}

#pragma once
#include "deskhub/terminal/Screen.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace deskhub::term {

enum class TermKey : uint8_t {
    Char = 0,
    Enter,
    Backspace,
    Tab,
    Escape,
    Up,
    Down,
    Right,
    Left,
    Home,
    End,
    PageUp,
    PageDown,
    Insert,
    Delete,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
};

struct TermMods {
    bool shift = false;
    bool alt = false;
    bool ctrl = false;

    bool operator==(const TermMods&) const = default;
};

struct TermKeyEvent {
    TermKey key = TermKey::Char;
    char32_t codepoint = 0;
    TermMods mods{};
};

std::string EncodeKey(const TermKeyEvent& event, const TerminalModes& modes);
std::string EncodeText(std::string_view text, const TerminalModes& modes);

}

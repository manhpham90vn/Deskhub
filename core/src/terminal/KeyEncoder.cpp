#include "deskhub/terminal/KeyEncoder.h"

namespace deskhub::term {

namespace {

constexpr char kEsc = '\x1B';
constexpr char kDel = '\x7F';

int ModifierCode(const TermMods& mods) {
    int code = 1;
    if (mods.shift) code += 1;
    if (mods.alt) code += 2;
    if (mods.ctrl) code += 4;
    return code;
}

bool HasModifier(const TermMods& mods) {
    return mods.shift || mods.alt || mods.ctrl;
}

std::string CsiTilde(int number, const TermMods& mods) {
    std::string out(1, kEsc);
    out += '[';
    out += std::to_string(number);
    if (HasModifier(mods)) {
        out += ';';
        out += std::to_string(ModifierCode(mods));
    }
    out += '~';
    return out;
}

std::string CursorSequence(char final, const TermMods& mods, bool applicationCursor) {
    std::string out(1, kEsc);
    if (HasModifier(mods)) {
        out += "[1;";
        out += std::to_string(ModifierCode(mods));
        out += final;
        return out;
    }
    out += applicationCursor ? 'O' : '[';
    out += final;
    return out;
}

std::string FunctionKeySs3(char final, const TermMods& mods) {
    std::string out(1, kEsc);
    if (HasModifier(mods)) {
        out += "[1;";
        out += std::to_string(ModifierCode(mods));
        out += final;
        return out;
    }
    out += 'O';
    out += final;
    return out;
}

std::string ControlChar(char32_t cp) {
    if (cp >= U'a' && cp <= U'z') return std::string(1, char(cp - U'a' + 1));
    if (cp >= U'A' && cp <= U'Z') return std::string(1, char(cp - U'A' + 1));
    switch (cp) {
        case U' ':
        case U'@': return std::string(1, '\0');
        case U'[': return "\x1B";
        case U'\\': return "\x1C";
        case U']': return "\x1D";
        case U'^': return "\x1E";
        case U'_':
        case U'/': return "\x1F";
        case U'?': return std::string(1, kDel);
        default: return {};
    }
}

std::string WithAlt(std::string body, const TermMods& mods) {
    if (!mods.alt) return body;
    return std::string(1, kEsc) + body;
}

}

std::string EncodeKey(const TermKeyEvent& event, const TerminalModes& modes) {
    switch (event.key) {
        case TermKey::Char: {
            if (event.codepoint == 0) return {};
            if (event.mods.ctrl) {
                const std::string control = ControlChar(event.codepoint);
                if (!control.empty()) return WithAlt(control, event.mods);
            }
            return WithAlt(EncodeUtf8(event.codepoint), event.mods);
        }
        case TermKey::Enter: return WithAlt("\r", event.mods);
        case TermKey::Tab:
            if (event.mods.shift) return "\x1B[Z";
            return WithAlt("\t", event.mods);
        case TermKey::Escape: return WithAlt("\x1B", event.mods);
        case TermKey::Backspace:
            if (event.mods.ctrl) return WithAlt("\x08", event.mods);
            return WithAlt(std::string(1, kDel), event.mods);
        case TermKey::Up: return CursorSequence('A', event.mods, modes.applicationCursor);
        case TermKey::Down: return CursorSequence('B', event.mods, modes.applicationCursor);
        case TermKey::Right: return CursorSequence('C', event.mods, modes.applicationCursor);
        case TermKey::Left: return CursorSequence('D', event.mods, modes.applicationCursor);
        case TermKey::Home: return CursorSequence('H', event.mods, modes.applicationCursor);
        case TermKey::End: return CursorSequence('F', event.mods, modes.applicationCursor);
        case TermKey::Insert: return CsiTilde(2, event.mods);
        case TermKey::Delete: return CsiTilde(3, event.mods);
        case TermKey::PageUp: return CsiTilde(5, event.mods);
        case TermKey::PageDown: return CsiTilde(6, event.mods);
        case TermKey::F1: return FunctionKeySs3('P', event.mods);
        case TermKey::F2: return FunctionKeySs3('Q', event.mods);
        case TermKey::F3: return FunctionKeySs3('R', event.mods);
        case TermKey::F4: return FunctionKeySs3('S', event.mods);
        case TermKey::F5: return CsiTilde(15, event.mods);
        case TermKey::F6: return CsiTilde(17, event.mods);
        case TermKey::F7: return CsiTilde(18, event.mods);
        case TermKey::F8: return CsiTilde(19, event.mods);
        case TermKey::F9: return CsiTilde(20, event.mods);
        case TermKey::F10: return CsiTilde(21, event.mods);
        case TermKey::F11: return CsiTilde(23, event.mods);
        case TermKey::F12: return CsiTilde(24, event.mods);
    }
    return {};
}

std::string EncodeText(std::string_view text, const TerminalModes&) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '\n')
            out.push_back('\r');
        else
            out.push_back(c);
    }
    return out;
}

}

#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/terminal/KeyEncoder.h"

#include <cstdio>
#include <string>

using namespace deskhub::term;

namespace {

std::string Key(TermKey key, TermMods mods = {}, const TerminalModes& modes = {}) {
    TermKeyEvent ev;
    ev.key = key;
    ev.mods = mods;
    return EncodeKey(ev, modes);
}

std::string Ch(char32_t cp, TermMods mods = {}) {
    TermKeyEvent ev;
    ev.key = TermKey::Char;
    ev.codepoint = cp;
    ev.mods = mods;
    return EncodeKey(ev, TerminalModes{});
}

void TestPlainCharacters() {
    std::printf("[keys] letters go out as themselves, UTF-8 and all...\n");
    Check(Ch(U'a') == "a", "a letter is one byte");
    Check(Ch(0x1EA1) == "\xE1\xBA\xA1", "a Vietnamese letter goes out as UTF-8");
    Check(Ch(0x1F600) == "\xF0\x9F\x98\x80", "and so does an emoji");
    Check(Ch(0).empty(), "a key with no character behind it sends nothing");
}

void TestControlCombinations() {
    std::printf("[keys] Ctrl folds a letter into its control code - this is the one that matters...\n");
    Check(Ch(U'c', {false, false, true}) == std::string(1, '\x03'),
        "Ctrl-C is the byte that interrupts a running command");
    Check(Ch(U'C', {true, false, true}) == std::string(1, '\x03'),
        "Ctrl-Shift-C sends the same byte");
    Check(Ch(U'd', {false, false, true}) == std::string(1, '\x04'), "Ctrl-D ends the input");
    Check(Ch(U'z', {false, false, true}) == std::string(1, '\x1A'), "Ctrl-Z suspends it");
    Check(Ch(U'[', {false, false, true}) == "\x1B", "Ctrl-[ is another way to type Escape");
    Check(Ch(U'\\', {false, false, true}) == "\x1C", "Ctrl-backslash quits");
    Check(Ch(U']', {false, false, true}) == "\x1D" && Ch(U'^', {false, false, true}) == "\x1E",
        "the remaining bracket controls are there");
    Check(Ch(U'_', {false, false, true}) == "\x1F" && Ch(U'/', {false, false, true}) == "\x1F",
        "Ctrl-underscore and Ctrl-slash both send unit separator");
    Check(Ch(U' ', {false, false, true}) == std::string(1, '\0') &&
              Ch(U'@', {false, false, true}) == std::string(1, '\0'),
        "Ctrl-space and Ctrl-@ send a null byte");
    Check(Ch(U'?', {false, false, true}) == "\x7F", "Ctrl-? sends delete");
    Check(Ch(U'1', {false, false, true}) == "1",
        "a Ctrl combination with no control code sends the character itself");
}

void TestAltPrefix() {
    std::printf("[keys] Alt puts an escape in front of whatever the key sends...\n");
    Check(Ch(U'b', {false, true, false}) ==
              "\x1B"
              "b",
        "Alt-b is escape then b");
    Check(Ch(U'c', {false, true, true}) == std::string("\x1B") + '\x03',
        "Alt-Ctrl-C keeps both");
    Check(Key(TermKey::Enter, {false, true, false}) == "\x1B\r", "Alt-Enter too");
    Check(Key(TermKey::Escape, {false, true, false}) == "\x1B\x1B", "and Alt-Escape");
}

void TestNamedKeys() {
    std::printf("[keys] the keys a virtual keyboard has to add by hand...\n");
    Check(Key(TermKey::Escape) == "\x1B", "Escape is what gets you out of vim");
    Check(Key(TermKey::Enter) == "\r", "Enter sends a carriage return, not a newline");
    Check(Key(TermKey::Tab) == "\t", "Tab completes");
    Check(Key(TermKey::Tab, {true, false, false}) == "\x1B[Z", "Shift-Tab walks back");
    Check(Key(TermKey::Backspace) == "\x7F", "Backspace sends delete, as every unix shell expects");
    Check(Key(TermKey::Backspace, {false, false, true}) == "\x08",
        "Ctrl-Backspace sends the older code");
    Check(Key(TermKey::Backspace, {false, true, false}) == "\x1B\x7F", "Alt-Backspace deletes a word");

    Check(Key(TermKey::Insert) == "\x1B[2~" && Key(TermKey::Delete) == "\x1B[3~",
        "insert and delete are numbered sequences");
    Check(Key(TermKey::PageUp) == "\x1B[5~" && Key(TermKey::PageDown) == "\x1B[6~",
        "and so are the page keys");
    Check(Key(TermKey::Delete, {true, false, false}) == "\x1B[3;2~",
        "a modifier is spliced in before the tilde");
}

void TestArrowsFollowTheMode() {
    std::printf("[keys] arrows change shape when the program asks them to...\n");
    TerminalModes normal;
    TerminalModes application;
    application.applicationCursor = true;

    Check(Key(TermKey::Up, {}, normal) == "\x1B[A", "in normal mode an arrow is a CSI");
    Check(Key(TermKey::Up, {}, application) == "\x1BOA", "in application mode it is an SS3");
    Check(Key(TermKey::Down, {}, application) == "\x1BOB" &&
              Key(TermKey::Right, {}, application) == "\x1BOC" &&
              Key(TermKey::Left, {}, application) == "\x1BOD",
        "all four arrows follow the same rule");
    Check(Key(TermKey::Home, {}, application) == "\x1BOH" &&
              Key(TermKey::End, {}, application) == "\x1BOF",
        "so do Home and End");

    Check(Key(TermKey::Up, {false, false, true}, application) == "\x1B[1;5A",
        "a modified arrow is always a CSI, whatever the mode");
    Check(Key(TermKey::Right, {true, false, false}) == "\x1B[1;2C", "Shift is modifier 2");
    Check(Key(TermKey::Right, {false, true, false}) == "\x1B[1;3C", "Alt is 3");
    Check(Key(TermKey::Right, {false, false, true}) == "\x1B[1;5C", "Ctrl is 5");
    Check(Key(TermKey::Right, {true, true, true}) == "\x1B[1;8C", "and all three together are 8");
}

void TestFunctionKeys() {
    std::printf("[keys] the twelve function keys and their two shapes...\n");
    Check(Key(TermKey::F1) == "\x1BOP" && Key(TermKey::F2) == "\x1BOQ" &&
              Key(TermKey::F3) == "\x1BOR" && Key(TermKey::F4) == "\x1BOS",
        "F1 to F4 are SS3 sequences");
    Check(Key(TermKey::F5) == "\x1B[15~" && Key(TermKey::F6) == "\x1B[17~" &&
              Key(TermKey::F7) == "\x1B[18~" && Key(TermKey::F8) == "\x1B[19~",
        "F5 to F8 are numbered");
    Check(Key(TermKey::F9) == "\x1B[20~" && Key(TermKey::F10) == "\x1B[21~" &&
              Key(TermKey::F11) == "\x1B[23~" && Key(TermKey::F12) == "\x1B[24~",
        "and so are F9 to F12, with the gaps the standard leaves");
    Check(Key(TermKey::F1, {true, false, false}) == "\x1B[1;2P",
        "a modified F1 switches to the CSI form");
}

void TestTypedText() {
    std::printf("[keys] typed text is newline-corrected...\n");
    TerminalModes plain;
    Check(EncodeText("a\nb", plain) == "a\rb",
        "a newline in the text becomes the return the shell expects");
}

void TestEveryKeyProducesSomething() {
    std::printf("[keys] no key and modifier combination ever produces nothing by accident...\n");
    TerminalModes modes;
    for (int m = 0; m < 8; ++m) {
        const TermMods mods{(m & 1) != 0, (m & 2) != 0, (m & 4) != 0};
        modes.applicationCursor = (m & 1) != 0;
        for (uint8_t k = uint8_t(TermKey::Enter); k <= uint8_t(TermKey::F12); ++k) {
            TermKeyEvent ev;
            ev.key = TermKey(k);
            ev.mods = mods;
            Check(!EncodeKey(ev, modes).empty(), "every named key sends bytes");
        }
        for (char32_t cp = U'!'; cp <= U'~'; ++cp)
            Check(!Ch(cp, mods).empty(), "and so does every printable character");
    }
}

}

void RunKeyEncoderTests() {
    TestPlainCharacters();
    TestControlCombinations();
    TestAltPrefix();
    TestNamedKeys();
    TestArrowsFollowTheMode();
    TestFunctionKeys();
    TestTypedText();
    TestEveryKeyProducesSomething();
}

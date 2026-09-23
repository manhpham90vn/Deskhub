#include "deskhub/terminal/KeyEncoder.h"
#include "deskhub/terminal/Screen.h"
#include "deskhub/terminal/VtParser.h"

#include <cstddef>
#include <cstdint>
#include <span>

using namespace deskhub;
using namespace deskhub::term;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::span<const uint8_t> bytes(data, size);

    VtParser parser;
    std::vector<VtEvent> events;
    parser.Parse(bytes, events);

    const uint16_t cols = uint16_t(20 + (size % 60));
    const uint16_t rows = uint16_t(4 + (size % 20));
    Screen screen(TermSize{cols, rows}, 32);
    screen.Write(bytes);
    screen.TakeResponse();

    for (size_t at = 0; at + 1 < size; at += 512) {
        screen.Resize(TermSize{uint16_t(1 + data[at] % 200), uint16_t(1 + data[at + 1] % 60)});
        screen.Write(bytes.subspan(at));
    }

    TermKeyEvent key;
    key.key = TermKey(size ? data[0] % 27 : 0);
    key.codepoint = size > 1 ? char32_t(data[1]) : U'a';
    key.mods = TermMods{size > 2 && (data[2] & 1) != 0, size > 2 && (data[2] & 2) != 0,
        size > 2 && (data[2] & 4) != 0};
    EncodeKey(key, screen.Modes());
    EncodeText(std::string_view(reinterpret_cast<const char*>(data), size), screen.Modes());
    return 0;
}

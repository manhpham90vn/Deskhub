#include "deskhub/qr/QrCode.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

void Require(bool condition) {
    if (!condition) __builtin_trap();
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    const auto code = deskhub::EncodeQr(text);
    if (!code) {
        Require(size == 0 || size > 2331);
        return 0;
    }
    Require(code->size >= 21 && code->size <= 177 && (code->size - 21) % 4 == 0);
    Require(code->modules.size() == size_t(code->size) * size_t(code->size));
    int darkCount = 0;
    for (int y = 0; y < code->size; ++y) {
        for (int x = 0; x < code->size; ++x) darkCount += code->Dark(x, y) ? 1 : 0;
    }
    Require(darkCount > 0 && darkCount < code->size * code->size);
    Require(!code->Dark(-1, 0) && !code->Dark(0, -1) && !code->Dark(code->size, 0) && !code->Dark(0, code->size));
    const std::string rendered = deskhub::RenderQrText(*code);
    Require(!rendered.empty() && rendered.back() == '\n');
    return 0;
}

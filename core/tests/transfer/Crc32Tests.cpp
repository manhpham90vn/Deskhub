#include "Tests.h"
#include "support/TestSupport.h"

#include "deskhub/transfer/Crc32.h"

#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>
#include <vector>

using namespace deskhub;

namespace {

std::span<const uint8_t> Bytes(std::string_view text) {
    return {reinterpret_cast<const uint8_t*>(text.data()), text.size()};
}

uint32_t Crc32Of(std::span<const uint8_t> bytes) {
    Crc32 crc;
    crc.Update(bytes);
    return crc.Value();
}

void TestKnownVectors() {
    std::printf("[crc] the published CRC-32 vectors come out right...\n");
    Check(Crc32Of({}) == 0u, "the empty input hashes to zero");
    Check(Crc32Of(Bytes("a")) == 0xE8B7BE43u, "\"a\" matches the reference value");
    Check(Crc32Of(Bytes("123456789")) == 0xCBF43926u, "the check string matches");
    Check(Crc32Of(Bytes("The quick brown fox jumps over the lazy dog")) == 0x414FA339u,
        "a sentence matches the reference value");
}

void TestStreamingMatchesOneShot() {
    std::printf("[crc] hashing in pieces equals hashing all at once...\n");
    std::vector<uint8_t> data(10000);
    for (size_t i = 0; i < data.size(); ++i) data[i] = uint8_t(Rnd());

    const uint32_t oneShot = Crc32Of(data);
    Crc32 streamed;
    size_t at = 0;
    while (at < data.size()) {
        const size_t step = 1 + Rnd() % 997;
        const size_t take = at + step > data.size() ? data.size() - at : step;
        streamed.Update(std::span<const uint8_t>(data).subspan(at, take));
        at += take;
    }
    Check(streamed.Value() == oneShot, "a chunked file hashes the same as a whole one");
}

void TestResetStartsOver() {
    std::printf("[crc] Reset returns the state to the empty hash...\n");
    Crc32 crc;
    crc.Update(Bytes("noise"));
    crc.Reset();
    Check(crc.Value() == 0u, "after Reset the hash is the empty one again");
    crc.Update(Bytes("123456789"));
    Check(crc.Value() == 0xCBF43926u, "and the next file hashes cleanly");
}

void TestSingleBitFlipIsCaught() {
    std::printf("[crc] a one-bit change in a chunk changes the hash...\n");
    std::vector<uint8_t> data(4096, 0x5A);
    const uint32_t clean = Crc32Of(data);
    data[2048] ^= 0x01;
    Check(Crc32Of(data) != clean, "a flipped bit does not hash to the same value");
}

}

void RunCrc32Tests() {
    TestKnownVectors();
    TestStreamingMatchesOneShot();
    TestResetStartsOver();
    TestSingleBitFlipIsCaught();
}

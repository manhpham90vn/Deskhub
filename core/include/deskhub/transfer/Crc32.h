#pragma once
#include <cstdint>
#include <span>

namespace deskhub {

class Crc32 {
public:
    void Update(std::span<const uint8_t> bytes);

    uint32_t Value() const {
        return ~state_;
    }

    void Reset() {
        state_ = 0xFFFFFFFFu;
    }

private:
    uint32_t state_ = 0xFFFFFFFFu;
};

}

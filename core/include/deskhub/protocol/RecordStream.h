#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace deskhub {

inline constexpr size_t kMaxRecordBacklog = kRecordPrefixSize + kMaxRecordSize;

class RecordStream {
public:
    void Append(std::span<const uint8_t> bytes);
    bool Next(std::vector<uint8_t>& out);
    void Reset();

    bool Failed() const {
        return failed_;
    }

private:
    void Compact();

    std::vector<uint8_t> buffer_{};
    size_t consumed_ = 0;
    bool failed_ = false;
};

}

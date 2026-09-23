#pragma once
#include "deskhub/protocol/Wire.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace deskhubp {

inline constexpr uint32_t kPtyReadChunk = 4096;

class Pty {
public:
    Pty();
    ~Pty();
    Pty(const Pty&) = delete;
    Pty& operator=(const Pty&) = delete;

    bool Start(const std::string& shell, deskhub::TermSize size);
    bool Running() const;

    int Read(uint8_t* buf, size_t cap, uint32_t waitMs);
    bool Write(std::span<const uint8_t> bytes);
    bool Resize(deskhub::TermSize size);

    int ExitCode() const;

    void Close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

std::string DefaultShell();

}

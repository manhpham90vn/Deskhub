#pragma once
#include <cstdint>

namespace deskhubp {

struct ConsoleSize {
    uint16_t columns = 80;
    uint16_t rows = 24;
};

bool StdinIsTty();
ConsoleSize ConsoleSizeNow();

bool ConsoleResized();

class EchoOff {
public:
    EchoOff();
    ~EchoOff();
    EchoOff(const EchoOff&) = delete;
    EchoOff& operator=(const EchoOff&) = delete;

    bool active() const {
        return active_;
    }

private:
    bool active_ = false;
};

inline constexpr int kStdinTimedOut = -1;
inline constexpr int kStdinClosed = -2;

int ReadStdinByte(uint32_t timeoutMs);

class RawConsole {
public:
    RawConsole();
    ~RawConsole();
    RawConsole(const RawConsole&) = delete;
    RawConsole& operator=(const RawConsole&) = delete;

    bool active() const {
        return active_;
    }

private:
    bool active_ = false;
};

}

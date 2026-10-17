#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace deskhubp {

struct LocalPeer {
    int32_t pid = 0;
    uint32_t uid = 0;
    std::string exePath{};
    std::vector<uint8_t> auditToken{};
};

class LocalSocket {
public:
    LocalSocket() = default;
    ~LocalSocket();
    LocalSocket(LocalSocket&& other) noexcept;
    LocalSocket& operator=(LocalSocket&& other) noexcept;
    LocalSocket(const LocalSocket&) = delete;
    LocalSocket& operator=(const LocalSocket&) = delete;

    static LocalSocket Listen(const std::string& path, uint32_t mode);
    static LocalSocket Connect(const std::string& path);

    std::optional<LocalSocket> Accept(int timeoutMs);
    bool WriteAll(std::span<const uint8_t> bytes);
    bool ReadSome(std::span<uint8_t> into, size_t& got);
    bool ReadExactly(std::span<uint8_t> into);
    std::optional<LocalPeer> Peer() const;
    void Shutdown();
    void Close();

    bool Valid() const {
        return fd_ >= 0;
    }

private:
    explicit LocalSocket(int fd) : fd_(fd) {}

    int fd_ = -1;
};

}
